/*
    Copyright (C) 2010, 2011  Red Hat, Inc.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/
#include "abrtlib.h"
#include "../btparser/backtrace.h"
#include "../btparser/frame.h"
#include "../btparser/location.h"
#include "parse_options.h"

#define PROGNAME "abrt-action-generate-backtrace"

static const char *dump_dir_name = ".";


static void create_hash(char hash_str[SHA1_RESULT_LEN*2 + 1], const char *pInput)
{
    unsigned len;
    unsigned char hash2[SHA1_RESULT_LEN];
    sha1_ctx_t sha1ctx;

    sha1_begin(&sha1ctx);
    sha1_hash(pInput, strlen(pInput), &sha1ctx);
    sha1_end(hash2, &sha1ctx);
    len = SHA1_RESULT_LEN;

    char *d = hash_str;
    unsigned char *s = hash2;
    while (len)
    {
        *d++ = "0123456789abcdef"[*s >> 4];
        *d++ = "0123456789abcdef"[*s & 0xf];
        s++;
        len--;
    }
    *d = '\0';
    //log("hash2:%s str:'%s'", hash_str, pInput);
}

int main(int argc, char **argv)
{
    char *env_verbose = getenv("ABRT_VERBOSE");
    if (env_verbose)
        g_verbose = atoi(env_verbose);

    /* Can't keep these strings/structs static: _() doesn't support that */
    const char *program_usage_string = _(
        PROGNAME" [options] -d DIR\n"
        "\n"
        "Analyzes C/C++ backtrace, generates duplication hash, backtrace rating, and identifies crash function in dump directory DIR"
    );
    enum {
        OPT_v = 1 << 0,
        OPT_d = 1 << 1
    };
    /* Keep enum above and order of options below in sync! */
    struct options program_options[] = {
        OPT__VERBOSE(&g_verbose),
        OPT_STRING('d', NULL, &dump_dir_name, "DIR", _("Dump directory")),
        OPT_END()
    };
    /*unsigned opts =*/ parse_opts(argc, argv, program_options, program_usage_string);

    putenv(xasprintf("ABRT_VERBOSE=%u", g_verbose));

    char *pfx = getenv("ABRT_PROG_PREFIX");
    if (pfx && string_to_bool(pfx))
        msg_prefix = PROGNAME;

    struct dump_dir *dd = dd_opendir(dump_dir_name, /*flags:*/ 0);
    if (!dd)
        return 1;

    char *package = dd_load_text(dd, FILENAME_PACKAGE);
    char *executable = dd_load_text(dd, FILENAME_EXECUTABLE);

    /* Read backtrace */
    char *backtrace_str = dd_load_text_ext(dd, FILENAME_BACKTRACE,
                                           DD_LOAD_TEXT_RETURN_NULL_ON_FAILURE);
    if (!backtrace_str)
    {
        dd_close(dd);
        return 1;
    }

    /* Compute backtrace hash */
    struct btp_location location;
    btp_location_init(&location);
    char *backtrace_str_ptr = backtrace_str;
    struct btp_backtrace *backtrace = btp_backtrace_parse(&backtrace_str_ptr, &location);
    free(backtrace_str);

    /* Store backtrace hash */
    if (!backtrace)
    {
        /*
         * The parser failed. Compute the UUID from the executable
         * and package only.  This is not supposed to happen often.
         */
        log(_("Backtrace parsing failed for %s"), dump_dir_name);
        log("%d:%d: %s", location.line, location.column, location.message);
        struct strbuf *emptybt = strbuf_new();
        strbuf_prepend_str(emptybt, executable);
        strbuf_prepend_str(emptybt, package);

        char hash_str[SHA1_RESULT_LEN*2 + 1];
        create_hash(hash_str, emptybt->buf);

        dd_save_text(dd, FILENAME_DUPHASH, hash_str);
        /*
         * Other parts of ABRT assume that if no rating is available,
         * it is ok to allow reporting of the bug. To be sure no bad
         * backtrace is reported, rate the backtrace with the lowest
         * rating.
         */
        dd_save_text(dd, FILENAME_RATING, "0");

        strbuf_free(emptybt);
        free(package);
        free(executable);
        dd_close(dd);

        /* Report success even if the parser failed, as the backtrace
         * has been created and rated. The failure is caused by a flaw
         * in the parser, not in the backtrace.
         */
        return 0;
    }

    /* Compute duplication hash. */
    char *str_hash_core = btp_backtrace_get_duplication_hash(backtrace);
    struct strbuf *str_hash = strbuf_new();
    strbuf_append_str(str_hash, package);
    strbuf_append_str(str_hash, executable);
    strbuf_append_str(str_hash, str_hash_core);

    char hash_str[SHA1_RESULT_LEN*2 + 1];
    create_hash(hash_str, str_hash->buf);

    dd_save_text(dd, FILENAME_DUPHASH, hash_str);
    strbuf_free(str_hash);
    free(str_hash_core);

    /* Compute the backtrace rating. */
    float quality = btp_backtrace_quality_complex(backtrace);
    const char *rating;
    if (quality < 0.6f)
        rating = "0";
    else if (quality < 0.7f)
        rating = "1";
    else if (quality < 0.8f)
        rating = "2";
    else if (quality < 0.9f)
        rating = "3";
    else
        rating = "4";
    dd_save_text(dd, FILENAME_RATING, rating);

    /* Get the function name from the crash frame. */
    struct btp_frame *crash_frame = btp_backtrace_get_crash_frame(backtrace);
    if (crash_frame)
    {
        if (crash_frame->function_name &&
            0 != strcmp(crash_frame->function_name, "??"))
        {
            dd_save_text(dd, FILENAME_CRASH_FUNCTION, crash_frame->function_name);
        }
        btp_frame_free(crash_frame);
    }
    btp_backtrace_free(backtrace);
    dd_close(dd);

    free(executable);
    free(package);

    return 0;
}
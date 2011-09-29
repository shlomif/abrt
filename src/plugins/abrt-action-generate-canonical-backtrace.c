/*
    Copyright (C) 2011  ABRT team
    Copyright (C) 2011  RedHat Inc

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

/* 60 seconds was too limiting on slow machines */
static int exec_timeout_sec = 240;

struct backtrace_entry {
    unsigned long address;
    char *build_id;
    unsigned build_id_offset;
    char *symbol;
    char *modname;
    char *fingerprint;
    /* fde_entry ptr? */
    /* linked list next ptr? */
    /* pointer to function disassembly? */
};

struct fde_entry {
    unsigned long start;
    unsigned length;
};

static void backtrace_add_build_id(GList *backtrace, unsigned long start, unsigned length,
            const char *build_id, unsigned build_id_len, const char *modname, unsigned modname_len)
{
    struct backtrace_entry *entry;

    while (backtrace != NULL)
    {
        entry = backtrace->data;
        if (start <= entry->address && entry->address <= start+length)
        {
            /* NOTE: we could get by with just one copy of the string, but that
             * would mean more bookkeeping for us ... */
            entry->build_id = xstrndup(build_id, build_id_len);
            entry->build_id_offset = entry->address - start;
            entry->modname = xstrndup(modname, modname_len);
        }

        backtrace = g_list_next(backtrace);
    }
}

static void assign_build_ids(GList *backtrace, const char *dump_dir_name)
{
    /* Run eu-unstrip -n to obtain the ids. This should be rewritten to read
     * them directly from the core. */
    char *unstrip_output = run_unstrip_n(dump_dir_name, /*timeout_sec:*/ 30);
    if (unstrip_output == NULL)
        error_msg_and_die("Running eu-unstrip failed");

    const char *cur = unstrip_output;

    unsigned long start;
    unsigned length;
    const char *build_id;
    unsigned build_id_len;
    const char *modname;
    unsigned modname_len;

    int ret;
    int chars_read;

    while (*cur)
    {
        /* beginning of the line */

        /* START+SIZE */
        ret = sscanf(cur, "0x%lx+0x%x %n", &start, &length, &chars_read);
        if (ret < 2)
        {
            goto eat_line;
        }
        cur += chars_read;

        /* BUILDID */
        build_id = cur;
        while (isxdigit(*cur))
        {
            cur++;
        }
        build_id_len = cur-build_id;

        /* there may be @ADDR after the ID */
        cur = skip_non_whitespace(cur);
        cur = skip_whitespace(cur);

        /* FILE */
        cur = skip_non_whitespace(cur);
        cur = skip_whitespace(cur);

        /* DEBUGFILE */
        cur = skip_non_whitespace(cur);
        cur = skip_whitespace(cur);

        /* MODULENAME */
        modname = cur;
        cur = skip_non_whitespace(cur);
        modname_len = cur-modname;

        backtrace_add_build_id(backtrace, start, length,
                    build_id, build_id_len, modname, modname_len);

eat_line:
        while (*cur)
        {
            cur++;
            if (*(cur-1) == '\n')
            {
                break;
            }
        }
    }

    free(unstrip_output);
}

static GList *extract_addresses(const char *str)
{
    const char *cur = str;
    const char *sym;

    unsigned frame_number;
    unsigned next_frame = 0;
    unsigned long address;

    int ret;
    int chars_read;

    struct backtrace_entry *entry;
    GList *backtrace = NULL;

    while (*cur)
    {
        /* check whether current line describes frame and if we haven't seen it
         * already (gdb prints the first one on start) */
        ret = sscanf(cur, "#%u 0x%lx in %n", &frame_number, &address, &chars_read);
        if (ret < 2 || frame_number != next_frame)
        {
            goto eat_line;
        }
        next_frame++;
        cur += chars_read;

        /* is symbol available? */
        if (*cur && *cur != '?')
        {
            sym = cur;
            cur = skip_non_whitespace(cur);
            /* XXX: we might terminate the cycle here if sym is __libc_start_main */
        }
        else
        {
            sym = NULL;
        }

        entry = xmalloc(sizeof(*entry));
        entry->address = address;
        entry->symbol = (sym ? xstrndup(sym, cur-sym) : NULL);
        entry->build_id = entry->modname = NULL;
        entry->build_id_offset = 0;
        backtrace = g_list_append(backtrace, entry);

eat_line:
        while (*cur)
        {
            cur++;
            if (*(cur-1) == '\n')
            {
                break;
            }
        }
    }

    return backtrace;
}

/* copypasted from abrt-action-generate-backtrace */
static char *get_gdb_output(const char *dump_dir_name)
{
    struct dump_dir *dd = dd_opendir(dump_dir_name, /*flags:*/ 0);
    if (!dd)
        return NULL;

    char *uid_str = dd_load_text_ext(dd, FILENAME_UID, DD_FAIL_QUIETLY_ENOENT | DD_LOAD_TEXT_RETURN_NULL_ON_FAILURE);
    uid_t uid = -1L;
    if (uid_str)
    {
        uid = xatoi_positive(uid_str);
        free(uid_str);
        if (uid == geteuid())
        {
            uid = -1L; /* no need to setuid/gid if we are already under right uid */
        }
    }
    char *executable = dd_load_text(dd, FILENAME_EXECUTABLE);
    dd_close(dd);

    char *args[11];
    args[0] = (char*)"gdb";
    args[1] = (char*)"-batch";

    /* NOTE: Do we need this or is it an artifact from using nonsystem debug dirs? */
    args[2] = (char*)"-ex";
    args[3] = (char*)"set debug-file-directory /usr/lib/debug";

    /* "file BINARY_FILE" is needed, without it gdb cannot properly
     * unwind the stack. Currently the unwind information is located
     * in .eh_frame which is stored only in binary, not in coredump
     * or debuginfo.
     *
     * Fedora GDB does not strictly need it, it will find the binary
     * by its build-id.  But for binaries either without build-id
     * (= built on non-Fedora GCC) or which do not have
     * their debuginfo rpm installed gdb would not find BINARY_FILE
     * so it is still makes sense to supply "file BINARY_FILE".
     *
     * Unfortunately, "file BINARY_FILE" doesn't work well if BINARY_FILE
     * was deleted (as often happens during system updates):
     * gdb uses specified BINARY_FILE
     * even if it is completely unrelated to the coredump.
     * See https://bugzilla.redhat.com/show_bug.cgi?id=525721
     *
     * TODO: check mtimes on COREFILE and BINARY_FILE and not supply
     * BINARY_FILE if it is newer (to at least avoid gdb complaining).
     */
    args[4] = (char*)"-ex";
    args[5] = xasprintf("file %s", executable);
    free(executable);

    args[6] = (char*)"-ex";
    args[7] = xasprintf("core-file %s/"FILENAME_COREDUMP, dump_dir_name);

    args[8] = (char*)"-ex";
    /*args[9] = ... see below */
    args[10] = NULL;

    /* Get the backtrace, but try to cap its size */
    /* Limit bt depth. With no limit, gdb sometimes OOMs the machine */
    unsigned bt_depth = 2048;
    char *bt = NULL;
    while (1)
    {
        args[9] = xasprintf("backtrace %u", bt_depth);
        bt = exec_vp(args, uid, /*redirect_stderr:*/ 1, exec_timeout_sec, NULL);
        free(args[9]);
        if ((bt && strnlen(bt, 256*1024) < 256*1024) || bt_depth <= 32)
        {
            break;
        }

        free(bt);
        bt_depth /= 2;
    }

    free(args[5]);
    free(args[7]);
    return bt;
}

int main(int argc, char **argv)
{
    abrt_init(argv);

    const char *dump_dir_name = ".";

    /* Can't keep these strings/structs static: _() doesn't support that */
    const char *program_usage_string = _(
        "\1 [-v] -d DIR\n"
        "\n"
        "Creates canonical backtrace from core dump and corresponding binary"
    );
    enum {
        OPT_v = 1 << 0,
        OPT_d = 1 << 1,
    };
    /* Keep enum above and order of options below in sync! */
    struct options program_options[] = {
        OPT__VERBOSE(&g_verbose),
        OPT_STRING('d', NULL, &dump_dir_name, "DIR", _("Dump directory")),
        OPT_END()
    };
    /*unsigned opts =*/ parse_opts(argc, argv, program_options, program_usage_string);

    export_abrt_envvars(0);


    VERB1 log("Querying gdb for backtrace");
    char *gdb_out = get_gdb_output(dump_dir_name);
    if (gdb_out == NULL)
        return 1;

    /* parse addresses and eventual symbols from the output*/
    GList *backtrace = extract_addresses(gdb_out);
    VERB1 log("Extracted %d frames from the backtrace", g_list_length(backtrace));
    free(gdb_out);

    /* eu-unstrip - build ids and library paths*/
    VERB1 log("Running eu-unstrip -n to obtain build ids");
    assign_build_ids(backtrace, dump_dir_name);

    struct backtrace_entry *entry;
    while (backtrace)
    {
        entry = backtrace->data;
        printf("Addr: %lx, sym: %s, bid: %s, off: %x, mod: %s\n",
                    entry->address, entry->symbol, entry->build_id, entry->build_id_offset, entry->modname);

        backtrace = g_list_next(backtrace);
    }

    return 0;
}

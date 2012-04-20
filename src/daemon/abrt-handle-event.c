/*
    Copyright (C) 2011  ABRT Team
    Copyright (C) 2011  RedHat inc.

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

#include "libabrt.h"
#include "run_event.h"

#include <btparser/frame.h>
#include <btparser/thread.h>
#include <btparser/normalize.h>
#include <btparser/metrics.h>
#include <btparser/core-backtrace.h>

#define BACKTRACE_DUP_THRESHOLD 2

static char *uid = NULL;
static char *uuid = NULL;
static struct btp_thread *corebt = NULL;
static char *crash_dump_dup_name = NULL;


#if 0
/* Useful only for debugging. */
static void print_thread(const struct btp_thread *thread)
{
    struct btp_frame *frame;

    for (frame = thread->frames; frame != NULL; frame = frame->next)
    {
        struct frame_aux *aux = frame->user_data;
        printf("%s %s+0x%jx %s %s\n", frame->function_name, aux->build_id,
                (uintmax_t)frame->address, aux->modname, aux->fingerprint);
    }
}
#endif


static int core_backtrace_is_duplicate(struct btp_thread *bt1, const char *bt2_text)
{
    int result;
    struct btp_thread *bt2 = btp_load_core_backtrace(bt2_text);
    if (bt2 == NULL)
    {
        VERB1 log("Failed to parse backtrace, considering it not duplicate");
        return 0;
    }

    int distance = btp_thread_levenshtein_distance_custom(bt1, bt2, true,
                                                          btp_core_backtrace_frame_cmp);
    if (distance == -1)
    {
        result = 0;
    }
    else
    {
        VERB2 log("Distance between backtraces: %d", distance);
        result = (distance <= BACKTRACE_DUP_THRESHOLD);
    }

    btp_free_core_backtrace(bt2);

    return result;
}

static void dup_uuid_init(const struct dump_dir *dd)
{
    if (uuid)
        return; /* we already checked it, don't do it again */

    /* don't do uuid-based check on crashes that have backtrace available
     * XXX: this relies on the fact that backtrace is created in the same event as UUID
     */
    if (dd_exist(dd, FILENAME_CORE_BACKTRACE))
        return;

    uuid = dd_load_text_ext(dd, FILENAME_UUID,
                            DD_FAIL_QUIETLY_ENOENT + DD_LOAD_TEXT_RETURN_NULL_ON_FAILURE
    );
}

static int dup_uuid_compare(const struct dump_dir *dd)
{
    char *dd_uuid;
    int different;

    if (!uuid)
        return 0;

    dd_uuid = dd_load_text_ext(dd, FILENAME_UUID, DD_FAIL_QUIETLY_ENOENT);
    different = strcmp(uuid, dd_uuid);
    free(dd_uuid);

    if (!different)
        log("Duplicate: UUID");

    return !different;
}

static void dup_uuid_fini(void)
{
    free(uuid);
    uuid = NULL;
}

static void dup_corebt_init(const struct dump_dir *dd)
{
    char *corebt_text;

    if (corebt)
        return; /* already checked */

    corebt_text = dd_load_text_ext(dd, FILENAME_CORE_BACKTRACE,
                            DD_FAIL_QUIETLY_ENOENT | DD_LOAD_TEXT_RETURN_NULL_ON_FAILURE
    );

    if (!corebt_text)
        return; /* no backtrace */

    corebt = btp_load_core_backtrace(corebt_text);
    free(corebt_text);
}

static int dup_corebt_compare(const struct dump_dir *dd)
{
    if (!corebt)
        return 0;

    int isdup;
    char *dd_corebt = dd_load_text_ext(dd, FILENAME_CORE_BACKTRACE,
            DD_FAIL_QUIETLY_ENOENT | DD_LOAD_TEXT_RETURN_NULL_ON_FAILURE);

    if (!dd_corebt)
        return 0;

    isdup = core_backtrace_is_duplicate(corebt, dd_corebt);
    free(dd_corebt);

    if (isdup)
        log("Duplicate: core backtrace");

    return isdup;
}

static void dup_corebt_fini(void)
{
    btp_free_core_backtrace(corebt);
    corebt = NULL;
}

/* This function is run after each post-create event is finished (there may be
 * multiple such events).
 *
 * It first checks if there is CORE_BACKTRACE or UUID item in the dump dir
 * we are processing.
 *
 * If there is a CORE_BACKTRACE, it iterates over all other dump
 * directories and computes similarity to their core backtraces (if any).
 * If one of them is similar enough to be considered duplicate, the function
 * saves the path to the dump directory in question and returns 1 to indicate
 * that we have indeed found a duplicate of currently processed dump directory.
 * No more events are processed and program prints the path to the other
 * directory and returns failure.
 *
 * If there is an UUID item (and no core backtrace), the function again
 * iterates over all other dump directories and compares this UUID to their
 * UUID. If there is a match, the path to the duplicate is saved and 1 is returned.
 *
 * If duplicate is not found as described above, the function returns 0 and we
 * either process remaining events if there are any, or successfully terminate
 * processing of the current dump directory.
 */
static int is_crash_a_dup(const char *dump_dir_name, void *param)
{
    int retval = 0; /* defaults to no dup found, "run_event, please continue iterating" */

    struct dump_dir *dd = dd_opendir(dump_dir_name, DD_OPEN_READONLY);
    if (!dd)
        return 0; /* wtf? (error, but will be handled elsewhere later) */

    dup_uuid_init(dd);
    dup_corebt_init(dd);

    dd_close(dd);

    DIR *dir = opendir(g_settings_dump_location);
    if (dir == NULL)
        goto end;

    /* Scan crash dumps looking for a dup */
    //TODO: explain why this is safe wrt concurrent runs
    struct dirent *dent;
    while ((dent = readdir(dir)) != NULL)
    {
        if (dot_or_dotdot(dent->d_name))
            continue; /* skip "." and ".." */
        const char *ext = strrchr(dent->d_name, '.');
        if (ext && strcmp(ext, ".new") == 0)
            continue; /* skip anything named "<dirname>.new" */

        char *dump_dir_name2 = concat_path_file(g_settings_dump_location, dent->d_name);

        if (strcmp(dump_dir_name, dump_dir_name2) == 0)
            goto next; /* we are never a dup of ourself */

        dd = dd_opendir(dump_dir_name2, /*flags:*/ DD_FAIL_QUIETLY_ENOENT | DD_OPEN_READONLY);
        if (!dd)
            goto next;

        /* crashes of different users are not considered duplicates */
        char *dd_uid = dd_load_text_ext(dd, FILENAME_UID, DD_FAIL_QUIETLY_ENOENT);
        if (strcmp(uid, dd_uid))
        {
            dd_close(dd);
            goto next;
        }

        if (dup_uuid_compare(dd)
         || dup_corebt_compare(dd)
        ) {
            dd_close(dd);
            crash_dump_dup_name = dump_dir_name2;
            retval = 1; /* "run_event, please stop iterating" */
            goto end;
        }
        dd_close(dd);

next:
        free(dump_dir_name2);
    }
    closedir(dir);

end:
    return retval;
}

static char *do_log(char *log_line, void *param)
{
    /* We pipe output of events to our log (which usually
     * includes syslog). Otherwise, errors on post-create result in
     * "Corrupted or bad dump DIR, deleting" without adequate explanation why.
     */
    log("%s", log_line);
    return log_line;
}

int main(int argc, char **argv)
{
    /* I18n */
    setlocale(LC_ALL, "");
#if ENABLE_NLS
    bindtextdomain(PACKAGE, LOCALEDIR);
    textdomain(PACKAGE);
#endif

    abrt_init(argv);

    const char *program_usage_string = _(
        "& [-v] -e|--event EVENT DIR..."
        );

    char *event_name = NULL;
    struct options program_options[] = {
        OPT__VERBOSE(&g_verbose),
        OPT_GROUP(""),
        OPT_STRING('e', "event" , &event_name, "EVENT",  _("Run EVENT on DIR")),
        OPT_END()
    };

    parse_opts(argc, argv, program_options, program_usage_string);
    argv += optind;
    if (!*argv || !event_name)
        show_usage_and_die(program_usage_string, program_options);

    load_abrt_conf();

    bool post_create = (strcmp(event_name, "post-create") == 0);
    char *dump_dir_name = NULL;
    while (*argv)
    {
        dump_dir_name = xstrdup(*argv++);
        int i = strlen(dump_dir_name);
        while (--i >= 0)
            if (dump_dir_name[i] != '/')
                break;
        dump_dir_name[++i] = '\0';

        struct dump_dir *dd = dd_opendir(dump_dir_name, /*flags:*/ 0);
        if (!dd)
            return 1;

        uid = dd_load_text(dd, FILENAME_UID);
        dd_close(dd);

        struct run_event_state *run_state = new_run_event_state();
        if (post_create)
            run_state->post_run_callback = is_crash_a_dup;
        run_state->logging_callback = do_log;
        int r = run_event_on_dir_name(run_state, dump_dir_name, event_name);
        if (r == 0 && run_state->children_count == 0)
            error_msg_and_die("No actions are found for event '%s'", event_name);
        free_run_event_state(run_state);

//TODO: consider this case:
// new dump is created, post-create detects that it is a dup,
// but then load_crash_info(dup_name) *FAILS*.
// In this case, we later delete damaged dup_name (right?)
// but new dump never gets its FILENAME_COUNT set!

        /* Is crash a dup? (In this case, is_crash_a_dup() should have
         * aborted "post-create" event processing as soon as it saw uuid
         * and determined that there is another crash with same uuid.
         * In this case it sets state.crash_dump_dup_name)
         */
        if (!crash_dump_dup_name)
        {
            /* No. Was there error on one of processing steps in run_event? */
            if (r != 0)
                return r; /* yes */
        }
        else
        {
            error_msg_and_die("DUP_OF_DIR: %s", crash_dump_dup_name);
        }

        if (post_create)
        {
            dup_uuid_fini();
            dup_corebt_fini();
        }
    }

    /* exit 0 means, that there is no duplicate of dump-dir */
    return 0;
}

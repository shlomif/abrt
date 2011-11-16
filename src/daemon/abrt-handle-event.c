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

#include "abrtlib.h"
#include "run_event.h"

static char *uid = NULL;
static char *uuid = NULL;
static char *crash_dump_dup_name = NULL;

/* There may be more than one way to check whether the new dump dir is a
 * duplicate of another. Furthermore we may or may not run a particular test
 * depending on what files are available in newly created dump dir (files are
 * added as various post-create events are run).
 *
 * This is an attempt to abstract such checks into two functions. See the
 * functions dup_uuid_init and dup_uuid_compare for concrete example.
 */
struct dup_checker
{
    /* This function is called every time a post-create event finishes. It may
     * e.g. check whether the files needed are available (or whether this check
     * has already run and therefore does not need to do so again).
     * The argument passed is the dump directory of the newly created crash.
     * If it returns nonzero, it means the initialization was successful and we
     * should perform this check when iterating through other dump directories.
     * If it is zero, this check will be skipped.
     */
    int (*init)(const struct dump_dir*);

    /* If the initialization returned nonzero, this function will be called for
     * all other dump directories against which we perform a duplicate check.
     * The argument will be the dump_dir struct of the current directory
     * against which we are checking.
     * If it returns nonzero, it means that our new dumpdir and the dumpdir
     * associated with the arguments are duplicate.
     * Return value of zero either means that the function can't decide if it
     * is a duplicate or it decided it isn't a duplicate.
     */
    int (*compare)(const struct dump_dir*);
};

static int dup_uuid_init(const struct dump_dir *dd)
{
    if (uuid)
        return 0; /* we already checked it, don't do it again */

    uuid = dd_load_text_ext(dd, FILENAME_UUID,
                            DD_FAIL_QUIETLY_ENOENT + DD_LOAD_TEXT_RETURN_NULL_ON_FAILURE
    );

    if (!uuid)
        return 0; /* we don't have UUID (yet), maybe next time */

    return 1;
}

static int dup_uuid_compare(const struct dump_dir *dd)
{
    char *dd_uuid;
    int different;

    dd_uuid = dd_load_text_ext(dd, FILENAME_UUID, DD_FAIL_QUIETLY_ENOENT);
    different = strcmp(uuid, dd_uuid);
    free(dd_uuid);

    return !different;
}

/* All checkers must be present in this array. */
struct dup_checker checkers[] =
{
    {.init = dup_uuid_init, .compare = dup_uuid_compare},
    {.init = NULL, .compare = NULL}
};

static int is_crash_a_dup(const char *dump_dir_name, void *param)
{
    int retval = 0; /* defaults to no dup found, "run_event, please continue iterating" */

    struct dump_dir *dd = dd_opendir(dump_dir_name, DD_OPEN_READONLY);
    if (!dd)
        return 0; /* wtf? (error, but will be handled elsewhere later) */

    /* initialize list of checkers that will be used */
    struct dup_checker *checker;
    GList *active_checkers = NULL;
    for (checker = checkers; checker->init != NULL; checker++)
    {
        if (checker->init(dd))
        {
            active_checkers = g_list_append(active_checkers, checker);
        }
    }

    dd_close(dd);

    if (active_checkers == NULL)
        return 0; /* no checkers ready, skip checking altogether */

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

        /* try each of the currently active checkers */
        GList *li;
        for (li = active_checkers; li != NULL; li = g_list_next(li))
        {
            checker = li->data;
            if(checker->compare(dd))
            {
                dd_close(dd);
                crash_dump_dup_name = dump_dir_name2;
                retval = 1; /* "run_event, please stop iterating" */
                goto end;
            }
        }
        dd_close(dd);

next:
        free(dump_dir_name2);
    }
    closedir(dir);

end:
    g_list_free(active_checkers);
    return retval;
}

static char *do_log(char *log_line, void *param)
{
    /* We pipe output of post-create events to our log (which usually
     * includes syslog). Otherwise, errors on post-create result in
     * "Corrupted or bad dump DIR, deleting" without adequate explanation why.
     */
    log("%s", log_line);
    return log_line;
}

int main(int argc, char **argv)
{
    abrt_init(argv);

    const char *program_usage_string = _(
        "\b [-v] -e|--event EVENT DUMP_DIR [DUMP_DIR]..."
        );

    char *event_name = NULL;
    struct options program_options[] = {
        OPT__VERBOSE(&g_verbose),
        OPT_GROUP(""),
        OPT_STRING('e', "event" , &event_name, "EVENT",  _("Run EVENT on DUMP_DIR")),
        OPT_END()
    };

    parse_opts(argc, argv, program_options, program_usage_string);
    if (!argv[optind] || !event_name)
        show_usage_and_die(program_usage_string, program_options);

    load_abrt_conf();

    const char *dump_dir_name = NULL;
    while (argv[optind])
    {
        dump_dir_name = argv[optind++];

        struct dump_dir *dd = dd_opendir(dump_dir_name, /*flags:*/ 0);
        if (!dd)
            return 1;

        uid = dd_load_text(dd, FILENAME_UID);
        dd_close(dd);

        struct run_event_state *run_state = new_run_event_state();
        if (strcmp(event_name, "post-create") == 0)
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

            /* Was uuid created after all? (In this case, is_crash_a_dup()
             * should have fetched it and created uuid)
             */
            if (!uuid)
            {
                /* no */
                printf("Dump directory '%s' has no UUID element\n", dump_dir_name);
                return 1;
            }
        }
        else
        {
            printf("DUP_OF_DIR: %s\n", crash_dump_dup_name);
            free(crash_dump_dup_name);
            return 1;
        }
    }

    /* exit 0 means, that there is no duplicate of dump-dir*/
    return 0;
}

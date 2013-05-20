typedef enum
{
    SC_ZERO;
    SC_ONE;
    SC_TWO;
    SC_NEW;
} strange_constants;

void report_error(const char *summary, const char *description);

int main(int argc, char *argv[])
{
    strange_constants sc = SC_NEW;

    switch(sc)
    {
        case SC_ZERO:
            /* */
            break;
        case SC_ONE:
            /* */
            break;
        case SC_TWO:
            /* */
            break;
        default:
            report_error("Unknown return code %d!", sc, "More information here ...");
            exit(1);
    }

    return 0;
}

/* For some unknown reason we don't want to fork */
void report_error(const char *summary, const char *description)
{
    /* Ha Ha!! */
    free(g_emergency_pool);

    report *r = report_new();
    report_set_summary(summary);
    report_set_description(description);

    problem_data *pd = report_get_data(r);
    problem_data_add(pd, "/foo/blah", "A configuration", "Configuration with something");
    problem_data_add(pd, "/foo/blah.tar.gz", "All configuration files", "A bunch of configuration files");

    error *error;
    reporter *reporter = reporters_get(GITHUB_REPORTER, error);

    reporter_config *cfg = reporter_config_new("url", "http://github.com",
                                               "project", "foo",
                                               "repository", "custom-app", NULL);

    report_job *search = reporter_search_job(reporter, cfg, data, error);

    report_job_set_error_handler(search, ui_error_notifier);
    report_job_set_progress_watcher(search, ui_progress_notifier);

    report_job_run(search, );

    if (NULL == known)
    {
        report_entry *re = lr_reporter_create(report, data, error);
    }
}

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
    report_attachement_text_file("/foo/blah", "A configuration", "Configuration with something");
    report_attachement_binary_file("/foo/blah.tar.gz", "All configuration files", "A bunch of configuration files");

    report_add_attributes("package", PACKAGE, "version", VERSION, NULL);
    /* report_add_memory_block(); */

    error *error;
    reporter *reporter = reporters_get(GITHUB_REPORTER, error);

    reporter_config *cfg = reporter_config_new("url", "http://github.com",
                                               "project", "foo",
                                               "repository", "custom-app", NULL);

    reportrs *known = reporter_search(reporter, cfg, data, error);

    if (NULL == known)
    {
        report_entry *re = lr_reporter_create(report, data, error);
    }
}

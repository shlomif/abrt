/*
 * Utility routines.
 *
 * Licensed under GPLv2, see file COPYING in this tarball for details.
 */
#ifndef LIBABRT_H_
#define LIBABRT_H_

#include <gio/gio.h> /* dbus */
#include "abrt-dbus.h"
/* libreport's internal functions we use: */
#include <libreport/internal_libreport.h>
#include "hooklib.h"

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

/* Must be after #include "config.h" */
#if ENABLE_NLS
# include <libintl.h>
# define _(S) gettext(S)
#else
# define _(S) (S)
#endif

#undef NORETURN
#define NORETURN __attribute__ ((noreturn))

#undef ARRAY_SIZE
#define ARRAY_SIZE(x) ((unsigned)(sizeof(x) / sizeof((x)[0])))

#ifdef __cplusplus
extern "C" {
#endif

/* Some libc's forget to declare these, do it ourself */
extern char **environ;
#if defined(__GLIBC__) && __GLIBC__ < 2
int vdprintf(int d, const char *format, va_list ap);
#endif


#define check_free_space abrt_check_free_space
void check_free_space(unsigned setting_MaxCrashReportsSize, const char *dump_location);
#define trim_problem_dirs abrt_trim_problem_dirs
void trim_problem_dirs(const char *dirname, double cap_size, const char *exclude_path);
#define run_unstrip_n abrt_run_unstrip_n
char *run_unstrip_n(const char *dump_dir_name, unsigned timeout_sec);
#define get_backtrace abrt_get_backtrace
char *get_backtrace(const char *dump_dir_name, unsigned timeout_sec, const char *debuginfo_dirs);


#define g_settings_nMaxCrashReportsSize abrt_g_settings_nMaxCrashReportsSize
extern unsigned int  g_settings_nMaxCrashReportsSize;
#define g_settings_sWatchCrashdumpArchiveDir abrt_g_settings_sWatchCrashdumpArchiveDir
extern char *        g_settings_sWatchCrashdumpArchiveDir;
#define g_settings_dump_location abrt_g_settings_dump_location
extern char *        g_settings_dump_location;
#define g_settings_delete_uploaded abrt_g_settings_delete_uploaded
extern bool          g_settings_delete_uploaded;


#define load_abrt_conf abrt_load_abrt_conf
int load_abrt_conf();
#define free_abrt_conf_data abrt_free_abrt_conf_data
void free_abrt_conf_data();


void migrate_to_xdg_dirs(void);

int check_recent_crash_file(const char *filename, const char *executable);

/* Returns 1 if abrtd daemon is running, 0 otherwise. */
#define daemon_is_ok abrt_daemon_is_ok
int daemon_is_ok();

/* Note: should be public since unit tests need to call it */
#define koops_extract_version abrt_koops_extract_version
char *koops_extract_version(const char *line);
#define kernel_tainted_short abrt_kernel_tainted_short
char *kernel_tainted_short(const char *kernel_bt);
#define kernel_tainted_long abrt_kernel_tainted_long
char *kernel_tainted_long(const char *tainted_short);
#define koops_hash_str abrt_koops_hash_str
void koops_hash_str(char hash_str[SHA1_RESULT_LEN*2 + 1], char *oops_buf, const char *oops_ptr);
#define koops_extract_oopses abrt_koops_extract_oopses
void koops_extract_oopses(GList **oops_list, char *buffer, size_t buflen);
#define koops_print_suspicious_strings abrt_koops_print_suspicious_strings
void koops_print_suspicious_strings(void);

/* dbus client api */
int chown_dir_over_dbus(const char *problem_dir_path);
int delete_problem_dirs_over_dbus(const GList *problem_dir_paths);
problem_data_t *get_problem_data_dbus(const char *problem_dir_path);
GList *get_problems_over_dbus(bool authorize);


#ifdef __cplusplus
}
#endif

#endif

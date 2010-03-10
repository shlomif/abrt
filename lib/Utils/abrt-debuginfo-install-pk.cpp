/* -*-mode:c++;c-file-style:"bsd";c-basic-offset:4;indent-tabs-mode:nil-*-
    Copyright (C) 2010  Nikola Pajkovsky (npajkovs@redhat.com)
    Copyright (C) 2010  RedHat inc.

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


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "abrt-debuginfo-install-pk.h"

#define PK_EXIT_CODE_SYNTAX_INVALID	3
#define PK_EXIT_CODE_FILE_NOT_FOUND	4
#define PK_EXIT_CODE_NOTHING_USEFUL	5


#define DI_DIRECTORY "/usr/local/var/cache/abrt-di"


typedef struct {
	GPtrArray		*repo_enabled;
    GPtrArray       *repo_disabled;
	PkClient		*client;
} debuginfo_install_t;

static PkClient *client = NULL;
static GPtrArray* install_packages;

static gboolean pk_debuginfo_install_remove_suffix (gchar *name, const gchar *suffix);
static gboolean pk_debuginfo_install_remove_suffix (gchar *name, const gchar *suffix);

static void download_progress_cb(PkProgress *progress, PkProgressType type, gpointer user_data);
static void search_progress_cb(PkProgress *progress, PkProgressType type, gpointer user_data);



static void download_progress_cb(PkProgress *progress, PkProgressType type, gpointer user_data)
{
    gint percentage;
    gchar* package_id = NULL;
    if (type == PK_PROGRESS_TYPE_PERCENTAGE)
    {
        g_object_get (progress, "percentage", &percentage, NULL);
        g_print ("%s:\t%i\n", "Percentage", percentage);
        goto out;
    }

    if (type == PK_PROGRESS_TYPE_PACKAGE_ID)
    {
	    g_object_get (progress, "package-id", &package_id, NULL);
        g_print("Downloading package: %s\n", package_id);
    }
out:
    g_free(package_id);
}

static void search_progress_cb(PkProgress *progress, PkProgressType type, gpointer user_data)
{
    gchar* package_id = NULL;
    gchar* printable = NULL;
    if (type == PK_PROGRESS_TYPE_PACKAGE_ID)
    {
	    g_object_get (progress, "package-id", &package_id, NULL);
/*
        gchar** split = NULL;
        split = pk_package_id_split(package_id);
        pk_debuginfo_install_remove_suffix(split[0], "-debuginfo");
        package_id = pk_package_id_build(split[0], split[1], split[2], "updates");
*/
		printable = pk_package_id_to_printable (package_id);

        g_ptr_array_add(install_packages, package_id);

	    g_print ("%s:\t%s\t%s\n", "Package", package_id, printable);
    }
    g_free(printable);
}

static void abrt_debuginfo_install_print_repos(debuginfo_install_t *di)
{
    for (guint ii = 0; ii < di->repo_enabled->len; ii++)
        g_print("Enabled repos_id: %s\n", (gchar*)g_ptr_array_index(di->repo_enabled, ii));

    for (guint ii = 0; ii < di->repo_disabled->len; ii++)
        g_print("Disabled repos_id: %s\n", (gchar*)g_ptr_array_index(di->repo_disabled, ii));
}

/**
 * pk_debuginfo_install_remove_suffix:
 **/
static gboolean pk_debuginfo_install_remove_suffix (gchar *name, const gchar *suffix)
{
	gboolean ret = FALSE;
	guint slen, len;

	if (!g_str_has_suffix (name, suffix))
		goto out;

	// get lengths
	len = strlen (name);
	slen = strlen (suffix);

	// same string
	if (len == slen)
		goto out;

	// truncate
	name[len-slen] = '\0';
	ret = TRUE;
out:
	return ret;
}

/**
 * pk_debuginfo_install_name_to_debuginfo:
 **/
static gchar* pk_debuginfo_install_name_to_debuginfo (const gchar *name)
{
	gchar *name_debuginfo = NULL;
	gchar *name_tmp = NULL;

	// nothing
	if (name == NULL)
		goto out;

	name_tmp = g_strdup (name);

	// remove suffix
	pk_debuginfo_install_remove_suffix (name_tmp, "-libs");

	// append -debuginfo
	name_debuginfo = g_strjoin ("-", name_tmp, "debuginfo", NULL);
out:
	g_free (name_tmp);
	return name_debuginfo;
}
/**
 * pk_debuginfo_install_enable_repos:
 **/
/*
static gboolean pk_debuginfo_install_enable_repos (debuginfo_install_t *di, GPtrArray *array, gboolean enable, GError **error)
{
	guint i;
	gboolean ret = TRUE;
	PkResults *results = NULL;
	gchar *repo_id;
	GError *error_local = NULL;
	PkError *error_code = NULL;

	// enable all debuginfo repos we found
	for (i = 0; i < array->len; i++)
    {
		repo_id = (gchar*) g_ptr_array_index (array, i);

		// enable this repo
		results = pk_client_repo_enable (client, repo_id, enable, NULL, NULL, NULL, &error_local);
		if (results == NULL) {
			*error = g_error_new (1, 0, "failed to enable %s: %s", repo_id, error_local->message);
			g_error_free (error_local);
			ret = FALSE;
			goto out;
		}

		// check error code
		error_code = pk_results_get_error_code (results);
		if (error_code != NULL) {
			*error = g_error_new (1, 0, "failed to enable repo: %s, %s",
                                    pk_error_enum_to_text (pk_error_get_code (error_code)),
                                    pk_error_get_details (error_code)
                                    );
			ret = FALSE;
			goto out;
		}

		g_object_unref (results);
	}
out:
	if (error_code != NULL)
		g_object_unref (error_code);
	return ret;
}
*/

static gchar* pk_debuginfo_install_resolve_name_to_id (const gchar *package_name, GError **error)
{
	PkResults *results = NULL;
	PkPackage *item;
	PkError *error_code = NULL;
	GPtrArray *list = NULL;
	GError *error_local = NULL;
	gchar **names;
	gchar *package_id = NULL;

	// resolve takes a char**
	names = g_strsplit (package_name, ";", -1);

	// resolve
	results = pk_client_resolve (client, pk_bitfield_from_enums (PK_FILTER_ENUM_NEWEST, -1), names, NULL, NULL, NULL, &error_local);
	if (results == NULL)
    {
		*error = g_error_new (1, 0, "failed to resolve: %s", error_local->message);
		g_error_free (error_local);
		goto out;
	}

	// check error code
	error_code = pk_results_get_error_code (results);
	if (error_code != NULL)
    {
		*error = g_error_new (1, 0, "failed to resolve: %s, %s", pk_error_enum_to_text (pk_error_get_code (error_code)), pk_error_get_details (error_code));
		goto out;
	}

	// check we only got one match
	list = pk_results_get_package_array (results);
	if (list->len == 0)
    {
		*error = g_error_new (1, 0, "no package %s found", package_name);
		goto out;
	}

	if (list->len > 1)
    {
		*error = g_error_new (1, 0, "more than one package found for %s", package_name);
		goto out;
	}

	// get the package id
	item =(PkPackage*) g_ptr_array_index (list, 0);
	package_id = g_strdup (pk_package_get_id (item));
out:
	if (error_code != NULL)
		g_object_unref (error_code);

	if (results != NULL)
		g_object_unref (results);

	if (list != NULL)
		g_ptr_array_unref (list);

	g_strfreev (names);
	return package_id;
}


static gboolean pk_debuginfo_install_add_deps(gchar **packages_search, GPtrArray *packages_results, GError **error)
{
	gboolean ret = TRUE;
	PkResults *results = NULL;
	PkPackage *item;
	gchar *package_id = NULL;
	GPtrArray *list = NULL;
	GError *error_local = NULL;
	gchar **package_ids = NULL;
	gchar *name_debuginfo;
	guint i;
	gchar **split;
	PkError *error_code = NULL;

	// get depends for them all, not adding dup's
	results = pk_client_get_depends (client, PK_FILTER_ENUM_NONE, packages_search, TRUE, NULL, NULL, NULL, &error_local);
	if (results == NULL)
    {
		*error = g_error_new (1, 0, "failed to get_depends: %s", error_local->message);
		g_error_free (error_local);
		ret = FALSE;
		goto out;
	}

	// check error code
	error_code = pk_results_get_error_code (results);
	if (error_code != NULL)
    {
		*error = g_error_new (1, 0, "failed to get depends: %s, %s", pk_error_enum_to_text (pk_error_get_code (error_code)), pk_error_get_details (error_code));
		ret = FALSE;
		goto out;
	}

	// add dependant packages
	list = pk_results_get_package_array (results);
    g_print("Found %i dependencies\n", list->len);
	for (i = 0; i < list->len; i++)
    {
		item = (PkPackage*) g_ptr_array_index (list, i);
		split = pk_package_id_split (pk_package_get_id (item));

		// add -debuginfo
		name_debuginfo = pk_debuginfo_install_name_to_debuginfo (split[PK_PACKAGE_ID_NAME]);
		g_strfreev (split);

		// resolve name
		package_id = pk_debuginfo_install_resolve_name_to_id (name_debuginfo, &error_local);
		if (package_id == NULL)
        {
			g_print ("Failed to find the package %s, or already installed: %s\n", name_debuginfo, error_local->message);
			g_error_free (error_local);

            // don't quit, this is non-fatal
			error = NULL;
		}

        // add to array to install
		if (package_id != NULL && !g_str_has_suffix (package_id, "installed"))
        {
            g_print("%s\n", package_id);
			g_ptr_array_add (packages_results, g_strdup (package_id));
		}

		g_free (package_id);
		g_free (name_debuginfo);
	}
out:
	if (error_code != NULL)
		g_object_unref (error_code);
	if (results != NULL)
		g_object_unref (results);
	if (list != NULL)
		g_ptr_array_unref (list);
	g_strfreev (package_ids);
	return ret;
}

/**
 * pk_debuginfo_install_get_repo_list:
 **/
static gboolean pk_debuginfo_install_get_repo_list (debuginfo_install_t *di, GError **error)
{
	gboolean ret = FALSE;
	PkResults *results = NULL;
	guint i;
	GPtrArray *array;
	GError *error_local = NULL;
	PkRepoDetail *item;
	PkError *error_code = NULL;
	gboolean enabled;
	gchar *repo_id;

	// get all repo details
	results = pk_client_get_repo_list (client, PK_FILTER_ENUM_NONE, NULL, NULL, NULL, &error_local);
	if (results == NULL)
    {
		*error = g_error_new (1, 0, "failed to get repo list: %s", error_local->message);
		g_error_free (error_local);
		goto out;
	}

	// check error code
	error_code = pk_results_get_error_code (results);
	if (error_code != NULL)
    {
		*error = g_error_new (1, 0, "failed to get repo list: %s, %s",
                                pk_error_enum_to_text (pk_error_get_code (error_code)),
                                pk_error_get_details (error_code)
                                );
		goto out;
	}

	// get results
	array = pk_results_get_repo_detail_array (results);
	for (i = 0; i < array->len; i++)
    {
		item = (PkRepoDetail*) g_ptr_array_index (array, i);
		g_object_get (item, "enabled", &enabled, "repo-id", &repo_id, NULL);
		if (enabled)
        {
			g_ptr_array_add (di->repo_enabled, repo_id);
        }
        else
        {
			g_ptr_array_add (di->repo_disabled, repo_id);
        }

	}
	ret = TRUE;
out:
	if (error_code != NULL)
		g_object_unref (error_code);
	if (results != NULL)
		g_object_unref (results);
	return ret;
}

static char** build_id2path(gchar** build_ids)
{
    GPtrArray *ids = g_ptr_array_new();
    for (int ii = 0; build_ids[ii] != NULL; ii++)
    {
        char prefix[2];
        strncpy(prefix, build_ids[ii], sizeof(prefix));
        prefix[2]='\0';
        build_ids[ii] += 2;
        gchar* path = g_strdup_printf("/usr/lib/debug/.build-id/%s/%s", prefix, build_ids[ii]);
        g_ptr_array_add(ids, path);
    }

    return pk_ptr_array_to_strv(ids);
}

// returned value need to be freed
char** build_id2package(const char** build_id)
{
	GError *error = NULL;
	PkError *error_code = NULL;
	PkResults *results = NULL;

    g_type_init ();

    install_packages = g_ptr_array_new();

    gchar** debuginfo_path = build_id2path((gchar**)build_id);

    client = pk_client_new();

    results = pk_client_search_files(client, 0, debuginfo_path, NULL, search_progress_cb, NULL, &error);
	if (results == NULL)
    {
		g_print("failed to find package %s", error->message);
        return NULL;
	}

	// check error code
	error_code = pk_results_get_error_code (results);
	if (error_code != NULL)
    {
		g_print("failed to find package %s, %s",
                                pk_error_enum_to_text (pk_error_get_code (error_code)),
                                pk_error_get_details (error_code)
                                );
        return NULL;
	}

    g_object_unref (client);
    return pk_ptr_array_to_strv(install_packages);
}

int download_package(char** package_id, const char **repos_id)
{
    GError* error = NULL;
	gboolean ret;
    gint retval;
    GPtrArray* package_ids_to_install = NULL;
	package_ids_to_install = g_ptr_array_new ();

    gchar** package = (gchar**)package_id;
    gchar** repos = (gchar**)repos_id;


    g_type_init ();
    client = pk_client_new();

	ret = pk_debuginfo_install_add_deps (package, package_ids_to_install, &error);
	if (!ret)
    {
		g_print ("FAILED: Could not find dependant packages: %s\n", error->message);
		g_error_free (error);

		// return correct failure retval
		retval = -1;
		goto out;
	}


    debuginfo_install_t* di;
    di = g_new0 (debuginfo_install_t, 1);

	// store as strings
	di->repo_enabled = g_ptr_array_new();
	di->repo_disabled = g_ptr_array_new();

    if (repos == NULL)
    {
        ret = pk_debuginfo_install_get_repo_list (di, &error);
	    if (!ret) {
		    g_print ("FAILED: Failed to enable sources list: %s\n", error->message);
		    g_error_free (error);

		    // return correct failure retval
		    retval = -1;
		    goto out;
	    }
    }
    else
    {
        for (int ii = 0; repos[ii] != NULL; ii++)
	        g_ptr_array_add (di->repo_enabled, repos[ii]);
    }
    abrt_debuginfo_install_print_repos(di);

    client = pk_client_new();

    pk_client_download_packages(client, pk_ptr_array_to_strv(package_ids_to_install), DI_DIRECTORY,  NULL, download_progress_cb, NULL, &error);

    g_object_unref (client);
out:
    return retval;
}

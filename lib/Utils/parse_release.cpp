/*
    Copyright (C) 2010  ABRT team
    Copyright (C) 2010  RedHat Inc

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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

using namespace std;

void parse_release(const char *pRelease, string& pProduct, string& pVersion)
{
    if (strstr(pRelease, "Rawhide"))
    {
        pProduct = "Fedora";
        pVersion = "rawhide";
        VERB3 log("%s: version:'%s' product:'%s'", __func__, pVersion.c_str(), pProduct.c_str());
        return;
    }
    if (strstr(pRelease, "Fedora"))
    {
        pProduct = "Fedora";
    }
    else if (strstr(pRelease, "Red Hat Enterprise Linux"))
    {
        pProduct = "Red Hat Enterprise Linux ";
    }

    const char *release = strstr(pRelease, "release");
    const char *space = release ? strchr(release, ' ') : NULL;

    if (space++) while (*space != '\0' && *space != ' ')
    {
        /* Eat string like "5.2" */
        pVersion += *space;
        if (pProduct == "Red Hat Enterprise Linux ")
        {
            pProduct += *space;
        }
        space++;
    }
    VERB3 log("%s: version:'%s' product:'%s'", __func__, pVersion.c_str(), pProduct.c_str());
}

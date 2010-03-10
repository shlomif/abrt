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

#include <dbus/dbus-glib.h>
#include <packagekit-glib2/packagekit.h>
#include <sys/types.h>

char** build_id2package(const char** build_id);

int download_package(char** package_id, const char** repos_id);

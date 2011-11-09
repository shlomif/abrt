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
#ifndef CANONICAL_BACKTRACE_H
#define CANONICAL_BACKTRACE_H 1

#ifdef __cplusplus
extern "C" {
#endif

/* Parse canonical backtrace from string. Returns NULL on error. */
#define load_canonical_backtrace abrt_load_canonical_backtrace
struct btp_thread* load_canonical_backtrace(const char *text);

/* Destroy canonical backtrace created by load_canonical_backtrace(). */
#define free_canonical_backtrace abrt_free_canonical_backtrace
void free_canonical_backtrace(struct btp_thread *thread);

/* Returns 1 when backtraces come from duplicate bug, 0 if they don't or error occured. */
#define canonical_backtrace_is_duplicate abrt_canonical_backtrace_is_duplicate
int canonical_backtrace_is_duplicate(struct btp_thread *bt1, const char *bt2_text);

#ifdef __cplusplus
}
#endif

#endif

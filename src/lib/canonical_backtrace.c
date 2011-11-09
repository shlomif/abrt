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

#include <btparser/frame.h>
#include <btparser/thread.h>
#include <btparser/normalize.h>
#include <btparser/metrics.h>

#define BACKTRACE_TRUNCATE_LENGTH 7
#define BACKTRACE_DUP_THRESHOLD 2

struct frame_aux
{
    char *build_id;
    char *modname;
    char *fingerprint;
};

static void free_frame_aux(void *user_data)
{
    struct frame_aux *aux = user_data;

    if (aux)
    {
        free(aux->build_id);
        free(aux->modname);
        free(aux->fingerprint);
    }
    free(aux);
}

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

static char *read_string(const char **inptr)
{
    const char *cur = *inptr;
    const char *str;
    int len;

    cur = skip_whitespace(cur);
    str = cur;
    cur = skip_non_whitespace(cur);

    len = cur-str;
    *inptr = cur;

    if (len == 1 && *str == '-')
    {
        return NULL;
    }

    return xstrndup(str, len);
}

struct btp_thread* load_canonical_backtrace(const char *text)
{
    const char *cur = text;
    int ret;
    int chars_read;
    uintmax_t off;

    struct btp_thread *thread = xmalloc(sizeof(*thread));
    struct btp_frame **prev_link = &(thread->frames);

    /* Parse the text. */
    while (*cur)
    {
        struct btp_frame *frame = xmalloc(sizeof(*frame));
        btp_frame_init(frame);
        struct frame_aux *aux = xmalloc(sizeof(*aux));
        frame->user_data = aux;
        frame->user_data_destructor = free_frame_aux;
        *prev_link = frame;
        prev_link = &(frame->next);

        /* BUILD ID */
        aux->build_id = read_string(&cur);

        /* OFFSET */
        cur = skip_whitespace(cur);
        ret = sscanf(cur, "0x%jx %n", &off, &chars_read);
        if (ret < 1)
        {
            btp_thread_free(thread);
            VERB1 log("Error parsing canonical backtrace");
            return NULL;
        }
        cur += chars_read;
        frame->address = (uint64_t)off;

        /* SYMBOL */
        char *symbol = read_string(&cur);
        /* btparser uses "??" to denote unknown function name */
        if (!symbol)
        {
            frame->function_name = xstrdup("??");
        }
        else
        {
            frame->function_name = symbol;
        }

        /* MODNAME */
        aux->modname = read_string(&cur);

        /* FINGERPRINT */
        aux->fingerprint = read_string(&cur);

        /* Skip the rest of the line. */
        while (*cur)
        {
            cur++;
            if (*(cur-1) == '\n')
            {
                break;
            }
        }
    }

    btp_normalize_thread(thread);
    btp_thread_remove_frames_below_n(thread, BACKTRACE_TRUNCATE_LENGTH);

    return thread;
}

void free_canonical_backtrace(struct btp_thread *thread)
{
    if (thread)
        btp_thread_free(thread);
}

static int canonical_backtrace_frame_compare(struct btp_frame *frame1, struct btp_frame *frame2)
{
    /* If both function names are known, compare them directly. */
    if (frame1->function_name && frame2->function_name
      && strcmp(frame1->function_name, "??") != 0
      && strcmp(frame2->function_name, "??") != 0)
    {
        return strcmp(frame1->function_name, frame2->function_name);
    }

    struct frame_aux *aux1 = frame1->user_data;
    struct frame_aux *aux2 = frame2->user_data;

    /* If build ids are equal, we can compare the offsets.
     * Note that this may miss the case where the same function is called from
     * other function in multiple places, which would pass if we were comparing
     * the function names. */
    if (aux1->build_id && aux2->build_id
      && strcmp(aux1->build_id, aux2->build_id) == 0)
    {
        return (frame1->address - frame2->address);
    }

    /* Compare the fingerprints if present. */
    if (aux1->fingerprint && aux2->fingerprint)
    {
        return strcmp(aux1->fingerprint, aux2->fingerprint);
    }

    /* No match, assume the functions are different. */
    return -1;
}

int canonical_backtrace_is_duplicate(struct btp_thread *bt1, const char *bt2_text)
{
    int result;
    struct btp_thread *bt2 = load_canonical_backtrace(bt2_text);
    if (bt2 == NULL)
    {
        VERB1 log("Failed to parse backtrace, considering it not duplicate");
        return 0;
    }

    int distance = thread_levenshtein_distance_custom(bt1, bt2, true, canonical_backtrace_frame_compare);
    if (distance == -1)
    {
        result = 0;
    }
    else
    {
        VERB2 log("Distance between backtraces: %d", distance);
        result = (distance <= BACKTRACE_DUP_THRESHOLD);
    }

    free_canonical_backtrace(bt2);

    return result;
}

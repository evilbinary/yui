#include "text_style_runs.h"

#include <stdlib.h>
#include <string.h>

void text_style_runs_free(TextStyleRun* runs)
{
    free(runs);
}

TextStyleRun* text_style_runs_clone(const TextStyleRun* runs, int count)
{
    TextStyleRun* copy;
    if (!runs || count <= 0) {
        return NULL;
    }
    copy = (TextStyleRun*)malloc((size_t)count * sizeof(TextStyleRun));
    if (!copy) {
        return NULL;
    }
    memcpy(copy, runs, (size_t)count * sizeof(TextStyleRun));
    return copy;
}

static void text_style_runs_normalize(TextStyleRun** runs, int* count)
{
    TextStyleRun* src;
    TextStyleRun* dst;
    int n;
    int i;
    int w;

    if (!runs || !count || !*runs || *count <= 0) {
        return;
    }
    src = *runs;
    n = *count;
    dst = (TextStyleRun*)malloc((size_t)n * sizeof(TextStyleRun));
    if (!dst) {
        return;
    }
    w = 0;
    for (i = 0; i < n; i++) {
        if (src[i].end <= src[i].start || src[i].flags == 0) {
            continue;
        }
        if (w > 0 &&
            dst[w - 1].end == src[i].start &&
            dst[w - 1].flags == src[i].flags) {
            dst[w - 1].end = src[i].end;
        } else {
            dst[w++] = src[i];
        }
    }
    free(src);
    if (w == 0) {
        free(dst);
        *runs = NULL;
        *count = 0;
        return;
    }
    *runs = dst;
    *count = w;
}

void text_style_runs_insert(TextStyleRun** runs, int* count, int pos, int n)
{
    int i;
    if (!runs || !count || n <= 0) {
        return;
    }
    for (i = 0; i < *count; i++) {
        TextStyleRun* r = &(*runs)[i];
        if (r->end <= pos) {
            continue;
        }
        if (r->start >= pos) {
            r->start += n;
            r->end += n;
        } else {
            /* insert inside run: expand */
            r->end += n;
        }
    }
}

void text_style_runs_delete(TextStyleRun** runs, int* count, int a, int b)
{
    int i;
    int del;
    if (!runs || !count || !*runs || a >= b) {
        return;
    }
    del = b - a;
    for (i = 0; i < *count; i++) {
        TextStyleRun* r = &(*runs)[i];
        if (r->end <= a) {
            continue;
        }
        if (r->start >= b) {
            r->start -= del;
            r->end -= del;
            continue;
        }
        /* overlap */
        if (r->start < a && r->end > b) {
            r->end -= del;
        } else if (r->start >= a && r->end <= b) {
            r->start = r->end; /* mark empty */
        } else if (r->start < a) {
            r->end = a;
        } else {
            r->start = a;
            r->end -= del;
            if (r->end < r->start) {
                r->end = r->start;
            }
        }
    }
    text_style_runs_normalize(runs, count);
}

static int text_style_runs_ensure_split(TextStyleRun** runs, int* count, int pos)
{
    TextStyleRun* items;
    int i;
    int n;
    if (!runs || !count || pos < 0) {
        return 0;
    }
    n = *count;
    for (i = 0; i < n; i++) {
        TextStyleRun* r = &(*runs)[i];
        if (pos <= r->start || pos >= r->end) {
            continue;
        }
        items = (TextStyleRun*)realloc(*runs, (size_t)(n + 1) * sizeof(TextStyleRun));
        if (!items) {
            return 0;
        }
        *runs = items;
        memmove(&items[i + 1], &items[i], (size_t)(n - i) * sizeof(TextStyleRun));
        items[i].end = pos;
        items[i + 1].start = pos;
        (*count)++;
        return 1;
    }
    return 1;
}

void text_style_runs_apply(TextStyleRun** runs, int* count, int a, int b,
                           uint32_t set_flags, uint32_t clear_flags)
{
    TextStyleRun* items;
    int i;
    int n;
    if (!runs || !count || a >= b) {
        return;
    }
    if (!*runs) {
        *count = 0;
    }
    text_style_runs_ensure_split(runs, count, a);
    text_style_runs_ensure_split(runs, count, b);

    /* Cover gaps with new runs when setting flags */
    if (set_flags) {
        int covered = a;
        n = *count;
        for (i = 0; i < n; i++) {
            TextStyleRun* r = &(*runs)[i];
            if (r->end <= a || r->start >= b) {
                continue;
            }
            if (r->start > covered && set_flags) {
                /* insert gap run [covered, r->start) */
                items = (TextStyleRun*)realloc(*runs, (size_t)(*count + 1) * sizeof(TextStyleRun));
                if (!items) {
                    break;
                }
                *runs = items;
                memmove(&items[i + 1], &items[i], (size_t)(*count - i) * sizeof(TextStyleRun));
                items[i].start = covered;
                items[i].end = items[i + 1].start;
                items[i].flags = set_flags;
                (*count)++;
                n = *count;
                i++;
            }
            covered = r->end > covered ? r->end : covered;
        }
        if (covered < b && set_flags) {
            items = (TextStyleRun*)realloc(*runs, (size_t)(*count + 1) * sizeof(TextStyleRun));
            if (items) {
                *runs = items;
                items[*count].start = covered;
                items[*count].end = b;
                items[*count].flags = set_flags;
                (*count)++;
            }
        }
        text_style_runs_ensure_split(runs, count, a);
        text_style_runs_ensure_split(runs, count, b);
    }

    for (i = 0; i < *count; i++) {
        TextStyleRun* r = &(*runs)[i];
        if (r->end <= a || r->start >= b) {
            continue;
        }
        r->flags |= set_flags;
        r->flags &= ~clear_flags;
    }
    text_style_runs_normalize(runs, count);
}

void text_style_runs_toggle(TextStyleRun** runs, int* count, int a, int b, uint32_t flags)
{
    uint32_t common;
    if (a >= b || flags == 0) {
        return;
    }
    common = text_style_runs_common_flags(*runs, *count, a, b);
    if ((common & flags) == flags) {
        text_style_runs_apply(runs, count, a, b, 0, flags);
    } else {
        text_style_runs_apply(runs, count, a, b, flags, 0);
    }
}

uint32_t text_style_runs_common_flags(const TextStyleRun* runs, int count, int a, int b)
{
    uint32_t common = (uint32_t)~0u;
    int covered = 0;
    int i;
    if (a >= b) {
        return 0;
    }
    for (i = 0; i < count; i++) {
        int s = runs[i].start;
        int e = runs[i].end;
        if (e <= a || s >= b) {
            continue;
        }
        if (s < a) s = a;
        if (e > b) e = b;
        if (e <= s) {
            continue;
        }
        common &= runs[i].flags;
        covered += e - s;
    }
    if (covered < (b - a)) {
        /* gaps have flags 0 */
        common = 0;
    }
    return common == (uint32_t)~0u ? 0 : common;
}

uint32_t text_style_runs_flags_at(const TextStyleRun* runs, int count, int pos)
{
    int i;
    for (i = 0; i < count; i++) {
        if (pos >= runs[i].start && pos < runs[i].end) {
            return runs[i].flags;
        }
    }
    return 0;
}

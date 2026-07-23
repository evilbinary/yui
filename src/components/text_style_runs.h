#ifndef YUI_TEXT_STYLE_RUNS_H
#define YUI_TEXT_STYLE_RUNS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    TEXT_STYLE_BOLD = 1 << 0,
    TEXT_STYLE_ITALIC = 1 << 1
} TextStyleFlags;

typedef struct TextStyleRun {
    int start; /* byte offset, half-open [start, end) */
    int end;
    uint32_t flags;
} TextStyleRun;

void text_style_runs_free(TextStyleRun* runs);
TextStyleRun* text_style_runs_clone(const TextStyleRun* runs, int count);

/* Shift runs after inserting n bytes at pos. */
void text_style_runs_insert(TextStyleRun** runs, int* count, int pos, int n);

/* Remove [a,b) and shift following runs. */
void text_style_runs_delete(TextStyleRun** runs, int* count, int a, int b);

/* Apply set/clear flags on [a,b); merges adjacent identical runs. */
void text_style_runs_apply(TextStyleRun** runs, int* count, int a, int b,
                           uint32_t set_flags, uint32_t clear_flags);

/* Toggle flags on range. */
void text_style_runs_toggle(TextStyleRun** runs, int* count, int a, int b, uint32_t flags);

/* Flags common to entire [a,b); 0 if empty/mixed for that bit is still reported via out_any. */
uint32_t text_style_runs_common_flags(const TextStyleRun* runs, int count, int a, int b);

/* Flags at caret (typing style fallback separate). */
uint32_t text_style_runs_flags_at(const TextStyleRun* runs, int count, int pos);

#ifdef __cplusplus
}
#endif

#endif

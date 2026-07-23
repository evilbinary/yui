#ifndef YUI_TEXT_EDIT_HISTORY_H
#define YUI_TEXT_EDIT_HISTORY_H

#include "text_style_runs.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    TEXT_EDIT_TYPING = 1,
    TEXT_EDIT_DELETE = 2,
    TEXT_EDIT_REPLACE = 3,
    TEXT_EDIT_STYLE = 4
} TextEditKind;

typedef struct TextEditSnapshot {
    char* text;
    int cursor_pos;
    int selection_start;
    int selection_end;
    TextStyleRun* runs;
    int run_count;
    uint32_t typing_style;
} TextEditSnapshot;

typedef struct TextEditHistory {
    TextEditSnapshot** undo;
    int undo_count;
    int undo_cap;
    TextEditSnapshot** redo;
    int redo_count;
    int redo_cap;
    int max_depth;
    int coalescing;
    int coalesce_kind;
    unsigned int coalesce_ticks;
    unsigned int coalesce_window_ms;
} TextEditHistory;

void text_edit_history_init(TextEditHistory* history, int max_depth);
void text_edit_history_clear(TextEditHistory* history);
void text_edit_history_free(TextEditHistory* history);

TextEditSnapshot* text_edit_snapshot_create(const char* text,
                                            int cursor_pos,
                                            int selection_start,
                                            int selection_end,
                                            const TextStyleRun* runs,
                                            int run_count,
                                            uint32_t typing_style);
void text_edit_snapshot_free(TextEditSnapshot* snap);

/* Capture current state before a mutating edit. Coalesces TYPING/DELETE. */
void text_edit_history_before_edit(TextEditHistory* history,
                                   TextEditSnapshot* current,
                                   TextEditKind kind,
                                   unsigned int now_ms);

int text_edit_history_can_undo(const TextEditHistory* history);
int text_edit_history_can_redo(const TextEditHistory* history);

/* Pops undo and returns snapshot to apply; caller must free. Pushes `current` to redo. */
TextEditSnapshot* text_edit_history_undo(TextEditHistory* history, TextEditSnapshot* current);
TextEditSnapshot* text_edit_history_redo(TextEditHistory* history, TextEditSnapshot* current);

#ifdef __cplusplus
}
#endif

#endif

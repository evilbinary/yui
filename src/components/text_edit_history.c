#include "text_edit_history.h"

#include <stdlib.h>
#include <string.h>

static void text_edit_history_clear_stack(TextEditSnapshot*** stack, int* count)
{
    int i;
    if (!stack || !*stack || !count) {
        return;
    }
    for (i = 0; i < *count; i++) {
        text_edit_snapshot_free((*stack)[i]);
        (*stack)[i] = NULL;
    }
    *count = 0;
}

void text_edit_snapshot_free(TextEditSnapshot* snap)
{
    if (!snap) {
        return;
    }
    free(snap->text);
    text_style_runs_free(snap->runs);
    free(snap);
}

TextEditSnapshot* text_edit_snapshot_create(const char* text,
                                            int cursor_pos,
                                            int selection_start,
                                            int selection_end,
                                            const TextStyleRun* runs,
                                            int run_count,
                                            uint32_t typing_style)
{
    TextEditSnapshot* snap = (TextEditSnapshot*)calloc(1, sizeof(TextEditSnapshot));
    if (!snap) {
        return NULL;
    }
    snap->text = text ? strdup(text) : strdup("");
    if (!snap->text) {
        free(snap);
        return NULL;
    }
    snap->cursor_pos = cursor_pos;
    snap->selection_start = selection_start;
    snap->selection_end = selection_end;
    snap->typing_style = typing_style;
    if (runs && run_count > 0) {
        snap->runs = text_style_runs_clone(runs, run_count);
        snap->run_count = snap->runs ? run_count : 0;
    }
    return snap;
}

void text_edit_history_init(TextEditHistory* history, int max_depth)
{
    if (!history) {
        return;
    }
    memset(history, 0, sizeof(*history));
    history->max_depth = max_depth > 0 ? max_depth : 80;
    history->coalesce_window_ms = 400;
}

void text_edit_history_clear(TextEditHistory* history)
{
    if (!history) {
        return;
    }
    text_edit_history_clear_stack(&history->undo, &history->undo_count);
    text_edit_history_clear_stack(&history->redo, &history->redo_count);
    history->coalescing = 0;
    history->coalesce_kind = 0;
    history->coalesce_ticks = 0;
}

void text_edit_history_free(TextEditHistory* history)
{
    if (!history) {
        return;
    }
    text_edit_history_clear(history);
    free(history->undo);
    free(history->redo);
    history->undo = NULL;
    history->redo = NULL;
    history->undo_cap = 0;
    history->redo_cap = 0;
}

static int text_edit_history_push(TextEditSnapshot*** stack, int* count, int* cap,
                                  int max_depth, TextEditSnapshot* snap)
{
    TextEditSnapshot** items;
    if (!stack || !count || !cap || !snap) {
        text_edit_snapshot_free(snap);
        return 0;
    }
    if (*count >= max_depth && max_depth > 0) {
        text_edit_snapshot_free((*stack)[0]);
        memmove(*stack, *stack + 1, (size_t)(*count - 1) * sizeof(TextEditSnapshot*));
        (*count)--;
    }
    if (*count >= *cap) {
        int new_cap = *cap ? (*cap * 2) : 16;
        items = (TextEditSnapshot**)realloc(*stack, (size_t)new_cap * sizeof(TextEditSnapshot*));
        if (!items) {
            text_edit_snapshot_free(snap);
            return 0;
        }
        *stack = items;
        *cap = new_cap;
    }
    (*stack)[(*count)++] = snap;
    return 1;
}

static TextEditSnapshot* text_edit_history_pop(TextEditSnapshot*** stack, int* count)
{
    TextEditSnapshot* snap;
    if (!stack || !count || *count <= 0) {
        return NULL;
    }
    snap = (*stack)[--(*count)];
    (*stack)[*count] = NULL;
    return snap;
}

void text_edit_history_before_edit(TextEditHistory* history,
                                   TextEditSnapshot* current,
                                   TextEditKind kind,
                                   unsigned int now_ms)
{
    int can_coalesce;

    if (!history || !current) {
        text_edit_snapshot_free(current);
        return;
    }

    can_coalesce = 0;
    if (history->coalescing &&
        (kind == TEXT_EDIT_TYPING || kind == TEXT_EDIT_DELETE) &&
        kind == history->coalesce_kind &&
        now_ms >= history->coalesce_ticks &&
        (now_ms - history->coalesce_ticks) <= history->coalesce_window_ms) {
        can_coalesce = 1;
    }

    if (can_coalesce) {
        history->coalesce_ticks = now_ms;
        text_edit_snapshot_free(current);
        return;
    }

    text_edit_history_clear_stack(&history->redo, &history->redo_count);
    text_edit_history_push(&history->undo, &history->undo_count, &history->undo_cap,
                           history->max_depth, current);

    if (kind == TEXT_EDIT_TYPING || kind == TEXT_EDIT_DELETE) {
        history->coalescing = 1;
        history->coalesce_kind = kind;
        history->coalesce_ticks = now_ms;
    } else {
        history->coalescing = 0;
        history->coalesce_kind = 0;
    }
}

int text_edit_history_can_undo(const TextEditHistory* history)
{
    return history && history->undo_count > 0;
}

int text_edit_history_can_redo(const TextEditHistory* history)
{
    return history && history->redo_count > 0;
}

TextEditSnapshot* text_edit_history_undo(TextEditHistory* history, TextEditSnapshot* current)
{
    TextEditSnapshot* prev;
    if (!history || !text_edit_history_can_undo(history)) {
        text_edit_snapshot_free(current);
        return NULL;
    }
    history->coalescing = 0;
    if (current) {
        text_edit_history_push(&history->redo, &history->redo_count, &history->redo_cap,
                               history->max_depth, current);
    }
    prev = text_edit_history_pop(&history->undo, &history->undo_count);
    return prev;
}

TextEditSnapshot* text_edit_history_redo(TextEditHistory* history, TextEditSnapshot* current)
{
    TextEditSnapshot* next;
    if (!history || !text_edit_history_can_redo(history)) {
        text_edit_snapshot_free(current);
        return NULL;
    }
    history->coalescing = 0;
    if (current) {
        text_edit_history_push(&history->undo, &history->undo_count, &history->undo_cap,
                               history->max_depth, current);
    }
    next = text_edit_history_pop(&history->redo, &history->redo_count);
    return next;
}

#include "text_component.h"
#include "../ytype.h"
#include "../layer.h"
#include "../layer_update.h"
#include "../event.h"
#include "../backend.h"
#include "../render.h"
#include "../util.h"
#include "text_edit_history.h"
#include "text_style_runs.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>


#define TEXT_LINE_SPACING 2
#define TEXT_LINE_NUMBER_GAP 12
#define TEXT_DEFAULT_PADDING 5
#define TEXT_INTERNAL_SCROLLBAR_WIDTH 5

static void text_component_get_padding(Layer* layer, int* top, int* right, int* bottom, int* left)
{
    int pad_top;
    int pad_right;
    int pad_bottom;
    int pad_left;

    if (!layer) {
        if (top) {
            *top = TEXT_DEFAULT_PADDING;
        }
        if (right) {
            *right = TEXT_DEFAULT_PADDING;
        }
        if (bottom) {
            *bottom = TEXT_DEFAULT_PADDING;
        }
        if (left) {
            *left = TEXT_DEFAULT_PADDING;
        }
        return;
    }

    pad_top = layer_padding_get(layer, 0);
    pad_right = layer_padding_get(layer, 1);
    pad_bottom = layer_padding_get(layer, 2);
    pad_left = layer_padding_get(layer, 3);
    if (pad_top == 0 && pad_right == 0 && pad_bottom == 0 && pad_left == 0) {
        pad_top = pad_right = pad_bottom = pad_left = TEXT_DEFAULT_PADDING;
    }

    if (top) {
        *top = pad_top;
    }
    if (right) {
        *right = pad_right;
    }
    if (bottom) {
        *bottom = pad_bottom;
    }
    if (left) {
        *left = pad_left;
    }
}

static int text_component_get_left_padding(TextComponent* component) {
    int left_padding = TEXT_DEFAULT_PADDING;
    Layer* layer = component ? component->layer : NULL;

    if (layer) {
        left_padding = layer_padding_get(layer, 3);
        if (left_padding == 0) {
            left_padding = TEXT_DEFAULT_PADDING;
        }
    }
    if (component && component->show_line_numbers && component->multiline) {
        left_padding += component->line_number_width + TEXT_LINE_NUMBER_GAP;
    }
    return left_padding;
}

static void text_component_get_content_rect(TextComponent* component, Layer* layer, Rect* out);
static void text_component_get_layout_line_range(TextComponent* component, const char* text,
                                                 int text_len, int line_index,
                                                 int* out_start, int* out_end);
static void text_component_insert_text(TextComponent* component, const char* text);
static void text_component_delete_selection(TextComponent* component);
static void text_component_delete_prev_char(TextComponent* component);
static void text_component_delete_next_char(TextComponent* component);

static TextEditSnapshot* text_component_capture_snapshot(TextComponent* component)
{
    const char* text;
    if (!component || !component->layer) {
        return NULL;
    }
    text = component->layer->text ? component->layer->text : "";
    return text_edit_snapshot_create(text,
                                    component->cursor_pos,
                                    component->selection_start,
                                    component->selection_end,
                                    component->style_runs,
                                    component->style_run_count,
                                    component->typing_style);
}

static void text_component_apply_snapshot(TextComponent* component, TextEditSnapshot* snap)
{
    if (!component || !snap) {
        return;
    }
    layer_set_text(component->layer, snap->text ? snap->text : "");
    component->cursor_pos = snap->cursor_pos;
    component->selection_start = snap->selection_start;
    component->selection_end = snap->selection_end;
    component->typing_style = snap->typing_style;
    text_style_runs_free(component->style_runs);
    component->style_runs = text_style_runs_clone(snap->runs, snap->run_count);
    component->style_run_count = component->style_runs ? snap->run_count : 0;
    component->cursor_visual_line = -1;
    text_component_invalidate_layout(component);
    text_component_update_content_height(component);
    text_component_update_scroll_for_cursor(component);
    text_component_trigger_on_change(component);
}

static void text_component_begin_edit(TextComponent* component, TextEditKind kind)
{
    TextEditSnapshot* snap;
    if (!component) {
        return;
    }
    if (component->history_suppress > 0) {
        return;
    }
    snap = text_component_capture_snapshot(component);
    if (!snap) {
        return;
    }
    text_edit_history_before_edit(&component->history, snap, kind, backend_get_ticks());
}

static void text_component_history_suppress_begin(TextComponent* component)
{
    if (component) {
        component->history_suppress++;
    }
}

static void text_component_history_suppress_end(TextComponent* component)
{
    if (component && component->history_suppress > 0) {
        component->history_suppress--;
    }
}

static int text_component_do_undo(TextComponent* component)
{
    TextEditSnapshot* current;
    TextEditSnapshot* prev;
    if (!component || !text_edit_history_can_undo(&component->history)) {
        return 0;
    }
    current = text_component_capture_snapshot(component);
    prev = text_edit_history_undo(&component->history, current);
    if (!prev) {
        return 0;
    }
    text_component_history_suppress_begin(component);
    text_component_apply_snapshot(component, prev);
    text_component_history_suppress_end(component);
    text_edit_snapshot_free(prev);
    return 1;
}

static int text_component_do_redo(TextComponent* component)
{
    TextEditSnapshot* current;
    TextEditSnapshot* next;
    if (!component || !text_edit_history_can_redo(&component->history)) {
        return 0;
    }
    current = text_component_capture_snapshot(component);
    next = text_edit_history_redo(&component->history, current);
    if (!next) {
        return 0;
    }
    text_component_history_suppress_begin(component);
    text_component_apply_snapshot(component, next);
    text_component_history_suppress_end(component);
    text_edit_snapshot_free(next);
    return 1;
}

static void text_component_normalize_selection_range(TextComponent* component, int* start, int* end)
{
    int s;
    int e;
    int text_len;
    if (!component || !start || !end) {
        return;
    }
    s = component->selection_start;
    e = component->selection_end;
    if (s < 0 || e < 0) {
        *start = component->cursor_pos;
        *end = component->cursor_pos;
        return;
    }
    if (s > e) {
        int t = s;
        s = e;
        e = t;
    }
    text_len = component->layer && component->layer->text ? (int)strlen(component->layer->text) : 0;
    if (s < 0) s = 0;
    if (e > text_len) e = text_len;
    *start = s;
    *end = e;
}

static int text_component_toggle_style(TextComponent* component, uint32_t flags)
{
    int start;
    int end;
    if (!component || flags == 0) {
        return 0;
    }
    text_component_normalize_selection_range(component, &start, &end);
    text_component_begin_edit(component, TEXT_EDIT_STYLE);
    if (start < end) {
        text_style_runs_toggle(&component->style_runs, &component->style_run_count, start, end, flags);
    } else {
        component->typing_style ^= flags;
    }
    mark_layer_dirty(component->layer, DIRTY_TEXT);
    return 1;
}

static DFont* text_component_font_for_flags(TextComponent* component, uint32_t flags)
{
    Font* font;
    if (!component || !component->layer || !component->layer->font) {
        return NULL;
    }
    font = component->layer->font;
    if (!(flags & TEXT_STYLE_BOLD)) {
        return font->default_font;
    }
    if (component->bold_font_cache && component->bold_font_size_cache == font->size) {
        return component->bold_font_cache;
    }
    component->bold_font_cache = backend_load_font_with_weight(font->path, font->size, "bold");
    component->bold_font_size_cache = font->size;
    return component->bold_font_cache ? component->bold_font_cache : font->default_font;
}

static int text_component_get_line_height(TextComponent* component) {
    if (!component || !component->layer) return 20;
    if (component->line_height_valid && component->cached_line_height > 0) {
        return component->cached_line_height;
    }
    int line_height = component->line_height > 0 ? component->line_height : 20;
    if (component->layer->font && component->layer->font->default_font) {
        Texture* temp_tex = backend_render_texture(component->layer->font->default_font, "X", component->layer->color);
        if (temp_tex) {
            int temp_width = 0, temp_height = 0;
            backend_query_texture(temp_tex, NULL, NULL, &temp_width, &temp_height);
            line_height = temp_height / yui_density;
            backend_render_text_destroy(temp_tex);
            component->cached_line_height = line_height;
            component->line_height_valid = 1;
        }
    }
    return line_height;
}

static unsigned int text_component_codepoint_at(const char* s, int* len) {
    const unsigned char* u = (const unsigned char*)s;
    int clen = utf8_char_len_at(s);
    unsigned int cp = 0;

    if (len) {
        *len = clen;
    }
    if (!u || clen <= 0) {
        return 0;
    }
    if (clen == 1) {
        return u[0];
    }
    if (clen == 2) {
        cp = ((unsigned int)(u[0] & 0x1F) << 6) |
             (unsigned int)(u[1] & 0x3F);
    } else if (clen == 3) {
        cp = ((unsigned int)(u[0] & 0x0F) << 12) |
             ((unsigned int)(u[1] & 0x3F) << 6) |
             (unsigned int)(u[2] & 0x3F);
    } else if (clen == 4) {
        cp = ((unsigned int)(u[0] & 0x07) << 18) |
             ((unsigned int)(u[1] & 0x3F) << 12) |
             ((unsigned int)(u[2] & 0x3F) << 6) |
             (unsigned int)(u[3] & 0x3F);
    }
    return cp;
}

static int text_component_is_grapheme_suffix(unsigned int cp) {
    return cp == 0xFE0E || cp == 0xFE0F || cp == 0x20E3 ||
           (cp >= 0x1F3FB && cp <= 0x1F3FF) ||
           (cp >= 0x0300 && cp <= 0x036F);
}

static int text_component_is_emoji_codepoint(unsigned int cp) {
    return (cp >= 0x1F300 && cp <= 0x1FAFF) ||
           (cp >= 0x2600 && cp <= 0x27BF) ||
           cp == 0xFE0F || cp == 0x200D;
}

static int text_component_estimate_cluster_width(TextComponent* component, unsigned int cp) {
    int font_size = 14;
    if (component && component->layer && component->layer->font &&
        component->layer->font->size > 0) {
        font_size = component->layer->font->size;
    }
    if (text_component_is_emoji_codepoint(cp)) {
        return font_size;
    }
    if (cp >= 0x80) {
        return font_size;
    }
    return (font_size + 1) / 2;
}

static int text_component_grapheme_end(const char* text, int start, int end) {
    int pos = start;
    int clen;
    unsigned int cp;

    if (pos >= end) {
        return pos;
    }
    clen = utf8_char_len_at(text + pos);
    if (clen <= 0) {
        return pos + 1;
    }
    pos += clen;
    while (pos < end) {
        cp = text_component_codepoint_at(text + pos, &clen);
        if (clen <= 0) {
            break;
        }
        if (text_component_is_grapheme_suffix(cp)) {
            pos += clen;
            continue;
        }
        if (cp == 0x200D) {
            pos += clen;
            if (pos >= end) {
                break;
            }
            clen = utf8_char_len_at(text + pos);
            if (clen <= 0) {
                break;
            }
            pos += clen;
            continue;
        }
        break;
    }
    return pos;
}

static int text_component_measure_width(TextComponent* component, const char* text, int start, int end) {
    int measured;
    int pos;
    int estimated = 0;

    if (!component || !component->layer || !component->layer->font || !component->layer->font->default_font) {
        return 0;
    }
    if (start >= end) {
        return 0;
    }

    measured = text_syntax_measure_range(component->layer->font->default_font, text, start, end,
                                         &component->syntax_config);

    pos = start;
    while (pos < end) {
        int clen;
        unsigned int cp = text_component_codepoint_at(text + pos, &clen);
        int gend = text_component_grapheme_end(text, pos, end);
        if (gend <= pos) {
            gend = pos + (clen > 0 ? clen : 1);
        }
        estimated += text_component_estimate_cluster_width(component, cp);
        pos = gend;
    }

    /*
     * backend_render_texture() is also what the normal rendering path uses,
     * so its width is the only reliable width for wrapping.  The estimate is
     * a fallback for backends that cannot create a texture for this text.
     */
    if (measured > 0) {
        return measured;
    }
    return estimated;
}

static int text_component_grapheme_safe_end(const char* text, int start, int pos, int line_end) {
    int clen;
    unsigned int cp;

    if (pos <= start || pos >= line_end) {
        return pos;
    }

    while (pos < line_end) {
        cp = text_component_codepoint_at(text + pos, &clen);
        if (clen <= 0) {
            break;
        }
        if (text_component_is_grapheme_suffix(cp)) {
            pos += clen;
            continue;
        }
        if (cp == 0x200D) {
            pos += clen;
            if (pos < line_end) {
                clen = utf8_char_len_at(text + pos);
                if (clen <= 0) {
                    break;
                }
                pos += clen;
            }
            continue;
        }
        break;
    }
    return pos;
}

static int text_component_find_wrap_end(const char* text, int start, int line_end, int max_width, TextComponent* component) {
    int pos;
    int clen;

    if (start >= line_end) return start;
    if (text_component_measure_width(component, text, start, line_end) <= max_width) {
        return line_end;
    }
    int lo = start + 1;
    int hi = line_end;
    while (lo < hi) {
        int mid = lo + (hi - lo) / 2;
        if (text_component_measure_width(component, text, start, mid) <= max_width) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    /* 二分按字节，结果对齐到 UTF-8 字符边界，避免中文截断乱码 */
    pos = lo - 1;
    if (pos < start) pos = start;
    pos = start + utf8_safe_prefix_bytes(text + start, pos - start);
    pos = text_component_grapheme_safe_end(text, start, pos, line_end);
    if (pos > line_end) {
        pos = line_end;
    }

    /*
     * The binary search probes byte offsets, which can land inside UTF-8
     * sequences.  Fill forward from the resulting valid grapheme boundary
     * using real measurements, so a valid character is never left behind
     * merely because an intermediate probe was an invalid UTF-8 fragment.
     */
    while (pos < line_end) {
        int next = text_component_grapheme_end(text, pos, line_end);
        if (next <= pos ||
            text_component_measure_width(component, text, start, next) > max_width) {
            break;
        }
        pos = next;
    }

    if (pos <= start) {
        pos = text_component_grapheme_end(text, start, line_end);
        if (pos <= start) {
            clen = utf8_char_len_at(text + start);
            if (clen <= 0) clen = 1;
            pos = start + clen;
        }
        if (pos > line_end) {
            pos = line_end;
        }
    }
    return pos;
}

void text_component_invalidate_layout(TextComponent* component) {
    if (!component) return;
    component->text_revision++;
    component->line_height_valid = 0;
    component->layout_cache_text_len = -1;
}

static void text_component_free_layout(TextComponent* component) {
    if (!component) return;
    free(component->layout_starts);
    component->layout_starts = NULL;
    component->layout_count = 0;
    component->layout_cache_revision = -1;
    component->layout_cache_max_width = 0;
    component->layout_cache_text_len = -1;
}

static void text_component_ensure_layout(TextComponent* component, int max_width) {
    if (!component || !component->layer) return;
    const char* text = component->layer->text ? component->layer->text : "";
    int text_len = (int)strlen(text);
    if (max_width < 1) max_width = 1;

    if (component->layout_starts &&
        component->layout_cache_revision == component->text_revision &&
        component->layout_cache_max_width == max_width &&
        component->layout_cache_text_len == text_len) {
        return;
    }
    int capacity = 64;
    int* starts = (int*)malloc(sizeof(int) * (size_t)capacity);
    if (!starts) return;

    int count = 0;
    int current_pos = 0;
    while (current_pos < text_len) {
        if (count >= capacity) {
            capacity *= 2;
            int* grown = (int*)realloc(starts, sizeof(int) * (size_t)capacity);
            if (!grown) break;
            starts = grown;
        }
        starts[count++] = current_pos;

        int line_end = current_pos;
        while (line_end < text_len && text[line_end] != '\n') {
            line_end++;
        }

        if (component->wrap) {
            int split_pos = text_component_find_wrap_end(text, current_pos, line_end, max_width, component);
            if (split_pos <= current_pos) {
                int clen = utf8_char_len_at(text + current_pos);
                split_pos = current_pos + (clen > 0 ? clen : 1);
            }
            if (split_pos < line_end) {
                current_pos = split_pos;
                continue;
            }
        }
        if (line_end < text_len && text[line_end] == '\n') {
            current_pos = line_end + 1;
        } else {
            current_pos = line_end;
        }
    }

    /*
     * A trailing empty visual line exists only for empty text or a final
     * explicit newline.  Adding one after every non-empty line made the
     * cursor and content height one line too large when wrap was disabled.
     */
    if (text_len == 0 || text[text_len - 1] == '\n') {
        if (count >= capacity) {
            int* grown = (int*)realloc(starts, sizeof(int) * (size_t)(capacity + 1));
            if (grown) {
                starts = grown;
                capacity++;
            }
        }
        if (count < capacity) {
            starts[count++] = text_len;
        }
    }

    free(component->layout_starts);
    component->layout_starts = starts;
    component->layout_count = count;
    component->layout_cache_revision = component->text_revision;
    component->layout_cache_max_width = max_width;
    component->layout_cache_text_len = text_len;
}

static int text_component_count_visual_lines_in_range(TextComponent* component, int max_width,
                                                      int start, int logical_end) {
    if (!component || !component->layer || !component->layer->text) return 1;
    char* text = component->layer->text;
    if (start >= logical_end) return 1;

    if (!component->wrap) {
        int lines = 0;
        int pos = start;
        while (pos < logical_end) {
            int next = pos;
            while (next < logical_end && text[next] != '\n') {
                next++;
            }
            lines++;
            pos = (next < logical_end && text[next] == '\n') ? next + 1 : next;
        }
        return lines > 0 ? lines : 1;
    }

    int width = text_component_measure_width(component, text, start, logical_end);
    if (width <= max_width) return 1;

    int lines = 0;
    int pos = start;
    while (pos < logical_end) {
        int split_pos = text_component_find_wrap_end(text, pos, logical_end, max_width, component);
        if (split_pos <= pos) {
            int clen = utf8_char_len_at(text + pos);
            split_pos = pos + (clen > 0 ? clen : 1);
        }
        lines++;
        pos = split_pos;
    }
    return lines > 0 ? lines : 1;
}

static int text_component_render_text_cluster_font(TextComponent* component, DFont* font,
                                                   const char* text, int start, int end,
                                                   int x, int y) {
    int len;
    char* buf;
    Texture* tex;
    int width = 0;
    int height = 0;
    Rect rect;
    int draw_w = 0;

    if (!component || !component->layer || !font) {
        return 0;
    }
    if (start >= end) {
        return 0;
    }

    len = end - start;
    buf = (char*)malloc((size_t)len + 1);
    if (!buf) {
        return 0;
    }
    memcpy(buf, text + start, (size_t)len);
    buf[len] = '\0';
    tex = backend_render_texture(font, buf, component->layer->color);
    free(buf);
    if (!tex) {
        return 0;
    }
    backend_query_texture(tex, NULL, NULL, &width, &height);
    draw_w = width / yui_density;
    if (draw_w < 1) {
        draw_w = 1;
    }
    rect = (Rect){x, y, draw_w, height / yui_density};
    backend_render_text_copy(tex, NULL, &rect);
    backend_render_text_destroy(tex);
    return draw_w;
}

static int text_component_render_text_cluster(TextComponent* component, const char* text,
                                               int start, int end, int x, int y) {
    if (!component || !component->layer || !component->layer->font || !component->layer->font->default_font) {
        return 0;
    }
    return text_component_render_text_cluster_font(component, component->layer->font->default_font,
                                                   text, start, end, x, y);
}

static void text_component_render_text_segment_graphemes_font(TextComponent* component, DFont* font,
                                                             const char* text, int start, int end,
                                                             int x, int y) {
    int pos = start;
    int cursor_x = x;

    while (pos < end) {
        int clen;
        unsigned int cp;
        int gend = text_component_grapheme_end(text, pos, end);
        int advance;

        if (gend <= pos) {
            clen = utf8_char_len_at(text + pos);
            gend = pos + (clen > 0 ? clen : 1);
        }
        {
            int drawn_w = text_component_render_text_cluster_font(component, font, text, pos, gend,
                                                                  cursor_x, y);
            if (drawn_w > 0) {
                cursor_x += drawn_w;
            } else {
                cp = text_component_codepoint_at(text + pos, &clen);
                advance = text_component_estimate_cluster_width(component, cp);
                if (advance < 1) {
                    advance = 1;
                }
                cursor_x += advance;
            }
        }
        pos = gend;
    }
}

static void text_component_render_text_segment_graphemes(TextComponent* component, const char* text,
                                                       int start, int end, int x, int y) {
    if (!component || !component->layer || !component->layer->font || !component->layer->font->default_font) {
        return;
    }
    text_component_render_text_segment_graphemes_font(component, component->layer->font->default_font,
                                                      text, start, end, x, y);
}

static int text_component_style_seg_end(TextComponent* component, int start, int end)
{
    int i;
    int seg_end = end;
    uint32_t flags;

    if (!component || start >= end) {
        return end;
    }
    flags = text_style_runs_flags_at(component->style_runs, component->style_run_count, start);
    for (i = 0; i < component->style_run_count; i++) {
        TextStyleRun* r = &component->style_runs[i];
        if (r->end <= start || r->start >= end) {
            continue;
        }
        if (r->start > start && r->start < seg_end) {
            seg_end = r->start;
        }
        if (start >= r->start && start < r->end) {
            if (r->end < seg_end) {
                seg_end = r->end;
            }
            if (r->flags != flags) {
                /* should not happen for flags_at, keep going */
            }
        }
    }
    if (seg_end <= start) {
        seg_end = start + 1;
    }
    if (seg_end > end) {
        seg_end = end;
    }
    return seg_end;
}

static void text_component_render_text_segment_plain(TextComponent* component, DFont* font,
                                                    const char* text, int start, int end,
                                                    int x, int y) {
    int len;
    char* buf;
    Texture* tex;

    if (!component || !font || start >= end) {
        return;
    }

    len = end - start;
    buf = (char*)malloc((size_t)len + 1);
    if (!buf) return;
    memcpy(buf, text + start, (size_t)len);
    buf[len] = '\0';
    tex = backend_render_texture(font, buf, component->layer->color);
    free(buf);
    if (!tex) {
        text_component_render_text_segment_graphemes_font(component, font, text, start, end, x, y);
        return;
    }
    {
        int width = 0, height = 0;
        backend_query_texture(tex, NULL, NULL, &width, &height);
        Rect rect = {x, y, width / yui_density, height / yui_density};
        backend_render_text_copy(tex, NULL, &rect);
        backend_render_text_destroy(tex);
    }
}

static void text_component_render_text_segment(TextComponent* component, const char* text,
                                               int start, int end, int x, int y) {
    if (!component || !component->layer || !component->layer->font || !component->layer->font->default_font) {
        return;
    }
    if (start >= end) return;

    if (component->syntax_config.lang != NULL) {
        text_syntax_render_range(component->layer->font->default_font, text, start, end,
                                 &component->syntax_config, x, y);
        return;
    }

    if (component->style_run_count > 0) {
        int pos = start;
        int cursor_x = x;
        while (pos < end) {
            uint32_t flags = text_style_runs_flags_at(component->style_runs,
                                                     component->style_run_count, pos);
            int seg_end = text_component_style_seg_end(component, pos, end);
            DFont* font = text_component_font_for_flags(component, flags);
            int len = seg_end - pos;
            char* buf;
            Texture* tex;
            if (!font) {
                font = component->layer->font->default_font;
            }
            buf = (char*)malloc((size_t)len + 1);
            if (!buf) {
                break;
            }
            memcpy(buf, text + pos, (size_t)len);
            buf[len] = '\0';
            tex = backend_render_texture(font, buf, component->layer->color);
            free(buf);
            if (!tex) {
                text_component_render_text_segment_graphemes_font(component, font, text, pos, seg_end,
                                                                  cursor_x, y);
                /* approximate advance for next piece */
                {
                    int approx = 0;
                    int p = pos;
                    while (p < seg_end) {
                        int clen = 0;
                        unsigned int cp = text_component_codepoint_at(text + p, &clen);
                        int adv = text_component_estimate_cluster_width(component, cp);
                        if (adv < 1) adv = 1;
                        approx += adv;
                        p += clen > 0 ? clen : 1;
                    }
                    cursor_x += approx;
                }
            } else {
                int width = 0, height = 0;
                backend_query_texture(tex, NULL, NULL, &width, &height);
                Rect rect = {cursor_x, y, width / yui_density, height / yui_density};
                backend_render_text_copy(tex, NULL, &rect);
                backend_render_text_destroy(tex);
                cursor_x += width / yui_density;
            }
            pos = seg_end;
        }
        return;
    }

    text_component_render_text_segment_plain(component, component->layer->font->default_font,
                                             text, start, end, x, y);
}

void text_component_set_syntax_highlight(TextComponent* component, const char* language) {
    if (!component) return;
    text_syntax_config_init(&component->syntax_config, language,
                            component->layer ? component->layer->color : (Color){0, 0, 0, 255});
}

static void text_component_parse_syntax_colors(TextComponent* component, cJSON* colors_obj) {
    if (!component || !colors_obj || !cJSON_IsObject(colors_obj)) return;
    cJSON* child = colors_obj->child;
    while (child) {
        if (cJSON_IsString(child) && child->valuestring) {
            Color color = component->syntax_config.default_color;
            parse_color(child->valuestring, &color);
            text_syntax_config_set_color(&component->syntax_config, child->string, color);
        }
        child = child->next;
    }
}

static const char* const TEXT_SYNTAX_COLOR_KEYS[] = {
    "keyword", "string", "number", "comment", "punctuation", NULL
};

static void text_component_apply_syntax_color_key(TextComponent* component, const char* key, const char* value) {
    if (!component || !key || !value) return;
    Color color = component->syntax_config.default_color;
    parse_color(value, &color);
    text_syntax_config_set_color(&component->syntax_config, key, color);
}

static int text_component_apply_selection_color_from_json(TextComponent* component, cJSON* color_obj) {
    if (!component || !color_obj) {
        return 0;
    }

    Color selection_color = component->selection_color;
    if (cJSON_IsString(color_obj)) {
        parse_color(color_obj->valuestring, &selection_color);
        text_component_set_selection_color(component, selection_color);
        return 1;
    }

    if (cJSON_IsObject(color_obj)) {
        if (cJSON_HasObjectItem(color_obj, "r")) {
            selection_color.r = cJSON_GetObjectItem(color_obj, "r")->valueint;
        }
        if (cJSON_HasObjectItem(color_obj, "g")) {
            selection_color.g = cJSON_GetObjectItem(color_obj, "g")->valueint;
        }
        if (cJSON_HasObjectItem(color_obj, "b")) {
            selection_color.b = cJSON_GetObjectItem(color_obj, "b")->valueint;
        }
        if (cJSON_HasObjectItem(color_obj, "a")) {
            selection_color.a = cJSON_GetObjectItem(color_obj, "a")->valueint;
        }
        text_component_set_selection_color(component, selection_color);
        return 1;
    }

    return 0;
}

static void text_component_apply_theme_style(Layer* layer, cJSON* style) {
    if (!layer || !style || !layer->component) return;

    TextComponent* component = (TextComponent*)layer->component;

    cJSON* bg_color = cJSON_GetObjectItem(style, "bgColor");
    if (bg_color && cJSON_IsString(bg_color)) {
        parse_color(bg_color->valuestring, &layer->bg_color);
    }

    cJSON* color = cJSON_GetObjectItem(style, "color");
    if (color && cJSON_IsString(color)) {
        parse_color(color->valuestring, &layer->color);
        component->syntax_config.default_color = layer->color;
    }

    cJSON* font_size = cJSON_GetObjectItem(style, "fontSize");
    if (font_size && cJSON_IsNumber(font_size) && layer->font) {
        layer->font->size = font_size->valueint;
        component->line_height_valid = 0;
    }

    cJSON* line_number_bg = cJSON_GetObjectItem(style, "lineNumberBgColor");
    if (line_number_bg && cJSON_IsString(line_number_bg)) {
        Color c;
        parse_color(line_number_bg->valuestring, &c);
        text_component_set_line_number_bg_color(component, c);
    }

    cJSON* line_number_color = cJSON_GetObjectItem(style, "lineNumberColor");
    if (line_number_color && cJSON_IsString(line_number_color)) {
        Color c;
        parse_color(line_number_color->valuestring, &c);
        text_component_set_line_number_color(component, c);
    }

    cJSON* cursor_color = cJSON_GetObjectItem(style, "cursorColor");
    if (cursor_color && cJSON_IsString(cursor_color)) {
        Color c;
        parse_color(cursor_color->valuestring, &c);
        text_component_set_cursor_color(component, c);
    }

    cJSON* selection_color = cJSON_GetObjectItem(style, "selectionColor");
    text_component_apply_selection_color_from_json(component, selection_color);

    cJSON* syntax_colors = cJSON_GetObjectItem(style, "syntaxColors");
    if (syntax_colors && cJSON_IsObject(syntax_colors)) {
        text_component_parse_syntax_colors(component, syntax_colors);
    }

    for (int i = 0; TEXT_SYNTAX_COLOR_KEYS[i]; i++) {
        cJSON* item = cJSON_GetObjectItem(style, TEXT_SYNTAX_COLOR_KEYS[i]);
        if (item && cJSON_IsString(item)) {
            text_component_apply_syntax_color_key(component, TEXT_SYNTAX_COLOR_KEYS[i], item->valuestring);
        }
    }

    {
        cJSON* padding = cJSON_GetObjectItem(style, "padding");
        if (padding && layer_padding_apply_from_json(layer->padding, padding) && layer->layout_manager) {
            memcpy(layer->layout_manager->padding, layer->padding, sizeof(layer->padding));
            mark_layer_dirty(layer, DIRTY_LAYOUT);
        }
    }

    mark_layer_dirty(layer, DIRTY_COLOR | DIRTY_TEXT);
}

// 创建文本组件
TextComponent* text_component_create(Layer* layer) {
    TextComponent* component = (TextComponent*)malloc(sizeof(TextComponent));
    memset(component, 0, sizeof(TextComponent));
    if (!component) {
        return NULL;
    }
    
    component->layer = layer;
    // 文本内容现在存储在 layer->text 中，不需要初始化 component->text
    component->placeholder[0] = '\0';
    component->cursor_pos = 0;
    component->cursor_visual_line = -1;
    component->selection_start = -1;
    component->selection_end = -1;
    component->max_length = MAX_TEXT * 4;
    component->cursor_color = (Color){0, 0, 0, 255};
    component->scroll_x = 0;
    component->scroll_y = 0;
    component->line_height = 20;
    component->multiline = 0;
    component->wrap = 1;
    component->editable = 1;  // 默认设置为可编辑状态
    component->show_line_numbers = 0;  // 默认不显示行号
    component->line_number_width = 40;  // 默认行号区域宽度
    component->line_number_color = (Color){133, 133, 133, 255};  // 默认行号颜色
    component->line_number_bg_color = (Color){30, 30, 30, 255};  // 默认行号背景颜色
    component->selection_color = (Color){70, 130, 180, 100};  // 默认选中颜色（半透明蓝色）
    component->is_selecting = 0;  // 默认不在选择状态
    component->cached_line_height = 0;  // 行高缓存初始化为0（无效）
    component->line_height_valid = 0;  // 标记缓存无效
    component->text_revision = 0;
    component->layout_starts = NULL;
    component->layout_count = 0;
    component->layout_cache_revision = -1;
    component->layout_cache_max_width = 0;
    component->layout_cache_text_len = -1;
    text_edit_history_init(&component->history, 80);
    component->history_suppress = 0;
    component->style_runs = NULL;
    component->style_run_count = 0;
    component->typing_style = 0;
    component->bold_font_cache = NULL;
    component->bold_font_size_cache = 0;
    text_syntax_config_init(&component->syntax_config, NULL, layer->color);
    
    // 初始化图层的文本字段
    if (!layer->text) {
        layer_set_text(layer, "");  // 初始化为空字符串
    }
    
    // 设置图层的焦点属性，使其可以获得焦点
    layer->focusable = 1;
    
    // 存储组件指针到图层的component字段中
    layer->component = component;
    layer->render = text_component_render;
    layer->handle_key_event = text_component_handle_key_event;
    layer->handle_pointer_event = text_component_handle_pointer_event;
    layer->register_event = text_component_register_event;
    layer->get_property = text_component_get_property;
    layer->set_property = text_component_set_property_from_json;
    layer->set_style = text_component_apply_theme_style;

    return component;
}

TextComponent* text_component_create_from_json(Layer* layer,cJSON* json_obj){
    TextComponent* component = text_component_create(layer);
    
    // 强制设置可编辑和可获得焦点
    layer->focusable = 1;
    component->editable = 1;
    
    // 设置文本内容
    if (cJSON_HasObjectItem(json_obj, "text")) {
        text_component_set_text(layer->component, cJSON_GetObjectItem(json_obj, "text")->valuestring);
    }
    
    // 解析placeholder属性
    if (cJSON_HasObjectItem(json_obj, "placeholder")) {
        text_component_set_placeholder(layer->component, cJSON_GetObjectItem(json_obj, "placeholder")->valuestring);
    }
    
    // 解析maxLength属性
    if (cJSON_HasObjectItem(json_obj, "maxLength")) {
        text_component_set_max_length(layer->component, cJSON_GetObjectItem(json_obj, "maxLength")->valueint);
    }
    
    // 解析multiline属性
    if (cJSON_HasObjectItem(json_obj, "multiline")) {
        text_component_set_multiline(layer->component, cJSON_IsTrue(cJSON_GetObjectItem(json_obj, "multiline")));
    }

    // 解析 wrap 属性：多行时是否按宽度自动换行，默认 true
    if (cJSON_HasObjectItem(json_obj, "wrap")) {
        text_component_set_wrap(layer->component, cJSON_IsTrue(cJSON_GetObjectItem(json_obj, "wrap")));
    }
    
    // 解析editable属性
    if (cJSON_HasObjectItem(json_obj, "editable")) {
        text_component_set_editable(layer->component, cJSON_IsTrue(cJSON_GetObjectItem(json_obj, "editable")));
    }
    
    // 解析showLineNumbers属性
    if (cJSON_HasObjectItem(json_obj, "showLineNumbers")) {
        text_component_set_show_line_numbers(layer->component, cJSON_IsTrue(cJSON_GetObjectItem(json_obj, "showLineNumbers")));
    }
    
    // 解析lineNumberWidth属性
    if (cJSON_HasObjectItem(json_obj, "lineNumberWidth")) {
        text_component_set_line_number_width(layer->component, cJSON_GetObjectItem(json_obj, "lineNumberWidth")->valueint);
    }
    
    // 解析selectionColor属性（顶层，兼容旧配置；style 内由 set_style 处理）
    if (cJSON_HasObjectItem(json_obj, "selectionColor")) {
        text_component_apply_selection_color_from_json(
            layer->component, cJSON_GetObjectItem(json_obj, "selectionColor"));
    }

      // 解析事件绑定
    cJSON* events = cJSON_GetObjectItem(json_obj, "events");
    if (events) {
        // 解析onChange事件
        if (cJSON_HasObjectItem(events, "onChange")) {
            cJSON* on_change_obj = cJSON_GetObjectItem(events, "onChange");
            if (cJSON_IsString(on_change_obj)) {
                const char* event_name = on_change_obj->valuestring;
                if (event_name[0] == '@') {
                    event_name++;
                }
                component->change_name = strdup(event_name);
                EventHandler handler = find_event_by_name(event_name);
                component->on_change = handler;
            }
        }
    }

    if (cJSON_HasObjectItem(json_obj, "syntaxHighlight")) {
        cJSON* syntax_obj = cJSON_GetObjectItem(json_obj, "syntaxHighlight");
        if (cJSON_IsString(syntax_obj) && syntax_obj->valuestring) {
            text_component_set_syntax_highlight(component, syntax_obj->valuestring);
        }
    }
    if (cJSON_HasObjectItem(json_obj, "syntaxColors")) {
        text_component_parse_syntax_colors(component, cJSON_GetObjectItem(json_obj, "syntaxColors"));
    }

    cJSON* style_obj = cJSON_GetObjectItem(json_obj, "style");
    if (style_obj && layer->set_style) {
        layer->set_style(layer, style_obj);
    }

    return component;
}

// 销毁文本组件
void text_component_destroy(TextComponent* component) {
    if (component) {
        text_component_free_layout(component);
        text_edit_history_free(&component->history);
        text_style_runs_free(component->style_runs);
        component->style_runs = NULL;
        component->style_run_count = 0;
        free(component);
    }
}



// 设置文本内容
void text_component_set_text(TextComponent* component, const char* text) {
    if (!component || !text) {
        return;
    }
    
    // 计算文本长度
    size_t text_len = strlen(text);
    
    // 移除最大长度限制，使用动态内存分配
    // 注释掉最大长度检查，因为现在使用 layer_set_text_with_size 进行动态内存分配
    /*
    size_t copy_len = text_len < (size_t)(component->max_length - 1) ? text_len : (size_t)(component->max_length - 1);
    
    // 创建临时字符串以存储截断后的文本
    char* temp_text = malloc(copy_len + 1);
    if (!temp_text) {
        return;
    }
    
    memcpy(temp_text, text, copy_len);
    temp_text[copy_len] = '\0';
    */
    
    // 使用 layer_set_text 设置文本（使用动态内存分配）
    layer_set_text(component->layer, text);
    
    // 更新组件状态
    component->cursor_pos = text_len;
    component->selection_start = -1;
    component->selection_end = -1;
    text_style_runs_free(component->style_runs);
    component->style_runs = NULL;
    component->style_run_count = 0;
    component->typing_style = 0;
    text_edit_history_clear(&component->history);
    
    // 使行高缓存失效（因为文本或字体可能改变）
    component->line_height_valid = 0;
    text_component_invalidate_layout(component);
    
    // 更新内容高度（只调用一次）
    text_component_update_content_height(component);
    
    // 触发 onChange 事件
    text_component_trigger_on_change(component);
}

// 设置占位符
void text_component_set_placeholder(TextComponent* component, const char* placeholder) {
    if (!component || !placeholder) {
        return;
    }
    
    strncpy(component->placeholder, placeholder, MAX_TEXT - 1);
    component->placeholder[MAX_TEXT - 1] = '\0';
}

// 设置最大长度
void text_component_set_max_length(TextComponent* component, int max_length) {
    if (!component) {
        return;
    }
    
    component->max_length = max_length;
    // 移除最大长度限制，使用动态内存分配
    // 注释掉最大长度检查，因为现在使用 layer_set_text_with_size 进行动态内存分配
    /*
    // 确保文本不超过新的最大长度
    if (strlen(component->layer->text) >= max_length) {
        // 截断文本
        char* temp_text = malloc(max_length);
        if (temp_text) {
            strncpy(temp_text, component->layer->text, max_length - 1);
            temp_text[max_length - 1] = '\0';
            layer_set_text(component->layer, temp_text);
            free(temp_text);
            component->cursor_pos = max_length - 1;
        }
    }
    */
}





// 设置光标颜色
void text_component_set_cursor_color(TextComponent* component, Color cursor_color) {
    if (!component) {
        return;
    }
    
    component->cursor_color = cursor_color;
}

// 设置多行模式
void text_component_set_multiline(TextComponent* component, int multiline) {
    if (!component) {
        return;
    }
    
    component->multiline = multiline;
    text_component_invalidate_layout(component);
}

void text_component_set_wrap(TextComponent* component, int wrap) {
    if (!component) {
        return;
    }

    component->wrap = wrap ? 1 : 0;
    text_component_invalidate_layout(component);
}

// 设置可编辑性
void text_component_set_editable(TextComponent* component, int editable) {
    if (!component) {
        return;
    }

    component->editable = editable;

    // 同时更新焦点属性
    if (component->layer) {
        component->layer->focusable = editable;
    }
}

// 设置是否显示行号
void text_component_set_show_line_numbers(TextComponent* component, int show_line_numbers) {
    if (!component) {
        return;
    }
    
    component->show_line_numbers = show_line_numbers;
}

// 设置行号区域宽度
void text_component_set_line_number_width(TextComponent* component, int width) {
    if (!component) {
        return;
    }
    
    component->line_number_width = width;
}

// 设置行号颜色
void text_component_set_line_number_color(TextComponent* component, Color color) {
    if (!component) {
        return;
    }
    
    component->line_number_color = color;
}

// 设置行号背景颜色
void text_component_set_line_number_bg_color(TextComponent* component, Color color) {
    if (!component) {
        return;
    }
    
    component->line_number_bg_color = color;
}

// 设置选中颜色
void text_component_set_selection_color(TextComponent* component, Color color) {
    if (!component) {
        return;
    }
    
    component->selection_color = color;
}

// 注册事件处理函数
int text_component_register_event(Layer* layer, const char* event_name, const char* event_func_name, EventHandler event_handler){
    if(strcmp(event_name,"change")==0 || strcmp(event_name,"onChange")==0){
        TextComponent* component = (TextComponent*)layer->component;
        component->on_change = event_handler;
        if (event_func_name && event_func_name[0] == '@') {
            event_func_name++;
        }
        component->change_name = strdup(event_func_name);
        return 0;
    }
    return -1;
}

// 通用属性获取函数
cJSON* text_component_get_property(Layer* layer, const char* property_name) {
    if (!layer || !property_name || !layer->component) {
        return NULL;
    }
    
    TextComponent* component = (TextComponent*)layer->component;
    
    if (strcmp(property_name, "value") == 0 || strcmp(property_name, "text") == 0) {
        // 获取文本内容
        if (layer->text && strlen(layer->text) > 0) {
            return cJSON_CreateString(layer->text);
        }
        return cJSON_CreateNull();
    }
    else if (strcmp(property_name, "placeholder") == 0) {
        // 获取占位符
        if (component->placeholder && strlen(component->placeholder) > 0) {
            return cJSON_CreateString(component->placeholder);
        }
        return cJSON_CreateNull();
    }
    else if (strcmp(property_name, "multiline") == 0) {
        // 是否多行
        return cJSON_CreateBool(component->multiline);
    }
    else if (strcmp(property_name, "wrap") == 0) {
        return cJSON_CreateBool(component->wrap);
    }
    else if (strcmp(property_name, "editable") == 0) {
        // 是否可编辑
        return cJSON_CreateBool(component->editable);
    }
    else if (strcmp(property_name, "maxLength") == 0) {
        // 最大长度
        return cJSON_CreateNumber(component->max_length);
    }
    else if (strcmp(property_name, "contentHeight") == 0 || strcmp(property_name, "content_height") == 0) {
        int pad_top;
        int pad_bottom;
        if (layer->dirty_flags & DIRTY_TEXT) {
            text_component_invalidate_layout(component);
            layer->dirty_flags &= ~DIRTY_TEXT;
        }
        text_component_update_content_height(component);
        text_component_get_padding(layer, &pad_top, NULL, &pad_bottom, NULL);
        return cJSON_CreateNumber(layer->content_height + pad_top + pad_bottom);
    }
    else if (strcmp(property_name, "cursorPos") == 0 || strcmp(property_name, "cursor") == 0) {
        return cJSON_CreateNumber(component->cursor_pos);
    }
    else if (strcmp(property_name, "selectionStart") == 0) {
        return cJSON_CreateNumber(component->selection_start);
    }
    else if (strcmp(property_name, "selectionEnd") == 0) {
        return cJSON_CreateNumber(component->selection_end);
    }
    else if (strcmp(property_name, "selection") == 0) {
        cJSON* obj = cJSON_CreateObject();
        cJSON_AddNumberToObject(obj, "start", component->selection_start);
        cJSON_AddNumberToObject(obj, "end", component->selection_end);
        return obj;
    }
    else if (strcmp(property_name, "selectedText") == 0) {
        int start = component->selection_start;
        int end = component->selection_end;
        int text_len;
        char* selected;
        int len;

        if (start < 0 || end < 0 || !layer->text) {
            return cJSON_CreateString("");
        }
        if (start > end) {
            int tmp = start;
            start = end;
            end = tmp;
        }
        text_len = (int)strlen(layer->text);
        if (start > text_len) start = text_len;
        if (end > text_len) end = text_len;
        if (start >= end) {
            return cJSON_CreateString("");
        }
        len = end - start;
        selected = (char*)malloc((size_t)len + 1);
        if (!selected) {
            return cJSON_CreateString("");
        }
        memcpy(selected, layer->text + start, (size_t)len);
        selected[len] = '\0';
        {
            cJSON* out = cJSON_CreateString(selected);
            free(selected);
            return out;
        }
    }
    else if (strcmp(property_name, "scrollX") == 0) {
        return cJSON_CreateNumber(component->scroll_x);
    }
    else if (strcmp(property_name, "scrollY") == 0 || strcmp(property_name, "scrollOffset") == 0) {
        return cJSON_CreateNumber(layer->scroll_offset);
    }
    else if (strcmp(property_name, "fontSize") == 0) {
        return layer->font ? cJSON_CreateNumber(layer->font->size) : cJSON_CreateNull();
    }
    else if (strcmp(property_name, "length") == 0) {
        return cJSON_CreateNumber(layer->text ? (double)strlen(layer->text) : 0);
    }
    else if (strcmp(property_name, "lineStart") == 0) {
        return cJSON_CreateNumber(get_line_start(component, component->cursor_pos));
    }
    else if (strcmp(property_name, "lineEnd") == 0) {
        return cJSON_CreateNumber(get_line_end(component, component->cursor_pos));
    }
    else if (strcmp(property_name, "canUndo") == 0) {
        return cJSON_CreateBool(text_edit_history_can_undo(&component->history));
    }
    else if (strcmp(property_name, "canRedo") == 0) {
        return cJSON_CreateBool(text_edit_history_can_redo(&component->history));
    }
    else if (strcmp(property_name, "bold") == 0 || strcmp(property_name, "italic") == 0) {
        uint32_t flag = (strcmp(property_name, "bold") == 0) ? TEXT_STYLE_BOLD : TEXT_STYLE_ITALIC;
        int start;
        int end;
        uint32_t flags;
        text_component_normalize_selection_range(component, &start, &end);
        if (start < end) {
            flags = text_style_runs_common_flags(component->style_runs, component->style_run_count, start, end);
        } else {
            flags = component->typing_style;
            if (flags == 0) {
                flags = text_style_runs_flags_at(component->style_runs, component->style_run_count,
                                                 component->cursor_pos > 0 ? component->cursor_pos - 1
                                                                          : component->cursor_pos);
            }
        }
        return cJSON_CreateBool((flags & flag) != 0);
    }
    else if (strcmp(property_name, "styles") == 0) {
        cJSON* arr = cJSON_CreateArray();
        int i;
        for (i = 0; i < component->style_run_count; i++) {
            cJSON* run = cJSON_CreateObject();
            cJSON_AddNumberToObject(run, "start", component->style_runs[i].start);
            cJSON_AddNumberToObject(run, "end", component->style_runs[i].end);
            cJSON_AddNumberToObject(run, "flags", (double)component->style_runs[i].flags);
            cJSON_AddItemToArray(arr, run);
        }
        return arr;
    }

    // 未知的属性，返回 NULL
    return NULL;
}

static int text_component_parse_int_value(cJSON* value, int* out)
{
    if (!value || !out) {
        return 0;
    }
    if (cJSON_IsNumber(value)) {
        *out = value->valueint;
        return 1;
    }
    if (cJSON_IsString(value) && value->valuestring) {
        *out = atoi(value->valuestring);
        return 1;
    }
    return 0;
}

static void text_component_clamp_cursor(TextComponent* component)
{
    int text_len;

    if (!component || !component->layer) {
        return;
    }
    text_len = component->layer->text ? (int)strlen(component->layer->text) : 0;
    if (component->cursor_pos < 0) {
        component->cursor_pos = 0;
    }
    if (component->cursor_pos > text_len) {
        component->cursor_pos = text_len;
    }
}

static void text_component_clamp_selection(TextComponent* component)
{
    int text_len;

    if (!component || !component->layer) {
        return;
    }
    text_len = component->layer->text ? (int)strlen(component->layer->text) : 0;
    if (component->selection_start < -1) {
        component->selection_start = -1;
    }
    if (component->selection_end < -1) {
        component->selection_end = -1;
    }
    if (component->selection_start > text_len) {
        component->selection_start = text_len;
    }
    if (component->selection_end > text_len) {
        component->selection_end = text_len;
    }
}

static int text_component_copy_selection_to_clipboard(TextComponent* component)
{
    int start;
    int end;
    int len;
    char* selected;

    if (!component || !component->layer || !component->layer->text) {
        return 0;
    }
    if (component->selection_start < 0 || component->selection_end < 0) {
        return 0;
    }
    start = component->selection_start;
    end = component->selection_end;
    if (start > end) {
        int tmp = start;
        start = end;
        end = tmp;
    }
    if (start >= end) {
        return 0;
    }
    len = end - start;
    selected = (char*)malloc((size_t)len + 1);
    if (!selected) {
        return 0;
    }
    memcpy(selected, component->layer->text + start, (size_t)len);
    selected[len] = '\0';
    backend_set_clipboard_text(selected);
    free(selected);
    return 1;
}

int text_component_set_property_from_json(Layer* layer, const char* key, cJSON* value, int is_creating)
{
    TextComponent* component;
    int n;

    (void)is_creating;
    if (!layer || !key || !value || !layer->component) {
        return 0;
    }
    component = (TextComponent*)layer->component;

    if (strcmp(key, "cursorPos") == 0 || strcmp(key, "cursor") == 0) {
        if (!text_component_parse_int_value(value, &n)) {
            return 0;
        }
        component->cursor_pos = n;
        component->cursor_visual_line = -1;
        text_component_clamp_cursor(component);
        return 1;
    }
    if (strcmp(key, "selectionStart") == 0) {
        if (!text_component_parse_int_value(value, &n)) {
            return 0;
        }
        component->selection_start = n;
        text_component_clamp_selection(component);
        return 1;
    }
    if (strcmp(key, "selectionEnd") == 0) {
        if (!text_component_parse_int_value(value, &n)) {
            return 0;
        }
        component->selection_end = n;
        text_component_clamp_selection(component);
        return 1;
    }
    if (strcmp(key, "selection") == 0) {
        if (cJSON_IsObject(value)) {
            cJSON* start = cJSON_GetObjectItem(value, "start");
            cJSON* end = cJSON_GetObjectItem(value, "end");
            if (text_component_parse_int_value(start, &n)) {
                component->selection_start = n;
            }
            if (text_component_parse_int_value(end, &n)) {
                component->selection_end = n;
            }
            text_component_clamp_selection(component);
            return 1;
        }
        if (cJSON_IsString(value) && value->valuestring) {
            int start = 0;
            int end = 0;
            if (sscanf(value->valuestring, "%d,%d", &start, &end) == 2 ||
                sscanf(value->valuestring, "%d:%d", &start, &end) == 2) {
                component->selection_start = start;
                component->selection_end = end;
                text_component_clamp_selection(component);
                return 1;
            }
        }
        return 0;
    }
    if (strcmp(key, "scrollX") == 0) {
        if (!text_component_parse_int_value(value, &n)) {
            return 0;
        }
        component->scroll_x = n < 0 ? 0 : n;
        return 1;
    }
    if (strcmp(key, "scrollY") == 0 || strcmp(key, "scrollOffset") == 0) {
        if (!text_component_parse_int_value(value, &n)) {
            return 0;
        }
        layer->scroll_offset = n < 0 ? 0 : n;
        return 1;
    }
    if (strcmp(key, "scrollToCursor") == 0) {
        text_component_update_scroll_for_cursor(component);
        return 1;
    }
    if (strcmp(key, "copySelection") == 0) {
        return text_component_copy_selection_to_clipboard(component) ? 1 : 0;
    }
    if (strcmp(key, "clearSelection") == 0) {
        component->selection_start = -1;
        component->selection_end = -1;
        return 1;
    }
    if (strcmp(key, "selectAll") == 0) {
        int text_len = layer->text ? (int)strlen(layer->text) : 0;
        component->selection_start = 0;
        component->selection_end = text_len;
        component->cursor_pos = text_len;
        return 1;
    }
    if (strcmp(key, "insertText") == 0) {
        if (!component->editable) {
            return 0;
        }
        if (!cJSON_IsString(value) || !value->valuestring) {
            return 0;
        }
        text_component_insert_text(component, value->valuestring);
        return 1;
    }
    if (strcmp(key, "deleteSelection") == 0) {
        if (!component->editable) {
            return 0;
        }
        if (component->selection_start >= 0 && component->selection_end >= 0 &&
            component->selection_start != component->selection_end) {
            text_component_delete_selection(component);
        }
        return 1;
    }
    if (strcmp(key, "cutSelection") == 0) {
        if (!component->editable) {
            return 0;
        }
        if (!text_component_copy_selection_to_clipboard(component)) {
            return 0;
        }
        text_component_delete_selection(component);
        return 1;
    }
    if (strcmp(key, "paste") == 0) {
        char* clip;
        if (!component->editable) {
            return 0;
        }
        clip = backend_get_clipboard_text();
        if (!clip) {
            return 0;
        }
        text_component_insert_text(component, clip);
        free(clip);
        return 1;
    }
    if (strcmp(key, "backspace") == 0) {
        if (!component->editable) {
            return 0;
        }
        text_component_delete_prev_char(component);
        return 1;
    }
    if (strcmp(key, "deleteForward") == 0) {
        if (!component->editable) {
            return 0;
        }
        text_component_delete_next_char(component);
        return 1;
    }
    if (strcmp(key, "undo") == 0) {
        if (!component->editable) {
            return 0;
        }
        return text_component_do_undo(component);
    }
    if (strcmp(key, "redo") == 0) {
        if (!component->editable) {
            return 0;
        }
        return text_component_do_redo(component);
    }
    if (strcmp(key, "bold") == 0 || strcmp(key, "italic") == 0) {
        uint32_t flag = (strcmp(key, "bold") == 0) ? TEXT_STYLE_BOLD : TEXT_STYLE_ITALIC;
        int start;
        int end;
        int enable;
        if (!component->editable) {
            return 0;
        }
        enable = cJSON_IsTrue(value) || (cJSON_IsNumber(value) && value->valueint) ||
                 (cJSON_IsString(value) && value->valuestring &&
                  (strcmp(value->valuestring, "1") == 0 || strcmp(value->valuestring, "true") == 0));
        text_component_normalize_selection_range(component, &start, &end);
        text_component_begin_edit(component, TEXT_EDIT_STYLE);
        if (start < end) {
            if (enable) {
                text_style_runs_apply(&component->style_runs, &component->style_run_count,
                                      start, end, flag, 0);
            } else {
                text_style_runs_apply(&component->style_runs, &component->style_run_count,
                                      start, end, 0, flag);
            }
        } else if (enable) {
            component->typing_style |= flag;
        } else {
            component->typing_style &= ~flag;
        }
        mark_layer_dirty(component->layer, DIRTY_TEXT);
        return 1;
    }
    if (strcmp(key, "toggleBold") == 0) {
        if (!component->editable) {
            return 0;
        }
        return text_component_toggle_style(component, TEXT_STYLE_BOLD);
    }
    if (strcmp(key, "toggleItalic") == 0) {
        if (!component->editable) {
            return 0;
        }
        return text_component_toggle_style(component, TEXT_STYLE_ITALIC);
    }
    if (strcmp(key, "moveLineStart") == 0) {
        component->cursor_pos = get_line_start(component, component->cursor_pos);
        component->cursor_visual_line = -1;
        if (!(cJSON_IsTrue(value) || (cJSON_IsNumber(value) && value->valueint) ||
              (cJSON_IsString(value) && value->valuestring &&
               (strcmp(value->valuestring, "1") == 0 || strcmp(value->valuestring, "true") == 0)))) {
            component->selection_start = -1;
            component->selection_end = -1;
        }
        text_component_update_scroll_for_cursor(component);
        return 1;
    }
    if (strcmp(key, "moveLineEnd") == 0) {
        component->cursor_pos = get_line_end(component, component->cursor_pos);
        component->cursor_visual_line = -1;
        component->selection_start = -1;
        component->selection_end = -1;
        text_component_update_scroll_for_cursor(component);
        return 1;
    }
    if (strcmp(key, "maxLength") == 0) {
        if (!text_component_parse_int_value(value, &n)) {
            return 0;
        }
        text_component_set_max_length(component, n);
        return 1;
    }
    if (strcmp(key, "editable") == 0) {
        int on = 0;
        if (cJSON_IsBool(value)) {
            on = cJSON_IsTrue(value);
        } else if (!text_component_parse_int_value(value, &on)) {
            return 0;
        }
        text_component_set_editable(component, on);
        return 1;
    }
    if (strcmp(key, "fontSize") == 0) {
        if (!text_component_parse_int_value(value, &n)) {
            return 0;
        }
        if (!layer->font) {
            layer->font = (Font*)calloc(1, sizeof(Font));
            if (!layer->font) {
                return 0;
            }
            strcpy(layer->font->path, "Roboto-Regular.ttf");
            strcpy(layer->font->weight, "normal");
        }
        layer->font->size = n;
        text_component_invalidate_layout(component);
        mark_layer_dirty(layer, DIRTY_TEXT | DIRTY_LAYOUT);
        return 1;
    }

    return 0;
}




// 设置 onChange 回调函数
void text_component_set_on_change(TextComponent* component, EventHandler callback, void* user_data) {
    if (!component) {
        return;
    }
    
    component->on_change = callback;
    component->change_name = user_data;
}

// 调用 onChange 回调函数
void text_component_trigger_on_change(TextComponent* component) {
    // 如果没有事件处理器但有事件名称，尝试查找事件处理器
    if(component->on_change == NULL && component->change_name != NULL){
        const char* event_name = component->change_name;
        if (event_name[0] == '@') {
            event_name++;
        }
        EventHandler handler = find_event_by_name(event_name);
        component->on_change = handler;
    }
    
    // 检查是否有可用的事件处理器
    if (component->on_change) {
        component->on_change(component->layer);
    } else if (component->change_name) {
        printf("text_component_trigger_on_change not found onchange event %s\n", component->change_name);
        print_registered_events();
    }
}

// 删除选中文本
static void text_component_delete_selection(TextComponent* component) {
    if (component->selection_start == -1 || component->selection_end == -1) {
        return;
    }
    
    int start = component->selection_start;
    int end = component->selection_end;
    
    if (start > end) {
        int temp = start;
        start = end;
        end = temp;
    }
    
    // 确保start不为负数
    if (start < 0) start = 0;
    // 确保end不超过文本长度
    int text_len = strlen(component->layer->text);
    if (end > text_len) end = text_len;
    
    // 只有当start < end时才执行删除操作
    if (start < end) {
        text_component_begin_edit(component, TEXT_EDIT_DELETE);

        // 计算新文本的长度
        size_t new_len = text_len - (end - start);
        
        // 创建临时缓冲区存储新文本
        char* new_text = malloc(new_len + 1);
        if (!new_text) {
            return;
        }
        
        // 复制start之前的部分
        memcpy(new_text, component->layer->text, start);
        // 复制end之后的部分
        memcpy(new_text + start, component->layer->text + end, text_len - end + 1);
        
        // 设置新文本
        layer_set_text(component->layer, new_text);
        free(new_text);

        text_style_runs_delete(&component->style_runs, &component->style_run_count, start, end);
        
        component->cursor_pos = start;
        text_component_invalidate_layout(component);
        
        // 更新内容高度
        text_component_update_content_height(component);
        
        // 触发 onChange 事件
        text_component_trigger_on_change(component);
    }
    
    component->selection_start = -1;
    component->selection_end = -1;
}

// 在光标位置插入字符
static void text_component_insert_char(TextComponent* component, char c) {
    if (!component->editable) {
        return;
    }

    int len = strlen(component->layer->text);
    int has_selection = 0;
    int insert_at;

    // 如果有选中的文本，先删除
    if (component->selection_start != -1 && component->selection_end != -1) {
        int start = component->selection_start;
        int end = component->selection_end;
        if (start > end) {
            int temp = start;
            start = end;
            end = temp;
        }
        if (start != end) {
            has_selection = 1;
        }
    }

    text_component_begin_edit(component, has_selection ? TEXT_EDIT_REPLACE : TEXT_EDIT_TYPING);

    if (has_selection) {
        text_component_history_suppress_begin(component);
        text_component_delete_selection(component);
        text_component_history_suppress_end(component);
        // 更新len，因为删除选择后文本长度可能改变
        len = strlen(component->layer->text);
    }
    
    // 确保cursor_pos在有效范围内
    if (component->cursor_pos < 0) component->cursor_pos = 0;
    if (component->cursor_pos > len) component->cursor_pos = len;
    insert_at = component->cursor_pos;
    
    // 计算新文本长度
    size_t new_len = len + 1;
    
    // 创建临时缓冲区存储新文本
    char* new_text = malloc(new_len + 1);
    if (!new_text) {
        return;
    }
    
    // 复制光标位置之前的部分
    memcpy(new_text, component->layer->text, component->cursor_pos);
    // 插入新字符
    new_text[component->cursor_pos] = c;
    // 复制光标位置之后的部分
    memcpy(new_text + component->cursor_pos + 1, component->layer->text + component->cursor_pos, len - component->cursor_pos + 1);
    
    // 设置新文本
    layer_set_text(component->layer, new_text);
    free(new_text);

    text_style_runs_insert(&component->style_runs, &component->style_run_count, insert_at, 1);
    if (component->typing_style) {
        text_style_runs_apply(&component->style_runs, &component->style_run_count,
                              insert_at, insert_at + 1, component->typing_style, 0);
    }
    
    component->cursor_pos++;
    text_component_invalidate_layout(component);
    
    // 更新内容高度（只调用一次）
    text_component_update_content_height(component);
    
    // 触发 onChange 事件
    text_component_trigger_on_change(component);
    
    // 更新滚动位置，确保光标可见
    text_component_update_scroll_for_cursor(component);
}

// 更新滚动位置以确保光标可见
void text_component_update_scroll_for_cursor(TextComponent* component) {
    Rect render_rect;
    int line_height;
    int line_stride;
    int text_len;
    const char* text;
    int cursor_line = 0;
    int i;

    if (!component || !component->layer) {
        return;
    }

    text_component_get_content_rect(component, component->layer, &render_rect);

    /* Multiline: keep cursor line visible via vertical scroll_offset */
    if (component->multiline) {
        if (!component->layer->text) {
            component->layer->scroll_offset = 0;
            return;
        }
        text = component->layer->text;
        text_len = (int)strlen(text);
        line_height = text_component_get_line_height(component);
        line_stride = line_height + TEXT_LINE_SPACING;
        text_component_ensure_layout(component, render_rect.w > 0 ? render_rect.w : 1);

        for (i = 0; i < component->layout_count; i++) {
            int start = 0;
            int end = 0;
            text_component_get_layout_line_range(component, text, text_len, i, &start, &end);
            if (component->cursor_pos < end ||
                (component->cursor_pos == end &&
                 ((end < text_len && text[end] == '\n') ||
                  component->cursor_visual_line == i)) ||
                i == component->layout_count - 1) {
                cursor_line = i;
                break;
            }
        }

        {
            int cursor_top = cursor_line * line_stride;
            int cursor_bottom = cursor_top + line_height;
            int visible = render_rect.h > 0 ? render_rect.h : component->layer->rect.h;
            int max_scroll;
            int content_h = text_component_calculate_content_height(component);

            if (component->layer->scroll_offset < 0) {
                component->layer->scroll_offset = 0;
            }
            if (cursor_top < component->layer->scroll_offset) {
                component->layer->scroll_offset = cursor_top;
            } else if (cursor_bottom > component->layer->scroll_offset + visible) {
                component->layer->scroll_offset = cursor_bottom - visible;
            }

            max_scroll = content_h - visible;
            if (max_scroll < 0) {
                max_scroll = 0;
            }
            if (component->layer->scroll_offset < 0) {
                component->layer->scroll_offset = 0;
            }
            if (component->layer->scroll_offset > max_scroll) {
                component->layer->scroll_offset = max_scroll;
            }
        }
        return;
    }

    /* Single-line: horizontal scroll_x */
    if (!component->layer->font || !component->layer->font->default_font) {
        return;
    }

    {
        int full_text_width = 0;
        int visible_width;
        int cursor_x;
        int cursor_pixel_x;
        char* before_cursor;

        if (component->layer->text && strlen(component->layer->text) > 0) {
            Texture* full_tex = backend_render_texture(component->layer->font->default_font,
                                                      component->layer->text,
                                                      component->layer->color);
            if (full_tex) {
                int full_width, full_height;
                backend_query_texture(full_tex, NULL, NULL, &full_width, &full_height);
                full_text_width = full_width / yui_density;
                backend_render_text_destroy(full_tex);
            }
        }

        visible_width = render_rect.w;
        if (full_text_width <= visible_width) {
            component->scroll_x = 0;
            return;
        }

        if (component->cursor_pos <= 0) {
            component->scroll_x = 0;
            return;
        }

        before_cursor = (char*)malloc((size_t)component->cursor_pos + 1);
        if (!before_cursor) {
            return;
        }
        strncpy(before_cursor, component->layer->text, (size_t)component->cursor_pos);
        before_cursor[component->cursor_pos] = '\0';

        cursor_x = render_rect.x;
        {
            Texture* text_tex = backend_render_texture(component->layer->font->default_font,
                                                      before_cursor,
                                                      component->layer->color);
            if (text_tex) {
                int text_width, text_height;
                backend_query_texture(text_tex, NULL, NULL, &text_width, &text_height);
                cursor_x = render_rect.x + text_width / yui_density;
                backend_render_text_destroy(text_tex);
            }
        }
        free(before_cursor);

        cursor_pixel_x = cursor_x - render_rect.x + component->scroll_x;
        if (cursor_pixel_x < component->scroll_x) {
            component->scroll_x = cursor_pixel_x;
        } else if (cursor_pixel_x > component->scroll_x + visible_width) {
            component->scroll_x = cursor_pixel_x - visible_width;
        }
        if (component->scroll_x < 0) {
            component->scroll_x = 0;
        }
        if (component->scroll_x > full_text_width - visible_width) {
            component->scroll_x = full_text_width - visible_width;
        }
    }
}

// 计算文本内容总高度
int text_component_calculate_content_height(TextComponent* component) {
    if (!component || !component->layer) {
        return 0;
    }

    if (!component->multiline) {
        return component->layer->rect.h;
    }

    const char* text = component->layer->text ? component->layer->text : "";
    int line_height = text_component_get_line_height(component);
    Rect content_rect;
    text_component_get_content_rect(component, component->layer, &content_rect);
    int max_width = content_rect.w;
    if (max_width < 1) max_width = 1;

    text_component_ensure_layout(component, max_width);
    if (component->layout_count <= 0) {
        return line_height + TEXT_LINE_SPACING;
    }
    return component->layout_count * (line_height + TEXT_LINE_SPACING);
}



// 删除光标前的字符
static void text_component_delete_prev_char(TextComponent* component) {
    if (!component->editable) {
        return;
    }
    
    // 如果有选中的文本，先删除（只要选区存在就删除）
    if (component->selection_start != -1 && component->selection_end != -1) {
        // 确保start和end不相等，或者都指向有效位置
        int start = component->selection_start;
        int end = component->selection_end;
        if (start > end) {
            int temp = start;
            start = end;
            end = temp;
        }
        
        // 如果选区有效（start != end），删除选中内容
        if (start != end) {
            text_component_delete_selection(component);
            return;
        } else {
            // 选区无效，清除选区标记
            component->selection_start = -1;
            component->selection_end = -1;
        }
    }
    
    // 没有选区或选区无效，删除光标前的字符
    if (component->cursor_pos <= 0) {
        return;
    }
    
    int len = strlen(component->layer->text);
    
    // 确保不会越界
    if (component->cursor_pos > len) {
        component->cursor_pos = len;
    }
    
    if (component->cursor_pos > 0) {
        // 获取光标前一个 UTF-8 字符的字节长度
        int char_len = get_prev_utf8_char_len(component->layer->text, component->cursor_pos);
        if (char_len <= 0) char_len = 1;
        int del_start = component->cursor_pos - char_len;
        int del_end = component->cursor_pos;

        text_component_begin_edit(component, TEXT_EDIT_DELETE);
        
        // 计算新文本长度
        size_t new_len = len - char_len;
        
        // 创建临时缓冲区存储新文本
        char* new_text = malloc(new_len + 1);
        if (!new_text) {
            return;
        }
        
        // 复制光标位置之前的部分（不包括要删除的字符）
        memcpy(new_text, component->layer->text, component->cursor_pos - char_len);
        // 复制光标位置之后的部分
        memcpy(new_text + component->cursor_pos - char_len, component->layer->text + component->cursor_pos, len - component->cursor_pos + 1);
        
        // 设置新文本
        layer_set_text(component->layer, new_text);
        free(new_text);

        text_style_runs_delete(&component->style_runs, &component->style_run_count, del_start, del_end);
        
        component->cursor_pos -= char_len;
        text_component_invalidate_layout(component);
        
        // 触发 onChange 事件
        text_component_trigger_on_change(component);
        
        // 更新滚动位置，确保光标可见
        text_component_update_scroll_for_cursor(component);
    }
}

// 删除光标后的字符
static void text_component_delete_next_char(TextComponent* component) {
    if (!component->editable) {
        return;
    }
    
    // 如果有选中的文本，先删除（只要选区存在就删除）
    if (component->selection_start != -1 && component->selection_end != -1) {
        // 确保start和end不相等，或者都指向有效位置
        int start = component->selection_start;
        int end = component->selection_end;
        if (start > end) {
            int temp = start;
            start = end;
            end = temp;
        }
        
        // 如果选区有效（start != end），删除选中内容
        if (start != end) {
            text_component_delete_selection(component);
            return;
        } else {
            // 选区无效，清除选区标记
            component->selection_start = -1;
            component->selection_end = -1;
        }
    }
    
    int len = strlen(component->layer->text);
    
    // 确保cursor_pos在有效范围内
    if (component->cursor_pos < 0) component->cursor_pos = 0;
    if (component->cursor_pos >= len) {
        return; // 已经在文本末尾，无需删除
    }

    text_component_begin_edit(component, TEXT_EDIT_DELETE);
    
    // 计算新文本长度
    size_t new_len = len - 1;
    
    // 创建临时缓冲区存储新文本
    char* new_text = malloc(new_len + 1);
    if (!new_text) {
        return;
    }
    
    // 复制光标位置之前的部分
    memcpy(new_text, component->layer->text, component->cursor_pos);
    // 复制光标位置之后的部分（跳过要删除的字符）
    memcpy(new_text + component->cursor_pos, component->layer->text + component->cursor_pos + 1, len - component->cursor_pos);
    
    // 设置新文本
    layer_set_text(component->layer, new_text);
    free(new_text);

    text_style_runs_delete(&component->style_runs, &component->style_run_count,
                           component->cursor_pos, component->cursor_pos + 1);
    text_component_invalidate_layout(component);
    
    // 更新内容高度
    text_component_update_content_height(component);
    
    // 触发 onChange 事件
    text_component_trigger_on_change(component);
}

// 规范化插入文本：\r\n / \r -> \n，跳过其它不可见控制字符
static int text_component_normalize_insert_text(const char* src, char* dst) {
    int j = 0;
    for (int i = 0; src[i]; i++) {
        unsigned char c = (unsigned char)src[i];
        if (c == '\r') {
            if (src[i + 1] != '\n') {
                dst[j++] = '\n';
            }
        } else if (c < 32 && c != '\n' && c != '\t') {
            continue;
        } else {
            dst[j++] = (char)c;
        }
    }
    dst[j] = '\0';
    return j;
}

// 在光标位置插入文本
static void text_component_insert_text(TextComponent* component, const char* text) {
    if (!component || !text || !component->editable) {
        return;
    }

    int src_len = (int)strlen(text);
    if (src_len == 0) {
        return;
    }

    char* normalized = (char*)malloc((size_t)src_len + 1);
    if (!normalized) {
        return;
    }
    int text_len = text_component_normalize_insert_text(text, normalized);
    if (text_len == 0) {
        free(normalized);
        return;
    }
    const char* insert_text = normalized;
    
    int current_len = strlen(component->layer->text);
    int has_selection = 0;
    int insert_at;
    
    // 如果有选中的文本，先删除
    if (component->selection_start != -1 && component->selection_end != -1) {
        int start = component->selection_start;
        int end = component->selection_end;
        if (start > end) {
            int temp = start;
            start = end;
            end = temp;
        }
        if (start != end) {
            has_selection = 1;
        }
    }

    if (has_selection) {
        text_component_begin_edit(component, TEXT_EDIT_REPLACE);
        text_component_history_suppress_begin(component);
        text_component_delete_selection(component);
        text_component_history_suppress_end(component);
        current_len = strlen(component->layer->text);
    }
    
    // 确保cursor_pos在有效范围内
    if (component->cursor_pos < 0) component->cursor_pos = 0;
    if (component->cursor_pos > current_len) component->cursor_pos = current_len;
    insert_at = component->cursor_pos;
    
    // 检查是否包含换行符，如果包含则设置为多行模式
    int has_newlines = 0;
    for (int i = 0; i < text_len; i++) {
        if (insert_text[i] == '\n') {
            has_newlines = 1;
            break;
        }
    }
    if (has_newlines && !component->multiline) {
        component->multiline = 1;
    }
    
    // Enforce maxLength when configured (> 0)
    if (component->max_length > 0 && current_len + text_len >= component->max_length) {
        text_len = component->max_length - current_len - 1;
        if (text_len <= 0) {
            free(normalized);
            return;
        }
    }

    if (!has_selection) {
        text_component_begin_edit(component, TEXT_EDIT_REPLACE);
    }
    
    // 计算新文本长度
    size_t new_len = current_len + text_len;
    
    // 创建临时缓冲区存储新文本
    char* new_text = malloc(new_len + 1);
    if (!new_text) {
        free(normalized);
        return;
    }
    
    // 复制光标位置之前的部分
    memcpy(new_text, component->layer->text, component->cursor_pos);
    // 插入新文本
    memcpy(new_text + component->cursor_pos, insert_text, text_len);
    // 复制光标位置之后的部分
    memcpy(new_text + component->cursor_pos + text_len, 
           component->layer->text + component->cursor_pos, 
           current_len - component->cursor_pos + 1);
    
    // 设置新文本
    layer_set_text(component->layer, new_text);
    free(new_text);
    free(normalized);

    text_style_runs_insert(&component->style_runs, &component->style_run_count, insert_at, text_len);
    if (component->typing_style) {
        text_style_runs_apply(&component->style_runs, &component->style_run_count,
                              insert_at, insert_at + text_len, component->typing_style, 0);
    }
    
    component->cursor_pos += text_len;
    text_component_invalidate_layout(component);
    
    // 更新内容高度（只调用一次）
    text_component_update_content_height(component);
    
    // 触发 onChange 事件
    text_component_trigger_on_change(component);
    
    // 清除选择
    component->selection_start = -1;
    component->selection_end = -1;
}

// 处理键盘事件
int text_component_handle_key_event(Layer* layer, KeyEvent* event) {
    if (!layer || !event || !layer->component) {
        return 0;
    }

    TextComponent* component = (TextComponent*)layer->component;

    if (!component->editable) {
        return 0;
    }
    
    // 处理特殊键
    if (event->type == KEY_EVENT_DOWN) {
        switch (event->data.key.key_code) {
            case SDLK_BACKSPACE:
                text_component_delete_prev_char(component);
                break;
            case SDLK_DELETE:
                text_component_delete_next_char(component);
                break;
            case SDLK_LEFT:
                if (component->cursor_pos > 0) {
                    // 如果按住Shift键，需要先检查是否已有选择
                    if (event->data.key.mod & KMOD_SHIFT) {
                        // 如果没有选择，开始新选择
                        if (component->selection_start == -1) {
                            component->selection_start = component->cursor_pos;
                        }
                        // 移动光标
                        component->cursor_pos--;
                        // 更新选择结束位置
                        component->selection_end = component->cursor_pos;
                    } else {
                        // 如果没有按Shift，只是移动光标并清除选择
                        component->cursor_pos--;
                        component->selection_start = -1;
                        component->selection_end = -1;
                    }
                    
                    // 更新滚动位置，确保光标可见
                    text_component_update_scroll_for_cursor(component);
                }
                break;
            case SDLK_RIGHT:
                if (component->cursor_pos < strlen(component->layer->text)) {
                    // 如果按住Shift键，需要先检查是否已有选择
                    if (event->data.key.mod & KMOD_SHIFT) {
                        // 如果没有选择，开始新选择
                        if (component->selection_start == -1) {
                            component->selection_start = component->cursor_pos;
                        }
                        // 移动光标
                        component->cursor_pos++;
                        // 更新选择结束位置
                        component->selection_end = component->cursor_pos;
                    } else {
                        // 如果没有按Shift，只是移动光标并清除选择
                        component->cursor_pos++;
                        component->selection_start = -1;
                        component->selection_end = -1;
                    }
                    
                    // 更新滚动位置，确保光标可见
                    text_component_update_scroll_for_cursor(component);
                }
                break;
            case SDLK_UP:
                if (component->multiline) {
                    // 先保存当前光标位置作为选择起始点
                    int old_cursor_pos = component->cursor_pos;
                    
                    // 简单的向上移动光标实现
                    int pos = component->cursor_pos;
                    while (pos > 0 && component->layer->text[pos] != '\n') {
                        pos--;
                    }
                    if (pos > 0) {
                        pos--;
                        // 尽量保持在同一列
                        int col = 0;
                        int temp = component->cursor_pos;
                        while (temp > 0 && component->layer->text[temp - 1] != '\n') {
                            col++;
                            temp--;
                        }
                        
                        temp = pos;
                        while (col > 0 && temp < strlen(component->layer->text) && component->layer->text[temp] != '\n') {
                            temp++;
                            col--;
                        }
                        component->cursor_pos = temp;
                    } else {
                        component->cursor_pos = 0;
                    }
                    
                    // 处理选择
                    if (event->data.key.mod & KMOD_SHIFT) {
                        // 如果没有选择，开始新选择
                        if (component->selection_start == -1) {
                            component->selection_start = old_cursor_pos;
                        }
                        // 更新选择结束位置
                        component->selection_end = component->cursor_pos;
                    } else {
                        // 清除选择
                        component->selection_start = -1;
                        component->selection_end = -1;
                    }
                }
                break;
            case SDLK_DOWN:
                if (component->multiline) {
                    // 先保存当前光标位置作为选择起始点
                    int old_cursor_pos = component->cursor_pos;
                    
                    // 简单的向下移动光标实现
                    int pos = component->cursor_pos;
                    while (pos < strlen(component->layer->text) && component->layer->text[pos] != '\n') {
                        pos++;
                    }
                    if (pos < strlen(component->layer->text)) {
                        pos++;
                        // 尽量保持在同一列
                        int col = 0;
                        int temp = component->cursor_pos;
                        while (temp > 0 && component->layer->text[temp - 1] != '\n') {
                            col++;
                            temp--;
                        }
                        
                        temp = pos;
                        while (col > 0 && temp < strlen(component->layer->text) && component->layer->text[temp] != '\n') {
                            temp++;
                            col--;
                        }
                        component->cursor_pos = temp;
                    } else {
                        component->cursor_pos = strlen(component->layer->text);
                    }
                    
                    // 处理选择
                    if (event->data.key.mod & KMOD_SHIFT) {
                        // 如果没有选择，开始新选择
                        if (component->selection_start == -1) {
                            component->selection_start = old_cursor_pos;
                        }
                        // 更新选择结束位置
                        component->selection_end = component->cursor_pos;
                    } else {
                        // 清除选择
                        component->selection_start = -1;
                        component->selection_end = -1;
                    }
                }
                break;
            case SDLK_RETURN:
            case SDLK_KP_ENTER:
                // 无论是否为多行模式，都支持换行
                text_component_insert_char(component, '\n');
                // 如果是单行模式，设置为多行模式以显示换行效果
                if (!component->multiline) {
                    component->multiline = true;
                }
                break;
            case SDLK_a:
                // Ctrl+A 全选
                if (event->data.key.mod & KMOD_CTRL) {
                    component->selection_start = 0;
                    component->selection_end = strlen(component->layer->text);
                    component->cursor_pos = strlen(component->layer->text);
                } else if (event->type == KEY_EVENT_TEXT_INPUT) {
                    text_component_insert_char(component, 'a');
                }
                break;
            case SDLK_c:
                // Ctrl+C 复制（暂时仅打印到控制台）
                if (event->data.key.mod & KMOD_CTRL) {
                    if (component->selection_start != -1 && component->selection_end != -1) {
                        int start = component->selection_start;
                        int end = component->selection_end;
                        if (start > end) {
                            int temp = start;
                            start = end;
                            end = temp;
                        }
                        
                        // 提取选中文本
                        int len = end - start;
                        char* selected_text = (char*)malloc(len + 1);
                        if (selected_text) {
                            strncpy(selected_text, component->layer->text + start, len);
                            selected_text[len] = '\0';
                            printf("Copied: '%s'\n", selected_text);
                            // 复制到剪贴板
                            backend_set_clipboard_text(selected_text);
                            free(selected_text);
                        }
                    }
                } else if (event->type == KEY_EVENT_TEXT_INPUT) {
                    text_component_insert_char(component, 'c');
                }
                break;
            case SDLK_v:
                // Ctrl+V 粘贴
                if (event->data.key.mod & KMOD_CTRL) {
                    // 从剪贴板读取文本
                    char* clipboard_text = backend_get_clipboard_text();
                    if (clipboard_text) {
                        text_component_insert_text(component, clipboard_text);
                        free(clipboard_text);
                    }
                } else if (event->type == KEY_EVENT_TEXT_INPUT) {
                    text_component_insert_char(component, 'v');
                }
                break;
            case SDLK_x:
                // Ctrl+X 剪切
                if (event->data.key.mod & KMOD_CTRL) {
                    if (component->selection_start != -1 && component->selection_end != -1) {
                        int start = component->selection_start;
                        int end = component->selection_end;
                        if (start > end) {
                            int temp = start;
                            start = end;
                            end = temp;
                        }
                        
                        // 提取选中文本
                        int len = end - start;
                        char* selected_text = (char*)malloc(len + 1);
                        if (selected_text) {
                            strncpy(selected_text, component->layer->text + start, len);
                            selected_text[len] = '\0';
                            printf("Cut: '%s'\n", selected_text);
                            // 复制到剪贴板
                            backend_set_clipboard_text(selected_text);
                            free(selected_text);

                            // 删除选中文本
                            text_component_delete_selection(component);
                        }
                    }
                } else if (event->type == KEY_EVENT_TEXT_INPUT) {
                    text_component_insert_char(component, 'x');
                }
                break;
            case SDLK_z:
                if (event->data.key.mod & KMOD_CTRL) {
                    if (event->data.key.mod & KMOD_SHIFT) {
                        text_component_do_redo(component);
                    } else {
                        text_component_do_undo(component);
                    }
                } else if (event->type == KEY_EVENT_TEXT_INPUT) {
                    text_component_insert_char(component, 'z');
                }
                break;
            case SDLK_y:
                if (event->data.key.mod & KMOD_CTRL) {
                    text_component_do_redo(component);
                } else if (event->type == KEY_EVENT_TEXT_INPUT) {
                    text_component_insert_char(component, 'y');
                }
                break;
            case SDLK_b:
            case 'B':
                if (event->data.key.mod & KMOD_CTRL) {
                    text_component_toggle_style(component, TEXT_STYLE_BOLD);
                    return 1;
                } else if (event->type == KEY_EVENT_TEXT_INPUT) {
                    text_component_insert_char(component, 'b');
                }
                break;
            case SDLK_i:
            case 'I':
                if (event->data.key.mod & KMOD_CTRL) {
                    text_component_toggle_style(component, TEXT_STYLE_ITALIC);
                    return 1;
                } else if (event->type == KEY_EVENT_TEXT_INPUT) {
                    text_component_insert_char(component, 'i');
                }
                break;
            default:
                // 处理其他普通字符输入
                if (event->type == KEY_EVENT_TEXT_INPUT) {
                    for (int i = 0; i < strlen(event->data.text.text); i++) {
                        // 移除isprint限制，支持所有字符（包括中文等多字节字符）
                        // 但过滤掉控制字符（ASCII < 32，但不包括制表符、换行符等特殊控制字符）
                        unsigned char c = event->data.text.text[i];
                        if (c >= 32 || c == '\t' || c == '\n') {
                            text_component_insert_char(component, event->data.text.text[i]);
                        }
                    }
                }
                break;
        }
    } else if (event->type == KEY_EVENT_TEXT_INPUT) {
        // 处理文本输入
        for (int i = 0; i < strlen(event->data.text.text); i++) {
            // 移除isprint限制，支持所有字符（包括中文等多字节字符）
            // 但过滤掉控制字符（ASCII < 32，但不包括制表符、换行符等特殊控制字符）
            unsigned char c = event->data.text.text[i];
            if (c >= 32 || c == '\t' || c == '\n') {
                text_component_insert_char(component, event->data.text.text[i]);
            }
        }
    } else if (event->type == KEY_EVENT_TEXT_EDITING) {
        // 处理 IME 文本编辑事件（候选词预览）
        // 这里可以显示候选词或组合字符串
        // 暂时只打印调试信息
        printf("IME Editing: '%s', start=%d, length=%d\n",
               event->data.text.text,
               event->data.text.start,
               event->data.text.length);
    }
    return 0;
}







static int text_component_get_vertical_scrollbar_track(Layer* layer, Rect* track_out) {
    if (!layer || !track_out) return 0;

    int visible_height = layer->rect.h;
    visible_height -= layer_padding_get(layer, 0) + layer_padding_get(layer, 2);

    if ((layer->scrollable == 1 || layer->scrollable == 3) &&
        layer->scrollbar_v && layer->scrollbar_v->visible &&
        layer->content_height > visible_height) {
        int scrollbar_width = layer->scrollbar_v->thickness > 0 ? layer->scrollbar_v->thickness : 8;
        track_out->x = layer->rect.x + layer->rect.w - scrollbar_width;
        track_out->y = layer->rect.y;
        track_out->w = scrollbar_width;
        track_out->h = visible_height;
        return 1;
    }

    TextComponent* component = (TextComponent*)layer->component;
    if (component && component->multiline &&
        layer->scrollable != 1 && layer->scrollable != 3) {
        int pad_top;
        int pad_right;
        int pad_bottom;
        int inner_visible;
        text_component_get_padding(layer, &pad_top, &pad_right, &pad_bottom, NULL);
        inner_visible = layer->rect.h - pad_top - pad_bottom;
        if (layer->content_height <= inner_visible) {
            return 0;
        }
        track_out->x = layer->rect.x + layer->rect.w - pad_right - TEXT_INTERNAL_SCROLLBAR_WIDTH;
        track_out->y = layer->rect.y + pad_top;
        track_out->w = TEXT_INTERNAL_SCROLLBAR_WIDTH;
        track_out->h = layer->rect.h - pad_top - pad_bottom;
        return 1;
    }
    return 0;
}

static int text_component_point_in_vertical_scrollbar(Layer* layer, Point pt) {
    Rect track;
    if (!text_component_get_vertical_scrollbar_track(layer, &track)) return 0;
    return point_in_rect(pt, track);
}

static void text_component_get_content_rect(TextComponent* component, Layer* layer, Rect* out) {
    if (!component || !layer || !out) return;

    int pad_top;
    int pad_right;
    int pad_bottom;
    int left_padding = text_component_get_left_padding(component);
    text_component_get_padding(layer, &pad_top, &pad_right, &pad_bottom, NULL);

    out->x = layer->rect.x + left_padding;
    out->y = layer->rect.y + pad_top;
    out->w = layer->rect.w - left_padding - pad_right;
    out->h = layer->rect.h - pad_top - pad_bottom;

    Rect track;
    if (text_component_get_vertical_scrollbar_track(layer, &track)) {
        int content_right = out->x + out->w;
        if (content_right > track.x) {
            out->w = track.x - out->x;
            if (out->w < 0) out->w = 0;
        }
    }
}

static void text_component_get_line_range_at_pos(TextComponent* component, Layer* layer,
                                                 int pos, int* out_start, int* out_end) {
    if (!component || !layer || !out_start || !out_end) return;

    const char* text = layer->text ? layer->text : "";
    int text_len = (int)strlen(text);
    if (pos < 0) pos = 0;
    if (pos > text_len) pos = text_len;

    if (!component->multiline) {
        *out_start = 0;
        *out_end = text_len;
        return;
    }

    Rect render_rect;
    text_component_get_content_rect(component, layer, &render_rect);
    text_component_ensure_layout(component, render_rect.w);

    for (int i = 0; i < component->layout_count; i++) {
        int start = component->layout_starts[i];
        int end = (i + 1 < component->layout_count)
            ? component->layout_starts[i + 1] : text_len;
        while (end > start && text[end - 1] == '\n') {
            end--;
        }

        if (pos >= start && pos <= end) {
            *out_start = start;
            *out_end = end;
            return;
        }
    }

    *out_start = get_line_start(component, pos);
    *out_end = get_line_end(component, pos);
}

static void text_component_focus(TextComponent* component) {
    if (!component || !component->layer) return;
    Layer* layer = component->layer;

    if (focused_layer && focused_layer != layer) {
        CLEAR_STATE(focused_layer, LAYER_STATE_FOCUSED);
    }
    focused_layer = layer;
    SET_STATE(layer, LAYER_STATE_FOCUSED);

    if (component->editable) {
        backend_start_text_input();
        Rect rect = {
            layer->rect.x,
            layer->rect.y + layer->rect.h,
            layer->rect.w,
            0
        };
        backend_set_text_input_rect(&rect);
    }
}

static void text_component_blur(TextComponent* component) {
    if (!component || !component->layer) return;
    Layer* layer = component->layer;

    CLEAR_STATE(layer, LAYER_STATE_FOCUSED);
    if (focused_layer == layer) {
        focused_layer = NULL;
    }
    backend_stop_text_input();
}

// 处理鼠标事件
int text_component_handle_pointer_event(Layer* layer, PointerEvent* event) {
    if (!layer || !event || !layer->component) {
        return 0;
    }
    
    TextComponent* component = (TextComponent*)layer->component;
    Point pt = {event->x, event->y};

    if (event->phase == POINTER_WHEEL && component->multiline &&
        point_in_rect(pt, layer->rect)) {
        Rect content_rect;
        int max_scroll;

        text_component_get_content_rect(component, layer, &content_rect);
        text_component_update_content_height(component);
        max_scroll = layer->content_height - content_rect.h;
        if (max_scroll > 0) {
            layer->scroll_offset += event->delta_y * 20;
            if (layer->scroll_offset < 0) layer->scroll_offset = 0;
            if (layer->scroll_offset > max_scroll) layer->scroll_offset = max_scroll;
            return 1;
        }
    }

    if (layer->scrollbar_v && layer->scrollbar_v->is_dragging) {
        if (event->phase == POINTER_UP && event->button == SDL_BUTTON_LEFT) {
            component->is_selecting = 0;
        }
        return 1;
    }

    if (text_component_point_in_vertical_scrollbar(layer, pt)) {
        if (event->phase == POINTER_UP && event->button == SDL_BUTTON_LEFT) {
            component->is_selecting = 0;
        }
        return 1;
    }

    Rect content_rect;
    text_component_get_content_rect(component, layer, &content_rect);

    if (event->phase == POINTER_DOUBLE_TAP) {
        if (point_in_rect(pt, content_rect)) {
            int click_pos = text_component_get_position_from_point(component, pt, layer);
            int line_start, line_end;
            if (layer->focusable) {
                text_component_focus(component);
            }
            text_component_get_line_range_at_pos(component, layer, click_pos,
                                                   &line_start, &line_end);
            component->selection_start = line_start;
            component->selection_end = line_end;
            component->cursor_pos = line_end;
            component->is_selecting = 0;
            text_component_update_scroll_for_cursor(component);
            mark_layer_dirty(layer, DIRTY_TEXT);
            return 1;
        }
    }
    
    // 鼠标左键按下 - 设置光标位置并开始选择
    if (event->phase == POINTER_DOWN && event->button == SDL_BUTTON_LEFT) {
        if (point_in_rect(pt, content_rect)) {
            if (layer->focusable) {
                text_component_focus(component);
            }

            int click_pos = text_component_get_position_from_point(component, pt, layer);

            component->cursor_pos = click_pos;
            component->selection_start = click_pos;
            component->selection_end = click_pos;
            component->is_selecting = 1;
            text_component_update_scroll_for_cursor(component);
            return 1;
        }

        if (point_in_rect(pt, layer->rect)) {
            component->selection_start = -1;
            component->selection_end = -1;
            component->is_selecting = 0;
        } else if (focused_layer == layer) {
            component->selection_start = -1;
            component->selection_end = -1;
            component->is_selecting = 0;
            text_component_blur(component);
        }
    }
    // 鼠标拖动 - 更新选择范围
    else if (event->phase == POINTER_MOVE && (event->button == SDL_BUTTON_LEFT) && component->is_selecting) {
        int drag_pos;
        
        if (point_in_rect(pt, content_rect)) {
            // 在文本区域内，使用正常计算
            drag_pos = text_component_get_position_from_point(component, pt, layer);
        } else {
            // 在文本区域外，根据位置估算
            if (pt.x < layer->rect.x) {
                // 拖到左侧，选择到当前光标所在行的行首
                drag_pos = get_line_start(component, component->cursor_pos);
            } else if (pt.x > layer->rect.x + layer->rect.w) {
                // 拖到右侧，选择到当前光标所在行的行尾
                drag_pos = get_line_end(component, component->cursor_pos);
            } else if (pt.y < layer->rect.y) {
                // 拖到上方，选择到文本开头
                drag_pos = 0;
            } else if (pt.y > layer->rect.y + layer->rect.h) {
                // 拖到下方，选择到文本末尾
                drag_pos = strlen(component->layer->text);
            } else {
                // 默认情况，保持当前位置
                drag_pos = component->cursor_pos;
            }
        }
        
        // 更新选择范围和光标位置
        component->selection_end = drag_pos;
        component->cursor_pos = drag_pos;
        
        // 更新滚动位置，确保光标可见
        text_component_update_scroll_for_cursor(component);
    }
    // 鼠标释放 - 结束选择
    else if (event->phase == POINTER_UP && event->button == SDL_BUTTON_LEFT) {
        component->is_selecting = 0;
    }
    return 0;
}









// 辅助函数：获取指定位置所在行的起始位置
int get_line_start(TextComponent* component, int pos) {
    if (!component || pos < 0) {
        return 0;
    }
    
    // 限制pos在文本范围内
    if (pos > strlen(component->layer->text)) {
        pos = strlen(component->layer->text);
    }
    
    // 从pos向前查找换行符
    int start = pos;
    while (start > 0 && component->layer->text[start - 1] != '\n') {
        start--;
    }
    
    return start;
}

// 辅助函数：获取指定位置所在行的结束位置
int get_line_end(TextComponent* component, int pos) {
    if (!component || pos < 0) {
        return 0;
    }
    
    // 限制pos在文本范围内
    if (pos > strlen(component->layer->text)) {
        pos = strlen(component->layer->text);
    }
    
    // 从pos向后查找换行符或文本末尾
    int end = pos;
    while (end < strlen(component->layer->text) && component->layer->text[end] != '\n') {
        end++;
    }
    
    return end;
}

static void text_component_get_layout_line_range(TextComponent* component, const char* text,
                                                 int text_len, int line_index,
                                                 int* out_start, int* out_end) {
    int start = 0;
    int end = 0;

    if (component && component->layout_count > 0 &&
        line_index >= 0 && line_index < component->layout_count) {
        start = component->layout_starts[line_index];
        end = (line_index + 1 < component->layout_count)
            ? component->layout_starts[line_index + 1] : text_len;
        while (end > start && text[end - 1] == '\n') {
            end--;
        }
    }

    if (out_start) *out_start = start;
    if (out_end) *out_end = end;
}

static int text_component_position_in_layout_line(TextComponent* component, const char* text,
                                                  int start, int end, int click_x) {
    int best_pos = start;
    int best_distance = abs(click_x);
    int pos = start;

    while (pos < end) {
        int next = text_component_grapheme_end(text, pos, end);
        int width;
        int distance;

        if (next <= pos) {
            int clen = utf8_char_len_at(text + pos);
            next = pos + (clen > 0 ? clen : 1);
        }
        if (next > end) next = end;

        width = text_component_measure_width(component, text, start, next);
        distance = abs(click_x - width);
        if (distance < best_distance) {
            best_distance = distance;
            best_pos = next;
        }
        pos = next;
    }
    return best_pos;
}

// 辅助函数：根据鼠标坐标计算对应的文本位置
int text_component_get_position_from_point(TextComponent* component, Point pt, Layer* layer) {
    if (!component || !layer) {
        return 0;
    }
    
    Rect render_rect;
    text_component_get_content_rect(component, layer, &render_rect);
    
    // 边界检查
    if (pt.y < render_rect.y) return 0;
    if (pt.y > render_rect.y + render_rect.h) return strlen(component->layer->text);
    if (pt.x < render_rect.x) return 0;
    
    // 如果没有文本，返回0
    if (strlen(component->layer->text) == 0) {
        return 0;
    }
    
    if (component->multiline) {
        const char* text = component->layer->text;
        int text_len = (int)strlen(text);
        int line_height = text_component_get_line_height(component);
        int line_stride = line_height + TEXT_LINE_SPACING;
        int target_line = (pt.y - render_rect.y + layer->scroll_offset) / line_stride;
        int start;
        int end;

        text_component_ensure_layout(component, render_rect.w);
        if (target_line < 0) target_line = 0;
        if (target_line >= component->layout_count) return text_len;
        component->cursor_visual_line = target_line;
        text_component_get_layout_line_range(component, text, text_len, target_line, &start, &end);
        return text_component_position_in_layout_line(component, text, start, end,
                                                      pt.x - render_rect.x);
    } else {
        component->cursor_visual_line = -1;
        return text_component_position_in_layout_line(component, component->layer->text,
                                                      0, (int)strlen(component->layer->text),
                                                      pt.x - render_rect.x + component->scroll_x);
    }
}

// 渲染文本组件
void text_component_render(Layer* layer) {
    if (!layer || !layer->component) {
        return;
    }
    TextComponent* component = (TextComponent*)layer->component;
    if (layer->font && !layer->font->default_font) {
        load_all_fonts(layer);
    }
    if (layer->dirty_flags & DIRTY_TEXT) {
        text_component_invalidate_layout(component);
        layer->dirty_flags &= ~DIRTY_TEXT;
    }
    component->syntax_config.default_color = layer->color;
    text_component_update_content_height(component);
    // 绘制背景
    backend_render_fill_rect(&layer->rect, layer->bg_color);
    
    // 如果显示行号且为多行模式，绘制行号背景和行号
    if (component->show_line_numbers && component->multiline) {
        int pad_top;
        int pad_right;
        int pad_bottom;
        int pad_left;
        Rect line_number_bg;
        text_component_get_padding(layer, &pad_top, &pad_right, &pad_bottom, &pad_left);
        line_number_bg.x = layer->rect.x + pad_left;
        line_number_bg.y = layer->rect.y + pad_top;
        line_number_bg.w = component->line_number_width;
        line_number_bg.h = layer->rect.h - pad_top - pad_bottom;
        backend_render_fill_rect(&line_number_bg, component->line_number_bg_color);
        
        // 绘制分隔线
        Rect separator_line = {
            line_number_bg.x + line_number_bg.w,
            line_number_bg.y,
            1,
            line_number_bg.h
        };
        Color separator_color = {51, 51, 51, 255};
        backend_render_fill_rect(&separator_line, separator_color);
        
        // 获取行高
        int line_height = text_component_get_line_height(component);
        
        // 遍历文本，使用与渲染相同的算法，为每个逻辑行渲染行号
        char* text = component->layer->text;
        int text_len = strlen(text);
        int current_pos = 0;
        int visual_line = 0;  // 视觉行号
        int logical_line = 1;  // 逻辑行号（从1开始）
        // 计算文本内容区域的宽度（与正文渲染区域一致）
        Rect content_rect_for_wrap;
        text_component_get_content_rect(component, layer, &content_rect_for_wrap);
        int max_width = content_rect_for_wrap.w;
        
        while (current_pos < text_len) {
            // 记录当前逻辑行的起始视觉行
            int logical_line_visual_start = visual_line;
            
            // 查找当前逻辑行的结束位置
            int line_end = current_pos;
            while (line_end < text_len) {
                if (text[line_end] == '\n') {
                    break;
                }
                line_end++;
            }
            
            // 处理这个逻辑行的所有视觉行（包括自动换行产生的）
            int logical_line_pos = current_pos;
            if (logical_line_pos >= line_end) {
                // 空行，也占一个视觉行
                visual_line++;
            } else {
                while (logical_line_pos < line_end) {
                    // 计算剩余文本的宽度
                    int current_width = 0;
                    char* temp_line = (char*)malloc(line_end - logical_line_pos + 1);
                    if (temp_line) {
                        strncpy(temp_line, text + logical_line_pos, line_end - logical_line_pos);
                        temp_line[line_end - logical_line_pos] = '\0';
                        
                        Texture* line_tex = backend_render_texture(layer->font->default_font, temp_line, layer->color);
                        if (line_tex) {
                            int line_width, line_height_ignore;
                            backend_query_texture(line_tex, NULL, NULL, &line_width, &line_height_ignore);
                            current_width = line_width / yui_density;
                            backend_render_text_destroy(line_tex);
                        }
                        
                        free(temp_line);
                    }
                    
                    // 确定当前视觉行的结束位置
                    int split_pos = logical_line_pos;
                    if (current_width <= max_width || !component->wrap) {
                        split_pos = line_end;
                    } else {
                        split_pos = logical_line_pos;
                        while (split_pos < line_end) {
                            char* test_line = (char*)malloc(split_pos - logical_line_pos + 1);
                            if (test_line) {
                                strncpy(test_line, text + logical_line_pos, split_pos - logical_line_pos);
                                test_line[split_pos - logical_line_pos] = '\0';
                                
                                Texture* test_tex = backend_render_texture(layer->font->default_font, test_line, layer->color);
                                if (test_tex) {
                                    int test_width, test_height;
                                    backend_query_texture(test_tex, NULL, NULL, &test_width, &test_height);
                                    if (test_width / yui_density > max_width) {
                                        backend_render_text_destroy(test_tex);
                                        free(test_line);
                                        break;
                                    }
                                    backend_render_text_destroy(test_tex);
                                }
                                
                                free(test_line);
                            }
                            split_pos++;
                        } 
                        
                        if (split_pos > logical_line_pos) {
                            split_pos--;
                        }
                    }
                    
                    if (split_pos < logical_line_pos) {
                        split_pos = logical_line_pos;
                    }
                    
                    logical_line_pos = split_pos;
                    visual_line++;
                }
            }
            
            // 为这个逻辑行渲染行号（只在第一个视觉行的位置）
            int line_y = line_number_bg.y + logical_line_visual_start * (line_height + 2) - layer->scroll_offset;
            
            // 检查是否在可见范围内
            if (line_y + line_height > line_number_bg.y && line_y < line_number_bg.y + line_number_bg.h) {
                char line_num_str[16];
                snprintf(line_num_str, sizeof(line_num_str), "%d", logical_line);
                
                Texture* line_num_tex = backend_render_texture(layer->font->default_font, line_num_str, component->line_number_color);
                if (line_num_tex) {
                    int num_width, num_height;
                    backend_query_texture(line_num_tex, NULL, NULL, &num_width, &num_height);
                    
                    Rect line_num_rect = {
                        line_number_bg.x + line_number_bg.w - num_width / yui_density - 10,
                        line_y,
                        num_width / yui_density,
                        num_height / yui_density
                    };
                    
                    backend_render_text_copy(line_num_tex, NULL, &line_num_rect);
                    backend_render_text_destroy(line_num_tex);
                }
            }
            
            // 移动到下一个逻辑行
            current_pos = line_end;
            if (current_pos < text_len && text[current_pos] == '\n') {
                current_pos++;
            }
            logical_line++;
        }
    }
    
    // 准备渲染区域（排除滚动条轨道）
    Rect render_rect;
    text_component_get_content_rect(component, layer, &render_rect);
    
    // 保存当前裁剪区域
    Rect prev_clip;
    backend_render_get_clip_rect(&prev_clip);
    
    // 设置裁剪区域
    backend_render_set_clip_rect(&render_rect);
    
    // 如果文本为空且有占位符，显示占位符
    if (strlen(component->layer->text) == 0 && strlen(component->placeholder) > 0) {
        // 使用透明度较低的颜色绘制占位符
        Color placeholder_color = {150, 150, 150, 128};
        Texture* tex = backend_render_texture(layer->font->default_font, component->placeholder, placeholder_color);
        if (tex) {
            int text_width, text_height;
            backend_query_texture(tex, NULL, NULL, &text_width, &text_height);
            
            // 计算文本位置
            Rect text_rect = {
                render_rect.x,
                // 多行模式下在顶部对齐，单行模式下垂直居中
                component->multiline ? render_rect.y : render_rect.y + (render_rect.h - text_height / yui_density) / 2,
                text_width / yui_density,
                text_height / yui_density
            };
            
            // 确保文本不会超出边界
            if (text_rect.x + text_rect.w > render_rect.x + render_rect.w) {
                text_rect.w = render_rect.x + render_rect.w - text_rect.x;
            }
            if (text_rect.y + text_rect.h > render_rect.y + render_rect.h) {
                text_rect.h = render_rect.y + render_rect.h - text_rect.y;
            }
            
            backend_render_text_copy(tex, NULL, &text_rect);
            backend_render_text_destroy(tex);
        }
    } else {
        // 绘制选择背景
        if (component->selection_start != -1 && component->selection_end != -1) {
            // 使用可配置的选择背景颜色
            Color selection_bg = component->selection_color;
            
            // 获取行高
            int line_height = text_component_get_line_height(component);
            
            // 确保start <= end
            int start = component->selection_start;
            int end = component->selection_end;
            if (start > end) {
                int temp = start;
                start = end;
                end = temp;
            }
            
            if (component->multiline) {
                // 多行模式：需要考虑自动换行，使用与文本渲染完全相同的逻辑
                char* text = component->layer->text;
                int text_len = strlen(text);
                int line_y = render_rect.y - layer->scroll_offset;
                int current_pos = 0;
                int max_width = render_rect.w;
                
                // 遍历文本，模拟渲染过程（与文本渲染使用相同的算法）
                while (current_pos < text_len) {
                    // 查找当前行可以显示的最大文本长度
                    int line_end = current_pos;
                    
                    // 尝试找到合适的换行点（找到\n或文本末尾）
                    while (line_end < text_len) {
                        // 遇到换行符时立即换行
                        if (text[line_end] == '\n') {
                            break;
                        }
                        line_end++;
                    }
                    
                    // 计算整行文本的宽度
                    int current_width = 0;
                    char* temp_line = (char*)malloc(line_end - current_pos + 1);
                    if (temp_line) {
                        strncpy(temp_line, text + current_pos, line_end - current_pos);
                        temp_line[line_end - current_pos] = '\0';
                        
                        Texture* line_tex = backend_render_texture(layer->font->default_font, temp_line, layer->color);
                        if (line_tex) {
                            int line_width, line_height_ignore;
                            backend_query_texture(line_tex, NULL, NULL, &line_width, &line_height_ignore);
                            current_width = line_width / yui_density;
                            backend_render_text_destroy(line_tex);
                        }
                        
                        free(temp_line);
                    }
                    
                    // 确定当前视觉行的结束位置
                    int split_pos = current_pos;
                    
                    // 如果文本没有超过宽度限制，或关闭了自动换行
                    if (current_width <= max_width || !component->wrap) {
                        // 使用整行（到\n或文本末尾）
                        split_pos = line_end;
                    }
                    // 如果文本超过宽度限制，需要硬换行
                    else {
                        // 找到最大的不超过宽度的位置
                        split_pos = current_pos;
                        while (split_pos < line_end) {
                            char* test_line = (char*)malloc(split_pos - current_pos + 1);
                            if (test_line) {
                                strncpy(test_line, text + current_pos, split_pos - current_pos);
                                test_line[split_pos - current_pos] = '\0';
                                
                                Texture* test_tex = backend_render_texture(layer->font->default_font, test_line, layer->color);
                                if (test_tex) {
                                    int test_width, test_height;
                                    backend_query_texture(test_tex, NULL, NULL, &test_width, &test_height);
                                    if (test_width / yui_density > max_width) {
                                        backend_render_text_destroy(test_tex);
                                        free(test_line);
                                        break;
                                    }
                                    backend_render_text_destroy(test_tex);
                                }
                                
                                free(test_line);
                            }
                            split_pos++;
                        } 
                        
                        if (split_pos > current_pos) {
                            split_pos--;
                        }
                    }
                    
                    // 确保split_pos >= current_pos，防止负长度
                    if (split_pos < current_pos) {
                        split_pos = current_pos;
                    }
                    
                    // 处理这一视觉行的选择背景
                    int visual_line_start = current_pos;
                    int visual_line_end = split_pos;
                    
                    // 检查选择区域是否与当前视觉行相交
                    if (start < visual_line_end && end > visual_line_start) {
                        // 计算当前视觉行内的选择起始和结束位置
                        int sel_start_in_line = (start > visual_line_start) ? start : visual_line_start;
                        int sel_end_in_line = (end < visual_line_end) ? end : visual_line_end;
                        
                        // 计算选择起始位置的X坐标
                        int sel_start_x = render_rect.x;
                        if (sel_start_in_line > visual_line_start) {
                            int before_len = sel_start_in_line - visual_line_start;
                            char* before_sel = (char*)malloc(before_len + 1);
                            if (before_sel) {
                                strncpy(before_sel, text + visual_line_start, before_len);
                                before_sel[before_len] = '\0';
                                
                                Texture* before_tex = backend_render_texture(layer->font->default_font, before_sel, layer->color);
                                if (before_tex) {
                                    int before_width, before_height;
                                    backend_query_texture(before_tex, NULL, NULL, &before_width, &before_height);
                                    sel_start_x = render_rect.x + before_width / yui_density;
                                    backend_render_text_destroy(before_tex);
                                }
                                free(before_sel);
                            }
                        }
                        
                        // 计算选择区域的宽度
                        int sel_width = 0;
                        int sel_len = sel_end_in_line - sel_start_in_line;
                        if (sel_len > 0) {
                            char* sel_text = (char*)malloc(sel_len + 1);
                            if (sel_text) {
                                strncpy(sel_text, text + sel_start_in_line, sel_len);
                                sel_text[sel_len] = '\0';
                                
                                Texture* sel_tex = backend_render_texture(layer->font->default_font, sel_text, layer->color);
                                if (sel_tex) {
                                    int sel_tex_width, sel_tex_height;
                                    backend_query_texture(sel_tex, NULL, NULL, &sel_tex_width, &sel_tex_height);
                                    sel_width = sel_tex_width / yui_density;
                                    backend_render_text_destroy(sel_tex);
                                }
                                free(sel_text);
                            }
                        }
                        
                        // 绘制选择区域
                        Rect sel_rect = {
                            sel_start_x,
                            line_y,
                            sel_width,
                            line_height
                        };
                        
                        // 确保选择区域在可见范围内
                        if (sel_rect.y + sel_rect.h > render_rect.y && sel_rect.y < render_rect.y + render_rect.h) {
                            backend_render_fill_rect(&sel_rect, selection_bg);
                        }
                    }
                    
                    // 移动到下一视觉行
                    // 如果是在换行符处结束，直接跳到下一个字符
                    if (split_pos < text_len && text[split_pos] == '\n') {
                        current_pos = split_pos + 1;
                    } else {
                        current_pos = split_pos;
                    }
                    
                    // 移动到下一行
                    line_y += line_height + 2; // 加2作为行间距
                }
            } else {
                // 单行模式 - 使用实际渲染宽度
                int sel_start_x = render_rect.x;
                int sel_width = 0;
                
                // 计算选择起始位置的X坐标
                if (start > 0) {
                    char* before_sel = (char*)malloc(start + 1);
                    if (before_sel) {
                        strncpy(before_sel, component->layer->text, start);
                        before_sel[start] = '\0';
                        
                        Texture* before_tex = backend_render_texture(layer->font->default_font, before_sel, layer->color);
                        if (before_tex) {
                            int before_width, before_height;
                            backend_query_texture(before_tex, NULL, NULL, &before_width, &before_height);
                            sel_start_x = render_rect.x + before_width / yui_density;
                            backend_render_text_destroy(before_tex);
                        }
                        free(before_sel);
                    }
                }
                
                // 计算选择区域的宽度
                int sel_len = end - start;
                if (sel_len > 0) {
                    char* sel_text = (char*)malloc(sel_len + 1);
                    if (sel_text) {
                        strncpy(sel_text, component->layer->text + start, sel_len);
                        sel_text[sel_len] = '\0';
                        
                        Texture* sel_tex = backend_render_texture(layer->font->default_font, sel_text, layer->color);
                        if (sel_tex) {
                            int sel_tex_width, sel_tex_height;
                            backend_query_texture(sel_tex, NULL, NULL, &sel_tex_width, &sel_tex_height);
                            sel_width = sel_tex_width / yui_density;
                            backend_render_text_destroy(sel_tex);
                        }
                        free(sel_text);
                    }
                }
                
                Rect sel_rect = {
                    sel_start_x,
                    render_rect.y + (render_rect.h - line_height) / 2,
                    sel_width,
                    line_height
                };
                
                backend_render_fill_rect(&sel_rect, selection_bg);
            }
        }
        
        // 绘制文本
        if (component->multiline) {
            char* text = component->layer->text;
            int text_len = (int)strlen(text);
            int line_height = text_component_get_line_height(component);
            int line_stride = line_height + TEXT_LINE_SPACING;

            text_component_ensure_layout(component, render_rect.w);

            int first_visible_line = layer->scroll_offset / line_stride;
            int max_visible_lines = render_rect.h / line_stride + 2;
            int last_visible_line = first_visible_line + max_visible_lines;

            for (int line_index = 0; line_index < component->layout_count; line_index++) {
                if (line_index < first_visible_line) continue;
                if (line_index >= last_visible_line) break;

                int start = component->layout_starts[line_index];
                int end = (line_index + 1 < component->layout_count)
                    ? component->layout_starts[line_index + 1] : text_len;
                while (end > start && text[end - 1] == '\n') {
                    end--;
                }
                if (end <= start) continue;

                int line_y = render_rect.y + line_index * line_stride - layer->scroll_offset;
                if (line_y + line_height < render_rect.y || line_y > render_rect.y + render_rect.h) {
                    continue;
                }

                text_component_render_text_segment(component, text, start, end, render_rect.x, line_y);
            }

            if (component->layout_count <= 0 && text_len > 0) {
                text_component_render_text_segment(component, text, 0, text_len,
                                                   render_rect.x, render_rect.y - layer->scroll_offset);
            }

            int total_text_height = layer->content_height;
            if (total_text_height <= render_rect.h) {
                layer->scroll_offset = 0;
            } else {
                if (layer->scroll_offset < 0) layer->scroll_offset = 0;
                int max_scroll_y = total_text_height - render_rect.h;
                if (layer->scroll_offset > max_scroll_y) layer->scroll_offset = max_scroll_y;
            }
        } else {
            int line_height = text_component_get_line_height(component);
            int text_len = (int)strlen(component->layer->text);
            int text_y = render_rect.y + (render_rect.h - line_height) / 2;
            text_component_render_text_segment(component, component->layer->text, 0, text_len,
                                               render_rect.x - component->scroll_x, text_y);
        }
    }
    
    // 如果组件可编辑，绘制光标
    if (component->editable && HAS_STATE(layer, LAYER_STATE_FOCUSED)) {
        // 确保光标位置在有效范围内
        int text_len = strlen(component->layer->text);
        if (component->cursor_pos < 0) component->cursor_pos = 0;
        if (component->cursor_pos > text_len) component->cursor_pos = text_len;
        
        // 获取行高
        int line_height = text_component_get_line_height(component);
        
        if (component->multiline) {
            const char* text = component->layer->text;
            int cursor_line = 0;
            int line_start = 0;
            int line_end = 0;
            int cursor_x;
            int cursor_y;
            int line_stride = line_height + TEXT_LINE_SPACING;

            text_component_ensure_layout(component, render_rect.w);
            for (int i = 0; i < component->layout_count; i++) {
                int start;
                int end;
                text_component_get_layout_line_range(component, text, text_len, i, &start, &end);

                if (component->cursor_pos < end ||
                    (component->cursor_pos == end &&
                     ((end < text_len && text[end] == '\n') ||
                      component->cursor_visual_line == i)) ||
                    i == component->layout_count - 1) {
                    cursor_line = i;
                    line_start = start;
                    line_end = end;
                    break;
                }
            }

            if (component->cursor_pos < line_start) component->cursor_pos = line_start;
            if (component->cursor_pos > line_end) component->cursor_pos = line_end;
            cursor_x = render_rect.x + text_component_measure_width(
                component, text, line_start, component->cursor_pos);
            cursor_y = render_rect.y + cursor_line * line_stride - layer->scroll_offset;

            backend_render_fill_rect(&(Rect){cursor_x, cursor_y, 2, line_height},
                                     component->cursor_color);
        } else if (component->multiline) {
            // 旧的多行光标计算路径（保留以便后续移除）
            char* text = component->layer->text;
            int current_pos = 0;
            int visual_line = 0;
            int max_width = render_rect.w;
            int cursor_x = render_rect.x;
            int cursor_y = render_rect.y - layer->scroll_offset;
            
            // 遍历文本，使用与渲染相同的算法找到光标所在的视觉行
            while (current_pos < text_len && current_pos < component->cursor_pos) {
                // 查找当前行可以显示的最大文本长度
                int line_end = current_pos;
                
                // 尝试找到合适的换行点
                while (line_end < text_len) {
                    if (text[line_end] == '\n') {
                        break;
                    }
                    line_end++;
                }
                
                // 计算整行文本的宽度
                int current_width = 0;
                char* temp_line = (char*)malloc(line_end - current_pos + 1);
                if (temp_line) {
                    strncpy(temp_line, text + current_pos, line_end - current_pos);
                    temp_line[line_end - current_pos] = '\0';
                    
                    Texture* line_tex = backend_render_texture(layer->font->default_font, temp_line, layer->color);
                    if (line_tex) {
                        int line_width, line_height_ignore;
                        backend_query_texture(line_tex, NULL, NULL, &line_width, &line_height_ignore);
                        current_width = line_width / yui_density;
                        backend_render_text_destroy(line_tex);
                    }
                    
                    free(temp_line);
                }
                
                // 确定当前视觉行的结束位置
                int split_pos = current_pos;
                
                // 如果文本没有超过宽度限制，或关闭了自动换行
                if (current_width <= max_width || !component->wrap) {
                    // 使用整行（到\n或文本末尾）
                    split_pos = line_end;
                }
                // 如果文本超过宽度限制，需要硬换行
                else {
                    split_pos = current_pos;
                    while (split_pos < line_end) {
                        char* test_line = (char*)malloc(split_pos - current_pos + 1);
                        if (test_line) {
                            strncpy(test_line, text + current_pos, split_pos - current_pos);
                            test_line[split_pos - current_pos] = '\0';
                            
                            Texture* test_tex = backend_render_texture(layer->font->default_font, test_line, layer->color);
                            if (test_tex) {
                                int test_width, test_height;
                                backend_query_texture(test_tex, NULL, NULL, &test_width, &test_height);
                                if (test_width / yui_density > max_width) {
                                    backend_render_text_destroy(test_tex);
                                    free(test_line);
                                    break;
                                }
                                backend_render_text_destroy(test_tex);
                            }
                            
                            free(test_line);
                        }
                        split_pos++;
                    } 
                    
                    if (split_pos > current_pos) {
                        split_pos--;
                    }
                }
                
                if (split_pos < current_pos) {
                    split_pos = current_pos;
                }
                
                // 检查光标是否在当前视觉行内
                if (component->cursor_pos <= split_pos) {
                    // 光标在当前视觉行，计算X坐标
                    int len_to_cursor = component->cursor_pos - current_pos;
                    if (len_to_cursor > 0) {
                        char* text_before_cursor = (char*)malloc(len_to_cursor + 1);
                        if (text_before_cursor) {
                            strncpy(text_before_cursor, text + current_pos, len_to_cursor);
                            text_before_cursor[len_to_cursor] = '\0';
                            
                            Texture* before_tex = backend_render_texture(layer->font->default_font, text_before_cursor, layer->color);
                            if (before_tex) {
                                int before_width, before_height;
                                backend_query_texture(before_tex, NULL, NULL, &before_width, &before_height);
                                cursor_x = render_rect.x + before_width / yui_density;
                                backend_render_text_destroy(before_tex);
                            }
                            free(text_before_cursor);
                        }
                    } else {
                        cursor_x = render_rect.x;
                    }
                    
                    cursor_y = render_rect.y + visual_line * (line_height + 2) - layer->scroll_offset;
                    break;
                }
                
                // 移动到下一视觉行
                visual_line++;
                if (split_pos < text_len && text[split_pos] == '\n') {
                    current_pos = split_pos + 1;
                } else {
                    current_pos = split_pos;
                }
            }
            
            // 如果光标在文本末尾
            if (current_pos >= component->cursor_pos && component->cursor_pos == text_len) {
                // 计算文本最后一行
                cursor_x = render_rect.x;
                
                // 获取最后一行文本（从当前pos到文本末尾）
                int remaining_len = text_len - current_pos;
                if (remaining_len > 0) {
                    char* last_line = (char*)malloc(remaining_len + 1);
                    if (last_line) {
                        strncpy(last_line, text + current_pos, remaining_len);
                        last_line[remaining_len] = '\0';
                        
                        // 计算最后一行文本的宽度
                        Texture* last_tex = backend_render_texture(layer->font->default_font, last_line, layer->color);
                        if (last_tex) {
                            int last_width, last_height;
                            backend_query_texture(last_tex, NULL, NULL, &last_width, &last_height);
                            cursor_x = render_rect.x + last_width / yui_density;
                            backend_render_text_destroy(last_tex);
                        }
                        free(last_line);
                    }
                }
                
                cursor_y = render_rect.y + visual_line * (line_height + 2) - layer->scroll_offset;
            }
            
            // 绘制光标
            Rect cursor_rect = {
                cursor_x,
                cursor_y,
                2,
                line_height
            };
            
            backend_render_fill_rect(&cursor_rect, component->cursor_color);
        } else {
            // 单行模式：计算光标前文本的宽度，并应用滚动偏移
            int char_width = 0;
            if (component->cursor_pos > 0) {
                char* temp_text = (char*)malloc(component->cursor_pos + 1);
                if (temp_text) {
                    strncpy(temp_text, component->layer->text, component->cursor_pos);
                    temp_text[component->cursor_pos] = '\0';
                    
                    Texture* cursor_text_texture = backend_render_texture(layer->font->default_font, temp_text, layer->color);
                    if (cursor_text_texture) {
                        int text_width;
                        backend_query_texture(cursor_text_texture, NULL, NULL, &text_width, NULL);
                        char_width = text_width / yui_density;
                        backend_render_text_destroy(cursor_text_texture);
                    }
                    free(temp_text);
                }
            }
            
            Rect cursor_rect = {
                render_rect.x + char_width - component->scroll_x,  // 应用水平滚动偏移
                render_rect.y + (render_rect.h - line_height) / 2,
                2,
                line_height
            };
            
            backend_render_fill_rect(&cursor_rect, component->cursor_color);
        }
    }
    
    // 恢复裁剪区域
    backend_render_set_clip_rect(&prev_clip);
    
    // 多行滚动条：只读气泡不显示；且未启用 layer 通用垂直滚动条时才自绘
    if (component->multiline && component->editable &&
        layer->scrollable != 1 && layer->scrollable != 3) {
        int pad_top;
        int pad_right;
        int pad_bottom;
        int total_text_height = layer->content_height;
        int visible_height;
        Rect scrollbar_bg;
        text_component_get_padding(layer, &pad_top, &pad_right, &pad_bottom, NULL);
        visible_height = layer->rect.h - pad_top - pad_bottom;

        // 只有当文本总高度大于可见高度时才显示滚动条
        if (total_text_height > visible_height) {
            scrollbar_bg.x = layer->rect.x + layer->rect.w - pad_right - TEXT_INTERNAL_SCROLLBAR_WIDTH;
            scrollbar_bg.y = layer->rect.y + pad_top;
            scrollbar_bg.w = TEXT_INTERNAL_SCROLLBAR_WIDTH;
            scrollbar_bg.h = layer->rect.h - pad_top - pad_bottom;
            Color scrollbar_bg_color = {200, 200, 200, 255};
            backend_render_fill_rect(&scrollbar_bg, scrollbar_bg_color);
            
            // 计算滚动条滑块位置和大小
            float scroll_percent = 0;
            if (total_text_height > visible_height) {
                scroll_percent = (float)layer->scroll_offset / (total_text_height - visible_height);
            }
            int slider_height = (int)(visible_height * (float)visible_height / total_text_height);
            if (slider_height < 20) slider_height = 20; // 滑块最小高度
            
            Rect scrollbar_slider = {
                scrollbar_bg.x,
                scrollbar_bg.y + (int)(scroll_percent * (scrollbar_bg.h - slider_height)),
                scrollbar_bg.w,
                slider_height
            };
            Color scrollbar_slider_color = {150, 150, 150, 255};
            backend_render_fill_rect(&scrollbar_slider, scrollbar_slider_color);
        }
    }
}

 

// 更新图层的内容高度
void text_component_update_content_height(TextComponent* component) {
    if (!component || !component->layer) {
        return;
    }
    
    // 计算并设置内容高度
    int content_height = text_component_calculate_content_height(component);
    component->layer->content_height = content_height;
    
    {
        int pad_top;
        int pad_right;
        int pad_bottom;
        int visible_height;
        text_component_get_padding(component->layer, &pad_top, &pad_right, &pad_bottom, NULL);
        visible_height = component->layer->rect.h - pad_top - pad_bottom;
        if (content_height <= visible_height) {
            component->layer->scroll_offset = 0;
        }
    }
}






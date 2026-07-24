#include "table_component.h"
#include "../backend.h"
#include "../event.h"
#include "../layer_update.h"
#include "../popup_manager.h"
#include "../render.h"
#include "../util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>



#define TABLE_CELL_PAD_X 8
#define TABLE_COL_MIN_WIDTH 40
#define TABLE_RESIZE_HANDLE 6
#define TABLE_DOUBLE_CLICK_MS 400
#define TABLE_TOOLTIP_DELAY_MS 400
#define TABLE_TOOLTIP_ROW_NONE -2

static void table_intersect_rect(Rect* out, const Rect* a, const Rect* b);
static void table_draw_cell_text(Layer* layer, const char* text, Color color,
                                int x, int y, int w, int h, TableColumnAlign align);
static void table_get_cell_rect(TableComponent* component, Layer* layer,
                                int row, int col, Rect* out);
static void table_edit_sync_scroll(TableComponent* component);
static void table_component_apply_theme_style(Layer* layer, cJSON* style);

static int table_viewport_width(TableComponent* component, Layer* layer);
static int table_row_at_point(TableComponent* component, Layer* layer, int x, int y);
static int table_point_in_header(TableComponent* component, Layer* layer, int x, int y);
static int table_resize_column_at(TableComponent* component, Layer* layer, int x, int y);
static char* table_json_value_to_string(cJSON* item);
static void table_tooltip_schedule_show(TableComponent* component, Layer* layer);
static int table_measure_text_width(Layer* layer, const char* text);

#define TABLE_TOOLTIP_PAD 6
#define TABLE_TOOLTIP_MAX_W 420

static int table_tooltip_line_height(Layer* layer) {
    int h = 16;
    if (layer && layer->font && layer->font->size > 0) {
        h = layer->font->size + 4;
    }
    return h > 14 ? h : 14;
}

/* 返回不超过 max_w 的最长 UTF-8 前缀字节数 */
static int table_tooltip_fit_bytes(Layer* layer, const char* text, int max_w) {
    int pos = 0;
    int fit = 0;
    int len;

    if (!text || !text[0] || max_w <= 0) {
        return 0;
    }
    len = (int)strlen(text);
    if (table_measure_text_width(layer, text) <= max_w) {
        return len;
    }
    while (pos < len) {
        int clen = utf8_char_len_at(text + pos);
        char buf[1024];
        if (clen <= 0) {
            clen = 1;
        }
        if (pos + clen >= (int)sizeof(buf)) {
            break;
        }
        memcpy(buf, text, (size_t)(pos + clen));
        buf[pos + clen] = '\0';
        if (table_measure_text_width(layer, buf) > max_w) {
            break;
        }
        pos += clen;
        fit = pos;
    }
    if (fit <= 0) {
        fit = utf8_char_len_at(text);
        if (fit <= 0) {
            fit = 1;
        }
    }
    return fit;
}

static int table_tooltip_count_lines(Layer* layer, const char* text, int max_w) {
    int lines = 0;
    const char* p = text;
    if (!p || !p[0]) {
        return 0;
    }
    while (*p) {
        int fit = table_tooltip_fit_bytes(layer, p, max_w);
        if (fit <= 0) {
            fit = 1;
        }
        p += fit;
        lines++;
    }
    return lines > 0 ? lines : 1;
}

static void table_tooltip_layer_render(Layer* layer) {
    Color bg = {50, 50, 60, 230};
    Color text_color = {220, 220, 220, 255};
    int max_w;
    int line_h;
    int y;
    const char* p;

    if (!layer) {
        return;
    }

    backend_render_fill_rect(&layer->rect, bg);

    if (!layer->text || !layer->text[0]) {
        return;
    }

    max_w = layer->rect.w - TABLE_TOOLTIP_PAD * 2;
    if (max_w < 8) {
        max_w = 8;
    }
    line_h = table_tooltip_line_height(layer);
    y = layer->rect.y + TABLE_TOOLTIP_PAD;
    p = layer->text;

    while (*p) {
        char buf[1024];
        int fit = table_tooltip_fit_bytes(layer, p, max_w);
        Texture* tex;
        if (fit <= 0) {
            fit = 1;
        }
        if (fit >= (int)sizeof(buf)) {
            fit = (int)sizeof(buf) - 1;
        }
        memcpy(buf, p, (size_t)fit);
        buf[fit] = '\0';

        tex = render_text(layer, buf, text_color);
        if (tex) {
            int tw = 0, th = 0;
            Rect text_rect;
            backend_query_texture(tex, NULL, NULL, &tw, &th);
            text_rect.x = layer->rect.x + TABLE_TOOLTIP_PAD;
            text_rect.y = y;
            text_rect.w = tw / yui_density;
            text_rect.h = th / yui_density;
            backend_render_text_copy(tex, NULL, &text_rect);
            backend_render_text_destroy(tex);
        }

        p += fit;
        y += line_h;
    }
}

static void table_hide_tooltip(TableComponent* component) {
    if (!component || !component->tooltip_popup) return;

    Layer* tl = ((PopupLayer*)component->tooltip_popup)->layer;
    popup_manager_remove(tl);
    if (tl && tl->text) free(tl->text);
    if (tl) free(tl);
    component->tooltip_popup = NULL;
}

static void table_tooltip_reset(TableComponent* component) {
    if (!component) return;
    table_hide_tooltip(component);
    component->tooltip_row = TABLE_TOOLTIP_ROW_NONE;
    component->tooltip_col = -1;
    component->tooltip_overflow = 0;
}

static int table_measure_text_width(Layer* layer, const char* text) {
    if (!layer || !text || !text[0] || !layer->font || !layer->font->default_font) return 0;

    Texture* tex = backend_render_texture(layer->font->default_font, text, (Color){255, 255, 255, 255});
    if (!tex) return 0;

    int tw = 0, th = 0;
    backend_query_texture(tex, NULL, NULL, &tw, &th);
    backend_render_text_destroy(tex);
    return tw / yui_density;
}

static int table_text_overflows(Layer* layer, const char* text, int cell_w) {
    int avail = cell_w - TABLE_CELL_PAD_X * 2;
    if (avail < 1) avail = 1;
    return table_measure_text_width(layer, text) > avail;
}

static int table_column_index_at_x(TableComponent* component, Layer* layer, int x) {
    if (!component || !layer) return -1;

    int viewport_left = layer->rect.x;
    int viewport_right = layer->rect.x + table_viewport_width(component, layer);
    if (x < viewport_left || x >= viewport_right) return -1;

    int col_x = layer->rect.x - layer->scroll_offset_x;
    for (int c = 0; c < component->column_count; c++) {
        int cw = component->columns[c].computed_width;
        int cell_left = col_x;
        int cell_right = col_x + cw;
        int hit_left = cell_left > viewport_left ? cell_left : viewport_left;
        int hit_right = cell_right < viewport_right ? cell_right : viewport_right;
        if (hit_left < hit_right && x >= hit_left && x < hit_right) {
            return c;
        }
        col_x += cw;
    }
    return -1;
}

static char* table_get_body_cell_text(TableComponent* component, int row, int col) {
    if (!component || !component->layer || row < 0 || col < 0 || col >= component->column_count) {
        return NULL;
    }

    Layer* layer = component->layer;
    if (!layer->data || !layer->data->json || !cJSON_IsArray(layer->data->json)) {
        return NULL;
    }

    cJSON* row_json = cJSON_GetArrayItem(layer->data->json, row);
    if (!row_json) return NULL;

    cJSON* value = cJSON_GetObjectItem(row_json, component->columns[col].key);
    return table_json_value_to_string(value);
}

static const char* table_get_tooltip_text(TableComponent* component, int row, int col,
                                          char** owned_text) {
    if (!component || col < 0 || col >= component->column_count) return NULL;
    if (owned_text) *owned_text = NULL;

    if (row < 0) {
        return component->columns[col].title;
    }

    char* text = table_get_body_cell_text(component, row, col);
    if (owned_text) *owned_text = text;
    return text;
}

static void table_show_tooltip(TableComponent* component, Layer* layer,
                               const char* text, int mouse_x, int mouse_y) {
    int tw;
    int sw = 800, sh = 600;
    int max_box_w;
    int content_max;
    int box_w;
    int line_h;
    int lines;
    Layer* tl;

    if (!component || !layer || !text || !text[0] || component->tooltip_popup) return;
    if (!layer->font || !layer->font->default_font) return;

    tw = table_measure_text_width(layer, text);
    if (tw <= 0) return;

    tl = calloc(1, sizeof(Layer));
    if (!tl) return;

    tl->type = LABEL;
    tl->visible = VISIBLE;
    tl->font = layer->font;

    backend_get_windowsize(&sw, &sh);
    max_box_w = sw - 24;
    if (max_box_w > TABLE_TOOLTIP_MAX_W) {
        max_box_w = TABLE_TOOLTIP_MAX_W;
    }
    if (max_box_w < 80) {
        max_box_w = 80;
    }
    content_max = max_box_w - TABLE_TOOLTIP_PAD * 2;
    if (content_max < 8) {
        content_max = 8;
    }

    box_w = tw + TABLE_TOOLTIP_PAD * 2;
    if (box_w > max_box_w) {
        box_w = max_box_w;
    }

    line_h = table_tooltip_line_height(layer);
    lines = table_tooltip_count_lines(layer, text, content_max);
    if (lines < 1) {
        lines = 1;
    }

    tl->rect.w = box_w;
    tl->rect.h = lines * line_h + TABLE_TOOLTIP_PAD * 2;
    if (tl->rect.h < 24) {
        tl->rect.h = 24;
    }

    /* 贴边夹紧，避免长文本往左翻导致左边截断 */
    tl->rect.x = mouse_x + 12;
    tl->rect.y = mouse_y + 12;
    if (tl->rect.x + tl->rect.w > sw - 8) {
        tl->rect.x = sw - 8 - tl->rect.w;
    }
    if (tl->rect.x < 8) {
        tl->rect.x = 8;
    }
    if (tl->rect.y + tl->rect.h > sh - 8) {
        tl->rect.y = sh - 8 - tl->rect.h;
    }
    if (tl->rect.y < 8) {
        tl->rect.y = 8;
    }

    tl->text = strdup(text);
    if (!tl->text) {
        free(tl);
        return;
    }
    tl->render = table_tooltip_layer_render;

    PopupLayer* popup = popup_layer_create(tl, POPUP_TYPE_TOOLTIP, 100);
    if (popup && popup_manager_add(popup)) {
        component->tooltip_popup = popup;
    } else {
        free(tl->text);
        free(tl);
    }
}

static void table_tooltip_update_hover(TableComponent* component, Layer* layer, int x, int y) {
    if (!component || !layer) return;

    if (component->resizing_column >= 0 || component->editing_row >= 0) {
        table_tooltip_reset(component);
        return;
    }

    int row = TABLE_TOOLTIP_ROW_NONE;
    int col = -1;

    if (table_point_in_header(component, layer, x, y)) {
        if (table_resize_column_at(component, layer, x, y) >= 0) {
            table_tooltip_reset(component);
            return;
        }
        row = -1;
        col = table_column_index_at_x(component, layer, x);
    } else {
        int body_row = table_row_at_point(component, layer, x, y);
        if (body_row >= 0) {
            row = body_row;
            col = table_column_index_at_x(component, layer, x);
        }
    }

    if (col < 0) {
        table_tooltip_reset(component);
        return;
    }

    char* owned_text = NULL;
    const char* text = table_get_tooltip_text(component, row, col, &owned_text);
    if (!text || !text[0]) {
        free(owned_text);
        table_tooltip_reset(component);
        return;
    }

    int cell_w = component->columns[col].computed_width;
    int overflows = table_text_overflows(layer, text, cell_w);
    free(owned_text);

    if (!overflows) {
        table_tooltip_reset(component);
        return;
    }

    if (row != component->tooltip_row || col != component->tooltip_col) {
        table_hide_tooltip(component);
        component->tooltip_row = row;
        component->tooltip_col = col;
        component->tooltip_overflow = 1;
        component->tooltip_hover_start = backend_get_ticks();
        table_tooltip_schedule_show(component, layer);
        return;
    }

    component->tooltip_overflow = 1;
    table_tooltip_schedule_show(component, layer);
}

static void table_tooltip_schedule_show(TableComponent* component, Layer* layer) {
    if (!component || !layer) return;
    if (component->tooltip_overflow && !component->tooltip_popup) {
        mark_layer_dirty(layer, DIRTY_TEXT);
    }
}

static void table_tooltip_try_show(TableComponent* component, Layer* layer) {
    if (!component || !layer || component->tooltip_popup) return;
    if (component->tooltip_row < -1 || component->tooltip_col < 0 || !component->tooltip_overflow) return;

    Uint32 now = backend_get_ticks();
    if (now - component->tooltip_hover_start < TABLE_TOOLTIP_DELAY_MS) {
        table_tooltip_schedule_show(component, layer);
        return;
    }

    char* owned_text = NULL;
    const char* text = table_get_tooltip_text(component, component->tooltip_row,
                                              component->tooltip_col, &owned_text);
    if (!text || !text[0]) {
        free(owned_text);
        return;
    }

    int mx = 0, my = 0;
    backend_get_pointer_state(&mx, &my);
    table_show_tooltip(component, layer, text, mx, my);
    free(owned_text);
}

static int table_edit_has_selection(TableComponent* component) {
    return component && component->edit_sel_start >= 0 &&
           component->edit_sel_start != component->edit_sel_end;
}

static int table_edit_sel_lo(TableComponent* component) {
    if (!component || component->edit_sel_start < 0) return component ? component->edit_cursor : 0;
    return component->edit_sel_start < component->edit_sel_end ?
           component->edit_sel_start : component->edit_sel_end;
}

static int table_edit_sel_hi(TableComponent* component) {
    if (!component || component->edit_sel_start < 0) return component ? component->edit_cursor : 0;
    return component->edit_sel_start > component->edit_sel_end ?
           component->edit_sel_start : component->edit_sel_end;
}

static void table_edit_clear_selection(TableComponent* component) {
    if (!component) return;
    component->edit_sel_start = -1;
    component->edit_sel_end = -1;
}

static void table_edit_select_all_text(TableComponent* component) {
    if (!component) return;
    int len = (int)strlen(component->edit_buffer);
    component->edit_sel_start = 0;
    component->edit_sel_end = len;
    component->edit_cursor = len;
}

static void table_edit_remove_selection(TableComponent* component) {
    if (!component || !table_edit_has_selection(component)) return;

    int lo = table_edit_sel_lo(component);
    int hi = table_edit_sel_hi(component);
    int len = (int)strlen(component->edit_buffer);
    memmove(component->edit_buffer + lo, component->edit_buffer + hi, (size_t)(len - hi + 1));
    component->edit_cursor = lo;
    table_edit_clear_selection(component);
}

static int table_edit_text_width(Layer* layer, const char* text, int char_index) {
    if (!layer || !text || char_index <= 0) return 0;

    int len = (int)strlen(text);
    if (char_index > len) char_index = len;

    char prefix[TABLE_EDIT_BUF_SIZE];
    if (char_index >= TABLE_EDIT_BUF_SIZE) char_index = TABLE_EDIT_BUF_SIZE - 1;
    memcpy(prefix, text, (size_t)char_index);
    prefix[char_index] = '\0';

    Texture* tex = render_text(layer, prefix[0] ? prefix : " ", layer->color);
    if (!tex) return 0;

    int tw = 0, th = 0;
    backend_query_texture(tex, NULL, NULL, &tw, &th);
    backend_render_text_destroy(tex);
    return (int)(tw / yui_density);
}

static void table_edit_move_cursor(TableComponent* component, int direction, int keep_selection) {
    if (!component || component->editing_row < 0) return;

    const char* text = component->edit_buffer;
    int len = (int)strlen(text);
    int new_cursor = component->edit_cursor;

    if (direction < 0) {
        if (component->edit_cursor > 0) {
            new_cursor = component->edit_cursor - get_prev_utf8_char_len(text, component->edit_cursor);
        } else {
            new_cursor = 0;
        }
    } else if (direction > 0) {
        if (component->edit_cursor < len) {
            new_cursor = component->edit_cursor + get_current_utf8_char_len(text, component->edit_cursor);
        } else {
            new_cursor = len;
        }
    }

    if (keep_selection) {
        if (component->edit_sel_start < 0) {
            component->edit_sel_start = component->edit_cursor;
        }
        component->edit_cursor = new_cursor;
        component->edit_sel_end = component->edit_cursor;
    } else {
        if (table_edit_has_selection(component)) {
            component->edit_cursor = direction < 0 ? table_edit_sel_lo(component) : table_edit_sel_hi(component);
        } else {
            component->edit_cursor = new_cursor;
        }
        table_edit_clear_selection(component);
    }
    table_edit_sync_scroll(component);
    mark_layer_dirty(component->layer, DIRTY_TEXT);
}

static int table_edit_pos_from_mouse_x(TableComponent* component, Layer* layer, Rect cell, int mouse_x) {
    if (!component || !layer) return 0;

    const char* text = component->edit_buffer;
    int len = (int)strlen(text);
    if (len <= 0) return 0;

    int draw_x = cell.x + TABLE_CELL_PAD_X;
    int local_x = mouse_x - draw_x + component->edit_scroll_x;
    if (local_x <= 0) return 0;

    int pos = 0;
    while (pos < len) {
        int next = pos + get_current_utf8_char_len(text, pos);
        int w_pos = table_edit_text_width(layer, text, pos);
        int w_next = table_edit_text_width(layer, text, next);
        if (w_next >= local_x) {
            return (local_x - w_pos) < (w_next - local_x) ? pos : next;
        }
        pos = next;
    }
    return len;
}

static int table_handle_edit_mouse(TableComponent* component, Layer* layer, PointerEvent* event) {
    if (!component || component->editing_row < 0 || component->editing_col < 0) return 0;

    Rect cell;
    table_get_cell_rect(component, layer, component->editing_row, component->editing_col, &cell);
    Point pt = {event->x, event->y};
    int in_edit_cell = point_in_rect(pt, cell);

    if (event->phase == POINTER_MOVE) {
        if (component->edit_mouse_selecting) {
            int pos = in_edit_cell ? table_edit_pos_from_mouse_x(component, layer, cell, event->x) :
                     (event->x < cell.x ? 0 : (int)strlen(component->edit_buffer));
            component->edit_cursor = pos;
            component->edit_sel_end = pos;
            table_edit_sync_scroll(component);
            mark_layer_dirty(layer, DIRTY_TEXT);
            return 1;
        }
        return 0;
    }

    if (event->button != SDL_BUTTON_LEFT) return 0;

    if (event->phase == POINTER_DOWN) {
        if (!in_edit_cell) return 0;
        int pos = table_edit_pos_from_mouse_x(component, layer, cell, event->x);
        component->edit_cursor = pos;
        component->edit_sel_start = pos;
        component->edit_sel_end = pos;
        component->edit_mouse_selecting = 1;
        table_edit_sync_scroll(component);
        mark_layer_dirty(layer, DIRTY_TEXT);
        return 1;
    }

    if (event->phase == POINTER_UP) {
        if (!component->edit_mouse_selecting) return 0;
        component->edit_mouse_selecting = 0;
        if (component->edit_sel_start == component->edit_sel_end) {
            table_edit_clear_selection(component);
        }
        mark_layer_dirty(layer, DIRTY_TEXT);
        return 1;
    }

    return 0;
}

static int table_row_count(TableComponent* component) {
    if (!component || !component->layer || !component->layer->data || !component->layer->data->json) {
        return 0;
    }
    if (!cJSON_IsArray(component->layer->data->json)) return 0;
    return cJSON_GetArraySize(component->layer->data->json);
}

static char* table_json_value_to_string(cJSON* item) {
    if (!item) return strdup("NULL");
    if (cJSON_IsString(item)) {
        return strdup(item->valuestring ? item->valuestring : "");
    }
    if (cJSON_IsNumber(item)) {
        char buf[64];
        if (is_cjson_float(item)) {
            snprintf(buf, sizeof(buf), "%g", item->valuedouble);
        } else {
            snprintf(buf, sizeof(buf), "%d", item->valueint);
        }
        return strdup(buf);
    }
    if (cJSON_IsTrue(item)) return strdup("true");
    if (cJSON_IsFalse(item)) return strdup("false");
    if (cJSON_IsNull(item)) return strdup("NULL");
    char* printed = cJSON_PrintUnformatted(item);
    return printed ? printed : strdup("");
}

static TableColumnAlign table_parse_align(const char* value) {
    if (!value) return TABLE_ALIGN_LEFT;
    if (strcmp(value, "center") == 0) return TABLE_ALIGN_CENTER;
    if (strcmp(value, "right") == 0) return TABLE_ALIGN_RIGHT;
    return TABLE_ALIGN_LEFT;
}

static void table_free_columns(TableComponent* component) {
    if (!component) return;
    free(component->columns);
    component->columns = NULL;
    component->column_count = 0;
}

static int table_parse_columns(TableComponent* component, cJSON* columns_json) {
    if (!component || !columns_json || !cJSON_IsArray(columns_json)) return 0;

    int count = cJSON_GetArraySize(columns_json);
    if (count <= 0) return 0;

    TableColumn* cols = (TableColumn*)calloc((size_t)count, sizeof(TableColumn));
    if (!cols) return 0;

    for (int i = 0; i < count; i++) {
        cJSON* col = cJSON_GetArrayItem(columns_json, i);
        if (!col) continue;

        cJSON* key = cJSON_GetObjectItem(col, "key");
        cJSON* title = cJSON_GetObjectItem(col, "title");
        cJSON* width = cJSON_GetObjectItem(col, "width");
        cJSON* flex = cJSON_GetObjectItem(col, "flex");
        cJSON* align = cJSON_GetObjectItem(col, "align");

        if (key && cJSON_IsString(key) && key->valuestring) {
            strncpy(cols[i].key, key->valuestring, sizeof(cols[i].key) - 1);
        }
        if (title && cJSON_IsString(title) && title->valuestring) {
            strncpy(cols[i].title, title->valuestring, sizeof(cols[i].title) - 1);
        } else if (cols[i].key[0]) {
            strncpy(cols[i].title, cols[i].key, sizeof(cols[i].title) - 1);
        }
        if (width && cJSON_IsNumber(width)) {
            cols[i].width = width->valueint;
        }
        if (flex && cJSON_IsNumber(flex)) {
            cols[i].flex = (float)flex->valuedouble;
        } else if (cols[i].width <= 0) {
            cols[i].flex = 1.0f;
        }
        if (align && cJSON_IsString(align)) {
            cols[i].align = table_parse_align(align->valuestring);
        } else {
            cols[i].align = TABLE_ALIGN_LEFT;
        }
    }

    table_free_columns(component);
    component->columns = cols;
    component->column_count = count;
    return 1;
}

static int table_build_auto_columns(TableComponent* component, cJSON* data) {
    if (!component || !data || !cJSON_IsArray(data)) return 0;
    cJSON* first = cJSON_GetArrayItem(data, 0);
    if (!first || !cJSON_IsObject(first)) return 0;

    int count = 0;
    for (cJSON* child = first->child; child; child = child->next) {
        if (child->string) count++;
    }
    if (count <= 0) return 0;

    TableColumn* cols = (TableColumn*)calloc((size_t)count, sizeof(TableColumn));
    if (!cols) return 0;

    int i = 0;
    for (cJSON* child = first->child; child; child = child->next) {
        if (!child->string) continue;
        strncpy(cols[i].key, child->string, sizeof(cols[i].key) - 1);
        strncpy(cols[i].title, child->string, sizeof(cols[i].title) - 1);
        cols[i].flex = 1.0f;
        cols[i].align = TABLE_ALIGN_LEFT;
        i++;
    }

    table_free_columns(component);
    component->columns = cols;
    component->column_count = i;
    return 1;
}

static int table_visible_body_height(TableComponent* component, Layer* layer) {
    if (!component || !layer) return 0;
    int h = layer->rect.h - component->header_height;
    return h > 0 ? h : 0;
}

static int table_body_content_height(TableComponent* component) {
    if (!component) return 0;
    return table_row_count(component) * component->row_height;
}

static int table_total_scroll_height(TableComponent* component) {
    if (!component) return 0;
    return table_body_content_height(component) + component->header_height;
}

static int table_needs_vertical_scroll(TableComponent* component, Layer* layer) {
    if (!layer || !(layer->scrollable == 1 || layer->scrollable == 3)) return 0;
    if (!layer->scrollbar_v || !layer->scrollbar_v->visible) return 0;
    return table_body_content_height(component) > table_visible_body_height(component, layer);
}

static int table_viewport_width(TableComponent* component, Layer* layer) {
    if (!layer) return 0;
    int w = layer->rect.w;
    if (table_needs_vertical_scroll(component, layer)) {
        int thickness = layer->scrollbar_v->thickness > 0 ? layer->scrollbar_v->thickness : 8;
        w -= thickness;
    }
    return w > 0 ? w : 0;
}

static void table_clamp_scroll(TableComponent* component, Layer* layer) {
    if (!component || !layer) return;

    int viewport_w = table_viewport_width(component, layer);
    int max_scroll_x = layer->content_width - viewport_w;
    if (max_scroll_x < 0) max_scroll_x = 0;
    if (layer->scroll_offset_x > max_scroll_x) layer->scroll_offset_x = max_scroll_x;
    if (layer->scroll_offset_x < 0) layer->scroll_offset_x = 0;

    int visible_body = table_visible_body_height(component, layer);
    int body_h = table_body_content_height(component);
    int max_scroll_y = body_h - visible_body;
    if (max_scroll_y < 0) max_scroll_y = 0;
    if (layer->scroll_offset > max_scroll_y) layer->scroll_offset = max_scroll_y;
    if (layer->scroll_offset < 0) layer->scroll_offset = 0;
}

static int table_vertical_thumb_rect(TableComponent* component, Layer* layer, Rect* thumb_out) {
    if (!component || !layer || !thumb_out || !layer->scrollbar_v) return 0;

    int content_height = table_total_scroll_height(component);
    int visible_height = layer->rect.h;
    if (content_height <= visible_height) return 0;

    int thumb_h = (int)((float)visible_height / content_height * visible_height);
    if (thumb_h < 20) thumb_h = 20;
    int thumb_y = layer->rect.y;
    if (content_height > visible_height) {
        thumb_y += (int)((float)layer->scroll_offset / (content_height - visible_height) *
                         (visible_height - thumb_h));
    }
    if (thumb_y < layer->rect.y) thumb_y = layer->rect.y;
    if (thumb_y > layer->rect.y + visible_height - thumb_h) {
        thumb_y = layer->rect.y + visible_height - thumb_h;
    }

    int thickness = layer->scrollbar_v->thickness > 0 ? layer->scrollbar_v->thickness : 8;
    thumb_out->x = layer->rect.x + layer->rect.w - thickness;
    thumb_out->y = thumb_y;
    thumb_out->w = thickness;
    thumb_out->h = thumb_h;
    return 1;
}

static void table_apply_vertical_thumb_drag(TableComponent* component, Layer* layer, int mouse_y) {
    if (!component || !layer || !layer->scrollbar_v) return;

    int content_height = table_total_scroll_height(component);
    int visible_height = layer->rect.h;
    if (content_height <= visible_height) return;

    int thumb_h = (int)((float)visible_height / content_height * visible_height);
    if (thumb_h < 20) thumb_h = 20;

    int new_thumb_y = mouse_y - layer->scrollbar_v->drag_offset;
    if (new_thumb_y < layer->rect.y) new_thumb_y = layer->rect.y;
    if (new_thumb_y > layer->rect.y + visible_height - thumb_h) {
        new_thumb_y = layer->rect.y + visible_height - thumb_h;
    }

    float scroll_ratio = (float)(new_thumb_y - layer->rect.y) / (visible_height - thumb_h);
    layer->scroll_offset = (int)(scroll_ratio * (content_height - visible_height));
    layer->content_height = content_height;
    table_clamp_scroll(component, layer);
}

static int table_get_vertical_scrollbar_track(TableComponent* component, Layer* layer, Rect* track_out) {
    if (!layer || !track_out || !component) return 0;
    int visible_height = layer->rect.h;
    if (table_needs_vertical_scroll(component, layer)) {
        int thickness = layer->scrollbar_v->thickness > 0 ? layer->scrollbar_v->thickness : 8;
        track_out->x = layer->rect.x + layer->rect.w - thickness;
        track_out->y = layer->rect.y;
        track_out->w = thickness;
        track_out->h = visible_height;
        return 1;
    }
    return 0;
}

static int table_point_in_scrollbar(TableComponent* component, Layer* layer, int x, int y) {
    Rect track;
    if (!table_get_vertical_scrollbar_track(component, layer, &track)) return 0;
    Point pt = {x, y};
    return point_in_rect(pt, track);
}

static int table_handle_vertical_scrollbar_mouse(TableComponent* component, Layer* layer, PointerEvent* event) {
    if (!layer->scrollbar_v) return 0;

    table_component_update_content_size(component);

    if (!table_needs_vertical_scroll(component, layer) && !layer->scrollbar_v->is_dragging) {
        return 0;
    }

    if (layer->scrollbar_v->is_dragging) {
        if (event->phase == POINTER_MOVE) {
            table_apply_vertical_thumb_drag(component, layer, event->y);
            mark_layer_dirty(layer, DIRTY_TEXT);
            return 1;
        }
        if (event->phase == POINTER_UP && event->button == SDL_BUTTON_LEFT) {
            layer->scrollbar_v->is_dragging = 0;
            component->pressed_row = -1;
            return 1;
        }
        return 1;
    }

    if (event->phase == POINTER_WHEEL) {
        return 0;
    }

    if (event->button != SDL_BUTTON_LEFT) {
        return table_point_in_scrollbar(component, layer, event->x, event->y);
    }

    if (event->phase == POINTER_DOWN) {
        Rect thumb;
        if (table_vertical_thumb_rect(component, layer, &thumb)) {
            Point pt = {event->x, event->y};
            if (point_in_rect(pt, thumb)) {
                layer->scrollbar_v->is_dragging = 1;
                layer->scrollbar_v->drag_offset = event->y - thumb.y;
                component->pressed_row = -1;
                return 1;
            }
        }
    }

    return table_point_in_scrollbar(component, layer, event->x, event->y);
}

static void table_component_handle_scroll_event(Layer* layer, int scroll_delta) {
    TableComponent* component;
    int old_offset;

    if (!layer || !layer->component || scroll_delta == 0) {
        return;
    }
    component = (TableComponent*)layer->component;
    table_component_update_content_size(component);
    if (!table_needs_vertical_scroll(component, layer)) {
        return;
    }

    old_offset = layer->scroll_offset;
    layer->scroll_offset += scroll_delta * 20;
    table_clamp_scroll(component, layer);
    if (layer->scroll_offset != old_offset) {
        mark_layer_dirty(layer, DIRTY_TEXT);
    }
}

static int table_component_apply_wheel(TableComponent* component, Layer* layer, PointerEvent* event) {
    int old_y;
    int old_x;
    int changed = 0;

    if (!component || !layer || !event) {
        return 0;
    }

    table_component_update_content_size(component);

    if (event->delta_y != 0 &&
        (layer->scrollable == 1 || layer->scrollable == 3) &&
        table_needs_vertical_scroll(component, layer)) {
        old_y = layer->scroll_offset;
        layer->scroll_offset += event->delta_y * 20;
        table_clamp_scroll(component, layer);
        if (layer->scroll_offset != old_y) {
            changed = 1;
        }
    }

    if (event->delta_x != 0 &&
        (layer->scrollable == 2 || layer->scrollable == 3) &&
        layer->scrollbar_h) {
        int viewport_w = table_viewport_width(component, layer);
        int max_scroll_x = layer->content_width - viewport_w;
        if (max_scroll_x > 0) {
            old_x = layer->scroll_offset_x;
            layer->scroll_offset_x += event->delta_x * 20;
            table_clamp_scroll(component, layer);
            if (layer->scroll_offset_x != old_x) {
                changed = 1;
            }
        }
    }

    if (changed) {
        mark_layer_dirty(layer, DIRTY_TEXT);
        return 1;
    }
    return 0;
}

static void table_compute_column_widths(TableComponent* component, int viewport_width) {
    if (!component || component->column_count <= 0 || viewport_width <= 0) return;

    int fixed_total = 0;
    float flex_total = 0.0f;
    for (int i = 0; i < component->column_count; i++) {
        if (component->columns[i].width > 0) {
            fixed_total += component->columns[i].width;
        } else {
            flex_total += component->columns[i].flex > 0.0f ? component->columns[i].flex : 1.0f;
        }
    }

    int remaining = viewport_width - fixed_total;
    if (remaining < 0) remaining = 0;

    for (int i = 0; i < component->column_count; i++) {
        if (component->columns[i].width > 0) {
            component->columns[i].computed_width = component->columns[i].width;
        } else {
            float weight = component->columns[i].flex > 0.0f ? component->columns[i].flex : 1.0f;
            component->columns[i].computed_width =
                flex_total > 0.0f ? (int)(remaining * (weight / flex_total)) : 0;
            if (component->columns[i].computed_width < 40) {
                component->columns[i].computed_width = 40;
            }
        }
    }
}

static int table_total_content_width(TableComponent* component) {
    int total = 0;
    if (!component) return 0;
    for (int i = 0; i < component->column_count; i++) {
        total += component->columns[i].computed_width;
    }
    return total;
}

void table_component_update_content_size(TableComponent* component) {
    if (!component || !component->layer) return;

    Layer* layer = component->layer;
    int viewport_w = table_viewport_width(component, layer);
    table_compute_column_widths(component, viewport_w);

    int content_w = table_total_content_width(component);
    int row_count = table_row_count(component);
    int body_h = row_count * component->row_height;

    layer->content_width = content_w > viewport_w ? content_w : viewport_w;
    // 含表头高度，使通用滚动条算法与表体滚动范围一致（layer->rect.h 为整表高度）
    layer->content_height = body_h + component->header_height;

    table_clamp_scroll(component, layer);
}

static int table_row_at_point(TableComponent* component, Layer* layer, int x, int y) {
    if (!component || !layer) return -1;
    if (y < layer->rect.y + component->header_height) return -1;

    int viewport_w = table_viewport_width(component, layer);
    if (x < layer->rect.x || x >= layer->rect.x + viewport_w) return -1;

    int rel_y = y - (layer->rect.y + component->header_height) + layer->scroll_offset;
    if (rel_y < 0) return -1;

    int idx = rel_y / component->row_height;
    int count = table_row_count(component);
    if (idx < 0 || idx >= count) return -1;
    return idx;
}

static int table_column_at_point(TableComponent* component, Layer* layer, int x, int y) {
    if (table_row_at_point(component, layer, x, y) < 0) return -1;
    return table_column_index_at_x(component, layer, x);
}

static void table_get_cell_rect(TableComponent* component, Layer* layer,
                                int row, int col, Rect* out) {
    if (!out || row < 0 || col < 0 || col >= component->column_count) return;

    int body_y = layer->rect.y + component->header_height;
    int x = layer->rect.x - layer->scroll_offset_x;
    for (int i = 0; i < col; i++) {
        x += component->columns[i].computed_width;
    }
    out->x = x;
    out->y = body_y + row * component->row_height - layer->scroll_offset;
    out->w = component->columns[col].computed_width;
    out->h = component->row_height;
}

static void table_edit_update_scroll_for_cursor(TableComponent* component, const Rect* cell) {
    if (!component || !cell || component->editing_row < 0) return;

    Layer* layer = component->layer;
    if (!layer) return;

    int visible_width = cell->w - TABLE_CELL_PAD_X * 2;
    if (visible_width < 1) visible_width = 1;

    int buflen = (int)strlen(component->edit_buffer);
    int full_width = table_edit_text_width(layer, component->edit_buffer, buflen);
    if (full_width <= visible_width) {
        component->edit_scroll_x = 0;
        return;
    }

    int cursor_x = table_edit_text_width(layer, component->edit_buffer, component->edit_cursor);
    if (cursor_x < component->edit_scroll_x) {
        component->edit_scroll_x = cursor_x;
    } else if (cursor_x > component->edit_scroll_x + visible_width) {
        component->edit_scroll_x = cursor_x - visible_width;
    }

    int max_scroll = full_width - visible_width;
    if (max_scroll < 0) max_scroll = 0;
    if (component->edit_scroll_x < 0) component->edit_scroll_x = 0;
    if (component->edit_scroll_x > max_scroll) component->edit_scroll_x = max_scroll;
}

static void table_edit_sync_scroll(TableComponent* component) {
    if (!component || component->editing_row < 0 || component->editing_col < 0) return;

    Layer* layer = component->layer;
    if (!layer) return;

    Rect cell;
    table_get_cell_rect(component, layer, component->editing_row, component->editing_col, &cell);
    table_edit_update_scroll_for_cursor(component, &cell);
}

static void table_cancel_edit(TableComponent* component) {
    if (!component) return;
    component->editing_row = -1;
    component->editing_col = -1;
    component->edit_buffer[0] = '\0';
    component->edit_cursor = 0;
    component->edit_sel_start = -1;
    component->edit_sel_end = -1;
    component->edit_mouse_selecting = 0;
    component->edit_orig_number = 0;
    component->edit_scroll_x = 0;
    backend_stop_text_input();
}

static void table_set_cell_json_value(cJSON* row, const char* key, const char* text, int prefer_number) {
    if (!row || !key || !text) return;

    cJSON* old = cJSON_GetObjectItem(row, key);
    if (prefer_number) {
        char* end = NULL;
        double num = strtod(text, &end);
        if (end && end != text && *end == '\0') {
            cJSON* num_item = cJSON_CreateNumber(num);
            if (num_item) {
                if (old) cJSON_ReplaceItemInObject(row, key, num_item);
                else cJSON_AddItemToObject(row, key, num_item);
            }
            return;
        }
    }

    if (strcmp(text, "NULL") == 0) {
        cJSON* null_item = cJSON_CreateNull();
        if (null_item) {
            if (old) cJSON_ReplaceItemInObject(row, key, null_item);
            else cJSON_AddItemToObject(row, key, null_item);
        }
        return;
    }

    cJSON* str_item = cJSON_CreateString(text);
    if (!str_item) return;
    if (old) cJSON_ReplaceItemInObject(row, key, str_item);
    else cJSON_AddItemToObject(row, key, str_item);
}

static int table_commit_edit(TableComponent* component) {
    if (!component || component->editing_row < 0 || component->editing_col < 0) return 0;

    Layer* layer = component->layer;
    if (!layer || !layer->data || !layer->data->json || !cJSON_IsArray(layer->data->json)) {
        table_cancel_edit(component);
        return 0;
    }

    cJSON* row = cJSON_GetArrayItem(layer->data->json, component->editing_row);
    if (!row || component->editing_col >= component->column_count) {
        table_cancel_edit(component);
        return 0;
    }

    TableColumn* col = &component->columns[component->editing_col];
    table_set_cell_json_value(row, col->key, component->edit_buffer, component->edit_orig_number);

    component->selected_row = component->editing_row;
    component->selected_col = component->editing_col;
    table_cancel_edit(component);
    mark_layer_dirty(layer, DIRTY_TEXT);
    return 1;
}

static void table_begin_edit(TableComponent* component, int row, int col) {
    if (!component || !component->editable) return;
    if (row < 0 || col < 0 || col >= component->column_count) return;

    Layer* layer = component->layer;
    if (!layer || !layer->data || !layer->data->json) return;

    cJSON* row_json = cJSON_GetArrayItem(layer->data->json, row);
    if (!row_json) return;

    TableColumn* column = &component->columns[col];
    cJSON* value = cJSON_GetObjectItem(row_json, column->key);
    char* text = table_json_value_to_string(value);
    if (!text) return;

    strncpy(component->edit_buffer, text, TABLE_EDIT_BUF_SIZE - 1);
    component->edit_buffer[TABLE_EDIT_BUF_SIZE - 1] = '\0';
    free(text);

    component->editing_row = row;
    component->editing_col = col;
    component->edit_orig_number = value && cJSON_IsNumber(value);
    table_edit_select_all_text(component);
    component->selected_row = row;
    component->selected_col = col;
    component->edit_mouse_selecting = 0;
    component->edit_scroll_x = 0;

    Rect cell;
    table_get_cell_rect(component, layer, row, col, &cell);
    backend_start_text_input();
    Rect ime_rect = {cell.x, cell.y + cell.h, cell.w, 0};
    backend_set_text_input_rect(&ime_rect);

    mark_layer_dirty(layer, DIRTY_TEXT);
}

static void table_copy_edit_selection(TableComponent* component) {
    if (!component || component->editing_row < 0) return;

    if (table_edit_has_selection(component)) {
        int lo = utf8_safe_prefix_bytes(component->edit_buffer, table_edit_sel_lo(component));
        int hi = utf8_safe_prefix_bytes(component->edit_buffer, table_edit_sel_hi(component));
        int len = hi - lo;
        if (len <= 0) return;
        char* text = (char*)malloc((size_t)len + 1);
        if (!text) return;
        memcpy(text, component->edit_buffer + lo, (size_t)len);
        text[len] = '\0';
        backend_set_clipboard_text(text);
        free(text);
        return;
    }
    backend_set_clipboard_text(component->edit_buffer);
}

static void table_copy_cell(TableComponent* component) {
    if (!component) return;
    if (component->editing_row >= 0) {
        table_copy_edit_selection(component);
        return;
    }
    if (component->selected_row < 0 || component->selected_col < 0) return;

    Layer* layer = component->layer;
    if (!layer || !layer->data || !layer->data->json) return;

    cJSON* row = cJSON_GetArrayItem(layer->data->json, component->selected_row);
    if (!row || component->selected_col >= component->column_count) return;

    char* text = table_json_value_to_string(
        cJSON_GetObjectItem(row, component->columns[component->selected_col].key));
    if (text) {
        backend_set_clipboard_text(text);
        free(text);
    }
}

static void table_copy_row(TableComponent* component) {
    if (!component || component->selected_row < 0) return;

    Layer* layer = component->layer;
    if (!layer || !layer->data || !layer->data->json || component->column_count <= 0) return;

    cJSON* row = cJSON_GetArrayItem(layer->data->json, component->selected_row);
    if (!row) return;

    char* buf = (char*)malloc((size_t)component->column_count * 256);
    if (!buf) return;
    buf[0] = '\0';

    for (int c = 0; c < component->column_count; c++) {
        char* text = table_json_value_to_string(
            cJSON_GetObjectItem(row, component->columns[c].key));
        if (!text) text = strdup("");
        if (c > 0) strcat(buf, "\t");
        strncat(buf, text, 255);
        free(text);
    }
    backend_set_clipboard_text(buf);
    free(buf);
}

static void table_move_selection(TableComponent* component, int drow, int dcol) {
    if (!component) return;
    if (component->editing_row >= 0) return;

    int row_count = table_row_count(component);
    if (row_count <= 0 || component->column_count <= 0) return;

    if (component->selected_row < 0) component->selected_row = 0;
    if (component->selected_col < 0) component->selected_col = 0;

    component->selected_row += drow;
    component->selected_col += dcol;

    if (component->selected_row < 0) component->selected_row = 0;
    if (component->selected_row >= row_count) component->selected_row = row_count - 1;
    if (component->selected_col < 0) component->selected_col = 0;
    if (component->selected_col >= component->column_count) {
        component->selected_col = component->column_count - 1;
    }
    mark_layer_dirty(component->layer, DIRTY_TEXT);
}

static void table_edit_insert_text(TableComponent* component, const char* text) {
    if (!component || !text || component->editing_row < 0) return;

    table_edit_remove_selection(component);

    int len = (int)strlen(component->edit_buffer);
    int add_len = (int)strlen(text);
    if (add_len <= 0) return;
    if (len + add_len >= TABLE_EDIT_BUF_SIZE - 1) return;

    memmove(component->edit_buffer + component->edit_cursor + add_len,
            component->edit_buffer + component->edit_cursor,
            (size_t)(len - component->edit_cursor + 1));
    memcpy(component->edit_buffer + component->edit_cursor, text, (size_t)add_len);
    component->edit_cursor += add_len;
    table_edit_sync_scroll(component);
    mark_layer_dirty(component->layer, DIRTY_TEXT);
}

static void table_edit_backspace(TableComponent* component) {
    if (!component || component->editing_row < 0) return;

    if (table_edit_has_selection(component)) {
        table_edit_remove_selection(component);
        table_edit_sync_scroll(component);
        mark_layer_dirty(component->layer, DIRTY_TEXT);
        return;
    }
    if (component->edit_cursor <= 0) return;

    int len = (int)strlen(component->edit_buffer);
    int char_len = get_prev_utf8_char_len(component->edit_buffer, component->edit_cursor);
    if (char_len <= 0) char_len = 1;
    int pos = component->edit_cursor - char_len;
    memmove(component->edit_buffer + pos,
            component->edit_buffer + component->edit_cursor,
            (size_t)(len - component->edit_cursor + 1));
    component->edit_cursor = pos;
    table_edit_sync_scroll(component);
    mark_layer_dirty(component->layer, DIRTY_TEXT);
}

static void table_edit_delete(TableComponent* component) {
    if (!component || component->editing_row < 0) return;

    if (table_edit_has_selection(component)) {
        table_edit_remove_selection(component);
        table_edit_sync_scroll(component);
        mark_layer_dirty(component->layer, DIRTY_TEXT);
        return;
    }

    int len = (int)strlen(component->edit_buffer);
    if (component->edit_cursor >= len) return;

    int char_len = get_current_utf8_char_len(component->edit_buffer, component->edit_cursor);
    if (char_len <= 0) char_len = 1;
    memmove(component->edit_buffer + component->edit_cursor,
            component->edit_buffer + component->edit_cursor + char_len,
            (size_t)(len - component->edit_cursor - char_len + 1));
    table_edit_sync_scroll(component);
    mark_layer_dirty(component->layer, DIRTY_TEXT);
}

static void table_edit_draw_text_part(Layer* layer, const char* text, Color color,
                                      int draw_x, int draw_y, int draw_h) {
    if (!text || !text[0] || draw_h <= 0) return;

    Texture* tex = render_text(layer, text, color);
    if (!tex) return;

    int tw = 0, th = 0;
    backend_query_texture(tex, NULL, NULL, &tw, &th);
    int part_w = (int)(tw / yui_density);
    int part_h = (int)(th / yui_density);
    if (part_w < 1) part_w = 1;
    if (part_h < 1) part_h = 1;
    if (part_h > draw_h) {
        float ratio = (float)part_w / (float)part_h;
        part_h = draw_h;
        part_w = (int)(draw_h * ratio);
        if (part_w < 1) part_w = 1;
    }

    Rect dst = {draw_x, draw_y, part_w, part_h};
    backend_render_text_copy(tex, NULL, &dst);
    backend_render_text_destroy(tex);
}

static void table_render_edit_cell(TableComponent* component, Layer* layer, Rect cell) {
    Color edit_bg = component->cell_focus_color;
    if (edit_bg.a == 0) edit_bg = (Color){40, 42, 54, 255};
    backend_render_fill_rect(&cell, edit_bg);

    Color text_color = layer->color;
    Color sel_bg = {137, 180, 250, 255};
    Color sel_fg = {17, 17, 27, 255};

    Rect cell_clip = {cell.x, cell.y, cell.w, cell.h};
    Rect prev_clip;
    backend_render_get_clip_rect(&prev_clip);
    Rect clip = cell_clip;
    if (prev_clip.w > 0 && prev_clip.h > 0) {
        table_intersect_rect(&clip, &cell_clip, &prev_clip);
    }
    backend_render_set_clip_rect(&clip);

    int draw_x = cell.x + TABLE_CELL_PAD_X - component->edit_scroll_x;
    int draw_y = cell.y;
    int draw_w = 0;
    int draw_h = 0;

    if (component->edit_buffer[0] && layer->font && layer->font->default_font) {
        Texture* tex = render_text(layer, component->edit_buffer, text_color);
        if (tex) {
            int tw = 0, th = 0;
            backend_query_texture(tex, NULL, NULL, &tw, &th);
            draw_w = (int)(tw / yui_density);
            draw_h = (int)(th / yui_density);
            if (draw_w < 1) draw_w = 1;
            if (draw_h < 1) draw_h = 1;

            int max_h = cell.h - 2;
            if (max_h < 1) max_h = 1;
            if (draw_h > max_h) {
                float ratio = (float)draw_w / (float)draw_h;
                draw_h = max_h;
                draw_w = (int)(max_h * ratio);
                if (draw_w < 1) draw_w = 1;
            }
            draw_y = cell.y + (cell.h - draw_h) / 2;
            backend_render_text_destroy(tex);
        }
    }

    int buflen = (int)strlen(component->edit_buffer);
    int has_sel = table_edit_has_selection(component);
    int lo = utf8_safe_prefix_bytes(component->edit_buffer, table_edit_sel_lo(component));
    int hi = utf8_safe_prefix_bytes(component->edit_buffer, table_edit_sel_hi(component));

    if (has_sel && buflen > 0 && draw_h > 0) {
        int x_lo = draw_x + table_edit_text_width(layer, component->edit_buffer, lo);
        int x_hi = draw_x + table_edit_text_width(layer, component->edit_buffer, hi);
        if (x_hi > x_lo) {
            Rect sel = {x_lo, draw_y, x_hi - x_lo, draw_h};
            backend_render_fill_rect(&sel, sel_bg);
        }

        if (lo > 0) {
            char part[TABLE_EDIT_BUF_SIZE];
            memcpy(part, component->edit_buffer, (size_t)lo);
            part[lo] = '\0';
            table_edit_draw_text_part(layer, part, text_color, draw_x, draw_y, draw_h);
        }
        if (hi > lo) {
            char part[TABLE_EDIT_BUF_SIZE];
            int sel_len = hi - lo;
            if (sel_len >= TABLE_EDIT_BUF_SIZE) sel_len = TABLE_EDIT_BUF_SIZE - 1;
            memcpy(part, component->edit_buffer + lo, (size_t)sel_len);
            part[sel_len] = '\0';
            int part_x = draw_x + table_edit_text_width(layer, component->edit_buffer, lo);
            table_edit_draw_text_part(layer, part, sel_fg, part_x, draw_y, draw_h);
        }
        if (hi < buflen) {
            char part[TABLE_EDIT_BUF_SIZE];
            int tail_len = buflen - hi;
            if (tail_len >= TABLE_EDIT_BUF_SIZE) tail_len = TABLE_EDIT_BUF_SIZE - 1;
            memcpy(part, component->edit_buffer + hi, (size_t)tail_len);
            part[tail_len] = '\0';
            int part_x = draw_x + table_edit_text_width(layer, component->edit_buffer, hi);
            table_edit_draw_text_part(layer, part, text_color, part_x, draw_y, draw_h);
        }
    } else if (buflen > 0 && draw_h > 0) {
        table_edit_draw_text_part(layer, component->edit_buffer, text_color,
                                  draw_x, draw_y, draw_h);
    }

    if (!has_sel && layer->font && layer->font->default_font) {
        int cursor_x = draw_x + table_edit_text_width(layer, component->edit_buffer, component->edit_cursor);
        int cursor_h = draw_h > 0 ? draw_h : cell.h - 4;
        if (cursor_h > cell.h - 4) cursor_h = cell.h - 4;
        if (cursor_h < 8) cursor_h = cell.h - 4;
        int cursor_y = cell.y + (cell.h - cursor_h) / 2;
        Rect cursor = {cursor_x, cursor_y, 1, cursor_h};
        backend_render_fill_rect(&cursor, text_color);
    }

    backend_render_set_clip_rect(&prev_clip);
}

static void table_dispatch_select(TableComponent* component, int index) {
    Layer* layer = component->layer;
    if (!layer || component->on_select_name[0] == '\0') return;
    if (!layer->data || !layer->data->json || !cJSON_IsArray(layer->data->json)) return;

    EventHandler handler = find_event_by_name(component->on_select_name);
    if (!handler) return;

    cJSON* item = cJSON_GetArrayItem(layer->data->json, index);
    if (!item) return;

    char* item_json = cJSON_PrintUnformatted(item);
    if (!item_json) return;

    if (!layer->event) {
        layer->event = calloc(1, sizeof(Event));
    }
    if (layer->event) {
        strncpy(layer->event->click_name, component->on_select_name, MAX_PATH - 1);
        layer->event->click_name[MAX_PATH - 1] = '\0';
    }

    layer_set_text(layer, item_json);
    free(item_json);
    handler(layer);
}

static int table_data_update(Layer* layer, cJSON* data) {
    if (!layer || !layer->component || !data || !cJSON_IsArray(data)) return 0;

    TableComponent* component = (TableComponent*)layer->component;

    if (component->auto_columns || component->column_count == 0) {
        table_build_auto_columns(component, data);
    }

    int already_owned = layer->data && layer->data->json == data;
    if (!already_owned) {
        if (layer->data) {
            if (layer->data->json) cJSON_Delete(layer->data->json);
            free(layer->data);
            layer->data = NULL;
        }

        layer->data = (Data*)malloc(sizeof(Data));
        if (!layer->data) return 0;

        layer->data->json = data;
    }

    layer->data->size = cJSON_GetArraySize(data);
    component->hovered_row = -1;
    component->pressed_row = -1;
    table_tooltip_reset(component);
    table_cancel_edit(component);
    component->selected_col = -1;
    if (component->selected_row >= layer->data->size) {
        component->selected_row = -1;
    }

    table_component_update_content_size(component);
    mark_layer_dirty(layer, DIRTY_LAYOUT | DIRTY_TEXT);
    return already_owned ? 0 : 1;
}

static void table_layer_destroy(Layer* layer) {
    if (!layer || !layer->component) return;
    table_component_destroy((TableComponent*)layer->component);
    layer->component = NULL;
}

TableComponent* table_component_create(Layer* layer) {
    TableComponent* component = (TableComponent*)calloc(1, sizeof(TableComponent));
    if (!component) return NULL;

    component->layer = layer;
    component->header_height = 32;
    component->row_height = 28;
    component->hovered_row = -1;
    component->selected_row = -1;
    component->pressed_row = -1;
    component->stripe_rows = 1;
    component->show_grid_lines = 1;
    component->header_bg_color = (Color){24, 24, 37, 255};
    component->header_text_color = (Color){166, 173, 200, 255};
    component->row_bg_color = (Color){17, 17, 27, 255};
    component->row_alt_bg_color = (Color){24, 24, 36, 255};
    component->row_hover_color = (Color){49, 50, 68, 255};
    component->row_selected_color = (Color){69, 71, 90, 255};
    component->grid_line_color = (Color){49, 50, 68, 255};
    component->resizable_columns = 1;
    component->resizing_column = -1;
    component->resize_col_hover = -1;
    component->layout_w = -1;
    component->layout_h = -1;
    component->selected_col = -1;
    component->editing_row = -1;
    component->editing_col = -1;
    component->edit_cursor = 0;
    component->edit_sel_start = -1;
    component->edit_sel_end = -1;
    component->edit_mouse_selecting = 0;
    component->edit_orig_number = 0;
    component->edit_scroll_x = 0;
    component->editable = 1;
    component->last_click_time = 0;
    component->last_click_row = -1;
    component->last_click_col = -1;
    component->tooltip_row = TABLE_TOOLTIP_ROW_NONE;
    component->tooltip_col = -1;
    component->cell_focus_color = (Color){49, 50, 68, 255};

    layer->component = component;
    layer->render = table_component_render;
    layer->handle_pointer_event = table_component_handle_pointer_event;
    layer->handle_scroll_event = table_component_handle_scroll_event;
    layer->handle_key_event = table_component_handle_key_event;
    layer->focusable = 1;
    layer->on_data_update = table_data_update;
    layer->on_destroy = table_layer_destroy;
    layer->set_style = table_component_apply_theme_style;

    return component;
}

void table_component_destroy(TableComponent* component) {
    if (!component) return;
    table_tooltip_reset(component);
    table_free_columns(component);
    free(component);
}

static void table_component_apply_theme_style(Layer* layer, cJSON* style) {
    if (!layer || !style || !layer->component) {
        return;
    }

    TableComponent* component = (TableComponent*)layer->component;

    cJSON* header_bg = cJSON_GetObjectItem(style, "headerBgColor");
    if (header_bg && cJSON_IsString(header_bg)) {
        parse_color(header_bg->valuestring, &component->header_bg_color);
    }
    cJSON* header_color = cJSON_GetObjectItem(style, "headerColor");
    if (header_color && cJSON_IsString(header_color)) {
        parse_color(header_color->valuestring, &component->header_text_color);
    }
    cJSON* row_alt = cJSON_GetObjectItem(style, "rowAltBgColor");
    if (row_alt && cJSON_IsString(row_alt)) {
        parse_color(row_alt->valuestring, &component->row_alt_bg_color);
    }
    cJSON* grid_line = cJSON_GetObjectItem(style, "gridLineColor");
    if (grid_line && cJSON_IsString(grid_line)) {
        parse_color(grid_line->valuestring, &component->grid_line_color);
    }
    cJSON* cell_focus = cJSON_GetObjectItem(style, "cellFocusColor");
    if (cell_focus && cJSON_IsString(cell_focus)) {
        parse_color(cell_focus->valuestring, &component->cell_focus_color);
    }
    cJSON* bg_color = cJSON_GetObjectItem(style, "bgColor");
    if (bg_color && cJSON_IsString(bg_color)) {
        parse_color(bg_color->valuestring, &layer->bg_color);
        component->row_bg_color = layer->bg_color;
    } else if (layer->bg_color.a > 0) {
        component->row_bg_color = layer->bg_color;
    }
    cJSON* color = cJSON_GetObjectItem(style, "color");
    if (color && cJSON_IsString(color)) {
        parse_color(color->valuestring, &layer->color);
    }
    cJSON* row_hover = cJSON_GetObjectItem(style, "rowHoverColor");
    if (row_hover && cJSON_IsString(row_hover)) {
        parse_color(row_hover->valuestring, &component->row_hover_color);
    }
    cJSON* row_selected = cJSON_GetObjectItem(style, "rowSelectedColor");
    if (row_selected && cJSON_IsString(row_selected)) {
        parse_color(row_selected->valuestring, &component->row_selected_color);
    }

    cJSON* scrollbar_color = cJSON_GetObjectItem(style, "scrollbarColor");
    if (scrollbar_color && cJSON_IsString(scrollbar_color)) {
        Color sc;
        parse_color(scrollbar_color->valuestring, &sc);
        if (layer->scrollbar_v) layer->scrollbar_v->color = sc;
        if (layer->scrollbar_h) layer->scrollbar_h->color = sc;
    }
    cJSON* scrollbar_track = cJSON_GetObjectItem(style, "scrollbarTrackColor");
    if (scrollbar_track && cJSON_IsString(scrollbar_track)) {
        Color sc;
        parse_color(scrollbar_track->valuestring, &sc);
        if (layer->scrollbar_v) layer->scrollbar_v->track_color = sc;
        if (layer->scrollbar_h) layer->scrollbar_h->track_color = sc;
    }

    mark_layer_dirty(layer, DIRTY_COLOR | DIRTY_TEXT);
}

TableComponent* table_component_create_from_json(Layer* layer, cJSON* json_obj) {
    TableComponent* component = table_component_create(layer);
    if (!component || !json_obj) return component;

    cJSON* header_height = cJSON_GetObjectItem(json_obj, "headerHeight");
    if (header_height && cJSON_IsNumber(header_height)) {
        component->header_height = header_height->valueint;
    }

    cJSON* row_height = cJSON_GetObjectItem(json_obj, "rowHeight");
    if (row_height && cJSON_IsNumber(row_height)) {
        component->row_height = row_height->valueint;
    }

    cJSON* stripe_rows = cJSON_GetObjectItem(json_obj, "stripeRows");
    if (stripe_rows) {
        component->stripe_rows = cJSON_IsTrue(stripe_rows);
    }

    cJSON* show_grid = cJSON_GetObjectItem(json_obj, "showGridLines");
    if (show_grid) {
        component->show_grid_lines = cJSON_IsTrue(show_grid);
    }

    cJSON* auto_columns = cJSON_GetObjectItem(json_obj, "autoColumns");
    if (auto_columns) {
        component->auto_columns = cJSON_IsTrue(auto_columns);
    }

    cJSON* resizable_columns = cJSON_GetObjectItem(json_obj, "resizableColumns");
    if (resizable_columns) {
        component->resizable_columns = cJSON_IsTrue(resizable_columns);
    }

    cJSON* editable = cJSON_GetObjectItem(json_obj, "editable");
    if (editable) {
        component->editable = cJSON_IsTrue(editable);
    }

    cJSON* columns = cJSON_GetObjectItem(json_obj, "columns");
    if (columns && cJSON_IsArray(columns)) {
        table_parse_columns(component, columns);
    }

    cJSON* style = cJSON_GetObjectItem(json_obj, "style");
    if (style) {
        table_component_apply_theme_style(layer, style);
    }

    cJSON* events = cJSON_GetObjectItem(json_obj, "events");
    if (events) {
        cJSON* on_select = cJSON_GetObjectItem(events, "onSelect");
        if (on_select && cJSON_IsString(on_select) && on_select->valuestring) {
            const char* handler_name = on_select->valuestring;
            if (handler_name[0] == '@') handler_name++;
            strncpy(component->on_select_name, handler_name,
                    sizeof(component->on_select_name) - 1);
            component->on_select_name[sizeof(component->on_select_name) - 1] = '\0';
        }
    }

    table_component_update_content_size(component);
    return component;
}

int table_component_get_row_count(TableComponent* component) {
    return table_row_count(component);
}

static void table_intersect_rect(Rect* out, const Rect* a, const Rect* b) {
    if (!out || !a || !b) return;
    int x1 = a->x > b->x ? a->x : b->x;
    int y1 = a->y > b->y ? a->y : b->y;
    int x2 = (a->x + a->w) < (b->x + b->w) ? (a->x + a->w) : (b->x + b->w);
    int y2 = (a->y + a->h) < (b->y + b->h) ? (a->y + a->h) : (b->y + b->h);
    out->x = x1;
    out->y = y1;
    out->w = x2 > x1 ? x2 - x1 : 0;
    out->h = y2 > y1 ? y2 - y1 : 0;
}

static void table_draw_cell_text(Layer* layer, const char* text, Color color,
                                int x, int y, int w, int h, TableColumnAlign align) {
    if (!layer || !text || !layer->font || !layer->font->default_font) return;

    Texture* tex = render_text(layer, text, color);
    if (!tex) return;

    int tw = 0, th = 0;
    backend_query_texture(tex, NULL, NULL, &tw, &th);
    int draw_w = (int)(tw / yui_density);
    int draw_h = (int)(th / yui_density);
    if (draw_w < 1) draw_w = 1;
    if (draw_h < 1) draw_h = 1;

    int max_h = h - 2;
    if (max_h < 1) max_h = 1;

    // 高度超出时等比缩小；宽度超出由裁剪处理，避免横向挤压变形
    if (draw_h > max_h) {
        float ratio = (float)draw_w / (float)draw_h;
        draw_h = max_h;
        draw_w = (int)(max_h * ratio);
        if (draw_w < 1) draw_w = 1;
    }

    int draw_x = x + TABLE_CELL_PAD_X;
    if (align == TABLE_ALIGN_CENTER) {
        draw_x = x + (w - draw_w) / 2;
    } else if (align == TABLE_ALIGN_RIGHT) {
        draw_x = x + w - TABLE_CELL_PAD_X - draw_w;
    }
    int draw_y = y + (h - draw_h) / 2;
    if (draw_y < y) draw_y = y;

    Rect cell_clip = {x, y, w, h};
    Rect prev_clip;
    backend_render_get_clip_rect(&prev_clip);
    Rect clip = cell_clip;
    if (prev_clip.w > 0 && prev_clip.h > 0) {
        table_intersect_rect(&clip, &cell_clip, &prev_clip);
    }
    backend_render_set_clip_rect(&clip);

    Rect dst = {draw_x, draw_y, draw_w, draw_h};
    backend_render_text_copy(tex, NULL, &dst);
    backend_render_text_destroy(tex);

    backend_render_set_clip_rect(&prev_clip);
}

static int table_point_in_header(TableComponent* component, Layer* layer, int x, int y) {
    if (!component || !layer) return 0;
    if (y < layer->rect.y || y >= layer->rect.y + component->header_height) return 0;
    int viewport_w = table_viewport_width(component, layer);
    return x >= layer->rect.x && x < layer->rect.x + viewport_w;
}

static int table_column_right_x(TableComponent* component, Layer* layer, int col_index) {
    int x = layer->rect.x - layer->scroll_offset_x;
    for (int i = 0; i <= col_index && i < component->column_count; i++) {
        x += component->columns[i].computed_width;
    }
    return x;
}

static int table_resize_column_at(TableComponent* component, Layer* layer, int x, int y) {
    if (!component->resizable_columns || component->column_count <= 0) return -1;
    if (!table_point_in_header(component, layer, x, y)) return -1;

    int viewport_w = table_viewport_width(component, layer);
    int half = TABLE_RESIZE_HANDLE / 2;
    if (TABLE_RESIZE_HANDLE % 2) half++;

    for (int i = 0; i < component->column_count; i++) {
        int edge_x = table_column_right_x(component, layer, i);
        if (edge_x < layer->rect.x - half || edge_x > layer->rect.x + viewport_w + half) {
            continue;
        }
        if (x >= edge_x - half && x <= edge_x + half) {
            return i;
        }
    }
    return -1;
}

static void table_apply_column_resize(TableComponent* component, int col_index, int new_width) {
    if (!component || col_index < 0 || col_index >= component->column_count) return;
    if (new_width < TABLE_COL_MIN_WIDTH) new_width = TABLE_COL_MIN_WIDTH;

    TableColumn* col = &component->columns[col_index];
    col->width = new_width;
    col->flex = 0.0f;
    table_component_update_content_size(component);
    mark_layer_dirty(component->layer, DIRTY_LAYOUT | DIRTY_TEXT);
}

static void table_render_header(TableComponent* component, Layer* layer, int viewport_w) {
    Rect header = {
        layer->rect.x,
        layer->rect.y,
        viewport_w,
        component->header_height
    };
    backend_render_fill_rect(&header, component->header_bg_color);

    int x = layer->rect.x - layer->scroll_offset_x;
    for (int i = 0; i < component->column_count; i++) {
        TableColumn* col = &component->columns[i];
        Rect cell = {x, header.y, col->computed_width, header.h};
        if (cell.x + cell.w > header.x && cell.x < header.x + header.w) {
            table_draw_cell_text(layer, col->title, component->header_text_color,
                                 cell.x, cell.y, cell.w, cell.h, col->align);
        }
        if (component->show_grid_lines) {
            int highlight = component->resizable_columns &&
                (component->resizing_column == i || component->resize_col_hover == i);
            Color line_color = highlight ? (Color){137, 180, 250, 255} : component->grid_line_color;
            Rect line = {cell.x + cell.w - 1, cell.y, 1, cell.h};
            backend_render_fill_rect(&line, line_color);
        }
        x += col->computed_width;
    }

    Rect bottom = {header.x, header.y + header.h - 1, header.w, 1};
    backend_render_fill_rect(&bottom, component->grid_line_color);
}

static void table_render_rows(TableComponent* component, Layer* layer, int viewport_w) {
    if (!layer->data || !layer->data->json || !cJSON_IsArray(layer->data->json)) return;

    Rect body_clip = {
        layer->rect.x,
        layer->rect.y + component->header_height,
        viewport_w,
        layer->rect.h - component->header_height
    };

    Rect prev_clip;
    backend_render_get_clip_rect(&prev_clip);
    backend_render_set_clip_rect(&body_clip);

    int row_count = cJSON_GetArraySize(layer->data->json);
    int first_row = layer->scroll_offset / component->row_height;
    if (first_row < 0) first_row = 0;
    int visible_rows = body_clip.h / component->row_height + 2;

    for (int r = first_row; r < row_count && r < first_row + visible_rows; r++) {
        cJSON* row = cJSON_GetArrayItem(layer->data->json, r);
        if (!row) continue;

        int row_y = body_clip.y + r * component->row_height - layer->scroll_offset;
        if (row_y + component->row_height < body_clip.y || row_y > body_clip.y + body_clip.h) {
            continue;
        }

        Color bg = component->row_bg_color;
        if (component->stripe_rows && (r % 2 == 1)) {
            bg = component->row_alt_bg_color;
        }
        if (r == component->hovered_row) {
            bg = component->row_hover_color;
        }
        if (r == component->selected_row) {
            bg = component->row_selected_color;
        }

        Rect row_rect = {body_clip.x, row_y, body_clip.w, component->row_height};
        backend_render_fill_rect(&row_rect, bg);

        int x = layer->rect.x - layer->scroll_offset_x;
        for (int c = 0; c < component->column_count; c++) {
            TableColumn* col = &component->columns[c];
            Rect cell = {x, row_y, col->computed_width, component->row_height};

            int is_editing = (r == component->editing_row && c == component->editing_col);
            int is_focused = (r == component->selected_row && c == component->selected_col);

            Color cell_bg = bg;
            if (is_focused && HAS_STATE(layer, LAYER_STATE_FOCUSED) && !is_editing) {
                Color focus_bg = component->cell_focus_color;
                if (focus_bg.a == 0) focus_bg = (Color){49, 50, 68, 255};
                cell_bg = focus_bg;
            }
            backend_render_fill_rect(&cell, cell_bg);

            if (is_editing) {
                table_render_edit_cell(component, layer, cell);
            } else {
                cJSON* value = cJSON_GetObjectItem(row, col->key);
                char* text = table_json_value_to_string(value);
                if (text) {
                    if (cell.x + cell.w > body_clip.x && cell.x < body_clip.x + body_clip.w) {
                        table_draw_cell_text(layer, text, layer->color,
                                             cell.x, cell.y, cell.w, cell.h, col->align);
                    }
                    free(text);
                }
            }

            if (component->show_grid_lines) {
                Rect line = {cell.x + cell.w - 1, cell.y, 1, cell.h};
                backend_render_fill_rect(&line, component->grid_line_color);
            }
            x += col->computed_width;
        }

        if (component->show_grid_lines) {
            Rect line = {body_clip.x, row_y + component->row_height - 1, body_clip.w, 1};
            backend_render_fill_rect(&line, component->grid_line_color);
        }
    }

    backend_render_set_clip_rect(&prev_clip);
}

void table_component_render(Layer* layer) {
    if (!layer || !layer->component) return;

    TableComponent* component = (TableComponent*)layer->component;
    if (layer->font && !layer->font->default_font) {
        load_all_fonts(layer);
    }

    table_component_update_content_size(component);

    if (component->layout_w != layer->rect.w || component->layout_h != layer->rect.h) {
        component->layout_w = layer->rect.w;
        component->layout_h = layer->rect.h;
    }

    table_clamp_scroll(component, layer);

    if (layer->bg_color.a > 0) {
        backend_render_fill_rect(&layer->rect, layer->bg_color);
    }

    int viewport_w = table_viewport_width(component, layer);
    if (component->column_count <= 0) {
        return;
    }

    table_render_header(component, layer, viewport_w);
    table_render_rows(component, layer, viewport_w);
    table_tooltip_try_show(component, layer);
}

int table_component_handle_pointer_event(Layer* layer, PointerEvent* event) {
    if (!layer || !event || !layer->component) return 0;

    TableComponent* component = (TableComponent*)layer->component;

    if (component->resizing_column >= 0) {
        if (event->phase == POINTER_MOVE) {
            int delta = event->x - component->resize_drag_start_x;
            table_apply_column_resize(component, component->resizing_column,
                                      component->resize_drag_start_w + delta);
            return 1;
        }
        if (event->phase == POINTER_UP && event->button == SDL_BUTTON_LEFT) {
            component->resizing_column = -1;
            component->resize_col_hover = table_resize_column_at(component, layer, event->x, event->y);
            return 1;
        }
        return 1;
    }

    if (table_handle_vertical_scrollbar_mouse(component, layer, event)) {
        return 1;
    }

    if (event->phase == POINTER_WHEEL) {
        Point pt = {event->x, event->y};
        if (!point_in_rect(pt, layer->rect)) {
            return 0;
        }
        return table_component_apply_wheel(component, layer, event);
    }

    if (component->editing_row >= 0) {
        if (table_handle_edit_mouse(component, layer, event)) {
            return 1;
        }
        if ((event->phase == POINTER_DOWN || event->phase == POINTER_DOUBLE_TAP) &&
            event->button == SDL_BUTTON_LEFT) {
            int row = table_row_at_point(component, layer, event->x, event->y);
            int col = table_column_at_point(component, layer, event->x, event->y);
            if (row != component->editing_row || col != component->editing_col) {
                table_commit_edit(component);
            }
        }
    }

    int in_header = table_point_in_header(component, layer, event->x, event->y);
    int resize_col = in_header ? table_resize_column_at(component, layer, event->x, event->y) : -1;

    if (event->phase == POINTER_MOVE) {
        int prev_hover = component->resize_col_hover;
        component->resize_col_hover = resize_col;
        if (prev_hover != resize_col && (prev_hover >= 0 || resize_col >= 0)) {
            mark_layer_dirty(layer, DIRTY_TEXT);
        }
        if (in_header) {
            component->hovered_row = -1;
            table_tooltip_update_hover(component, layer, event->x, event->y);
            return 1;
        }
        if (component->editing_row >= 0) {
            table_tooltip_reset(component);
            return 1;
        }
        int row = table_row_at_point(component, layer, event->x, event->y);
        component->hovered_row = row >= 0 ? row : -1;
        if (row >= 0) {
            table_tooltip_update_hover(component, layer, event->x, event->y);
        } else {
            table_tooltip_reset(component);
        }
        return row >= 0;
    }

    if (event->phase == POINTER_DOWN || event->phase == POINTER_DOUBLE_TAP) {
        table_tooltip_reset(component);
    }

    if (in_header && event->button == SDL_BUTTON_LEFT && event->phase != POINTER_WHEEL) {
        if (event->phase == POINTER_DOWN && resize_col >= 0) {
            component->resizing_column = resize_col;
            component->resize_drag_start_x = event->x;
            component->resize_drag_start_w = component->columns[resize_col].computed_width;
            component->pressed_row = -1;
            return 1;
        }
        return 1;
    }

    int row = table_row_at_point(component, layer, event->x, event->y);
    int in_body = row >= 0;

    if (event->button != SDL_BUTTON_LEFT) {
        return in_body;
    }

    if (event->phase == POINTER_DOWN || event->phase == POINTER_DOUBLE_TAP) {
        component->pressed_row = in_body ? row : -1;
        component->hovered_row = in_body ? row : -1;
        if (in_body && component->editing_row < 0) {
            int col = table_column_at_point(component, layer, event->x, event->y);
            component->selected_row = row;
            component->selected_col = col;
            /* SDL 连点第二次为 DOUBLE_TAP；直接进编辑，避免只认 DOWN 时丢双击 */
            if (event->phase == POINTER_DOUBLE_TAP && component->editable) {
                if (col < 0 && component->last_click_col >= 0) {
                    col = component->last_click_col;
                    component->selected_col = col;
                }
                table_begin_edit(component, row, col);
            }
            mark_layer_dirty(layer, DIRTY_TEXT);
        }
        return in_body;
    }

    if (event->phase == POINTER_UP) {
        if (component->editing_row >= 0) {
            component->pressed_row = -1;
            return 1;
        }
        int clicked = in_body && component->pressed_row == row && row >= 0;
        component->pressed_row = -1;
        if (clicked) {
            int col = table_column_at_point(component, layer, event->x, event->y);
            Uint32 now = backend_get_ticks();
            int is_double = (row == component->last_click_row &&
                               now - component->last_click_time < TABLE_DOUBLE_CLICK_MS);
            if (is_double && component->last_click_col >= 0) {
                col = component->last_click_col;
            }

            component->selected_row = row;
            component->selected_col = col;
            component->last_click_time = now;
            component->last_click_row = row;
            component->last_click_col = col;

            if (is_double && component->editable) {
                table_begin_edit(component, row, col);
            } else {
                table_dispatch_select(component, row);
            }
            mark_layer_dirty(layer, DIRTY_TEXT);
        }
        return in_body;
    }

    return 0;
}

int table_component_handle_key_event(Layer* layer, KeyEvent* event) {
    if (!layer || !event || !layer->component) return 0;
    if (!HAS_STATE(layer, LAYER_STATE_FOCUSED)) return 0;

    TableComponent* component = (TableComponent*)layer->component;

    if (event->type == KEY_EVENT_TEXT_INPUT) {
        if (component->editing_row >= 0) {
            table_edit_insert_text(component, event->data.text.text);
            return 1;
        }
        if (component->editable && component->selected_row >= 0 && component->selected_col >= 0) {
            table_begin_edit(component, component->selected_row, component->selected_col);
            table_edit_clear_selection(component);
            component->edit_cursor = 0;
            component->edit_buffer[0] = '\0';
            if (event->data.text.text[0]) {
                table_edit_insert_text(component, event->data.text.text);
            }
            return 1;
        }
        return 0;
    }

    if (event->type != KEY_EVENT_DOWN) return 0;

    int ctrl = event->data.key.mod & KMOD_CTRL;
    int shift = event->data.key.mod & KMOD_SHIFT;

    if (ctrl && event->data.key.key_code == SDLK_c) {
        if (shift) {
            table_copy_row(component);
        } else {
            table_copy_cell(component);
        }
        return 1;
    }

    if (component->editing_row >= 0) {
        switch (event->data.key.key_code) {
            case SDLK_ESCAPE:
                table_cancel_edit(component);
                mark_layer_dirty(layer, DIRTY_TEXT);
                return 1;
            case SDLK_RETURN:
            case SDLK_KP_ENTER:
                table_commit_edit(component);
                return 1;
            case SDLK_TAB:
                table_commit_edit(component);
                table_move_selection(component, 0, shift ? -1 : 1);
                if (component->editable && component->selected_row >= 0 && component->selected_col >= 0) {
                    table_begin_edit(component, component->selected_row, component->selected_col);
                }
                return 1;
            case SDLK_BACKSPACE:
                table_edit_backspace(component);
                return 1;
            case SDLK_DELETE:
                table_edit_delete(component);
                return 1;
            case SDLK_LEFT:
                table_edit_move_cursor(component, -1, shift);
                return 1;
            case SDLK_RIGHT:
                table_edit_move_cursor(component, 1, shift);
                return 1;
            case SDLK_HOME:
                if (shift) {
                    if (component->edit_sel_start < 0) {
                        component->edit_sel_start = component->edit_cursor;
                    }
                    component->edit_cursor = 0;
                    component->edit_sel_end = 0;
                } else {
                    component->edit_cursor = 0;
                    table_edit_clear_selection(component);
                }
                table_edit_sync_scroll(component);
                mark_layer_dirty(layer, DIRTY_TEXT);
                return 1;
            case SDLK_END:
                if (shift) {
                    int len = (int)strlen(component->edit_buffer);
                    if (component->edit_sel_start < 0) {
                        component->edit_sel_start = component->edit_cursor;
                    }
                    component->edit_cursor = len;
                    component->edit_sel_end = len;
                } else {
                    component->edit_cursor = (int)strlen(component->edit_buffer);
                    table_edit_clear_selection(component);
                }
                table_edit_sync_scroll(component);
                mark_layer_dirty(layer, DIRTY_TEXT);
                return 1;
            case SDLK_a:
                if (ctrl) {
                    table_edit_select_all_text(component);
                    component->edit_scroll_x = 0;
                    table_edit_sync_scroll(component);
                    mark_layer_dirty(layer, DIRTY_TEXT);
                    return 1;
                }
                break;
            case SDLK_v:
                if (ctrl) {
                    char* clipboard_text = backend_get_clipboard_text();
                    if (clipboard_text) {
                        table_edit_insert_text(component, clipboard_text);
                        free(clipboard_text);
                    }
                    return 1;
                }
                break;
            default:
                break;
        }
        return 0;
    }

    switch (event->data.key.key_code) {
        case SDLK_F2:
            if (component->editable && component->selected_row >= 0 && component->selected_col >= 0) {
                table_begin_edit(component, component->selected_row, component->selected_col);
                return 1;
            }
            return 0;
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
            if (component->editable && component->selected_row >= 0 && component->selected_col >= 0) {
                table_begin_edit(component, component->selected_row, component->selected_col);
                return 1;
            }
            return 0;
        case SDLK_UP:
            table_move_selection(component, -1, 0);
            return 1;
        case SDLK_DOWN:
            table_move_selection(component, 1, 0);
            return 1;
        case SDLK_LEFT:
            table_move_selection(component, 0, -1);
            return 1;
        case SDLK_RIGHT:
            table_move_selection(component, 0, 1);
            return 1;
        case SDLK_TAB:
            table_move_selection(component, 0, shift ? -1 : 1);
            return 1;
        default:
            return 0;
    }
}

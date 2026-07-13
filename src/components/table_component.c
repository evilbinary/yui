#include "table_component.h"
#include "../backend.h"
#include "../event.h"
#include "../layer_update.h"
#include "../render.h"
#include "../util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern float scale;

#define TABLE_CELL_PAD_X 8
#define TABLE_COL_MIN_WIDTH 40
#define TABLE_RESIZE_HANDLE 6

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

static int table_viewport_width(Layer* layer) {
    if (!layer) return 0;
    int w = layer->rect.w;
    if ((layer->scrollable == 1 || layer->scrollable == 3) &&
        layer->scrollbar_v && layer->scrollbar_v->visible &&
        layer->content_height > layer->rect.h) {
        int thickness = layer->scrollbar_v->thickness > 0 ? layer->scrollbar_v->thickness : 8;
        w -= thickness;
    }
    return w > 0 ? w : 0;
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
    int viewport_w = table_viewport_width(layer);
    table_compute_column_widths(component, viewport_w);

    int content_w = table_total_content_width(component);
    int row_count = table_row_count(component);
    int body_h = row_count * component->row_height;

    layer->content_width = content_w > viewport_w ? content_w : viewport_w;
    layer->content_height = body_h;

    int max_scroll_x = layer->content_width - viewport_w;
    if (max_scroll_x < 0) max_scroll_x = 0;
    if (layer->scroll_offset_x > max_scroll_x) layer->scroll_offset_x = max_scroll_x;
    if (layer->scroll_offset_x < 0) layer->scroll_offset_x = 0;

    int visible_body = layer->rect.h - component->header_height;
    if (visible_body < 0) visible_body = 0;
    int max_scroll_y = body_h - visible_body;
    if (max_scroll_y < 0) max_scroll_y = 0;
    if (layer->scroll_offset > max_scroll_y) layer->scroll_offset = max_scroll_y;
    if (layer->scroll_offset < 0) layer->scroll_offset = 0;
}

static int table_get_vertical_scrollbar_track(Layer* layer, Rect* track_out) {
    if (!layer || !track_out) return 0;
    int visible_height = layer->rect.h;
    if ((layer->scrollable == 1 || layer->scrollable == 3) &&
        layer->scrollbar_v && layer->scrollbar_v->visible &&
        layer->content_height > visible_height) {
        int thickness = layer->scrollbar_v->thickness > 0 ? layer->scrollbar_v->thickness : 8;
        track_out->x = layer->rect.x + layer->rect.w - thickness;
        track_out->y = layer->rect.y;
        track_out->w = thickness;
        track_out->h = visible_height;
        return 1;
    }
    return 0;
}

static int table_point_in_scrollbar(Layer* layer, int x, int y) {
    Rect track;
    if (!table_get_vertical_scrollbar_track(layer, &track)) return 0;
    Point pt = {x, y};
    return point_in_rect(pt, track);
}

static int table_row_at_point(TableComponent* component, Layer* layer, int x, int y) {
    if (!component || !layer) return -1;
    if (y < layer->rect.y + component->header_height) return -1;

    int viewport_w = table_viewport_width(layer);
    if (x < layer->rect.x || x >= layer->rect.x + viewport_w) return -1;

    int rel_y = y - (layer->rect.y + component->header_height) + layer->scroll_offset;
    if (rel_y < 0) return -1;

    int idx = rel_y / component->row_height;
    int count = table_row_count(component);
    if (idx < 0 || idx >= count) return -1;
    return idx;
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

static void table_data_update(Layer* layer, cJSON* data) {
    if (!layer || !layer->component || !data || !cJSON_IsArray(data)) return;

    TableComponent* component = (TableComponent*)layer->component;

    if (component->auto_columns || component->column_count == 0) {
        table_build_auto_columns(component, data);
    }

    if (layer->data) {
        if (layer->data->json) cJSON_Delete(layer->data->json);
        free(layer->data);
        layer->data = NULL;
    }

    layer->data = (Data*)malloc(sizeof(Data));
    if (!layer->data) return;

    layer->data->json = cJSON_Duplicate(data, 1);
    layer->data->size = cJSON_GetArraySize(data);
    component->hovered_row = -1;
    component->pressed_row = -1;
    if (component->selected_row >= layer->data->size) {
        component->selected_row = -1;
    }

    table_component_update_content_size(component);
    mark_layer_dirty(layer, DIRTY_LAYOUT | DIRTY_TEXT);
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

    layer->component = component;
    layer->render = table_component_render;
    layer->handle_mouse_event = table_component_handle_mouse_event;
    layer->focusable = 1;
    layer->on_data_update = table_data_update;
    layer->on_destroy = table_layer_destroy;

    return component;
}

void table_component_destroy(TableComponent* component) {
    if (!component) return;
    table_free_columns(component);
    free(component);
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

    cJSON* columns = cJSON_GetObjectItem(json_obj, "columns");
    if (columns && cJSON_IsArray(columns)) {
        table_parse_columns(component, columns);
    }

    cJSON* style = cJSON_GetObjectItem(json_obj, "style");
    if (style) {
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
        if (layer->bg_color.a > 0) {
            component->row_bg_color = layer->bg_color;
        }
        if (layer->color.a > 0) {
            component->header_text_color = layer->color;
        }
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
    int draw_w = (int)(tw / scale);
    int draw_h = (int)(th / scale);
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
    int viewport_w = table_viewport_width(layer);
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

    int viewport_w = table_viewport_width(layer);
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

            cJSON* value = cJSON_GetObjectItem(row, col->key);
            char* text = table_json_value_to_string(value);
            if (text) {
                if (cell.x + cell.w > body_clip.x && cell.x < body_clip.x + body_clip.w) {
                    table_draw_cell_text(layer, text, layer->color,
                                         cell.x, cell.y, cell.w, cell.h, col->align);
                }
                free(text);
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

    if (layer->bg_color.a > 0) {
        backend_render_fill_rect(&layer->rect, layer->bg_color);
    }

    int viewport_w = table_viewport_width(layer);
    if (component->column_count <= 0) {
        return;
    }

    table_render_header(component, layer, viewport_w);
    table_render_rows(component, layer, viewport_w);
}

int table_component_handle_mouse_event(Layer* layer, MouseEvent* event) {
    if (!layer || !event || !layer->component) return 0;

    TableComponent* component = (TableComponent*)layer->component;

    if (component->resizing_column >= 0) {
        if (event->state == SDL_MOUSEMOTION) {
            int delta = event->x - component->resize_drag_start_x;
            table_apply_column_resize(component, component->resizing_column,
                                      component->resize_drag_start_w + delta);
            return 1;
        }
        if (event->state == SDL_RELEASED && event->button == SDL_BUTTON_LEFT) {
            component->resizing_column = -1;
            component->resize_col_hover = table_resize_column_at(component, layer, event->x, event->y);
            return 1;
        }
        return 1;
    }

    if (layer->scrollbar_v && layer->scrollbar_v->is_dragging) {
        if (event->state == SDL_RELEASED && event->button == SDL_BUTTON_LEFT) {
            component->pressed_row = -1;
        }
        return 1;
    }
    if (table_point_in_scrollbar(layer, event->x, event->y)) {
        if (event->state == SDL_RELEASED && event->button == SDL_BUTTON_LEFT) {
            component->pressed_row = -1;
        }
        return 1;
    }

    int in_header = table_point_in_header(component, layer, event->x, event->y);
    int resize_col = in_header ? table_resize_column_at(component, layer, event->x, event->y) : -1;

    if (event->state == SDL_MOUSEMOTION) {
        int prev_hover = component->resize_col_hover;
        component->resize_col_hover = resize_col;
        if (prev_hover != resize_col && (prev_hover >= 0 || resize_col >= 0)) {
            mark_layer_dirty(layer, DIRTY_TEXT);
        }
        if (in_header) {
            component->hovered_row = -1;
            return 1;
        }
        int row = table_row_at_point(component, layer, event->x, event->y);
        component->hovered_row = row >= 0 ? row : -1;
        return row >= 0;
    }

    if (in_header && event->button == SDL_BUTTON_LEFT) {
        if (event->state == SDL_PRESSED && resize_col >= 0) {
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

    if (event->state == SDL_PRESSED) {
        component->pressed_row = in_body ? row : -1;
        component->hovered_row = in_body ? row : -1;
        if (in_body) {
            component->selected_row = row;
        }
        return in_body;
    }

    if (event->state == SDL_RELEASED) {
        int clicked = in_body && component->pressed_row == row && row >= 0;
        component->pressed_row = -1;
        if (clicked) {
            component->selected_row = row;
            table_dispatch_select(component, row);
        }
        return in_body;
    }

    return 0;
}

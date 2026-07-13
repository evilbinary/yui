#ifndef YUI_TABLE_COMPONENT_H
#define YUI_TABLE_COMPONENT_H

#include "../layer.h"
#include "../ytype.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    TABLE_ALIGN_LEFT,
    TABLE_ALIGN_CENTER,
    TABLE_ALIGN_RIGHT
} TableColumnAlign;

typedef struct TableColumn {
    char key[64];
    char title[128];
    int width;
    float flex;
    TableColumnAlign align;
    int computed_width;
} TableColumn;

typedef struct TableComponent {
    Layer* layer;
    TableColumn* columns;
    int column_count;
    int header_height;
    int row_height;
    int hovered_row;
    int selected_row;
    int pressed_row;
    int stripe_rows;
    int show_grid_lines;
    int auto_columns;
    Color header_bg_color;
    Color header_text_color;
    Color row_bg_color;
    Color row_alt_bg_color;
    Color row_hover_color;
    Color row_selected_color;
    Color grid_line_color;
    char item_click_name[128];
} TableComponent;

TableComponent* table_component_create(Layer* layer);
void table_component_destroy(TableComponent* component);
TableComponent* table_component_create_from_json(Layer* layer, cJSON* json_obj);
void table_component_update_content_size(TableComponent* component);
int table_component_get_row_count(TableComponent* component);
void table_component_render(Layer* layer);
int table_component_handle_mouse_event(Layer* layer, MouseEvent* event);

#ifdef __cplusplus
}
#endif

#endif

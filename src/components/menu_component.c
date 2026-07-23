#include "menu_component.h"
#include "../render.h"
#include "../backend.h"
#include "../util.h"
#include "../popup_manager.h"
#include "../event.h"
#include "../layer_update.h"
#include <stdlib.h>
#include <string.h>
#include "cJSON.h"

static void menu_component_apply_theme_style(Layer* layer, cJSON* style);

static void menu_layer_destroy(Layer* layer) {
    if (!layer || !layer->component) {
        return;
    }
    menu_component_destroy((MenuComponent*)layer->component);
    layer->component = NULL;
}

// 创建菜单组件
MenuComponent* menu_component_create(Layer* layer) {
    if (!layer) {
        return NULL;
    }
    
    MenuComponent* component = (MenuComponent*)malloc(sizeof(MenuComponent));
    if (!component) {
        return NULL;
    }
    
    memset(component, 0, sizeof(MenuComponent));
    component->layer = layer;
    component->popup_layer = NULL;
    component->item_count = 0;
    component->hovered_item = -1;
    component->opened_item = -1;
    component->is_popup = 0;
    component->is_submenu = 0;
    component->expanded = 0;
    component->parent_menu = NULL;
    component->item_height = 30;
    component->min_width = 80;
    component->content_width = 0;
    component->show_arrow = 0;
    component->user_data = NULL;
    component->on_popup_closed = NULL;
    
    // 设置默认颜色
    component->bg_color = (Color){255, 255, 255, 255};
    component->text_color = (Color){50, 50, 50, 255};
    component->hover_color = (Color){70, 130, 180, 255};
    component->disabled_color = (Color){150, 150, 150, 255};
    component->separator_color = (Color){200, 200, 200, 255};
    
    // 设置组件指针和自定义渲染函数
    layer->component = component;
    layer->render = menu_component_render;
    layer->set_style = menu_component_apply_theme_style;
    layer->on_destroy = menu_layer_destroy;


    // 绑定事件处理函数
    layer->handle_pointer_event = menu_component_handle_pointer_event;
    layer->handle_key_event = menu_component_handle_key_event;

    // 确保图层有可用的字体
    if (!layer->font) {
        // 从父层继承字体配置
        Layer* p = layer->parent;
        while (p) {
            if (p->font) {
                layer->font = malloc(sizeof(Font));
                if (layer->font) {
                    memcpy(layer->font, p->font, sizeof(Font));
                    layer->font->default_font = NULL; // 需要单独加载
                }
                break;
            }
            p = p->parent;
        }
        // 父层也没有字体，使用默认配置（load_all_fonts 会加载实际字体文件）
        if (!layer->font) {
            layer->font = malloc(sizeof(Font));
            if (layer->font) {
                memset(layer->font, 0, sizeof(Font));
                snprintf(layer->font->path, MAX_PATH, "%s", "Roboto-Regular.ttf");
                layer->font->size = 16;
                snprintf(layer->font->weight, sizeof(layer->font->weight), "%s", "normal");
                layer->font->default_font = NULL;
            }
        }
    }
    
    return component;
}

// 销毁菜单组件
void menu_component_destroy(MenuComponent* component) {
    if (!component) {
        return;
    }

    // 如果弹出菜单正在显示，先关闭它
    if (component->is_popup && component->popup_layer) {
        menu_component_hide_popup(component);
    }

    // 递归销毁所有子菜单
    if (component->items) {
        for (int i = 0; i < component->item_count; i++) {
            if (component->items[i].submenu) {
                menu_component_destroy(component->items[i].submenu);
                component->items[i].submenu = NULL;
            }
        }
        free(component->items);
        component->items = NULL;
        component->item_count = 0;
    }

    // 子菜单的内部图层由我们分配，不在主 Layer 树中，这里直接释放
    if (component->is_submenu && component->layer) {
        component->layer->on_destroy = NULL;
        component->layer->component = NULL;
        if (component->layer->font) {
            free(component->layer->font);
            component->layer->font = NULL;
        }
        free(component->layer);
        component->layer = NULL;
    } else if (component->layer) {
        component->layer->component = NULL;
    }
    free(component);
}

// 递归解析JSON菜单项（支持嵌套子菜单）
static void parse_menu_items_from_json(MenuComponent* component, cJSON* items_json) {
    if (!component || !items_json || !cJSON_IsArray(items_json)) return;

    int count = cJSON_GetArraySize(items_json);
    for (int i = 0; i < count; i++) {
        cJSON* item = cJSON_GetArrayItem(items_json, i);
        if (!item) continue;

        const char* text = "";
        const char* shortcut = "";
        int enabled = 1;
        int checked = 0;
        int separator = 0;

        if (cJSON_HasObjectItem(item, "text"))     text = cJSON_GetObjectItem(item, "text")->valuestring;
        if (cJSON_HasObjectItem(item, "shortcut")) shortcut = cJSON_GetObjectItem(item, "shortcut")->valuestring;
        if (cJSON_HasObjectItem(item, "enabled"))  enabled = cJSON_IsTrue(cJSON_GetObjectItem(item, "enabled"));
        if (cJSON_HasObjectItem(item, "checked"))  checked = cJSON_IsTrue(cJSON_GetObjectItem(item, "checked"));
        if (cJSON_HasObjectItem(item, "separator")) separator = cJSON_IsTrue(cJSON_GetObjectItem(item, "separator"));

        menu_component_add_item(component, text, shortcut, enabled, checked, separator, NULL, NULL);

        // 解析嵌套子菜单
        cJSON* submenu_json = cJSON_GetObjectItem(item, "items");
        if (submenu_json && cJSON_IsArray(submenu_json)) {
            MenuComponent* submenu = menu_component_add_submenu(component, i);
            if (submenu) {
                submenu->on_select_name[0] = '\0';
                // 继承父菜单的on_select_name，让子菜单项也能触发同一事件
                if (strlen(component->on_select_name) > 0) {
                    strncpy(submenu->on_select_name, component->on_select_name, sizeof(submenu->on_select_name) - 1);
                }
                parse_menu_items_from_json(submenu, submenu_json);
            }
        }
    }
}

// 计算UTF-8字符串的字符数（非字节数）
static int utf8_char_count(const char* s) {
    int count = 0;
    while (*s) {
        if ((*s & 0xC0) != 0x80) count++;
        s++;
    }
    return count;
}

// 根据内容自动计算菜单宽度
static void menu_calculate_auto_width(MenuComponent* component) {
    if (!component || !component->layer) return;

    Layer* layer = component->layer;
    int font_size = (layer->font) ? layer->font->size : 16;
    int max_width = 80;

    // 估算标题文本宽度（按字符数，非字节数）
    if (layer->text && strlen(layer->text) > 0) {
        int title_w = utf8_char_count(layer->text) * font_size + 48;
        if (title_w > max_width) max_width = title_w;
    }

    // 估算每个菜单项的文本宽度
    for (int i = 0; i < component->item_count; i++) {
        MenuItem* item = &component->items[i];
        if (item->separator) continue;

        int item_w = utf8_char_count(item->text) * font_size;

        // 快捷键宽度
        int sc_len = strlen(item->shortcut);
        if (sc_len > 0) {
            item_w += 24 + (int)(sc_len * font_size * 0.6f);
        }

        // 内边距: 12左 + 12右
        int padding = 24;
        if (item->checked) padding += 16;
        if (item->submenu) padding += 16;

        item_w += padding;
        if (item_w > max_width) max_width = item_w;
    }

    if (max_width < component->min_width) {
        max_width = component->min_width;
    }

    component->content_width = max_width;
    layer->rect.w = max_width;
    layer->fixed_width = max_width;
}

// 折叠时只保留标题栏命中区域，避免未显示的子菜单区域挡住下层滚轮/点击
static void menu_update_hit_rect(MenuComponent* component) {
    Layer* layer;
    int title_h;
    int visible_h;

    if (!component || !(layer = component->layer)) {
        return;
    }
    if (component->is_popup && component->popup_layer) {
        return;
    }

    title_h = (layer->text && layer->text[0]) ? component->item_height : 0;
    if (component->expanded) {
        visible_h = title_h + component->item_count * component->item_height;
    } else {
        visible_h = title_h > 0 ? title_h : component->item_height;
    }

    layer->rect.h = visible_h;
    layer->fixed_height = visible_h;
}

// 从JSON创建菜单组件
MenuComponent* menu_component_create_from_json(Layer* layer, cJSON* json) {
    if (!layer || !json) {
        return NULL;
    }

    MenuComponent* component = menu_component_create(layer);
    if (!component) {
        return NULL;
    }

    // 解析菜单项（递归支持子菜单）
    cJSON* items = cJSON_GetObjectItem(json, "items");
    parse_menu_items_from_json(component, items);

    // 根据内容自动计算菜单宽度
    menu_calculate_auto_width(component);
    menu_update_hit_rect(component);
    
    // 解析样式
    cJSON* style = cJSON_GetObjectItem(json, "style");
    if (style) {
        menu_component_apply_theme_style(layer, style);
    }

    // 解析 onSelect 事件
    cJSON* events = cJSON_GetObjectItem(json, "events");
    if (events) {
        if (cJSON_HasObjectItem(events, "onSelect")) {
            const char* handler_name = cJSON_GetObjectItem(events, "onSelect")->valuestring;
            // 去掉@前缀
            if (handler_name[0] == '@') handler_name++;
            strncpy(component->on_select_name, handler_name, sizeof(component->on_select_name) - 1);
            component->on_select_name[sizeof(component->on_select_name) - 1] = '\0';
        }
    }

    return component;
}

static void menu_component_apply_theme_style(Layer* layer, cJSON* style) {
    if (!layer || !style || !layer->component) {
        return;
    }

    MenuComponent* component = (MenuComponent*)layer->component;

    if (cJSON_HasObjectItem(style, "bgColor")) {
        parse_color(cJSON_GetObjectItem(style, "bgColor")->valuestring, &component->bg_color);
        layer->bg_color = component->bg_color;
    }
    if (cJSON_HasObjectItem(style, "textColor")) {
        parse_color(cJSON_GetObjectItem(style, "textColor")->valuestring, &component->text_color);
        layer->color = component->text_color;
    }
    if (cJSON_HasObjectItem(style, "color")) {
        parse_color(cJSON_GetObjectItem(style, "color")->valuestring, &component->text_color);
        layer->color = component->text_color;
    }
    if (cJSON_HasObjectItem(style, "hoverColor")) {
        parse_color(cJSON_GetObjectItem(style, "hoverColor")->valuestring, &component->hover_color);
    }
    if (cJSON_HasObjectItem(style, "disabledColor")) {
        parse_color(cJSON_GetObjectItem(style, "disabledColor")->valuestring, &component->disabled_color);
    }
    if (cJSON_HasObjectItem(style, "separatorColor")) {
        parse_color(cJSON_GetObjectItem(style, "separatorColor")->valuestring, &component->separator_color);
    }
    if (cJSON_HasObjectItem(style, "itemHeight")) {
        component->item_height = cJSON_GetObjectItem(style, "itemHeight")->valueint;
    }
    if (cJSON_HasObjectItem(style, "showArrow")) {
        component->show_arrow = cJSON_IsTrue(cJSON_GetObjectItem(style, "showArrow"));
    }

    mark_layer_dirty(layer, DIRTY_COLOR | DIRTY_TEXT | DIRTY_LAYOUT);
}

// 添加菜单项
void menu_component_add_item(MenuComponent* component, const char* text, const char* shortcut, 
                            int enabled, int checked, int separator, 
                            void (*callback)(void*), void* user_data) {
    if (!component) {
        return;
    }
    
    // 扩展菜单项数组
    component->items = (MenuItem*)realloc(component->items, 
                                         (component->item_count + 1) * sizeof(MenuItem));
    if (!component->items) {
        return;
    }
    
    MenuItem* item = &component->items[component->item_count];
    memset(item, 0, sizeof(MenuItem));
    
    if (text) {
        strncpy(item->text, text, sizeof(item->text) - 1);
        item->text[sizeof(item->text) - 1] = '\0';
    }
    
    if (shortcut) {
        strncpy(item->shortcut, shortcut, sizeof(item->shortcut) - 1);
        item->shortcut[sizeof(item->shortcut) - 1] = '\0';
    }
    
    item->enabled = enabled;
    item->checked = checked;
    item->separator = separator;
    item->callback = callback;
    item->user_data = user_data;
    
    component->item_count++;
}

// 清空菜单项
void menu_component_clear_items(MenuComponent* component) {
    if (!component) {
        return;
    }
    
    if (component->items) {
        free(component->items);
        component->items = NULL;
    }
    component->item_count = 0;
    component->hovered_item = -1;
    component->opened_item = -1;
}

// 设置颜色
void menu_component_set_colors(MenuComponent* component, Color bg, Color text, 
                              Color hover, Color disabled, Color separator) {
    if (!component) {
        return;
    }
    
    component->bg_color = bg;
    component->text_color = text;
    component->hover_color = hover;
    component->disabled_color = disabled;
    component->separator_color = separator;
}

// 设置菜单项高度
void menu_component_set_item_height(MenuComponent* component, int height) {
    if (!component) {
        return;
    }
    component->item_height = height;
}

// 设置最小宽度
void menu_component_set_min_width(MenuComponent* component, int width) {
    if (!component) {
        return;
    }
    component->min_width = width;
}

// 设置用户数据
void menu_component_set_user_data(MenuComponent* component, void* data) {
    if (!component) {
        return;
    }
    component->user_data = data;
}

// 为指定索引的菜单项创建子菜单
MenuComponent* menu_component_add_submenu(MenuComponent* component, int item_index) {
    if (!component || item_index < 0 || item_index >= component->item_count) {
        return NULL;
    }

    MenuItem* item = &component->items[item_index];
    if (item->separator) return NULL;
    if (item->submenu) return item->submenu;  // 已存在则返回

    // 创建子菜单的内部图层
    Layer* sub_layer = (Layer*)malloc(sizeof(Layer));
    if (!sub_layer) return NULL;
    memset(sub_layer, 0, sizeof(Layer));

    // 继承父菜单的字体
    if (component->layer && component->layer->font) {
        sub_layer->font = (Font*)malloc(sizeof(Font));
        if (sub_layer->font) {
            memcpy(sub_layer->font, component->layer->font, sizeof(Font));
            sub_layer->font->default_font = NULL;
        }
    }

    MenuComponent* submenu = menu_component_create(sub_layer);
    if (!submenu) {
        if (sub_layer->font) free(sub_layer->font);
        free(sub_layer);
        return NULL;
    }

    submenu->is_submenu = 1;
    submenu->parent_menu = component;

    // 继承父菜单的样式
    submenu->bg_color = component->bg_color;
    submenu->text_color = component->text_color;
    submenu->hover_color = component->hover_color;
    submenu->disabled_color = component->disabled_color;
    submenu->separator_color = component->separator_color;
    submenu->item_height = component->item_height;
    submenu->min_width = component->min_width;

    item->submenu = submenu;
    return submenu;
}

// 获取指定位置对应的菜单项索引
int menu_component_get_item_at_position(MenuComponent* component, int x, int y) {
    if (!component) {
        return -1;
    }
    
    // 使用弹出菜单图层的矩形，如果没有弹出菜单则使用普通图层
    Rect* rect = component->popup_layer ? &component->popup_layer->rect : &component->layer->rect;
    
    // 检查是否在菜单范围内
    if (x < rect->x || x >= rect->x + rect->w || 
        y < rect->y || y >= rect->y + rect->h) {
        return -1;
    }
    
    // 计算标题偏移
    int y_offset = (component->layer->text && strlen(component->layer->text) > 0) ? component->item_height : 0;
    
    // 计算菜单项索引
    int relative_y = y - rect->y - y_offset;
    if (relative_y < 0) return -1; // 标题区域，不在菜单项上
    int index = relative_y / component->item_height;
    
    if (index >= 0 && index < component->item_count) {
        return index;
    }
    
    return -1;
}

// 沿着parent_menu链关闭所有菜单
static void close_menu_chain(MenuComponent* component) {
    MenuComponent* cur = component;
    while (cur) {
        if (cur->is_popup) {
            menu_component_hide_popup(cur);
        } else {
            cur->hovered_item = -1;
            cur->expanded = 0;
            // 关闭子菜单
            if (cur->opened_item >= 0 && cur->items && cur->opened_item < cur->item_count) {
                MenuItem* sub_item = &cur->items[cur->opened_item];
                if (sub_item->submenu && sub_item->submenu->is_popup) {
                    menu_component_hide_popup(sub_item->submenu);
                }
                cur->opened_item = -1;
            }
            if (cur->layer) {
                popup_manager_remove(cur->layer);
            }
        }
        cur = cur->parent_menu;
    }
}

// 执行菜单项点击
static void menu_item_click(MenuComponent* component, MenuItem* item) {
    if (!item || item->separator || !item->enabled) return;

    if (item->submenu) {
        // 有子菜单的项：展开/收起子菜单
        if (component->opened_item >= 0) {
            MenuItem* prev = &component->items[component->opened_item];
            if (prev->submenu && prev->submenu->is_popup) {
                menu_component_hide_popup(prev->submenu);
            }
            if (component->opened_item == component->hovered_item) {
                component->opened_item = -1;
                return;
            }
        }
        // 展开子菜单
        Rect* rect = component->popup_layer ? &component->popup_layer->rect : &component->layer->rect;
        int y_offset = (component->layer->text && strlen(component->layer->text) > 0) ? component->item_height : 0;
        int sub_x = rect->x + rect->w;
        int sub_y = rect->y + y_offset + component->hovered_item * component->item_height;
        menu_component_show_popup(item->submenu, sub_x, sub_y);
        component->opened_item = component->hovered_item;
        return;
    }

    // 执行回调
    if (item->callback) {
        item->callback(item->user_data);
    } else if (strlen(component->on_select_name) > 0) {
        // 保存原始文本，设置点击项文本供JS读取
        char saved_text[256] = {0};
        const char* orig = layer_get_text(component->layer);
        strncpy(saved_text, orig, sizeof(saved_text) - 1);
        layer_set_text(component->layer, item->text);

        // 设置事件路由信息，供 js_module_common_event 调用对应的JS函数
        if (!component->layer->event) {
            component->layer->event = malloc(sizeof(Event));
            if (component->layer->event) {
                memset(component->layer->event, 0, sizeof(Event));
            }
        }
        if (component->layer->event) {
            strncpy(component->layer->event->click_name, component->on_select_name, MAX_PATH - 1);
            component->layer->event->click_name[MAX_PATH - 1] = '\0';
        }

        EventHandler handler = find_event_by_name(component->on_select_name);
        if (handler) handler(component->layer);

        // 恢复原始文本
        layer_set_text(component->layer, saved_text);
    }

    // 关闭整个菜单链
    close_menu_chain(component);
}

// 处理键盘事件
int menu_component_handle_key_event(Layer* layer, KeyEvent* event) {
    MenuComponent* component = (MenuComponent*)layer->component;
    if (!component || component->item_count == 0) {
        return 0;
    }

    if (event->type == KEY_EVENT_DOWN) {
        switch (event->data.key.key_code) {
            case 38: // 上箭头
                if (!component->expanded) break;
                if (component->hovered_item <= 0) {
                    component->hovered_item = component->item_count - 1;
                } else {
                    component->hovered_item--;
                }
                // 跳过分隔符（最多遍历整个数组防止死循环）
                for (int _i = 0; _i < component->item_count; _i++) {
                    if (component->hovered_item < 0 ||
                        !component->items[component->hovered_item].separator) break;
                    if (component->hovered_item <= 0) {
                        component->hovered_item = component->item_count - 1;
                    } else {
                        component->hovered_item--;
                    }
                }
                break;

            case 40: // 下箭头
                if (!component->expanded) break;
                if (component->hovered_item < 0 || component->hovered_item >= component->item_count - 1) {
                    component->hovered_item = 0;
                } else {
                    component->hovered_item++;
                }
                // 跳过分隔符（最多遍历整个数组防止死循环）
                for (int _i = 0; _i < component->item_count; _i++) {
                    if (component->hovered_item >= component->item_count ||
                        !component->items[component->hovered_item].separator) break;
                    if (component->hovered_item >= component->item_count - 1) {
                        component->hovered_item = 0;
                    } else {
                        component->hovered_item++;
                    }
                }
                break;

            case 39: // 右箭头 — 展开子菜单
                if (!component->expanded || component->hovered_item < 0) break;
                if (component->items[component->hovered_item].submenu) {
                    menu_item_click(component, &component->items[component->hovered_item]);
                }
                break;

            case 37: // 左箭头 — 关闭子菜单返回父菜单
                if (component->parent_menu && component->is_popup) {
                    menu_component_hide_popup(component);
                    if (component->parent_menu->expanded) {
                        component->parent_menu->opened_item = -1;
                    }
                } else if (component->opened_item >= 0) {
                    MenuItem* sub_item = &component->items[component->opened_item];
                    if (sub_item->submenu && sub_item->submenu->is_popup) {
                        menu_component_hide_popup(sub_item->submenu);
                    }
                    component->opened_item = -1;
                }
                break;

            case 13: // 回车键
            case 32: // 空格键
                if (!component->expanded) break;
                if (component->hovered_item >= 0 && component->hovered_item < component->item_count) {
                    menu_item_click(component, &component->items[component->hovered_item]);
                }
                break;

            case 27: // ESC键
                if (component->is_popup && component->parent_menu) {
                    // 子菜单ESC返回父菜单
                    menu_component_hide_popup(component);
                    if (component->parent_menu->expanded) {
                        component->parent_menu->opened_item = -1;
                    }
                } else if (component->expanded) {
                    menu_component_collapse(component);
                }
                break;
        }
    }
    return 0;
}

// 内联展开菜单关闭回调
static void menu_inline_close_callback(PopupLayer* popup) {
    if (!popup || !popup->layer) return;
    MenuComponent* component = (MenuComponent*)popup->layer->component;
    if (component) {
        component->expanded = 0;
        component->hovered_item = -1;
        menu_update_hit_rect(component);
    }
}

// 收起展开的内联菜单
void menu_component_collapse(MenuComponent* component) {
    if (!component || !component->expanded) return;

    // 先设状态再清理，防止重入问题
    component->expanded = 0;
    component->hovered_item = -1;

    if (component->opened_item >= 0 && component->items &&
        component->opened_item < component->item_count) {
        MenuItem* item = &component->items[component->opened_item];
        if (item->submenu && item->submenu->is_popup) {
            menu_component_hide_popup(item->submenu);
        }
        component->opened_item = -1;
    }

    // 从弹出层管理器移除（如果已注册）
    if (component->layer) {
        popup_manager_remove(component->layer);
    }

    menu_update_hit_rect(component);
}

// 处理鼠标事件
int menu_component_handle_pointer_event(Layer* layer, PointerEvent* event) {
    MenuComponent* component = (MenuComponent*)layer->component;
    if (!component) {
        return 0;
    }

    Rect* rect = &layer->rect;
    int is_popup_style = (layer->text == NULL || layer->text[0] == '\0');

    if (event->phase == POINTER_MOVE) {
        if (component->expanded || is_popup_style) {
            int prev_hovered = component->hovered_item;
            component->hovered_item = menu_component_get_item_at_position(component, event->x, event->y);

            // 子菜单 hover 展开/切换
            if (component->hovered_item >= 0 && component->hovered_item != prev_hovered) {
                MenuItem* item = &component->items[component->hovered_item];
                if (item->submenu) {
                    // 关闭之前打开的不同子菜单
                    if (component->opened_item >= 0 && component->opened_item != component->hovered_item) {
                        MenuItem* prev = &component->items[component->opened_item];
                        if (prev->submenu && prev->submenu->is_popup) {
                            menu_component_hide_popup(prev->submenu);
                        }
                    }
                    // 展开新子菜单
                    Rect* mrect = component->popup_layer ? &component->popup_layer->rect : &component->layer->rect;
                    int y_off = (component->layer->text && strlen(component->layer->text) > 0) ? component->item_height : 0;
                    int sub_x = mrect->x + mrect->w;
                    int sub_y = mrect->y + y_off + component->hovered_item * component->item_height;
                    menu_component_show_popup(item->submenu, sub_x, sub_y);
                    component->opened_item = component->hovered_item;
                } else if (component->opened_item >= 0) {
                    // hover移到没有子菜单的项，关闭已打开的子菜单
                    MenuItem* prev = &component->items[component->opened_item];
                    if (prev->submenu && prev->submenu->is_popup) {
                        menu_component_hide_popup(prev->submenu);
                    }
                    component->opened_item = -1;
                }
            }
        } else {
            component->hovered_item = -1;
        }

    } else if (event->phase == POINTER_DOWN && event->button == SDL_BUTTON_LEFT) {
        int y_offset = (layer->text && strlen(layer->text) > 0) ? component->item_height : 0;
        int relative_y = event->y - rect->y;

        if (component->expanded || is_popup_style) {
            int clicked_item = menu_component_get_item_at_position(component, event->x, event->y);
            if (clicked_item >= 0 && clicked_item < component->item_count) {
                menu_item_click(component, &component->items[clicked_item]);
            } else {
                // 点击空白区域或标题收起
                menu_component_collapse(component);
                // 同时关闭打开的子菜单
                if (component->opened_item >= 0) {
                    MenuItem* prev = &component->items[component->opened_item];
                    if (prev->submenu && prev->submenu->is_popup) {
                        menu_component_hide_popup(prev->submenu);
                    }
                    component->opened_item = -1;
                }
            }
        } else {
            // 折叠状态：点击标题区域展开（必须先确认点击在菜单矩形内）
            if (relative_y < y_offset &&
                event->x >= rect->x && event->x < rect->x + rect->w &&
                event->y >= rect->y) {
                component->expanded = 1;
                component->hovered_item = -1;
                menu_update_hit_rect(component);

                PopupLayer* popup = popup_layer_create(layer, POPUP_TYPE_MENU, 100);
                if (popup) {
                    popup->auto_close = true;
                    popup->close_callback = menu_inline_close_callback;
                    popup_manager_add(popup);
                }
            }
        }
    }
    return 0;
}

// 渲染菜单组件
void menu_component_render(Layer* layer) {
    if (!layer || !layer->component) {
        return;
    }

    MenuComponent* component = (MenuComponent*)layer->component;
    Rect* rect = &layer->rect;

    // 折叠时只绘制标题栏区域，展开时绘制完整菜单
    // 弹出式菜单（无标题文字）直接绘制完整菜单
    int title_h = (layer->text && strlen(layer->text) > 0) ? component->item_height : 0;
    int is_popup_style = (title_h == 0);
    Rect bg_rect = (component->expanded || is_popup_style) ? *rect :
                   (Rect){rect->x, rect->y, rect->w, title_h > 0 ? title_h + 1 : 0};

    // 绘制背景
    if (component->bg_color.a > 0 && bg_rect.h > 0) {
        if (layer->radius > 0 && component->expanded) {
            backend_render_rounded_rect(&bg_rect, component->bg_color, layer->radius);
        } else {
            backend_render_fill_rect(&bg_rect, component->bg_color);
        }
    }

    int y_offset = 0;

    // 绘制标题文本
    if (layer->text && strlen(layer->text) > 0) {
        Color title_color = component->text_color;
        
        // 绘制标题背景（悬停或展开时高亮）
        Rect title_bar = {rect->x, rect->y, rect->w, component->item_height};
        if (component->expanded) {
            Color highlight = component->hover_color;
            backend_render_fill_rect(&title_bar, highlight);
        }
        
        // 绘制标题文字
        Texture* title_texture = render_text(layer, layer->text, title_color);
        if (title_texture) {
            int title_width, title_height;
            backend_query_texture(title_texture, NULL, NULL, &title_width, &title_height);
            
            Rect title_rect = {
                rect->x + 12,
                rect->y + (component->item_height - title_height / yui_density) / 2,
                title_width / yui_density,
                title_height / yui_density
            };
            
            backend_render_text_copy(title_texture, NULL, &title_rect);
            backend_render_text_destroy(title_texture);
        }
        
        // 绘制下拉箭头 ▼
        if (component->show_arrow) {
            const char* arrow = component->expanded ? "▲" : "▼";
            Texture* arrow_texture = render_text(layer, arrow, title_color);
            if (arrow_texture) {
                int arrow_width, arrow_height;
                backend_query_texture(arrow_texture, NULL, NULL, &arrow_width, &arrow_height);
                Rect arrow_rect = {
                    rect->x + rect->w - arrow_width / yui_density - 12,
                    rect->y + (component->item_height - arrow_height / yui_density) / 2,
                    arrow_width / yui_density,
                    arrow_height / yui_density
                };
                backend_render_text_copy(arrow_texture, NULL, &arrow_rect);
                backend_render_text_destroy(arrow_texture);
            }
        }
        
        // 标题分隔线（仅展开时显示）
        if (component->expanded) {
            int line_y = rect->y + component->item_height;
            backend_render_line(rect->x + 5, line_y, rect->x + rect->w - 5, line_y, component->separator_color);
        }

        y_offset = component->item_height;
    }

    // 展开或弹出式菜单时绘制菜单项
    if (component->expanded || is_popup_style) {
        // 绘制菜单项
        for (int i = 0; i < component->item_count; i++) {
            MenuItem* item = &component->items[i];
            int item_y = rect->y + y_offset + i * component->item_height;
            Rect item_rect = {rect->x, item_y, rect->w, component->item_height};
            
            if (item->separator) {
                // 绘制分隔符
                int separator_y = item_y + component->item_height / 2;
                backend_render_line(rect->x + 5, separator_y, rect->x + rect->w - 5, separator_y, 
                                   component->separator_color);
            } else {
                // 绘制菜单项背景（悬停且启用时高亮）
                if (i == component->hovered_item && item->enabled) {
                    Color hover_bg = component->hover_color;
                    if (layer->radius > 0) {
                        backend_render_rounded_rect(&item_rect, hover_bg, 0);
                    } else {
                        backend_render_fill_rect(&item_rect, hover_bg);
                    }
                }
                
                // 确定文本颜色
                Color text_color = item->enabled ? component->text_color : component->disabled_color;
                
                // 绘制复选框（如果有的话）
                if (item->checked) {
                    Rect check_rect = {rect->x + 8, item_y + component->item_height / 2 - 6, 12, 12};
                    backend_render_rect(&check_rect, text_color);
                    backend_render_line(check_rect.x + 2, check_rect.y + 6, 
                                      check_rect.x + 5, check_rect.y + 9, text_color);
                    backend_render_line(check_rect.x + 5, check_rect.y + 9, 
                                      check_rect.x + 10, check_rect.y + 4, text_color);
                }
                
                // 绘制菜单项文本
                int text_x = rect->x + (item->checked ? 28 : 12);
                int right_padding = item->submenu ? 24 : 12;
                Texture* text_texture = render_text(layer, item->text, text_color);
                if (text_texture) {
                    int text_width, text_height;
                    backend_query_texture(text_texture, NULL, NULL, &text_width, &text_height);
                    
                    Rect text_rect = {
                        text_x,
                        item_y + (component->item_height - text_height / yui_density) / 2,
                        text_width / yui_density,
                        text_height / yui_density
                    };
                    
                    backend_render_text_copy(text_texture, NULL, &text_rect);
                    backend_render_text_destroy(text_texture);
                }
                
                // 绘制子菜单箭头 ▶
                if (item->submenu) {
                    Texture* sub_arrow = render_text(layer, "▶", text_color);
                    if (sub_arrow) {
                        int aw, ah;
                        backend_query_texture(sub_arrow, NULL, NULL, &aw, &ah);
                        Rect sa_rect = {
                            rect->x + rect->w - aw / yui_density - 8,
                            item_y + (component->item_height - ah / yui_density) / 2,
                            aw / yui_density,
                            ah / yui_density
                        };
                        backend_render_text_copy(sub_arrow, NULL, &sa_rect);
                        backend_render_text_destroy(sub_arrow);
                    }
                }
                
                // 绘制快捷键文本
                if (strlen(item->shortcut) > 0) {
                    Texture* shortcut_texture = render_text(layer, item->shortcut, text_color);
                    if (shortcut_texture) {
                        int shortcut_width, shortcut_height;
                        backend_query_texture(shortcut_texture, NULL, NULL, &shortcut_width, &shortcut_height);
                        
                        int shortcut_x = rect->x + rect->w - shortcut_width / yui_density - right_padding;
                        Rect shortcut_rect = {
                            shortcut_x,
                            item_y + (component->item_height - shortcut_height / yui_density) / 2,
                            shortcut_width / yui_density,
                            shortcut_height / yui_density
                        };
                        
                        backend_render_text_copy(shortcut_texture, NULL, &shortcut_rect);
                        backend_render_text_destroy(shortcut_texture);
                    }
                }
            }
        }
    }
}

// 弹出菜单关闭回调
static void menu_popup_close_callback(PopupLayer* popup) {
    if (!popup) return;
    
    // 先保存layer指针，因为在popup_manager_remove中，popup会被释放
    Layer* layer = popup->layer;
    if (!layer) return;
    
    MenuComponent* component = (MenuComponent*)layer->component;
    if (component) {
        component->is_popup = 0;
        // 注意：popup_layer 会在 popup_manager 中被释放，不要在这里再次设置为NULL
        // component->popup_layer = NULL;
        
        // 调用用户自定义的关闭回调
        if (component->on_popup_closed) {
            component->on_popup_closed(component);
        }
    }
}

// 显示弹出菜单
bool menu_component_show_popup(MenuComponent* component, int x, int y) {
    if (!component || component->is_popup) {
        return false;
    }
    
    // 创建弹出菜单图层
    component->popup_layer = (Layer*)malloc(sizeof(Layer));
    if (!component->popup_layer) {
        return false;
    }
    
    memset(component->popup_layer, 0, sizeof(Layer));
    
    // 计算菜单大小（优先使用自动计算宽度）
    int menu_width = component->content_width > 0 ? component->content_width : component->min_width;
    int menu_height = component->item_count * component->item_height;
    
    // 设置弹出菜单的位置和大小
    component->popup_layer->rect.x = x;
    component->popup_layer->rect.y = y;
    component->popup_layer->rect.w = menu_width;
    component->popup_layer->rect.h = menu_height;
    
    // 复制基本属性
    component->popup_layer->radius = 4;  // 默认圆角
    component->popup_layer->component = component;
    component->popup_layer->render = menu_component_render;
    component->popup_layer->handle_pointer_event = menu_component_handle_pointer_event;
    component->popup_layer->handle_key_event = menu_component_handle_key_event;
    
    // 创建弹出层并添加到弹出管理器
    PopupLayer* popup = popup_layer_create(component->popup_layer, POPUP_TYPE_MENU, 100);
    if (!popup) {
        free(component->popup_layer);
        component->popup_layer = NULL;
        return false;
    }
    
    // 设置关闭回调
    popup->close_callback = menu_popup_close_callback;
    popup->auto_close = true;
    
    // 添加到弹出管理器
    if (!popup_manager_add(popup)) {
        free(component->popup_layer);
        component->popup_layer = NULL;
        // Since popup_manager_add failed, popup is not in the list, so we can safely free it
        free(popup);
        return false;
    }
    
    component->is_popup = 1;
    component->hovered_item = -1;
    
    return true;
}

// 隐藏弹出菜单
void menu_component_hide_popup(MenuComponent* component) {
    if (!component || !component->is_popup || !component->popup_layer) {
        return;
    }
    
    // 保存指针以便调用 popup_manager_remove
    Layer* popup_layer = component->popup_layer;
    component->popup_layer = NULL;  // 先设置为NULL，防止回调中再次使用
    
    // 从弹出管理器中移除
    popup_manager_remove(popup_layer);
    
    component->is_popup = 0;
}

// 检查弹出菜单是否打开
bool menu_component_is_popup_opened(MenuComponent* component) {
    return component ? component->is_popup : false;
}

// 设置弹出菜单关闭回调
void menu_component_set_popup_closed_callback(MenuComponent* component, void (*callback)(MenuComponent* menu)) {
    if (component) {
        component->on_popup_closed = callback;
    }
}
#include "treeview_component.h"
#include "../backend.h"
#include "../event.h"
#include "../util.h"
#include <stdlib.h>
#include <string.h>


// 创建树视图组件
TreeViewComponent* treeview_component_create(Layer* layer) {
    TreeViewComponent* component = (TreeViewComponent*)malloc(sizeof(TreeViewComponent));
    if (!component) return NULL;
    
    component->layer = layer;
    component->root_nodes = NULL;
    component->root_count = 0;
    component->selected_node = NULL;
    component->item_height = 24;
    component->indent_width = 20;
    component->font_size = 14; // 默认字体大小
    component->text_color = (Color){205, 214, 244, 255};  // Catppuccin 文本色
    component->selected_text_color = (Color){205, 214, 244, 255};
    component->selected_bg_color = (Color){69, 71, 90, 255};    // Catppuccin surface1
    component->hover_bg_color = (Color){49, 50, 68, 255};       // Catppuccin surface0
    component->expand_icon_color = (Color){108, 112, 134, 255}; // Catppuccin overlay0
    component->user_data = NULL;
    component->on_node_selected = NULL;
    component->on_node_expanded = NULL;
    component->icon_size = 0;
    component->expand_icon = strdup("▶");
    component->collapse_icon = strdup("▼");
    component->expand_icon_path = NULL;
    component->collapse_icon_path = NULL;
    component->on_select_name = NULL;
    component->on_select_handler = NULL;
    component->on_expand_name = NULL;
    component->on_expand_handler = NULL;
    
    // 设置组件
    layer->component = component;
    layer->render = treeview_component_render;
    layer->handle_mouse_event = treeview_component_handle_mouse_event;
    layer->handle_key_event = treeview_component_handle_key_event;
    
    return component;
}

// 销毁树视图组件
void treeview_component_destroy(TreeViewComponent* component) {
    if (!component) return;
    
    // 销毁所有根节点及其子节点
    if (component->root_nodes) {
        for (int i = 0; i < component->root_count; i++) {
            treeview_destroy_node(component->root_nodes[i]);
        }
        free(component->root_nodes);
    }

    if (component->expand_icon) free(component->expand_icon);
    if (component->collapse_icon) free(component->collapse_icon);
    if (component->expand_icon_path) free(component->expand_icon_path);
    if (component->collapse_icon_path) free(component->collapse_icon_path);
    if (component->on_select_name) free(component->on_select_name);
    if (component->on_expand_name) free(component->on_expand_name);

    free(component);
}

// 创建节点
TreeNode* treeview_create_node(const char* text) {
    TreeNode* node = (TreeNode*)malloc(sizeof(TreeNode));
    if (!node) return NULL;
    
    node->text = text ? strdup(text) : NULL;
    node->children = NULL;
    node->child_count = 0;
    node->expanded = 0;
    node->selected = 0;
    node->level = 0;
    node->expandable = 0;
    node->user_data = NULL;
    node->parent = NULL;
    node->expand_icon = NULL;
    node->collapse_icon = NULL;
    node->expand_icon_path = NULL;
    node->collapse_icon_path = NULL;
    node->expand_icon_tex = NULL;
    node->collapse_icon_tex = NULL;
    node->icon = NULL;
    node->icon_text = NULL;
    node->icon_tex = NULL;
    
    return node;
}

// 销毁节点及其子节点
void treeview_destroy_node(TreeNode* node) {
    if (!node) return;
    
    // 递归销毁所有子节点
    if (node->children) {
        for (int i = 0; i < node->child_count; i++) {
            treeview_destroy_node(node->children[i]);
        }
        free(node->children);
    }
    
    // 释放文本
    if (node->text) {
        free(node->text);
    }
    if (node->expand_icon) free(node->expand_icon);
    if (node->collapse_icon) free(node->collapse_icon);
    if (node->expand_icon_path) free(node->expand_icon_path);
    if (node->collapse_icon_path) free(node->collapse_icon_path);
    if (node->expand_icon_tex) backend_render_text_destroy(node->expand_icon_tex);
    if (node->collapse_icon_tex) backend_render_text_destroy(node->collapse_icon_tex);
    if (node->icon_tex) backend_render_text_destroy(node->icon_tex);
    if (node->icon) free(node->icon);
    if (node->icon_text) free(node->icon_text);
    
    free(node);
}

// 添加根节点
int treeview_add_root_node(TreeViewComponent* component, TreeNode* node) {
    if (!component || !node) return -1;
    
    // 扩展根节点数组
    TreeNode** new_nodes = (TreeNode**)realloc(component->root_nodes, (component->root_count + 1) * sizeof(TreeNode*));
    if (!new_nodes) return -1;
    
    component->root_nodes = new_nodes;
    node->level = 0;
    node->parent = NULL;
    component->root_nodes[component->root_count++] = node;
    
    return component->root_count - 1;
}

// 添加子节点
int treeview_add_child_node(TreeNode* parent, TreeNode* child) {
    if (!parent || !child) return -1;
    
    // 扩展子节点数组
    TreeNode** new_children = (TreeNode**)realloc(parent->children, (parent->child_count + 1) * sizeof(TreeNode*));
    if (!new_children) return -1;
    
    parent->children = new_children;
    child->level = parent->level + 1;
    child->parent = parent;
    parent->children[parent->child_count++] = child;
    
    return parent->child_count - 1;
}

// 移除节点
void treeview_remove_node(TreeViewComponent* component, TreeNode* node) {
    if (!component || !node) return;
    
    // 如果是根节点
    if (node->parent == NULL) {
        // 查找在根节点数组中的位置
        for (int i = 0; i < component->root_count; i++) {
            if (component->root_nodes[i] == node) {
                // 移动后面的根节点
                for (int j = i; j < component->root_count - 1; j++) {
                    component->root_nodes[j] = component->root_nodes[j + 1];
                }
                
                // 缩小根节点数组
                component->root_count--;
                if (component->root_count > 0) {
                    TreeNode** new_nodes = (TreeNode**)realloc(component->root_nodes, component->root_count * sizeof(TreeNode*));
                    if (new_nodes) {
                        component->root_nodes = new_nodes;
                    }
                } else {
                    free(component->root_nodes);
                    component->root_nodes = NULL;
                }
                
                break;
            }
        }
    } else {
        // 如果是子节点
        TreeNode* parent = node->parent;
        
        // 查找在父节点子节点数组中的位置
        for (int i = 0; i < parent->child_count; i++) {
            if (parent->children[i] == node) {
                // 移动后面的子节点
                for (int j = i; j < parent->child_count - 1; j++) {
                    parent->children[j] = parent->children[j + 1];
                }
                
                // 缩小子节点数组
                parent->child_count--;
                if (parent->child_count > 0) {
                    TreeNode** new_children = (TreeNode**)realloc(parent->children, parent->child_count * sizeof(TreeNode*));
                    if (new_children) {
                        parent->children = new_children;
                    }
                } else {
                    free(parent->children);
                    parent->children = NULL;
                }
                
                break;
            }
        }
    }
    
    // 如果是选中的节点，清除选中状态
    if (component->selected_node == node) {
        component->selected_node = NULL;
    }
    
    // 销毁节点及其子节点
    treeview_destroy_node(node);
}

// 设置节点文本
void treeview_set_node_text(TreeNode* node, const char* text) {
    if (!node) return;
    
    if (node->text) {
        free(node->text);
    }
    node->text = text ? strdup(text) : NULL;
}

// 获取节点文本
const char* treeview_get_node_text(TreeNode* node) {
    if (!node) return NULL;
    return node->text;
}

// 展开节点
void treeview_expand_node(TreeNode* node) {
    if (!node || (node->child_count == 0 && !node->expandable)) return;
    node->expanded = 1;
}

// 折叠节点
void treeview_collapse_node(TreeNode* node) {
    if (!node) return;
    node->expanded = 0;
}

// 切换节点展开状态
void treeview_toggle_node(TreeNode* node) {
    if (!node || (node->child_count == 0 && !node->expandable)) return;
    node->expanded = !node->expanded;
}

// 检查节点是否展开
int treeview_is_node_expanded(TreeNode* node) {
    if (!node) return 0;
    return node->expanded;
}

// 设置选中节点
void treeview_set_selected_node(TreeViewComponent* component, TreeNode* node) {
    if (!component) return;
    
    // 清除之前选中的节点
    if (component->selected_node) {
        component->selected_node->selected = 0;
    }
    
    // 设置新的选中节点
    component->selected_node = node;
    if (node) {
        node->selected = 1;
        
        // 调用回调函数
        if (component->on_node_selected) {
            component->on_node_selected(node, component->user_data);
        }
    }
}

// 获取选中节点
TreeNode* treeview_get_selected_node(TreeViewComponent* component) {
    if (!component) return NULL;
    return component->selected_node;
}

// 获取节点的父节点
TreeNode* treeview_get_node_parent(TreeNode* node) {
    if (!node) return NULL;
    return node->parent;
}

// 获取节点的子节点数量
int treeview_get_child_count(TreeNode* node) {
    if (!node) return 0;
    return node->child_count;
}

// 获取节点的子节点
TreeNode* treeview_get_child(TreeNode* node, int index) {
    if (!node || index < 0 || index >= node->child_count) return NULL;
    return node->children[index];
}

// 设置项目高度
void treeview_set_item_height(TreeViewComponent* component, int height) {
    if (!component || height <= 0) return;
    component->item_height = height;
}

// 设置缩进宽度
void treeview_set_indent_width(TreeViewComponent* component, int width) {
    if (!component || width <= 0) return;
    component->indent_width = width;
}

// 设置字体大小
void treeview_set_font_size(TreeViewComponent* component, int size) {
    if (!component || size <= 0) return;
    component->font_size = size;
}


static void treeview_data_update(Layer* layer, cJSON* data) {
    if (!layer || !layer->component) return;
    TreeViewComponent* component = (TreeViewComponent*)layer->component;
    treeview_clear_all_root_nodes(component);
    if (cJSON_IsArray(data)) {
        int count = cJSON_GetArraySize(data);
        for (int i = 0; i < count; i++) {
            TreeNode* node = parse_tree_node(cJSON_GetArrayItem(data, i), 0, NULL);
            if (node) treeview_add_root_node(component, node);
        }
    }
}


TreeViewComponent* treeview_component_create_from_json(Layer* layer, cJSON* json_obj) {
    // 创建基础组件
    TreeViewComponent* component = treeview_component_create(layer);
    
    // 设置渲染函数和事件处理函数
    layer->render = treeview_component_render;
    layer->handle_mouse_event = treeview_component_handle_mouse_event;
    layer->handle_key_event = treeview_component_handle_key_event;
    
    // 注册数据更新回调
    layer->on_data_update = treeview_data_update;
    
    // 解析项目高度
    cJSON* itemHeight = cJSON_GetObjectItem(json_obj, "itemHeight");
    if (itemHeight) {
        treeview_set_item_height(component, itemHeight->valueint);
    }
    
    // 解析缩进宽度
    cJSON* indentWidth = cJSON_GetObjectItem(json_obj, "indentWidth");
    if (indentWidth) {
        treeview_set_indent_width(component, indentWidth->valueint);
    }

    // 解析图标大小
    cJSON* iconSize = cJSON_GetObjectItem(json_obj, "iconSize");
    if (iconSize) {
        component->icon_size = iconSize->valueint;
    }

    // 解析颜色
    cJSON* style = cJSON_GetObjectItem(json_obj, "style");
    if (style) {
        Color text_color = {205, 214, 244, 255}; // 默认 Catppuccin 文本色
        Color selected_text_color = {205, 214, 244, 255};
        Color selected_bg_color = {69, 71, 90, 255};    // Catppuccin surface1
        Color hover_bg_color = {49, 50, 68, 255};       // Catppuccin surface0
        Color expand_icon_color = {108, 112, 134, 255}; // Catppuccin overlay0
        
        cJSON* color = cJSON_GetObjectItem(style, "textColor");
        if (color && color->valuestring) {
            parse_color(color->valuestring, &text_color);
        }
        
        color = cJSON_GetObjectItem(style, "selectedTextColor");
        if (color && color->valuestring) {
            parse_color(color->valuestring, &selected_text_color);
        }
        
        color = cJSON_GetObjectItem(style, "selectedBgColor");
        if (color && color->valuestring) {
            parse_color(color->valuestring, &selected_bg_color);
        }
        
        color = cJSON_GetObjectItem(style, "hoverBgColor");
        if (color && color->valuestring) {
            parse_color(color->valuestring, &hover_bg_color);
        }
        
        color = cJSON_GetObjectItem(style, "expandIconColor");
        if (color && color->valuestring) {
            parse_color(color->valuestring, &expand_icon_color);
        }
        
        treeview_set_colors(component, text_color, selected_text_color, selected_bg_color, hover_bg_color, expand_icon_color);
    }

    // 解析事件名（handler 在触发时查找，因为此时事件尚未注册）
    cJSON* events = cJSON_GetObjectItem(json_obj, "events");
    if (events) {
        cJSON* onSelect = cJSON_GetObjectItem(events, "onSelect");
        if (onSelect && onSelect->valuestring) {
            const char* val = onSelect->valuestring;
            if (val[0] == '@') val++;
            component->on_select_name = strdup(val);
        }
        cJSON* onExpand = cJSON_GetObjectItem(events, "onExpand");
        if (onExpand && onExpand->valuestring) {
            const char* val = onExpand->valuestring;
            if (val[0] == '@') val++;
            component->on_expand_name = strdup(val);
        }
    }

    // 解析节点
    cJSON* nodes = cJSON_GetObjectItem(json_obj, "nodes");
    if (nodes && cJSON_IsArray(nodes)) {
        int node_count = cJSON_GetArraySize(nodes);
        
        for (int i = 0; i < node_count; i++) {
            cJSON* node_json = cJSON_GetArrayItem(nodes, i);
            TreeNode* node = parse_tree_node(node_json, 0, NULL);
            if (node) {
                treeview_add_root_node(component, node);
            }
        }
    }
    
    return component;
}

// 递归解析树节点
TreeNode* parse_tree_node(cJSON* node_json, int level, TreeNode* parent) {
    cJSON* text = cJSON_GetObjectItem(node_json, "text");
    if (!text) return NULL;
    
    TreeNode* node = treeview_create_node(text->valuestring);
    node->level = level;
    node->parent = parent;
    
    // 解析expanded属性
    cJSON* expanded = cJSON_GetObjectItem(node_json, "expanded");
    if (expanded && expanded->type == cJSON_True) {
        node->expanded = 1;
    }

    // 解析自定义展开/折叠图标
    cJSON* expandIcon = cJSON_GetObjectItem(node_json, "expandIcon");
    if (expandIcon && expandIcon->valuestring) {
        node->expand_icon = strdup(expandIcon->valuestring);
    }
    cJSON* collapseIcon = cJSON_GetObjectItem(node_json, "collapseIcon");
    if (collapseIcon && collapseIcon->valuestring) {
        node->collapse_icon = strdup(collapseIcon->valuestring);
    }
    cJSON* icon = cJSON_GetObjectItem(node_json, "icon");
    if (icon && icon->valuestring) {
        node->icon = strdup(icon->valuestring);
    }
    cJSON* iconText = cJSON_GetObjectItem(node_json, "icon_text");
    if (iconText && iconText->valuestring) {
        node->icon_text = strdup(iconText->valuestring);
    }
    
    // 解析children属性
    cJSON* children = cJSON_GetObjectItem(node_json, "children");
    if (children && cJSON_IsArray(children)) {
        node->expandable = 1;
        int child_count = cJSON_GetArraySize(children);
        
        for (int i = 0; i < child_count; i++) {
            cJSON* child_json = cJSON_GetArrayItem(children, i);
            TreeNode* child_node = parse_tree_node(child_json, level + 1, node);
            if (child_node) {
                treeview_add_child_node(node, child_node);
            }
        }
    }
    
    return node;
}

// 设置颜色
void treeview_set_colors(TreeViewComponent* component, Color text, Color selected_text, Color selected_bg, Color hover_bg, Color expand_icon) {
    if (!component) return;
    component->text_color = text;
    component->selected_text_color = selected_text;
    component->selected_bg_color = selected_bg;
    component->hover_bg_color = hover_bg;
    component->expand_icon_color = expand_icon;
}

// 设置用户数据
void treeview_set_user_data(TreeViewComponent* component, void* data) {
    if (!component) return;
    component->user_data = data;
}

// 清空所有根节点
void treeview_clear_all_root_nodes(TreeViewComponent* component) {
    if (!component) return;
    if (component->root_nodes) {
        for (int i = 0; i < component->root_count; i++) {
            treeview_destroy_node(component->root_nodes[i]);
        }
        free(component->root_nodes);
        component->root_nodes = NULL;
        component->root_count = 0;
    }
}

// 设置节点选中回调
void treeview_set_node_selected_callback(TreeViewComponent* component, void (*callback)(TreeNode*, void*)) {
    if (!component) return;
    component->on_node_selected = callback;
}

// 设置节点展开回调
void treeview_set_node_expanded_callback(TreeViewComponent* component, void (*callback)(TreeNode*, int, void*)) {
    if (!component) return;
    component->on_node_expanded = callback;
}

// 计算可见节点数量
int treeview_count_visible_nodes(TreeViewComponent* component) {
    if (!component || component->root_count == 0) return 0;
    
    int count = 0;
    
    // 递归计算可见节点
    for (int i = 0; i < component->root_count; i++) {
        TreeNode* node = component->root_nodes[i];
        count++;  // 计算当前节点
        
        // 如果节点展开，递归计算子节点
        if (node->expanded && node->child_count > 0) {
            // 使用栈来模拟递归
TreeNode** stack = (TreeNode**)malloc(sizeof(TreeNode*) * node->child_count);
            int stack_top = 0;
            
            // 将子节点压入栈
            for (int j = node->child_count - 1; j >= 0; j--) {
                stack[stack_top++] = node->children[j];
            }
            
            // 处理栈中的节点
            while (stack_top > 0) {
                TreeNode* current = stack[--stack_top];
                count++;  // 计算当前节点
                
                // 如果节点展开，将子节点压入栈
                if (current->expanded && current->child_count > 0) {
                    TreeNode** new_stack = (TreeNode**)realloc(stack, sizeof(TreeNode*) * (stack_top + current->child_count));
                    if (new_stack) {
                        stack = new_stack;
                        for (int j = current->child_count - 1; j >= 0; j--) {
                            stack[stack_top++] = current->children[j];
                        }
                    }
                }
            }
            
            free(stack);
        }
    }
    
    return count;
}

// 计算内容总高度
int treeview_calculate_content_height(TreeViewComponent* component) {
    if (!component) return 0;
    
    int visible_count = treeview_count_visible_nodes(component);
    return visible_count * component->item_height;
}

// 更新滚动条状态
void treeview_update_scrollbar(TreeViewComponent* component) {
    if (!component || !component->layer) return;
    
    Layer* layer = component->layer;
    
    // 计算内容高度
    int content_height = treeview_calculate_content_height(component);
    layer->content_height = content_height;
    
    int visible_height = layer->rect.h;
    
    // printf("DEBUG: treeview_update_scrollbar - content_height=%d, visible_height=%d, scrollable=%d\n", 
    //        content_height, visible_height, layer->scrollable);
    
    // // 更新滚动条可见性和位置
    // if ((layer->scrollable == 1 || layer->scrollable == 3) && layer->scrollbar_v) {
    //     layer->scrollbar_v->visible = (content_height > visible_height);
    //     printf("DEBUG: scrollbar_v visible set to %d\n", layer->scrollbar_v->visible);
    // }
}

// 滚动到指定节点
void treeview_scroll_to_node(TreeViewComponent* component, TreeNode* target_node) {
    if (!component || !target_node || !component->layer) return;
    
    Layer* layer = component->layer;
    
    // 计算目标节点在可见节点中的位置
    int node_index = 0;
    int found = 0;
    
    // 搜索根节点
    for (int i = 0; i < component->root_count && !found; i++) {
        TreeNode* node = component->root_nodes[i];
        if (node == target_node) {
            found = 1;
            break;
        }
        node_index++;
        
        // 搜索展开的子节点
        if (node->expanded && node->child_count > 0) {
            TreeNode** stack = (TreeNode**)malloc(sizeof(TreeNode*) * node->child_count);
            int stack_top = 0;
            
            for (int j = node->child_count - 1; j >= 0; j--) {
                stack[stack_top++] = node->children[j];
            }
            
            while (stack_top > 0 && !found) {
                TreeNode* current = stack[--stack_top];
                if (current == target_node) {
                    found = 1;
                    break;
                }
                node_index++;
                
                if (current->expanded && current->child_count > 0) {
                    TreeNode** new_stack = (TreeNode**)realloc(stack, sizeof(TreeNode*) * (stack_top + current->child_count));
                    if (new_stack) {
                        stack = new_stack;
                        for (int j = current->child_count - 1; j >= 0; j--) {
                            stack[stack_top++] = current->children[j];
                        }
                    }
                }
            }
            
            free(stack);
        }
    }
    
    if (found) {
        // 计算节点在屏幕上的Y坐标
        int node_y = layer->rect.y + node_index * component->item_height - layer->scroll_offset;
        
        // 检查节点是否完全可见
        int visible_top = layer->rect.y;
        int visible_bottom = layer->rect.y + layer->rect.h;
        int node_top = node_y;
        int node_bottom = node_y + component->item_height;
        
        // 如果节点不可见，滚动到使其可见
        if (node_top < visible_top) {
            // 节点在可视区域上方，向上滚动
            layer->scroll_offset += (visible_top - node_top);
            if (layer->scroll_offset < 0) layer->scroll_offset = 0;
        } else if (node_bottom > visible_bottom) {
            // 节点在可视区域下方，向下滚动
            layer->scroll_offset += (node_bottom - visible_bottom);
            
            // 确保不超过最大滚动偏移
            int content_height = treeview_calculate_content_height(component);
            int max_offset = content_height - layer->rect.h;
            if (max_offset < 0) max_offset = 0;
            if (layer->scroll_offset > max_offset) layer->scroll_offset = max_offset;
        }
        
        // 更新滚动条状态
        treeview_update_scrollbar(component);
    }
}

// 根据位置获取节点
TreeNode* treeview_get_node_from_position(TreeViewComponent* component, int x, int y) {
    if (!component || component->root_count == 0) return NULL;
    
    // 检查是否在组件区域内
    if (x < component->layer->rect.x || x >= component->layer->rect.x + component->layer->rect.w ||
        y < component->layer->rect.y || y >= component->layer->rect.y + component->layer->rect.h) {
        return NULL;
    }
    
    // 考虑滚动偏移
    int item_y = component->layer->rect.y - component->layer->scroll_offset;
    
    // 遍历可见节点
    for (int i = 0; i < component->root_count; i++) {
        TreeNode* node = component->root_nodes[i];
        
        // 检查是否点击在当前节点上
        if (y >= item_y && y < item_y + component->item_height) {
            return node;
        }
        item_y += component->item_height;
        
        // 如果节点展开，递归检查子节点
        if (node->expanded && node->child_count > 0) {
            // 使用栈来模拟递归
            TreeNode** stack = (TreeNode**)malloc(sizeof(TreeNode*) * node->child_count);
            int stack_top = 0;
            
            // 将子节点压入栈
            for (int j = node->child_count - 1; j >= 0; j--) {
                stack[stack_top++] = node->children[j];
            }
            
            // 处理栈中的节点
            while (stack_top > 0) {
                TreeNode* current = stack[--stack_top];
                
                // 检查是否点击在当前节点上
                if (y >= item_y && y < item_y + component->item_height) {
                    free(stack);
                    return current;
                }
                item_y += component->item_height;
                
                // 如果节点展开，将子节点压入栈
                if (current->expanded && current->child_count > 0) {
                    TreeNode** new_stack = (TreeNode**)realloc(stack, sizeof(TreeNode*) * (stack_top + current->child_count));
                    if (new_stack) {
                        stack = new_stack;
                        for (int j = current->child_count - 1; j >= 0; j--) {
                            stack[stack_top++] = current->children[j];
                        }
                    }
                }
            }
            
            free(stack);
        }
    }
    
    return NULL;
}

// 检查是否点击在展开/折叠图标上
int treeview_is_expand_icon_clicked(TreeViewComponent* component, TreeNode* node, int x, int y) {
    if (!component || !node || (node->child_count == 0 && !node->expandable)) return 0;
    
    // 计算节点位置，考虑滚动偏移
    int item_y = component->layer->rect.y - component->layer->scroll_offset;
    
    // 与渲染一致的左边距
    Layer* layer = component->layer;
    int left_margin = (layer->layout_manager && layer->layout_manager->padding[3] > 0)
        ? layer->layout_manager->padding[3] : 0;
    
    // 查找节点在可见节点中的位置
    for (int i = 0; i < component->root_count; i++) {
        TreeNode* current = component->root_nodes[i];
        
        if (current == node) {
            // 找到节点，检查是否点击在展开图标上
            int icon_x = component->layer->rect.x + current->level * component->indent_width + left_margin;
            int icon_y = item_y + (component->item_height - 10) / 2;
            
            return (x >= icon_x && x < icon_x + 10 && y >= icon_y && y < icon_y + 10);
        }
        item_y += component->item_height;
        
        // 如果节点展开，递归查找子节点
        if (current->expanded && current->child_count > 0) {
            // 使用栈来模拟递归
            TreeNode** stack = (TreeNode**)malloc(sizeof(TreeNode*) * current->child_count);
            int stack_top = 0;
            
            // 将子节点压入栈
            for (int j = current->child_count - 1; j >= 0; j--) {
                stack[stack_top++] = current->children[j];
            }
            
            // 处理栈中的节点
            while (stack_top > 0) {
                TreeNode* stack_node = stack[--stack_top];
                
                if (stack_node == node) {
                    // 找到节点，检查是否点击在展开图标上
                    int icon_x = component->layer->rect.x + stack_node->level * component->indent_width + left_margin;
                    int icon_y = item_y + (component->item_height - 10) / 2;
                    
                    free(stack);
                    return (x >= icon_x && x < icon_x + 10 && y >= icon_y && y < icon_y + 10);
                }
                item_y += component->item_height;
                
                // 如果节点展开，将子节点压入栈
                if (stack_node->expanded && stack_node->child_count > 0) {
                    TreeNode** new_stack = (TreeNode**)realloc(stack, sizeof(TreeNode*) * (stack_top + stack_node->child_count));
                    if (new_stack) {
                        stack = new_stack;
                        for (int j = stack_node->child_count - 1; j >= 0; j--) {
                            stack[stack_top++] = stack_node->children[j];
                        }
                    }
                }
            }
            
            free(stack);
        }
    }
    
    return 0;
}

// 序列化TreeNode为JSON
static char* treeview_node_to_json(TreeNode* node) {
    if (!node) return strdup("{}");
    cJSON* obj = cJSON_CreateObject();
    if (node->text) cJSON_AddStringToObject(obj, "text", node->text);
    if (node->icon) cJSON_AddStringToObject(obj, "icon", node->icon);
    if (node->icon_text) cJSON_AddStringToObject(obj, "icon_text", node->icon_text);
    cJSON_AddBoolToObject(obj, "expanded", node->expanded);
    cJSON_AddBoolToObject(obj, "expandable", node->expandable);
    char* json_str = cJSON_PrintUnformatted(obj);
    cJSON_Delete(obj);
    return json_str;
}

// 处理鼠标事件
void treeview_component_handle_mouse_event(Layer* layer, MouseEvent* event) {
    if (!layer || !event || !layer->component) return;
    
    TreeViewComponent* component = (TreeViewComponent*)layer->component;
    
    if (event->state == SDL_PRESSED && event->button == SDL_BUTTON_LEFT) {
        // 调整鼠标坐标以考虑滚动偏移
        int adjusted_y = event->y;
        // 注意：treeview_get_node_from_position 内部已经处理了滚动偏移，所以这里不需要调整
        
        TreeNode* node = treeview_get_node_from_position(component, event->x, event->y);
        
        if (node) {
            // 检查是否点击在展开/折叠图标上
            if (treeview_is_expand_icon_clicked(component, node, event->x, event->y)) {
                // 切换节点展开状态
                int old_expanded = node->expanded;
                treeview_toggle_node(node);
                
                // 更新滚动条状态
                treeview_update_scrollbar(component);
                
                // 调用回调函数
                if (component->on_node_expanded && old_expanded != node->expanded) {
                    component->on_node_expanded(node, node->expanded, component->user_data);
                }

                // 触发 JS onExpand 事件
                if (component->on_expand_name && old_expanded != node->expanded) {
                    EventHandler handler = find_event_by_name(component->on_expand_name);
                    if (handler) {
                        char* node_json = treeview_node_to_json(node);
                        if (node_json) {
                            if (!layer->event) {
                                layer->event = calloc(1, sizeof(Event));
                            }
                            strncpy(layer->event->click_name, component->on_expand_name, MAX_PATH - 1);
                            layer->event->click_name[MAX_PATH - 1] = '\0';
                            free(layer->text);
                            layer->text = strdup(node_json);
                            free(node_json);
                            handler(layer);
                        }
                    }
                }
            } else {
                // 选中节点
                treeview_set_selected_node(component, node);

                // 滚动到选中的节点
                treeview_scroll_to_node(component, node);

                // 触发 onSelect 事件
                if (component->on_select_name) {
                    EventHandler handler = find_event_by_name(component->on_select_name);
                    if (handler) {
                        char* node_json = treeview_node_to_json(node);
                        if (node_json) {
                            if (!layer->event) {
                                layer->event = calloc(1, sizeof(Event));
                            }
                            strncpy(layer->event->click_name, component->on_select_name, MAX_PATH - 1);
                            layer->event->click_name[MAX_PATH - 1] = '\0';
                            free(layer->text);
                            layer->text = strdup(node_json);
                            free(node_json);
                            handler(layer);
                        }
                    }
                }
            }
        }
    }
}

// 处理键盘事件
void treeview_component_handle_key_event(Layer* layer, KeyEvent* event) {
    if (!layer || !event || !layer->component) return;
    
    TreeViewComponent* component = (TreeViewComponent*)layer->component;
    
    if (event->type == KEY_EVENT_DOWN) {
        switch (event->data.key.key_code) {
            case SDLK_UP:
                // 选择上一个节点
                if (component->selected_node) {
                    // TODO: 实现选择上一个节点的逻辑
                }
                break;
                
            case SDLK_DOWN:
                // 选择下一个节点
                if (component->selected_node) {
                    // TODO: 实现选择下一个节点的逻辑
                }
                break;
                
            case SDLK_LEFT:
                // 折叠节点
                if (component->selected_node) {
                    treeview_collapse_node(component->selected_node);
                    
                    // 调用回调函数
                    if (component->on_node_expanded) {
                        component->on_node_expanded(component->selected_node, 0, component->user_data);
                    }
                }
                break;
                
            case SDLK_RIGHT:
                // 展开节点
                if (component->selected_node) {
                    treeview_expand_node(component->selected_node);
                    
                    // 调用回调函数
                    if (component->on_node_expanded) {
                        component->on_node_expanded(component->selected_node, 1, component->user_data);
                    }
                }
                break;
        }
    }
}

// 渲染树视图
void treeview_component_render(Layer* layer) {
    if (!layer || !layer->component) return;
    
    TreeViewComponent* component = (TreeViewComponent*)layer->component;
    
    // 更新滚动条状态
    treeview_update_scrollbar(component);
    
    // 设置裁剪区域
    Rect prev_clip;
    backend_render_get_clip_rect(&prev_clip);
    backend_render_set_clip_rect(&layer->rect);
    
    // 使用纹理获取实际文本高度
    int text_height = 20; // 默认高度
    if (layer->font && layer->font->default_font) {
        Texture* temp_tex = backend_render_texture(layer->font->default_font, "A", (Color){0, 0, 0, 255});
        if (temp_tex) {
            int temp_width, temp_height;
            backend_query_texture(temp_tex, NULL, NULL, &temp_width, &temp_height);
            text_height = temp_height / scale;
            backend_render_text_destroy(temp_tex);
        }
    }
    
    // 绘制背景
    Rect bg_rect = {layer->rect.x, layer->rect.y, layer->rect.w, layer->rect.h};
    if (layer->bg_color.a > 0) {
        if (layer->radius > 0)
            backend_render_rounded_rect(&bg_rect, layer->bg_color, layer->radius);
        else
            backend_render_fill_rect(&bg_rect, layer->bg_color);
    }
    
    // 计算左边距（使用 layer 的 padding，无配置时默认 20）
    int left_margin = (layer->layout_manager && layer->layout_manager->padding[3] > 0)
        ? layer->layout_manager->padding[3] : 0;
    
    // 应用滚动偏移
    int item_y = layer->rect.y - layer->scroll_offset;
    
    // 计算可见范围
    int visible_top = layer->rect.y;
    int visible_bottom = layer->rect.y + layer->rect.h;
    
    // 渲染可见节点，应用可见性裁剪
    for (int i = 0; i < component->root_count; i++) {
        TreeNode* node = component->root_nodes[i];
        
        // 简化的可见性检查：只跳过完全在可见区域外的节点
        int should_render_node = !(item_y + component->item_height < visible_top || item_y > visible_bottom);
        
        if (should_render_node) {
            // 绘制节点
            int text_x = layer->rect.x + node->level * component->indent_width + 20;
            int text_y = item_y + (component->item_height - text_height) / 2;
            
            // 绘制背景
            if (node->selected) {
                Rect sel_rect = {layer->rect.x, item_y, layer->rect.w, component->item_height};
                backend_render_fill_rect(&sel_rect, component->selected_bg_color);
            }
            
            // 绘制展开/折叠图标
            if (node->child_count > 0 || node->expandable) {
                int icon_x = layer->rect.x + node->level * component->indent_width + left_margin;
                int icon_y = item_y + (component->item_height - 10) / 2;

                // 尝试加载 SVG/图片图标 → 文字图标 → 默认矩形 +/-
                Texture* icon_tex = NULL;
                int tex_owned = 0;

                if (node->expanded) {
                    const char* path = node->collapse_icon_path ? node->collapse_icon_path : component->collapse_icon_path;
                    if (path && !node->collapse_icon_tex) {
                        node->collapse_icon_tex = backend_load_texture((char*)path);
                    }
                    if (node->collapse_icon_tex) { icon_tex = node->collapse_icon_tex; }
                    else {
                        const char* text = node->collapse_icon ? node->collapse_icon : component->collapse_icon;
                        if (text) { icon_tex = backend_render_texture(layer->font->default_font, text, component->expand_icon_color); tex_owned = 1; }
                    }
                } else {
                    const char* path = node->expand_icon_path ? node->expand_icon_path : component->expand_icon_path;
                    if (path && !node->expand_icon_tex) {
                        node->expand_icon_tex = backend_load_texture((char*)path);
                    }
                    if (node->expand_icon_tex) { icon_tex = node->expand_icon_tex; }
                    else {
                        const char* text = node->expand_icon ? node->expand_icon : component->expand_icon;
                        if (text) { icon_tex = backend_render_texture(layer->font->default_font, text, component->expand_icon_color); tex_owned = 1; }
                    }
                }

                if (icon_tex) {
                    int tw, th;
                    backend_query_texture(icon_tex, NULL, NULL, &tw, &th);
                    Rect dst = {icon_x, icon_y + (10 - th/scale) / 2, tw/scale, th/scale};
                    backend_render_text_copy(icon_tex, NULL, &dst);
                    if (tex_owned) backend_render_text_destroy(icon_tex);
                } else {
                    // 绘制图标背景
                    Rect icon_rect = {icon_x, icon_y, 10, 10};
                    backend_render_rounded_rect(&icon_rect, component->expand_icon_color, 2);

                    // 绘制图标符号
                    if (node->expanded) {
                        Rect minus_rect = {icon_x + 2, icon_y + 4, 6, 2};
                        backend_render_rect(&minus_rect, (Color){255, 255, 255, 255});
                    } else {
                        Rect minus_rect = {icon_x + 2, icon_y + 4, 6, 2};
                        backend_render_rect(&minus_rect, (Color){255, 255, 255, 255});
                        Rect plus_rect = {icon_x + 4, icon_y + 2, 2, 6};
                        backend_render_rect(&plus_rect, (Color){255, 255, 255, 255});
                    }
                }
            }

            // 绘制文本（优先使用 layer->color，降级到组件默认色）
            Color default_color = layer->color.a > 0 ? layer->color : component->text_color;
            Color text_color = node->selected ? component->selected_text_color : default_color;
            if (node->text && layer->font && layer->font->default_font) {
                // 计算文本起始X：展开图标(10px) + 间距(8px)
                int base_text_x = layer->rect.x + node->level * component->indent_width + left_margin + 18;
                int text_y_base = item_y + (component->item_height - text_height) / 2;
                int text_x_offset = 0;

                // 绘制节点icon: 优先加载SVG/图片，回退到icon_text文本
                Texture* node_icon_tex = NULL;
                int node_icon_owned = 0;
                if (node->icon && !node->icon_tex) {
                    // 只有看起来像文件路径时才尝试加载图片
                    if (strchr(node->icon, '.') || strchr(node->icon, '/') || strchr(node->icon, '\\')) {
                        node->icon_tex = backend_load_texture(node->icon);
                    }
                }
                if (node->icon_tex) {
                    node_icon_tex = node->icon_tex;
                } else if (node->icon_text) {
                    node_icon_tex = backend_render_texture(layer->font->default_font, node->icon_text, text_color);
                    node_icon_owned = 1;
                }
                if (node_icon_tex) {
                    int iw, ih;
                    backend_query_texture(node_icon_tex, NULL, NULL, &iw, &ih);
                    int icon_max_size = component->icon_size > 0 ? component->icon_size : component->item_height - 10;
                    int icon_w = iw / scale;
                    int icon_h = ih / scale;
                    if (icon_w > icon_max_size || icon_h > icon_max_size) {
                        float ratio = (float)icon_w / icon_h;
                        if (ratio > 1.0f) {
                            icon_w = icon_max_size;
                            icon_h = (int)(icon_max_size / ratio);
                        } else {
                            icon_h = icon_max_size;
                            icon_w = (int)(icon_max_size * ratio);
                        }
                    }
                    Rect ir = {base_text_x, text_y_base + (icon_max_size - icon_h) / 2, icon_w, icon_h};
                    backend_render_text_copy(node_icon_tex, NULL, &ir);
                    if (node_icon_owned) backend_render_text_destroy(node_icon_tex);
                    text_x_offset = icon_w + 4;
                }

                Texture* text_texture = backend_render_texture(layer->font->default_font, node->text, text_color);
                if (text_texture) {
                    int actual_text_width, actual_text_height;
                    backend_query_texture(text_texture, NULL, NULL, &actual_text_width, &actual_text_height);

                    // 计算文本位置(加上icon偏移)
                    int text_x = base_text_x + text_x_offset;
                    int text_y = item_y + (component->item_height - actual_text_height / scale) / 2;

                    Rect text_rect = {
                        text_x,
                        text_y,
                        actual_text_width / scale,
                        actual_text_height / scale
                    };

                    // 确保文本不会超出边界，同时裁剪源纹理防止字体压缩
                    Rect* p_src = NULL;
                    Rect src_clip;
                    if (text_rect.x + text_rect.w > layer->rect.x + layer->rect.w) {
                        int clipped_w = layer->rect.x + layer->rect.w - text_rect.x;
                        if (clipped_w <= 0) { backend_render_text_destroy(text_texture); continue; }
                        src_clip.x = 0;
                        src_clip.y = 0;
                        src_clip.w = (int)(clipped_w * scale);
                        src_clip.h = actual_text_height;
                        p_src = &src_clip;
                        text_rect.w = clipped_w;
                    }
                    if (text_rect.y + text_rect.h > item_y + component->item_height) {
                        text_rect.h = item_y + component->item_height - text_rect.y;
                    }

                    backend_render_text_copy(text_texture, p_src, &text_rect);
                    backend_render_text_destroy(text_texture);
                }
            }
        }
        
        item_y += component->item_height;
        
        // 如果节点展开，递归渲染子节点
        if (node->expanded && node->child_count > 0) {
            // 使用栈来模拟递归
            TreeNode** stack = (TreeNode**)malloc(sizeof(TreeNode*) * node->child_count);
            int stack_top = 0;
            
            // 将子节点压入栈
            for (int j = node->child_count - 1; j >= 0; j--) {
                stack[stack_top++] = node->children[j];
            }
            
            // 处理栈中的节点
            while (stack_top > 0) {
                TreeNode* current = stack[--stack_top];
                
                // 简化的子节点可见性检查：只跳过完全在可见区域外的节点
                int should_render_current = !(item_y + component->item_height < visible_top || item_y > visible_bottom);
                
                if (should_render_current) {
                        int current_text_x = layer->rect.x + current->level * component->indent_width + 20;
                    int current_text_y = item_y + (component->item_height - text_height) / 2;
                    
                    // 绘制背景
                    if (current->selected) {
                        Rect sel_rect = {layer->rect.x, item_y, layer->rect.w, component->item_height};
                        backend_render_fill_rect(&sel_rect, component->selected_bg_color);
                    }
                    
                    // 绘制展开/折叠图标
                    if (current->child_count > 0 || current->expandable) {
                        int icon_x = layer->rect.x + current->level * component->indent_width + left_margin;
                        int icon_y = item_y + (component->item_height - 10) / 2;

                        Texture* icon_tex = NULL;
                        int tex_owned = 0;

                        if (current->expanded) {
                            const char* path = current->collapse_icon_path ? current->collapse_icon_path : component->collapse_icon_path;
                            if (path && !current->collapse_icon_tex) {
                                current->collapse_icon_tex = backend_load_texture((char*)path);
                            }
                            if (current->collapse_icon_tex) { icon_tex = current->collapse_icon_tex; }
                            else {
                                const char* text = current->collapse_icon ? current->collapse_icon : component->collapse_icon;
                                if (text) { icon_tex = backend_render_texture(layer->font->default_font, text, component->expand_icon_color); tex_owned = 1; }
                            }
                        } else {
                            const char* path = current->expand_icon_path ? current->expand_icon_path : component->expand_icon_path;
                            if (path && !current->expand_icon_tex) {
                                current->expand_icon_tex = backend_load_texture((char*)path);
                            }
                            if (current->expand_icon_tex) { icon_tex = current->expand_icon_tex; }
                            else {
                                const char* text = current->expand_icon ? current->expand_icon : component->expand_icon;
                                if (text) { icon_tex = backend_render_texture(layer->font->default_font, text, component->expand_icon_color); tex_owned = 1; }
                            }
                        }

                        if (icon_tex) {
                            int tw, th;
                            backend_query_texture(icon_tex, NULL, NULL, &tw, &th);
                            Rect dst = {icon_x, icon_y + (10 - th/scale) / 2, tw/scale, th/scale};
                            backend_render_text_copy(icon_tex, NULL, &dst);
                            if (tex_owned) backend_render_text_destroy(icon_tex);
                        } else {
                            // 绘制图标背景
                            Rect icon_rect = {icon_x, icon_y, 10, 10};
                            backend_render_rounded_rect(&icon_rect, component->expand_icon_color, 2);

                            // 绘制图标符号
                            if (current->expanded) {
                                Rect minus_rect = {icon_x + 2, icon_y + 4, 6, 2};
                                backend_render_rect(&minus_rect, (Color){255, 255, 255, 255});
                            } else {
                                Rect minus_rect = {icon_x + 2, icon_y + 4, 6, 2};
                                backend_render_rect(&minus_rect, (Color){255, 255, 255, 255});
                                Rect plus_rect = {icon_x + 4, icon_y + 2, 2, 6};
                                backend_render_rect(&plus_rect, (Color){255, 255, 255, 255});
                            }
                        }
                    }
                    
                    // 绘制文本 - 修复后的代码
                    Color current_text_color = current->selected ? component->selected_text_color : component->text_color;
                    if (current->text && layer->font && layer->font->default_font) {
                        // base text x: 展开图标(10px) + 间距(10px)
                        int base_text_x = layer->rect.x + current->level * component->indent_width + left_margin + 20;
                        int text_y_base = item_y + (component->item_height - text_height) / 2;
                        int text_x_offset = 0;

                        // 绘制节点icon: 优先加载SVG/图片，回退到icon_text文本
                        Texture* child_icon_tex = NULL;
                        int child_icon_owned = 0;
                        if (current->icon && !current->icon_tex) {
                            if (strchr(current->icon, '.') || strchr(current->icon, '/') || strchr(current->icon, '\\')) {
                                current->icon_tex = backend_load_texture(current->icon);
                            }
                        }
                        if (current->icon_tex) {
                            child_icon_tex = current->icon_tex;
                        } else if (current->icon_text) {
                            child_icon_tex = backend_render_texture(layer->font->default_font, current->icon_text, current_text_color);
                            child_icon_owned = 1;
                        }
                        if (child_icon_tex) {
                            int iw, ih;
                            backend_query_texture(child_icon_tex, NULL, NULL, &iw, &ih);
                            int icon_max_size = component->icon_size > 0 ? component->icon_size : component->item_height - 10;
                            int icon_w = iw / scale;
                            int icon_h = ih / scale;
                            if (icon_w > icon_max_size || icon_h > icon_max_size) {
                                float ratio = (float)icon_w / icon_h;
                                if (ratio > 1.0f) {
                                    icon_w = icon_max_size;
                                    icon_h = (int)(icon_max_size / ratio);
                                } else {
                                    icon_h = icon_max_size;
                                    icon_w = (int)(icon_max_size * ratio);
                                }
                            }
                            Rect ir = {base_text_x, text_y_base + (icon_max_size - icon_h) / 2, icon_w, icon_h};
                            backend_render_text_copy(child_icon_tex, NULL, &ir);
                            if (child_icon_owned) backend_render_text_destroy(child_icon_tex);
                            text_x_offset = icon_w + 4;
                        }

                        Texture* text_texture = backend_render_texture(layer->font->default_font, current->text, current_text_color);
                        if (text_texture) {
                            int actual_text_width, actual_text_height;
                            backend_query_texture(text_texture, NULL, NULL, &actual_text_width, &actual_text_height);

                            // 计算文本位置
                            int current_text_x = base_text_x + text_x_offset;
                            int current_text_y = item_y + (component->item_height - actual_text_height / scale) / 2;

                            Rect text_rect = {
                                current_text_x,
                                current_text_y,
                                actual_text_width / scale,
                                actual_text_height / scale
                            };

                            // 确保文本不会超出边界，同时裁剪源纹理防止字体压缩
                            Rect* p_src = NULL;
                            Rect src_clip;
                            if (text_rect.x + text_rect.w > layer->rect.x + layer->rect.w) {
                                int clipped_w = layer->rect.x + layer->rect.w - text_rect.x;
                                if (clipped_w <= 0) { backend_render_text_destroy(text_texture); continue; }
                                src_clip.x = 0;
                                src_clip.y = 0;
                                src_clip.w = (int)(clipped_w * scale);
                                src_clip.h = actual_text_height;
                                p_src = &src_clip;
                                text_rect.w = clipped_w;
                            }
                            if (text_rect.y + text_rect.h > item_y + component->item_height) {
                                text_rect.h = item_y + component->item_height - text_rect.y;
                            }

                            backend_render_text_copy(text_texture, p_src, &text_rect);
                            backend_render_text_destroy(text_texture);
                        }
                    }
                }
                
                item_y += component->item_height;
                
                // 如果节点展开，将子节点压入栈
                if (current->expanded && current->child_count > 0) {
                    TreeNode** new_stack = (TreeNode**)realloc(stack, sizeof(TreeNode*) * (stack_top + current->child_count));
                    if (new_stack) {
                        stack = new_stack;
                        for (int j = current->child_count - 1; j >= 0; j--) {
                            stack[stack_top++] = current->children[j];
                        }
                    }
                }
            }
            
            free(stack);
        }
    }
    
    // 恢复裁剪区域
    backend_render_set_clip_rect(&prev_clip);
}
#include "layout.h"
#include "util.h"
#include "layer_update.h"

#define printf

static int layout_scale_value(int value, float scale);

static int layout_layer_is_grid(const Layer* layer)
{
    if (!layer) {
        return 0;
    }
    if (layer->type == GRID) {
        return 1;
    }
    return layer->layout_manager && layer->layout_manager->type == LAYOUT_GRID;
}

static void layout_apply_grid(Layer* layer)
{
    if (!layer || layer->child_count <= 0 || !layer->children) {
        return;
    }

    int columns = layer->layout_manager ? layer->layout_manager->columns : 1;
    if (columns <= 0) {
        columns = 1;
    }

    int spacing = layer->layout_manager ? layer->layout_manager->spacing : 0;
    int padding_top = layer_padding_get(layer, 0);
    int padding_right = layer_padding_get(layer, 1);
    int padding_bottom = layer_padding_get(layer, 2);
    int padding_left = layer_padding_get(layer, 3);

    int available_width = layer->rect.w - padding_left - padding_right;
    int available_height = layer->rect.h - padding_top - padding_bottom;

    int rows = (layer->child_count + columns - 1) / columns;
    int cell_width = (available_width - (columns - 1) * spacing) / columns;
    int cell_height = (rows > 0) ? (available_height - (rows - 1) * spacing) / rows : 0;
    if (cell_width < 0) {
        cell_width = 0;
    }
    if (cell_height < 0) {
        cell_height = 0;
    }

    int max_col_width = 0;
    int max_row_height = 0;
    for (int i = 0; i < layer->child_count; i++) {
        Layer* child = layer->children[i];
        if (!child || child->visible == IN_VISIBLE) {
            continue;
        }
        if (child->rect.w > max_col_width) {
            max_col_width = child->rect.w;
        }
        if (child->rect.h > max_row_height) {
            max_row_height = child->rect.h;
        }
    }

    layer->content_width = padding_left + padding_right +
                           columns * max_col_width + (columns - 1) * spacing;
    layer->content_height = padding_top + padding_bottom +
                            rows * max_row_height + (rows - 1) * spacing;

    for (int i = 0; i < layer->child_count; i++) {
        if (!layer->children[i] || layer->children[i]->visible == IN_VISIBLE) {
            continue;
        }
        int row = i / columns;
        int col = i % columns;
        layer->children[i]->rect.x = layer->rect.x + padding_left + col * (cell_width + spacing);
        layer->children[i]->rect.y = layer->rect.y + padding_top + row * (cell_height + spacing);
        layer->children[i]->rect.w = cell_width;
        layer->children[i]->rect.h = cell_height;
    }
}

static int layout_grid_place_last_child(Layer* layer)
{
    if (!layer || layer->child_count <= 0 || !layer->children ||
        !layout_layer_is_grid(layer)) {
        return 0;
    }

    int index = layer->child_count - 1;
    Layer* child = layer->children[index];
    if (!child || child->visible == IN_VISIBLE) {
        return 0;
    }

    int columns = layer->layout_manager ? layer->layout_manager->columns : 1;
    if (columns <= 0) {
        columns = 1;
    }

    int prev_count = layer->child_count - 1;
    int old_rows = prev_count > 0 ? (prev_count + columns - 1) / columns : 0;
    int new_rows = (layer->child_count + columns - 1) / columns;
    if (prev_count > 0 && new_rows != old_rows) {
        layout_apply_grid(layer);
        return 2;
    }

    int spacing = layer->layout_manager ? layer->layout_manager->spacing : 0;
    int padding_top = layer_padding_get(layer, 0);
    int padding_right = layer_padding_get(layer, 1);
    int padding_bottom = layer_padding_get(layer, 2);
    int padding_left = layer_padding_get(layer, 3);

    int available_width = layer->rect.w - padding_left - padding_right;
    int available_height = layer->rect.h - padding_top - padding_bottom;

    int cell_width = (available_width - (columns - 1) * spacing) / columns;
    int cell_height = (new_rows > 0)
        ? (available_height - (new_rows - 1) * spacing) / new_rows
        : 0;
    if (cell_width < 0) {
        cell_width = 0;
    }
    if (cell_height < 0) {
        cell_height = 0;
    }

    int row = index / columns;
    int col = index % columns;
    child->rect.x = layer->rect.x + padding_left + col * (cell_width + spacing);
    child->rect.y = layer->rect.y + padding_top + row * (cell_height + spacing);
    child->rect.w = cell_width;
    child->rect.h = cell_height;

    int max_col_width = 0;
    int max_row_height = 0;
    for (int i = 0; i < layer->child_count; i++) {
        Layer* item = layer->children[i];
        if (!item || item->visible == IN_VISIBLE) {
            continue;
        }
        if (item->rect.w > max_col_width) {
            max_col_width = item->rect.w;
        }
        if (item->rect.h > max_row_height) {
            max_row_height = item->rect.h;
        }
    }

    layer->content_width = padding_left + padding_right +
                           columns * max_col_width + (columns - 1) * spacing;
    layer->content_height = padding_top + padding_bottom +
                            new_rows * max_row_height + (new_rows - 1) * spacing;
    return 1;
}

int layout_after_append_child(Layer* layer)
{
    if (!layer) {
        return 0;
    }
    if (layout_layer_is_grid(layer)) {
        int placed = layout_grid_place_last_child(layer);
        if (placed > 0) {
            if (placed == 2 && layer->parent) {
                layout_layer(layer->parent);
            }
            return 1;
        }
    }
    return 0;
}

static int layout_horizontal_child_width(Layer* child, int available_width, float total_flex, int no_width_count) {
    if (!child) {
        return 50;
    }
    if (child->flex_ratio > 0 && total_flex > 0) {
        return (int)(available_width * (child->flex_ratio / total_flex));
    }
    if (child->fixed_width > 0) {
        return child->fixed_width;
    }
    if (no_width_count > 0) {
        return available_width / no_width_count;
    }
    return 50;
}

static LayoutManager* clone_layout_manager(const LayoutManager* src) {
    if (!src) {
        return NULL;
    }
    LayoutManager* copy = (LayoutManager*)malloc(sizeof(LayoutManager));
    if (!copy) {
        return NULL;
    }
    memcpy(copy, src, sizeof(LayoutManager));
    return copy;
}

static Event* clone_event(const Event* src) {
    if (!src) {
        return NULL;
    }
    Event* copy = (Event*)malloc(sizeof(Event));
    if (!copy) {
        return NULL;
    }
    memcpy(copy, src, sizeof(Event));
    return copy;
}

static Scrollbar* clone_scrollbar(const Scrollbar* src) {
    if (!src) {
        return NULL;
    }
    Scrollbar* copy = (Scrollbar*)malloc(sizeof(Scrollbar));
    if (!copy) {
        return NULL;
    }
    memcpy(copy, src, sizeof(Scrollbar));
    return copy;
}

void layout_layer(Layer* layer){
    // 添加调试信息，检查layer指针
    if (!layer) {
        printf("layout_layer: NULL layer pointer detected!\n");
        fflush(stdout);
        return;
    }
    
    printf("layout_layer: processing layer %s (type: %d, child_count: %d)\n", layer->id ? layer->id : "(null)", layer->type, layer->child_count);
    fflush(stdout);
    
    // 检查children数组是否为NULL但child_count>0
    if (layer->child_count > 0 && !layer->children) {
        printf("layout_layer: WARNING: layer %s has child_count>0 but NULL children array!\n", layer->id ? layer->id : "(null)");
        fflush(stdout);
        return;
    }
     // 计算 layer 的内容尺寸 - 通用算法（List 由 list_component 维护 content 尺寸）
     if (layer->type != LAYER_LIST) {
         layer->content_width = layer->rect.w;
         layer->content_height = layer->rect.h;
     }
     
    // 应用布局管理器
    if (layer->layout_manager && layer->child_count > 0) {
        printf("layout_layer: applying layout manager for layer %s\n", layer->id ? layer->id : "(null)");
        fflush(stdout);
        int padding_top = layer_padding_get(layer, 0);
        int padding_right = layer_padding_get(layer, 1);
        int padding_bottom = layer_padding_get(layer, 2);
        int padding_left = layer_padding_get(layer, 3);
        int spacing = layer->layout_manager->spacing;
        
        if(layer->parent!=NULL){
            printf("DEBUG: layer '%s' before parent adjustment: rect.w=%d, parent.rect.w=%d\n", 
                   layer->id ? layer->id : "(null)", layer->rect.w, layer->parent->rect.w);
            if(layer->rect.w==0){
                layer->rect.w=layer->parent->rect.w;
                printf("DEBUG: layer '%s' width set to parent width: %d\n", 
                       layer->id ? layer->id : "(null)", layer->rect.w);
            }
            if(layer->rect.h==0){
                layer->rect.h=layer->parent->rect.h;
            }
        }
        
        int content_width = layer->rect.w - padding_left - padding_right;
        int content_height = layer->rect.h - padding_top - padding_bottom;
        
        printf("layout_layer: content_size: %d x %d\n", content_width, content_height);
        fflush(stdout);
        
        if (layout_layer_is_grid(layer)) {
            layout_apply_grid(layer);
        } else if (layer->layout_manager->type == LAYOUT_HORIZONTAL) {
            printf("layout_layer: applying HORIZONTAL layout\n");
            fflush(stdout);
            // 计算总权重
            float total_flex = 0;
            int fixed_width_sum = 0;
            int no_width_count = 0;
            int valid_child_count = 0;

            for (int i = 0; i < layer->child_count; i++) {
                if (layer->children[i]->visible == IN_VISIBLE) {
                    printf("layout_layer: skipping invisible child[%d] in horizontal layout\n", i);
                    fflush(stdout);
                    continue;
                }
                if (!layer->children[i]) {
                    printf("layout_layer: WARNING: skipping NULL child[%d] in horizontal layout\n", i);
                    fflush(stdout);
                    continue;
                }
                valid_child_count++;

                if (layer->children[i]->flex_ratio > 0) {
                    total_flex += layer->children[i]->flex_ratio;
                } else if (layer->children[i]->fixed_width > 0) {
                    fixed_width_sum += layer->children[i]->fixed_width;
                } else {
                    no_width_count++;
                }
            }

            // 分配空间
            int available_width = content_width - fixed_width_sum -
                                 (valid_child_count - 1) * spacing;
            int current_x = layer->rect.x + padding_left;

            // 添加水平滚动偏移量
            if (layer->scrollable == 2 || layer->scrollable == 3) {
                current_x -= layer->scroll_offset_x;
            }

            // 初始化内容尺寸
            layer->content_width = padding_left + padding_right;
            layer->content_height = padding_top + padding_bottom;

            // 应用主轴对齐（justifyContent）
            printf("layout_layer: HORIZONTAL - justify=%d, content_width=%d, spacing=%d\n",
                   layer->layout_manager->justify, content_width, spacing);
            fflush(stdout);

            // 先计算所有子元素的总宽度（使用之前计算的valid_child_count）
            int total_children_width = 0;
            for (int i = 0; i < layer->child_count; i++) {
                if (layer->children[i]->visible == IN_VISIBLE) continue;
                if (!layer->children[i]) continue;

                int child_width = layout_horizontal_child_width(
                    layer->children[i], available_width, total_flex, no_width_count);
                total_children_width += child_width;
            }

            if (valid_child_count > 0) {
                if (layer->layout_manager->justify == LAYOUT_ALIGN_CENTER) {
                    // 水平居中
                    current_x = layer->rect.x + padding_left + (content_width - total_children_width - (valid_child_count - 1) * spacing) / 2;
                    printf("layout_layer: HORIZONTAL - CENTER alignment, current_x=%d\n", current_x);
                    fflush(stdout);
                } else if (layer->layout_manager->justify == LAYOUT_ALIGN_RIGHT) {
                    // 右对齐
                    current_x = layer->rect.x + padding_left + content_width - total_children_width - (valid_child_count - 1) * spacing;
                    printf("layout_layer: HORIZONTAL - RIGHT alignment, current_x=%d\n", current_x);
                    fflush(stdout);
                } else if (layer->layout_manager->justify == LAYOUT_ALIGN_SPACE_BETWEEN) {
                    // space-between: 两端对齐，子组件之间间距相等
                    if (valid_child_count > 1) {
                        spacing = (content_width - total_children_width) / (valid_child_count - 1);
                    }
                    printf("layout_layer: HORIZONTAL - SPACE-BETWEEN alignment, spacing=%d\n", spacing);
                    fflush(stdout);
                } else if (layer->layout_manager->justify == LAYOUT_ALIGN_SPACE_AROUND) {
                    // space-around: 每个子组件两侧间距相等
                    spacing = (content_width - total_children_width) / valid_child_count;
                    current_x = layer->rect.x + padding_left + spacing / 2; // 前面加一半间距
                    printf("layout_layer: HORIZONTAL - SPACE-AROUND alignment, spacing=%d, current_x=%d\n", spacing, current_x);
                    fflush(stdout);
                } else {
                    // 左对齐保持默认值
                    printf("layout_layer: HORIZONTAL - LEFT alignment, current_x=%d\n", current_x);
                    fflush(stdout);
                }
            }

            for (int i = 0; i < layer->child_count; i++) {
                Layer* child = layer->children[i];
                if (layer->children[i]->visible == IN_VISIBLE) {
                    printf("layout_layer: skipping invisible child[%d] of %s\n", i, layer->id ? layer->id : "(null)");
                    fflush(stdout);
                    continue;
                }
                if (!child) {
                    printf("layout_layer: WARNING: skipping NULL child[%d] in horizontal layout\n", i);
                    fflush(stdout);
                    continue;
                }

                if (child->flex_ratio > 0 && total_flex > 0) {
                    child->rect.w = (int)(available_width *
                                        (child->flex_ratio / total_flex));
                } else if (child->fixed_width > 0) {
                    child->rect.w = child->fixed_width;
                } else if (no_width_count > 0) {
                    child->rect.w = available_width / no_width_count;
                } else {
                    child->rect.w = 50;
                }

                child->rect.x = current_x;
                child->rect.y = layer->rect.y + padding_top;
                if (child->fixed_height > 0) {
                    child->rect.h = child->fixed_height;
                } else if (child->flex_ratio > 0) {
                    child->rect.h = content_height;
                } else {
                    child->rect.h = content_height;
                }

                // 应用垂直方向对齐（align属性）
                if (layer->layout_manager->align == LAYOUT_ALIGN_CENTER) {
                    // 居中对齐：垂直居中
                    int child_height = child->rect.h > 0 ? child->rect.h : content_height;
                    child->rect.y = layer->rect.y + padding_top + (content_height - child_height) / 2;
                } else if (layer->layout_manager->align == LAYOUT_ALIGN_LEFT) {
                    // 左对齐（在水平布局中表示顶部对齐）
                    child->rect.y = layer->rect.y + padding_top;
                } else if (layer->layout_manager->align == LAYOUT_ALIGN_RIGHT) {
                    // 右对齐（在水平布局中表示底部对齐）
                    int child_height = child->rect.h > 0 ? child->rect.h : content_height;
                    child->rect.y = layer->rect.y + padding_top + content_height - child_height;
                }

                // 累加内容宽度和计算最大高度
                layer->content_width += child->rect.w;
                if (i > 0) layer->content_width += spacing;

                if (child->rect.h > layer->content_height - padding_top - padding_bottom) {
                    layer->content_height = child->rect.h + padding_top + padding_bottom;
                }

                current_x += child->rect.w + spacing;
            }
        } else if (layer->layout_manager->type == LAYOUT_VERTICAL) {
            
            printf("layout_layer: applying VERTICAL layout\n");
            fflush(stdout);

            // 计算总权重
            float total_flex = 0;
            int fixed_height_sum = 0;
            int no_height_count=0;
            int valid_child_count = 0;
            
            printf("layout_layer: calculating flex for %d children\n", layer->child_count);
            fflush(stdout);
            
            for (int i = 0; i < layer->child_count; i++) {
                if (layer->children[i]->visible == IN_VISIBLE) {
                    printf("layout_layer: skipping invisible child[%d] of %s\n", i, layer->id ? layer->id : "(null)");
                    fflush(stdout);
                    continue;
                }
                // 添加NULL检查，防止访问无效指针
                if (!layer->children[i]) {
                    printf("layout_layer: WARNING: child[%d] is NULL during flex calculation\n", i);
                    fflush(stdout);
                    continue;
                }
                
                valid_child_count++;
                
                if (layer->children[i]->flex_ratio > 0) {
                    total_flex += layer->children[i]->flex_ratio;
                } else {
                    fixed_height_sum += layer->children[i]->fixed_height; // 默认高度
                }
                if(layer->children[i]->fixed_height==0){
                    no_height_count++;
                }
            }
            
            printf("layout_layer: found %d valid children\n", valid_child_count);
            fflush(stdout);
            
            // 如果没有有效子图层，跳过布局计算
            if (valid_child_count == 0) {
                printf("layout_layer: no valid children found, skipping layout\n");
                fflush(stdout);
                return;
            }
            
            // 分配空间
            int available_height = content_height - fixed_height_sum -
                                  (valid_child_count - 1) * spacing;
            if (available_height < 0) available_height = 0; // 防止负高度
            int current_y = layer->rect.y + padding_top;
            
            // 初始化内容尺寸
            layer->content_width = padding_left + padding_right;
            layer->content_height = padding_top + padding_bottom;
            
            // 应用主轴对齐（justifyContent）- 垂直方向
            printf("layout_layer: VERTICAL - justify=%d, content_height=%d, spacing=%d\n",
                   layer->layout_manager->justify, content_height, spacing);
            fflush(stdout);
            
            // 计算所有子元素的总高度（不包括间距）
            int total_children_height_no_spacing = fixed_height_sum;
            if (total_flex > 0 || no_height_count > 0) {
                // flex 或自动高度元素会填满可用空间
                total_children_height_no_spacing += available_height;
            }
            
            if (layer->layout_manager->justify == LAYOUT_ALIGN_CENTER) {
                // 垂直居中
                int total_height_with_spacing = total_children_height_no_spacing + (valid_child_count - 1) * spacing;
                current_y = layer->rect.y + padding_top + (content_height - total_height_with_spacing) / 2;
                printf("layout_layer: VERTICAL - CENTER alignment, current_y=%d\n", current_y);
                fflush(stdout);
            } else if (layer->layout_manager->justify == LAYOUT_ALIGN_RIGHT) {
                // 底部对齐
                int total_height_with_spacing = total_children_height_no_spacing + (valid_child_count - 1) * spacing;
                current_y = layer->rect.y + padding_top + content_height - total_height_with_spacing;
                printf("layout_layer: VERTICAL - BOTTOM alignment, current_y=%d\n", current_y);
                fflush(stdout);
            } else if (layer->layout_manager->justify == LAYOUT_ALIGN_SPACE_BETWEEN) {
                // space-between: 两端对齐，子组件之间间距相等
                if (valid_child_count > 1) {
                    spacing = (content_height - total_children_height_no_spacing) / (valid_child_count - 1);
                }
                printf("layout_layer: VERTICAL - SPACE-BETWEEN alignment, spacing=%d\n", spacing);
                fflush(stdout);
            } else if (layer->layout_manager->justify == LAYOUT_ALIGN_SPACE_AROUND) {
                // space-around: 每个子组件两侧间距相等
                spacing = (content_height - total_children_height_no_spacing) / valid_child_count;
                current_y = layer->rect.y + padding_top + spacing / 2; // 前面加一半间距
                printf("layout_layer: VERTICAL - SPACE-AROUND alignment, spacing=%d, current_y=%d\n", spacing, current_y);
                fflush(stdout);
            } else {
                // 顶部对齐保持默认值
                printf("layout_layer: VERTICAL - TOP alignment, current_y=%d\n", current_y);
                fflush(stdout);
            }
            
            // 如果是可滚动的List类型，考虑滚动偏移量
            if (layer->scrollable == 1 || layer->scrollable == 3) {
                current_y -= layer->scroll_offset;
            }

            printf("layout_layer: available_height: %d\n", available_height);
            fflush(stdout);

            for (int i = 0; i < layer->child_count; i++) {
                Layer* child = layer->children[i];

                if (!child) {
                    printf("layout_layer: WARNING: skipping NULL child[%d]\n", i);
                    fflush(stdout);
                    continue;
                }

                if (child->visible == IN_VISIBLE) {
                    printf("layout_layer: skipping invisible child[%d] of %s\n", i, layer->id ? layer->id : "(null)");
                    fflush(stdout);
                    continue;
                }

                if (child->flex_ratio > 0 && total_flex > 0) {
                    child->rect.h = (int)(available_height * 
                                        (child->flex_ratio / total_flex));
                } else if (child->fixed_height > 0) {
                    child->rect.h = child->fixed_height;
                } else {
                    // 自动计算高度
                    if (no_height_count > 0) {
                        child->rect.h = available_height/no_height_count; // 默认高度
                    } else {
                        child->rect.h = 50; //  fallback默认高度
                    }
                }
                
                child->rect.x = layer->rect.x + padding_left;
                child->rect.y = current_y;

                // 垂直布局：有固定宽度的子项保持固有宽度，其余撑满
                if (content_width > 0) {
                    if (child->fixed_width > 0) {
                        child->rect.w = child->fixed_width;
                    } else {
                        child->rect.w = content_width;
                    }
                } else if (child->rect.w <= 0) {
                    child->rect.w = layer->rect.w;
                }
                
                // 应用水平滚动偏移量
                if (layer->scrollable == 2 || layer->scrollable == 3) {
                    int original_x = child->rect.x;
                    child->rect.x -= layer->scroll_offset_x;
                    printf("DEBUG: Applied horizontal scroll offset to child '%s': x=%d -> %d (offset=%d)\n", 
                           child->id ? child->id : "(null)", original_x, child->rect.x, layer->scroll_offset_x);
                }
                
                // 累加内容高度和计算最大宽度
                layer->content_height += child->rect.h;
                if (i > 0) layer->content_height += spacing;
                
                if (child->rect.w > layer->content_width - padding_left - padding_right) {
                    layer->content_width = child->rect.w + padding_left + padding_right;
                }
                
                //printf("%s %s %s %d\n",child->type,child->id,child->text,child->rect.w);
                
                // 应用水平方向对齐（align属性）
                if (layer->layout_manager->align == LAYOUT_ALIGN_CENTER) {
                    // 水平居中对齐
                    child->rect.x = layer->rect.x + padding_left + (content_width - child->rect.w) / 2;
                } else if (layer->layout_manager->align == LAYOUT_ALIGN_LEFT) {
                    // 左对齐
                    child->rect.x = layer->rect.x + padding_left;
                } else if (layer->layout_manager->align == LAYOUT_ALIGN_RIGHT) {
                    // 右对齐
                    child->rect.x = layer->rect.x + padding_left + (content_width - child->rect.w);
                }
                current_y += child->rect.h + spacing;
            }
        } else if (layer->layout_manager->type == LAYOUT_CENTER) {
            for (int i = 0; i < layer->child_count; i++) {
                Layer* child = layer->children[i];
                if (!child || child->visible == IN_VISIBLE) {
                    continue;
                }
                if (child->fixed_width > 0) {
                    child->rect.w = child->fixed_width;
                }
                if (child->fixed_height > 0) {
                    child->rect.h = child->fixed_height;
                }
                child->rect.x = layer->rect.x + padding_left + (content_width - child->rect.w) / 2;
                child->rect.y = layer->rect.y + padding_top + (content_height - child->rect.h) / 2;
            }
        } else if (layer->layout_manager->type == LAYOUT_ABSOLUTE) {
            float sx = 1.0f;
            float sy = 1.0f;
            if (layer->layout_base_valid && layer->layout_base_rect.w > 0) {
                sx = (float)layer->rect.w / (float)layer->layout_base_rect.w;
            }
            if (layer->layout_base_valid && layer->layout_base_rect.h > 0) {
                sy = (float)layer->rect.h / (float)layer->layout_base_rect.h;
            }

            for (int i = 0; i < layer->child_count; i++) {
                Layer* child = layer->children[i];
                int rel_x;
                int rel_y;

                if (!child || child->visible == IN_VISIBLE) {
                    continue;
                }

                if (!child->layout_base_valid) {
                    child->layout_base_rect = child->rect;
                    child->layout_base_valid = 1;
                }

                rel_x = layout_scale_value(child->layout_base_rect.x, sx);
                rel_y = layout_scale_value(child->layout_base_rect.y, sy);
                child->rect.x = layer->rect.x + rel_x;
                child->rect.y = layer->rect.y + rel_y;

                if (layer->scrollable == 1 || layer->scrollable == 3) {
                    child->rect.y -= layer->scroll_offset;
                }
                if (layer->scrollable == 2 || layer->scrollable == 3) {
                    child->rect.x -= layer->scroll_offset_x;
                }
            }
        }
    } else if (layer->layout_manager) {
        printf("layout_layer: layer %s has layout_manager but no children\n", layer->id ? layer->id : "(null)");
        fflush(stdout);
    } else if (layer->child_count > 0) {
        printf("layout_layer: layer %s has children but no layout_manager\n", layer->id ? layer->id : "(null)");
        fflush(stdout);
    }

    if (layer->type == LAYER_LIST) {
        // List 由 list_component 自行渲染与布局，不在 layout 阶段生成子 Layer
    }

    // 其他情况：如果没有计算过content尺寸，使用layer自身的尺寸
    if (layer->content_width == 0 || layer->content_height == 0) {
        layer->content_width = layer->rect.w;
        layer->content_height = layer->rect.h;
    }

    // 检查sub指针并递归调用
    if(layer->sub!=NULL){
        printf("layout_layer: processing sub-layer of %s\n", layer->id ? layer->id : "(null)");
        fflush(stdout);
        layout_layer(layer->sub);
    } else {
        printf("layout_layer: layer %s has no sub-layer\n", layer->id ? layer->id : "(null)");
        fflush(stdout);
    }

    // 检查children数组并递归调用
    if(layer->child_count>0 && layer->children){
        printf("layout_layer: processing %d child layers of %s\n", layer->child_count, layer->id ? layer->id : "(null)");
        fflush(stdout);
        for (int i = 0; i < layer->child_count; i++) {
            if (!layer->children[i]) {
                printf("layout_layer: WARNING: layer %s child[%d] is NULL!\n", layer->id ? layer->id : "(null)", i);
                fflush(stdout);
                continue;
            }
            // 跳过不可见的子层
            if (layer->children[i]->visible == IN_VISIBLE) {
                printf("layout_layer: skipping invisible child[%d] of %s\n", i, layer->id ? layer->id : "(null)");
                fflush(stdout);
                continue;
            }
            printf("layout_layer: processing child[%d] of %s\n", i, layer->id ? layer->id : "(null)");
            fflush(stdout);
            layout_layer(layer->children[i]);
        }
    } else if (layer->child_count > 0) {
        printf("layout_layer: layer %s has %d children but NULL children array\n", layer->id ? layer->id : "(null)", layer->child_count);
        fflush(stdout);
    } else {
        printf("layout_layer: layer %s has no children\n", layer->id ? layer->id : "(null)");
        fflush(stdout);
    }
    
    if(layer->layout!=NULL){
        layer->layout(layer);
    }
    
    printf("layout_layer: layer %s content_size: %d x %d\n", 
           layer->id ? layer->id : "(null)", layer->content_width, layer->content_height);
    fflush(stdout);
    
    printf("layout_layer: finished processing layer %s\n", layer->id ? layer->id : "(null)");
    fflush(stdout);
}

static int layout_scale_value(int value, float scale) {
    if (value <= 0) return value;
    int scaled = (int)(value * scale + 0.5f);
    return scaled > 0 ? scaled : 1;
}

static int layout_is_resizable_view(const Layer* layer) {
    return layer && layer->type == VIEW && layer->layout_manager != NULL;
}

void layout_capture_base(Layer* layer) {
    if (!layer) return;
    layer->layout_base_rect = layer->rect;
    layer->layout_base_fixed_w = layer->fixed_width;
    layer->layout_base_fixed_h = layer->fixed_height;
    layer->layout_base_valid = 1;

    if (layer->children) {
        for (int i = 0; i < layer->child_count; i++) {
            if (layer->children[i]) {
                layout_capture_base(layer->children[i]);
            }
        }
    }
    if (layer->sub) {
        layout_capture_base(layer->sub);
    }
}

static void layout_apply_scale(Layer* layer, float sx, float sy, int is_root) {
    if (!layer) return;

    if (!is_root && layout_is_resizable_view(layer) && layer->layout_base_valid) {
        layer->rect.x = layout_scale_value(layer->layout_base_rect.x, sx);
        layer->rect.y = layout_scale_value(layer->layout_base_rect.y, sy);
        layer->rect.w = layout_scale_value(layer->layout_base_rect.w, sx);
        layer->rect.h = layout_scale_value(layer->layout_base_rect.h, sy);

        if (layer->layout_base_fixed_w > 0) {
            layer->fixed_width = layout_scale_value(layer->layout_base_fixed_w, sx);
        }
        if (layer->layout_base_fixed_h > 0) {
            layer->fixed_height = layout_scale_value(layer->layout_base_fixed_h, sy);
        }
    }

    if (layer->children) {
        for (int i = 0; i < layer->child_count; i++) {
            if (layer->children[i]) {
                layout_apply_scale(layer->children[i], sx, sy, 0);
            }
        }
    }
    if (layer->sub) {
        layout_apply_scale(layer->sub, sx, sy, 0);
    }
}

static void layout_restore_leaf_metrics(Layer* layer) {
    if (!layer) return;
    if (!layout_is_resizable_view(layer) && layer->layout_base_valid) {
        if (layer->layout_base_fixed_w > 0) {
            layer->fixed_width = layer->layout_base_fixed_w;
        }
        if (layer->layout_base_fixed_h > 0) {
            layer->fixed_height = layer->layout_base_fixed_h;
        }
    }
    if (layer->children) {
        for (int i = 0; i < layer->child_count; i++) {
            if (layer->children[i]) {
                layout_restore_leaf_metrics(layer->children[i]);
            }
        }
    }
    if (layer->sub) {
        layout_restore_leaf_metrics(layer->sub);
    }
}

void layout_dispatch_resize_events(Layer* layer, const ResizeEvent* event) {
    if (!layer || !event) return;

    if (layer->handle_resize_event) {
        layer->handle_resize_event(layer, event);
    }
        if (layer->event && layer->event->resize) {
        layer->event->resize(layer, event);
    } else if (layer->event && layer->event->resize_name[0] != '\0') {
        EventHandler handler = find_event_by_name(layer->event->resize_name);
        if (handler) {
            handler((void*)event);
        }
    }

    if (layer->children) {
        for (int i = 0; i < layer->child_count; i++) {
            if (layer->children[i]) {
                layout_dispatch_resize_events(layer->children[i], event);
            }
        }
    }
    if (layer->sub) {
        layout_dispatch_resize_events(layer->sub, event);
    }
}

void layout_resize(Layer* layer, int width, int height) {
    if (!layer || width <= 0 || height <= 0) return;
    if (!layer->layout_base_valid) {
        layout_capture_base(layer);
    }
    if (layer->layout_base_rect.w <= 0 || layer->layout_base_rect.h <= 0) return;

    int old_width = layer->rect.w;
    int old_height = layer->rect.h;
    float sx = (float)width / (float)layer->layout_base_rect.w;
    float sy = (float)height / (float)layer->layout_base_rect.h;
    layout_apply_scale(layer, sx, sy, 1);
    layer->rect.x = 0;
    layer->rect.y = 0;
    layer->rect.w = width;
    layer->rect.h = height;
    layout_restore_leaf_metrics(layer);
    layout_layer(layer);

    ResizeEvent resize_event = {
        old_width, old_height, width, height, sx, sy
    };
    layout_dispatch_resize_events(layer, &resize_event);
}

void layer_dump(const Layer* layer, int depth)
{
    int i;
    int pad;

    if (!layer) {
        return;
    }

    for (pad = 0; pad < depth; pad++) {
        fprintf(stdout, "  ");
    }
    fprintf(stdout, "[%s] type=%d rect=(x=%d y=%d w=%d h=%d) fixed=(w=%d h=%d) visible=%d\n",
            layer->id ? layer->id : "(null)",
            layer->type,
            layer->rect.x, layer->rect.y, layer->rect.w, layer->rect.h,
            layer->fixed_width, layer->fixed_height,
            layer->visible);

    if (layer->children) {
        for (i = 0; i < layer->child_count; i++) {
            if (layer->children[i]) {
                layer_dump(layer->children[i], depth + 1);
            }
        }
    }
    if (layer->sub) {
        layer_dump(layer->sub, depth + 1);
    }
}

int layout_scroll_vertical(Layer* layer, int delta_y) {
    if (!layer || delta_y == 0) return 0;

    int content_height = layer->content_height;
    int visible_height = layer->rect.h;
    if (layer->layout_manager) {
        visible_height -= layer->layout_manager->padding[0] + layer->layout_manager->padding[2];
    }
    if (content_height <= visible_height) return 0;

    int old_offset = layer->scroll_offset;
    layer->scroll_offset -= delta_y;
    if (layer->scroll_offset < 0) {
        layer->scroll_offset = 0;
    } else if (layer->scroll_offset > content_height - visible_height) {
        layer->scroll_offset = content_height - visible_height;
    }

    if (layer->scroll_offset != old_offset) {
        layout_layer(layer);
        mark_layer_dirty(layer, DIRTY_LAYOUT);
        return 1;
    }
    return 0;
}
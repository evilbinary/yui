#include "layout.h"
#include "util.h"

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
     // 计算layer的内容尺寸 - 通用算法
     layer->content_width = layer->rect.w;
     layer->content_height = layer->rect.h;
     
    // 应用布局管理器
    if (layer->layout_manager && layer->child_count > 0) {
        printf("layout_layer: applying layout manager for layer %s\n", layer->id ? layer->id : "(null)");
        fflush(stdout);
        // 检查layout_manager的padding是否有效
        if (!layer->layout_manager->padding) {
            printf("layout_layer: ERROR: layer %s has invalid layout_manager padding!\n", layer->id ? layer->id : "(null)");
            fflush(stdout);
            return;
        }
        
        int padding_top = layer->layout_manager->padding[0];
        int padding_right = layer->layout_manager->padding[1];
        int padding_bottom = layer->layout_manager->padding[2];
        int padding_left = layer->layout_manager->padding[3];
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
        
        if (layer->layout_manager->type == LAYOUT_HORIZONTAL) {
            printf("layout_layer: applying HORIZONTAL layout\n");
            fflush(stdout);
            // 计算总权重
            float total_flex = 0;
            int fixed_width_sum = 0;
            
            for (int i = 0; i < layer->child_count; i++) {
                if (!layer->children[i]) {
                    printf("layout_layer: WARNING: skipping NULL child[%d] in horizontal layout\n", i);
                    fflush(stdout);
                    continue;
                }
                if (layer->children[i]->flex_ratio > 0) {
                    total_flex += layer->children[i]->flex_ratio;
                } else {
                    fixed_width_sum += layer->children[i]->fixed_width > 0 ? 
                                      layer->children[i]->fixed_width : 50; // 默认宽度
                }
            }
            
            // 分配空间
            int available_width = content_width - fixed_width_sum - 
                                 (layer->child_count - 1) * spacing;
            int current_x = layer->rect.x + padding_left;
            
            // 添加水平滚动偏移量
            if (layer->scrollable == 2 || layer->scrollable == 3) {
                current_x -= layer->scroll_offset_x;
            }
            
            // 初始化内容尺寸
            layer->content_width = padding_left + padding_right;
            layer->content_height = padding_top + padding_bottom;
            
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
                } else {
                    child->rect.w = 50; // 默认宽度
                }
                
                child->rect.x = current_x;
                child->rect.y = layer->rect.y + padding_top;
                child->rect.h = content_height;
                
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
            int current_y = layer->rect.y + padding_top;
            
            // 初始化内容尺寸
            layer->content_width = padding_left + padding_right;
            layer->content_height = padding_top + padding_bottom;
            
            // 如果是可滚动的List类型，考虑滚动偏移量
            if (layer->scrollable == 1 || layer->scrollable == 3) {
                current_y -= layer->scroll_offset;
            }

            printf("layout_layer: available_height: %d\n", available_height);
            fflush(stdout);
            
            for (int i = 0; i < layer->child_count; i++) {
                Layer* child = layer->children[i];
                
                // 添加NULL检查
                if (!child) {
                    printf("layout_layer: WARNING: skipping NULL child[%d]\n", i);
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
                
                // 自动计算宽度以填充可用空间
                if(child->rect.w<=0){
                    child->rect.w = content_width;
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
                
                // 检查child->layout_manager是否存在
                if (child->layout_manager) {
                    if(child->layout_manager->type == LAYOUT_CENTER){
                        child->rect.x=layer->rect.x+ child->rect.w/2 + padding_left/2;
                    }else if(child->layout_manager->type == LAYOUT_LEFT){
                        child->rect.x =layer->rect.x+ padding_left;
                    }else if(child->layout_manager->type == LAYOUT_RIGHT){
                        child->rect.x =layer->rect.x + child->rect.w -padding_left/2;
                    }else{
                        //对齐
                        if(layer->layout_manager->align==LAYOUT_ALIGN_CENTER){
                            child->rect.x=layer->rect.x+ child->rect.w/2 + padding_left/2;
                        }else if(layer->layout_manager->align==LAYOUT_ALIGN_LEFT){
                            child->rect.x =layer->rect.x+ padding_left;
                        }else if(layer->layout_manager->align==LAYOUT_ALIGN_RIGHT){
                            child->rect.x =layer->rect.x + child->rect.w -padding_left/2;
                        }
                    }
                }
                current_y += child->rect.h + spacing;
            }
        }
    } else if (layer->layout_manager) {
        printf("layout_layer: layer %s has layout_manager but no children\n", layer->id ? layer->id : "(null)");
        fflush(stdout);
    } else if (layer->child_count > 0) {
        printf("layout_layer: layer %s has children but no layout_manager\n", layer->id ? layer->id : "(null)");
        fflush(stdout);
    }

    //LIST 布局
    if(layer->type==LIST){
        printf("layout_layer: processing LIST layout\n");
        fflush(stdout);
        int padding_top = layer->layout_manager ? layer->layout_manager->padding[0] : 0;
        int padding_left = layer->layout_manager ? layer->layout_manager->padding[3] : 0;
        int spacing = layer->layout_manager ? layer->layout_manager->spacing : 5;
        
        int current_x = layer->rect.x + padding_left;
        int current_y = layer->rect.y + padding_top;
    
        // 添加水平滚动偏移量
        if (layer->scrollable == 2 || layer->scrollable == 3) {
            current_x -= layer->scroll_offset_x;
        }
        
        // 添加垂直滚动偏移量
        if (layer->scrollable == 1 || layer->scrollable == 3) {
            current_y -= layer->scroll_offset;
        }
    
        //printf("layer %d %s %s %d,%d\n",layer->type,layer->id,layer->text,layer->rect.x,layer->rect.y);
        
        // 清理旧的子元素（如果有的话）
        if (layer->children) {
            for (int i = 0; i < layer->child_count; i++) {
                if (layer->children[i]) {
                    free(layer->children[i]);
                }
            }
            free(layer->children);
            layer->children = NULL;
            layer->child_count = 0;
        }
        
        // 根据数据源和模板动态生成列表项
        if (layer->item_template && layer->data) {
            int item_count = layer->data->size;
            //json 数据 todo 从接口获取
            layer->child_count = item_count;
            layer->children = malloc(item_count * sizeof(Layer*));
            
            for (int i = 0; i < item_count; i++) {
                // 创建基于模板的新项
                layer->children[i] = malloc(sizeof(Layer));
                memcpy(layer->children[i], layer->item_template, sizeof(Layer));
                
                // 检查可见性（在创建之后）
                if (layer->children[i]->visible == IN_VISIBLE) {
                    printf("layout_layer: skipping invisible child[%d] of %s\n", i, layer->id ? layer->id : "(null)");
                    fflush(stdout);
                    free(layer->children[i]);
                    layer->children[i] = NULL;
                    continue;
                }
                
                // 设置位置和尺寸
                layer->children[i]->rect.x = current_x;
                layer->children[i]->rect.y = current_y;
                layer->children[i]->rect.w = 300; // 固定宽度，可根据需要调整
                layer->children[i]->rect.h = 30; // 固定高度
                
                // 简单替换${name}为实际数据
                if (strstr(layer->children[i]->text, "${")) {
                    cJSON* item = cJSON_GetArrayItem(layer->data->json, i);
                    if (!item) {
                        printf("layout_layer: WARNING: invalid item data at index %d\n", i);
                        fflush(stdout);
                        continue;
                    }
              
                    cJSON* it = item->child;
                    char name[256];
                    char val[256];
    
                    while(it!=NULL){
                        if (!it->string) {
                            printf("layout_layer: WARNING: null key in JSON data\n");
                            fflush(stdout);
                            it = it->next;
                            continue;
                        }
                        sprintf(name,"${%s}",it->string);
                        if(cJSON_IsString(it)){
                            sprintf(val,"%s",it->valuestring);
                        }else if(cJSON_IsNumber(it)){
                            if(is_cjson_float(it)){
                                sprintf(val,"%f",it->valuedouble);
                            }else{
                                sprintf(val,"%d",it->valueint);
                            }
                            
                        }
    
                        char* result = replace_placeholder(layer->children[i]->text, name, val);
                        if(result){
                            strcpy(layer->children[i]->text, result);
                            free(result);
                        }
                        it = it->next;
                    }
                    
                }
                
                current_y += layer->children[i]->rect.h + spacing;
            }
            
            // 计算LIST的内容尺寸
            if (layer->data && layer->data->size > 0) {
                int item_height = 30; // 默认项目高度
                if (layer->item_template && layer->item_template->rect.h > 0) {
                    item_height = layer->item_template->rect.h;
                }
                layer->content_height = layer->data->size * item_height + (layer->data->size - 1) * spacing;
                layer->content_width = layer->rect.w;
            }
        }
    }else if(layer->type == GRID){
        // 获取Grid布局参数
        int columns = layer->layout_manager ? layer->layout_manager->columns : 1;
        if (columns <= 0) columns = 1;
        
        int spacing = layer->layout_manager ? layer->layout_manager->spacing : 0;
        int padding_top = layer->layout_manager ? layer->layout_manager->padding[0] : 0;
        int padding_right = layer->layout_manager ? layer->layout_manager->padding[1] : 0;
        int padding_bottom = layer->layout_manager ? layer->layout_manager->padding[2] : 0;
        int padding_left = layer->layout_manager ? layer->layout_manager->padding[3] : 0;
        
        // 计算可用空间
        int available_width = layer->rect.w - padding_left - padding_right;
        int available_height = layer->rect.h - padding_top - padding_bottom;
        
        printf("grid %d,%d\n",available_width, available_height);
        // 计算每个网格单元的尺寸
        int rows = (layer->child_count + columns - 1) / columns; // 向上取整计算行数
        int cell_width = (available_width - (columns - 1) * spacing) / columns;
        int cell_height = (rows > 0) ? (available_height - (rows - 1) * spacing) / rows : 0;
        
        // 计算最大列宽和行高
        int max_col_width = 0, max_row_height = 0;
        for (int i = 0; i < layer->child_count; i++) {
            Layer* child = layer->children[i];
            if (child->visible == IN_VISIBLE) {
                printf("layout_layer: skipping invisible child[%d] of %s\n", i, layer->id ? layer->id : "(null)");
                fflush(stdout);
                continue;
            }
            if (!child) continue;
            
            if (child->rect.w > max_col_width) max_col_width = child->rect.w;
            if (child->rect.h > max_row_height) max_row_height = child->rect.h;
        }
        
        // 计算内容尺寸
        layer->content_width = padding_left + padding_right + 
                              columns * max_col_width + (columns - 1) * spacing;
        layer->content_height = padding_top + padding_bottom + 
                               rows * max_row_height + (rows - 1) * spacing;
        
        // 渲染子元素
        for (int i = 0; i < layer->child_count; i++) {
            if (!layer->children[i]) {
                printf("layout_layer: WARNING: skipping NULL child[%d] in GRID layout\n", i);
                fflush(stdout);
                continue;
            }
            int row = i / columns;
            int col = i % columns;
            
            // 设置子元素位置和尺寸
            layer->children[i]->rect.x = layer->rect.x + padding_left + col * (cell_width + spacing);
            layer->children[i]->rect.y = layer->rect.y + padding_top + row * (cell_height + spacing);
            layer->children[i]->rect.w = cell_width;
            layer->children[i]->rect.h = cell_height;
            
        }
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
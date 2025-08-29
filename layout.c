#include "layout.h"
#include "util.h"

void layout_layer(Layer* layer){
// 应用布局管理器
    if (layer->layout_manager && layer->child_count > 0) {
        int padding_top = layer->layout_manager->padding[0];
        int padding_right = layer->layout_manager->padding[1];
        int padding_bottom = layer->layout_manager->padding[2];
        int padding_left = layer->layout_manager->padding[3];
        int spacing = layer->layout_manager->spacing;
        
        int content_width = layer->rect.w - padding_left - padding_right;
        int content_height = layer->rect.h - padding_top - padding_bottom;
        
        if (layer->layout_manager->type == LAYOUT_HORIZONTAL) {
            // 计算总权重
            float total_flex = 0;
            int fixed_width_sum = 0;
            
            for (int i = 0; i < layer->child_count; i++) {
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
            
            for (int i = 0; i < layer->child_count; i++) {
                Layer* child = layer->children[i];
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
                
                current_x += child->rect.w + spacing;
            }
        } else if (layer->layout_manager->type == LAYOUT_VERTICAL) {
            // 计算总权重
            float total_flex = 0;
            int fixed_height_sum = 0;
            
            for (int i = 0; i < layer->child_count; i++) {
                if (layer->children[i]->flex_ratio > 0) {
                    total_flex += layer->children[i]->flex_ratio;
                } else {
                    fixed_height_sum += layer->children[i]->fixed_height > 0 ? 
                                       layer->children[i]->fixed_height : 30; // 默认高度
                }
            }
            
            
            // 分配空间
            int available_height = content_height - fixed_height_sum - 
                                  (layer->child_count - 1) * spacing;
            int current_y = layer->rect.y + padding_top;
            
            // 如果是可滚动的List类型，考虑滚动偏移量
            if (layer->scrollable) {
                current_y -= layer->scroll_offset;
            }
            
            for (int i = 0; i < layer->child_count; i++) {
                Layer* child = layer->children[i];
                if (child->flex_ratio > 0 && total_flex > 0) {
                    child->rect.h = (int)(available_height * 
                                        (child->flex_ratio / total_flex));
                } else if (child->fixed_height > 0) {
                    child->rect.h = child->fixed_height;
                } else {
                    // 自动计算高度
                    child->rect.h = 30; // 默认高度
                }
                
                child->rect.x = layer->rect.x + padding_left;
                child->rect.y = current_y;
                // 自动计算宽度以填充可用空间
                if(child->rect.w<=0){
                    child->rect.w = content_width;
                }
                //printf("%s %s %s %d\n",child->type,child->id,child->text,child->rect.w);
                
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
                
                current_y += child->rect.h + spacing;
            }
        }
    }

    //LIST 布局
    if(layer->type==LIST){
        int padding_top = layer->layout_manager ? layer->layout_manager->padding[0] : 0;
        int padding_left = layer->layout_manager ? layer->layout_manager->padding[3] : 0;
        int spacing = layer->layout_manager ? layer->layout_manager->spacing : 5;
        
        int current_y = layer->rect.y + padding_top;

        if (layer->scrollable) {
            current_y -= layer->scroll_offset;
        }

        //printf("layer %d %s %s %d,%d\n",layer->type,layer->id,layer->text,layer->rect.x,layer->rect.y);
        
        // 清理旧的子元素（如果有的话）
        if (layer->children) {
            for (int i = 0; i < layer->child_count; i++) {
                free(layer->children[i]);
            }
            free(layer->children);
            layer->children = NULL;
            layer->child_count = 0;
        }
        
        // 根据数据源和模板动态生成列表项
        if (layer->item_template&& layer->data) {
            int item_count = layer->data->size;
            //json 数据 todo 从接口获取
            layer->child_count = item_count;
            layer->children = malloc(item_count * sizeof(Layer*));
            
            for (int i = 0; i < item_count; i++) {
                // 创建基于模板的新项
                layer->children[i] = malloc(sizeof(Layer));
                memcpy(layer->children[i], layer->item_template, sizeof(Layer));
                
                // 设置位置和尺寸
                layer->children[i]->rect.x = layer->rect.x + padding_left;
                layer->children[i]->rect.y = current_y;
                layer->children[i]->rect.w = layer->rect.w - (padding_left * 2);
                layer->children[i]->rect.h = 30; // 固定高度
                
                // 简单替换${name}为实际数据
                if (strstr(layer->children[i]->text, "${")) {
                    cJSON* item=cJSON_GetArrayItem(layer->data->json,i);
             
                    cJSON* it=item->child;
                    char name[256];
                    char val[256];

                    while(it!=NULL){
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
                        it=it->next;
                    }
                    
                }
                
                current_y += layer->children[i]->rect.h + spacing;
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
        
        // 计算每个网格单元的尺寸
        int rows = (layer->child_count + columns - 1) / columns; // 向上取整计算行数
        int cell_width = (available_width - (columns - 1) * spacing) / columns;
        int cell_height = (rows > 0) ? (available_height - (rows - 1) * spacing) / rows : 0;
        
        // 渲染子元素
        for (int i = 0; i < layer->child_count; i++) {
            int row = i / columns;
            int col = i % columns;
            
            // 设置子元素位置和尺寸
            layer->children[i]->rect.x = layer->rect.x + padding_left + col * (cell_width + spacing);
            layer->children[i]->rect.y = layer->rect.y + padding_top + row * (cell_height + spacing);
            layer->children[i]->rect.w = cell_width;
            layer->children[i]->rect.h = cell_height;
            
        
        }
    }

    if(layer->sub!=NULL){
        layout_layer(layer->sub);
    }
}
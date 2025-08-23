#include "layout.h"



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
    if(layer->sub!=NULL){
        layout_layer(layer->sub);
    }
}

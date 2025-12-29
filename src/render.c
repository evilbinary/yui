#include "layer.h"
#include "render.h"
#include "animate.h"
#include <limits.h>


// ====================== 资源加载器 ======================
void load_textures(Layer* root) {
    if (root->type==IMAGE&& strlen(root->source) > 0) {
        // 修改为使用image支持多种格式
        char path[MAX_PATH];
        snprintf(path, sizeof(path), "%s/%s", root->assets->path, root->source);

        root->texture=backend_load_texture(path);

       
    }
}

// 递归为所有图层加载字体（使用字体缓存）
void load_all_fonts(Layer* layer) {
    if (!layer) return;
    
    // 为当前图层加载字体
    if (layer->font) {
        // 如果字体已经加载，跳过
        if (layer->font->default_font != NULL) {
            if ((uintptr_t)layer->font->default_font != 0xbebebebebebebebeULL) {
                // 有效字体，跳过
            } else {
                // 损坏的字体指针，重新加载
                layer->font->default_font = NULL;
            }
        }
        
        // 构建字体路径
        char font_path[MAX_PATH];
        
        if (layer->assets && layer->assets->path[0] != '\0') {
            snprintf(font_path, sizeof(font_path), "%s/%s", layer->assets->path, layer->font->path);
        } else {
            snprintf(font_path, sizeof(font_path), "%s", layer->font->path);
        }
        
        // 确保字体大小和粗细有效
        if (layer->font->size == 0) {
            layer->font->size = 16;
        }
        if (strlen(layer->font->weight) == 0) {
            strcpy(layer->font->weight, "normal");
        }
        
        // 加载字体（使用缓存）
        printf("loading font for layer '%s': %s (size: %d, weight: %s)\n", 
               layer->id, font_path, layer->font->size, layer->font->weight);
        
        DFont* font = backend_load_font_with_weight(font_path, layer->font->size, layer->font->weight);
        
        if (font) {
            layer->font->default_font = font;
            printf("font loaded successfully for layer '%s': %p\n", layer->id, (void*)font);
        } else {
            printf("error: failed to load font for layer '%s': %s\n", layer->id, font_path);
        }
    }
    
    // 递归加载子图层的字体
    if (layer->children) {
        for (int i = 0; i < layer->child_count; i++) {
            load_all_fonts(layer->children[i]);
        }
    }
    
    // 处理子图层
    if (layer->sub) {
        load_all_fonts(layer->sub);
    }
}

void load_font(Layer* root){
    if (!root || !root->font) {
        printf("error: load_font called with invalid root or font\n");
        return;
    }
    
    // 如果字体已经加载，不要重复加载
    if (root->font->default_font != NULL) {
        // 检查字体指针是否被破坏
        if ((uintptr_t)root->font->default_font == 0xbebebebebebebebeULL) {
            printf("warning: root font pointer is corrupted, reloading\n");
            root->font->default_font = NULL;
        } else {
            printf("font already loaded for root layer, skipping\n");
            printf("root font: %p\n", (void*)root->font->default_font);
            return;
        }
    }
    
    // 加载默认字体 (需要在项目目录下提供字体文件)
    char font_path[MAX_PATH];
    if(root->assets){
        snprintf(font_path, sizeof(font_path), "%s/%s", root->assets->path, root->font->path);
    }else{
        snprintf(font_path, sizeof(font_path), "%s", root->font->path);
    }
    if(root->font->size==0){
        root->font->size=16;
    }
    if(strlen(root->font->weight) == 0){
        strcpy(root->font->weight, "normal");
    }

    printf("loading font: %s (size: %d, weight: %s)\n", font_path, root->font->size, root->font->weight);
    DFont* default_font=backend_load_font_with_weight(font_path, root->font->size, root->font->weight);
    
    if (!default_font) {
        printf("error: failed to load font %s\n", font_path);
        return;
    }

    root->font->default_font=default_font;
    printf("font loaded successfully for root layer: %p\n", (void*)root->font->default_font);

}

// 添加文字渲染函数
Texture* render_text(Layer* layer,const char* text, Color color) {
    if(layer->font==NULL){
        printf("error not found font %s %d\n",layer->id,layer->type);
        return NULL;
    }
    if (!layer->font->default_font) return NULL;
    
    Texture* texture= backend_render_texture(layer->font->default_font,text,color);
    
    return texture;
}

// ====================== 渲染管线 ======================
void render_layer(Layer* layer) {
    // 添加调试信息，检查layer指针是否为NULL
    if (!layer) {
        printf("render_layer: layer is NULL\n");
        return;
    }
    if(layer->visible==IN_VISIBLE){
        return;
    }
    
    // 在渲染图层之前更新动画状态
    layer_update_animation(layer);
    
    // 根据图层类型进行不同的渲染处理
    if(layer->render!=NULL){
            layer->render(layer);
    }else if(layer->type==VIEW) {
        //printf("layer->%s %dn",layer->id,layer->type);
    // 绘制背景
        if(layer->bg_color.a > 0) {
            // 如果启用了毛玻璃效果，先渲染毛玻璃效果
            if (layer->backdrop_filter) {
                backend_render_backdrop_filter(&layer->rect, layer->blur_radius, layer->saturation, layer->brightness);
            }
            
            if (layer->radius > 0) {
                backend_render_rounded_rect(&layer->rect, layer->bg_color, layer->radius);
            } else {
                backend_render_fill_rect(&layer->rect, layer->bg_color);
            }

        }else{
            // 默认渲染方式
            // 如果启用了毛玻璃效果，先渲染毛玻璃效果
            if (layer->backdrop_filter) {
                backend_render_backdrop_filter(&layer->rect, layer->blur_radius, layer->saturation, layer->brightness);
            }
            
            if (layer->radius > 0) {
                backend_render_rounded_rect(&layer->rect, layer->bg_color, layer->radius);
            } else {
                backend_render_fill_rect(&layer->rect, layer->bg_color);
            }
            
        }
    }
    // 保存当前渲染目标的裁剪区域
    Rect prev_clip;
    render_clip_start(layer,&prev_clip);
    // 递归渲染子图层
    for (int i = 0; i < layer->child_count; i++) {
        // 添加调试信息，检查children指针是否为NULL
        if (!layer->children) {
            printf("render_layer: layer->children is NULL for layer %s\n", layer->id);
            break;
        }
        if (!layer->children[i]) {
            printf("render_layer: layer->children[%d] is NULL for layer %s\n", i, layer->id);
            continue;
        }
        if(layer->children[i]->visible==IN_VISIBLE){
            continue;
        }
        render_layer(layer->children[i]);
    }

       // 递归渲染子图层
    if(layer->sub!=NULL){
        render_layer(layer->sub);
    }

        // 渲染滚动条
    // 渲染垂直滚动条
    if ((layer->scrollable == 1 || layer->scrollable == 3) && layer->scrollbar_v && layer->scrollbar_v->visible) {
       render_vertical_scrollbar(layer);
    }
    
    // 渲染水平滚动条
    // printf("DEBUG: Check horizontal scrollbar for layer '%s' - scrollable=%d, scrollbar_h=%p, visible=%d\n", 
    //        layer->id, layer->scrollable, (void*)layer->scrollbar_h, layer->scrollbar_h ? layer->scrollbar_h->visible : -1);
    if ((layer->scrollable == 2 || layer->scrollable == 3) && layer->scrollbar_h && layer->scrollbar_h->visible) {
    //    printf("DEBUG: Rendering horizontal scrollbar for layer '%s'\n", layer->id);
       render_horizontal_scrollbar(layer);
    }
    
    // 兼容性处理：旧的滚动条（向后兼容）
    if (layer->scrollable && layer->scrollbar && layer->scrollbar->visible) {
       render_scrollbar(layer);
    }
    
    render_clip_end(layer,&prev_clip);

#if DEBUG_VIEW
    Texture* text_texture = render_text(layer,layer->id, (Color){strlen(layer->id)*40%255, 0, 0, 255});
    Rect r={layer->rect.x+2,layer->rect.y,strlen(layer->id)*6,12};
    backend_render_text_copy(text_texture,NULL,&r);
    backend_render_text_destroy(text_texture);
    backend_render_rect(&layer->rect,(Color){strlen(layer->id)*40%255, 0, 0, 255});
#endif
}
void render_clip_start(Layer* layer,Rect* prev_clip){
    backend_render_get_clip_rect(prev_clip);
    // 设置当前图层的裁剪区域
    Rect clip_rect = layer->rect;
    
    // 如果存在父级裁剪区域，则取交集作为最终裁剪区域
    if (prev_clip->w > 0 && prev_clip->h > 0) {
        // 计算两个矩形的交集
        int left = fmax(clip_rect.x, prev_clip->x);
        int top = fmax(clip_rect.y, prev_clip->y);
        int right = fmin(clip_rect.x + clip_rect.w, prev_clip->x + prev_clip->w);
        int bottom = fmin(clip_rect.y + clip_rect.h, prev_clip->y + prev_clip->h);
        
        if (left < right && top < bottom) {
            clip_rect.x = left;
            clip_rect.y = top;
            clip_rect.w = right - left;
            clip_rect.h = bottom - top;
        } else {
            // 没有交集，设置一个空的裁剪区域
            clip_rect.w = 0;
            clip_rect.h = 0;
        }
    }
    
    // 应用裁剪区域
    backend_render_set_clip_rect(&clip_rect);
}

void render_clip_end(Layer* layer,Rect* prev_clip){
    // 恢复之前的裁剪区域
    backend_render_set_clip_rect(prev_clip);
    
}

void render_scrollbar(Layer* layer){
    int spacing = layer->layout_manager ? layer->layout_manager->spacing : 5;
    // 计算内容总高度
    int content_height = layer->content_height;
    
    int visible_height = layer->rect.h - layer->layout_manager->padding[0] - layer->layout_manager->padding[2];
    
    // 只有当内容高度超过可见高度时才显示滚动条
    if (content_height > visible_height) {
        // 计算滚动条尺寸和位置
        int scrollbar_width = layer->scrollbar->thickness > 0 ? layer->scrollbar->thickness : 8;
        int scrollbar_height = (int)((float)visible_height / content_height * visible_height);
        if (scrollbar_height < 20) scrollbar_height = 20; // 最小高度
        
        int scrollbar_x = layer->rect.x + layer->rect.w - scrollbar_width;
        int scrollbar_y = layer->rect.y + (int)((float)layer->scroll_offset / (content_height - visible_height) * (visible_height - scrollbar_height));
        
        // 确保滚动条位置在有效范围内
        if (scrollbar_y < layer->rect.y) scrollbar_y = layer->rect.y;
        if (scrollbar_y > layer->rect.y + visible_height - scrollbar_height) 
            scrollbar_y = layer->rect.y + visible_height - scrollbar_height;
        
        Rect scrollbar_rect = {scrollbar_x, scrollbar_y, scrollbar_width, scrollbar_height};
        
        // 绘制滚动条
        backend_render_fill_rect(&scrollbar_rect,layer->scrollbar->color);
  
    }

}

// 渲染垂直滚动条
void render_vertical_scrollbar(Layer* layer) {
    int spacing = layer->layout_manager ? layer->layout_manager->spacing : 5;
    // 计算内容总高度
    int content_height = layer->content_height;
    
    int visible_height = layer->rect.h;
    if (layer->layout_manager) {
        visible_height -= layer->layout_manager->padding[0] + layer->layout_manager->padding[2];
    }
    
    // 只有当内容高度超过可见高度时才显示滚动条
    if (content_height > visible_height) {
        // 计算滚动条尺寸和位置
        int scrollbar_width = layer->scrollbar_v->thickness > 0 ? layer->scrollbar_v->thickness : 8;
        int scrollbar_height = (int)((float)visible_height / content_height * visible_height);
        if (scrollbar_height < 20) scrollbar_height = 20; // 最小高度
        
        int scrollbar_x = layer->rect.x + layer->rect.w - scrollbar_width;
        int scrollbar_y = layer->rect.y + (int)((float)layer->scroll_offset / (content_height - visible_height) * (visible_height - scrollbar_height));
        
        // 确保滚动条位置在有效范围内
        if (scrollbar_y < layer->rect.y) scrollbar_y = layer->rect.y;
        if (scrollbar_y > layer->rect.y + visible_height - scrollbar_height) 
            scrollbar_y = layer->rect.y + visible_height - scrollbar_height;
        
        Rect scrollbar_rect = {scrollbar_x, scrollbar_y, scrollbar_width, scrollbar_height};
        
        // 绘制滚动条轨道
        Rect track_rect = {scrollbar_x, layer->rect.y, scrollbar_width, visible_height};
        Color track_color = {100, 100, 100, 50}; // 半透明灰色
        backend_render_fill_rect(&track_rect, track_color);
        
        // 绘制滚动条
        backend_render_fill_rect(&scrollbar_rect, layer->scrollbar_v->color);
    }
}

// 渲染水平滚动条
void render_horizontal_scrollbar(Layer* layer) {
    // printf("DEBUG: render_horizontal_scrollbar called for layer '%s'\n", layer->id);
    
    int spacing = layer->layout_manager ? layer->layout_manager->spacing : 5;
    
    // 打印容器的原始尺寸
    // printf("DEBUG: Layer '%s' - rect={x=%d, y=%d, w=%d, h=%d}, padding=[%d,%d,%d,%d]\n", 
    //        layer->id, layer->rect.x, layer->rect.y, layer->rect.w, layer->rect.h,
    //        layer->layout_manager ? layer->layout_manager->padding[0] : -1,
    //        layer->layout_manager ? layer->layout_manager->padding[1] : -1,
    //        layer->layout_manager ? layer->layout_manager->padding[2] : -1,
    //        layer->layout_manager ? layer->layout_manager->padding[3] : -1);
    
    // 计算内容总宽度
    int content_width = layer->content_width;
    
    int visible_width = layer->rect.w;
    if (layer->layout_manager) {
        visible_width -= layer->layout_manager->padding[1] + layer->layout_manager->padding[3];
    }
    
    // 只有当内容宽度超过可见宽度时才显示滚动条
    // printf("DEBUG: Layer '%s' - rect.w=%d, visible_width=%d, content_width=%d, scrollable=%d\n", 
    //        layer->id, layer->rect.w, visible_width, content_width, layer->scrollable);
    if (content_width > visible_width) {
        // printf("DEBUG: Content width exceeds visible width, showing horizontal scrollbar for layer '%s'\n", layer->id);
        // printf("DEBUG: About to calculate horizontal scrollbar metrics\n");
        // 计算滚动条尺寸和位置
        int scrollbar_height = layer->scrollbar_h->thickness > 0 ? layer->scrollbar_h->thickness : 8;
        int scrollbar_width = (int)((float)visible_width / content_width * visible_width);
        if (scrollbar_width < 20) scrollbar_width = 20; // 最小宽度
        
        // printf("DEBUG: scroll_offset_x=%d, content_width=%d, visible_width=%d\n", 
        //        layer->scroll_offset_x, content_width, visible_width);
        
        int scrollbar_x = layer->rect.x;
        if (content_width > visible_width) {
            scrollbar_x += (int)((float)layer->scroll_offset_x / (content_width - visible_width) * (visible_width - scrollbar_width));
        }
        int scrollbar_y = layer->rect.y + layer->rect.h - scrollbar_height;
        
        // printf("DEBUG: Horizontal scrollbar metrics - scrollbar_width=%d, scrollbar_height=%d, scrollbar_x=%d, scrollbar_y=%d\n", 
        //        scrollbar_width, scrollbar_height, scrollbar_x, scrollbar_y);
        
        // 确保滚动条位置在有效范围内
        if (scrollbar_x < layer->rect.x) scrollbar_x = layer->rect.x;
        if (scrollbar_x > layer->rect.x + visible_width - scrollbar_width) 
            scrollbar_x = layer->rect.x + visible_width - scrollbar_width;
        
        Rect scrollbar_rect = {scrollbar_x, scrollbar_y, scrollbar_width, scrollbar_height};
        
        // 绘制滚动条轨道
        Rect track_rect = {layer->rect.x, scrollbar_y, visible_width, scrollbar_height};
        Color track_color = {100, 100, 100, 50}; // 半透明灰色
        // printf("DEBUG: Drawing horizontal scrollbar track at x=%d, y=%d, w=%d, h=%d\n", 
        //        track_rect.x, track_rect.y, track_rect.w, track_rect.h);
        backend_render_fill_rect(&track_rect, track_color);
        
        // 绘制滚动条
        // printf("DEBUG: Drawing horizontal scrollbar thumb at x=%d, y=%d, w=%d, h=%d, color=(%d,%d,%d,%d)\n", 
        //        scrollbar_rect.x, scrollbar_rect.y, scrollbar_rect.w, scrollbar_rect.h,
        //        layer->scrollbar_h->color.r, layer->scrollbar_h->color.g, 
        //        layer->scrollbar_h->color.b, layer->scrollbar_h->color.a);
        backend_render_fill_rect(&scrollbar_rect, layer->scrollbar_h->color);
    }
}

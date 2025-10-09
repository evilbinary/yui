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

void load_font(Layer* root){
    // 加载默认字体 (需要在项目目录下提供字体文件)
    char font_path[MAX_PATH];
    if(root->assets){
        snprintf(font_path, sizeof(font_path), "%s/%s", root->assets->path, root->font->path);
    }else{
        snprintf(font_path, sizeof(font_path), "%s", root->font->path);
    }
    if(root->font&& root->font->size==0){
        root->font->size=16;
    }

    DFont* default_font=backend_load_font(font_path, root->font->size);

    root->font->default_font=default_font;
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
    
    // 在渲染图层之前更新动画状态
    layer_update_animation(layer);
    
    // 根据图层类型进行不同的渲染处理
    if(layer->render!=NULL){
            layer->render(layer);
    }else if(layer->type==VIEW) {
        //printf("layer->%s %d\n",layer->id,layer->type);
    // 绘制背景
        if(layer->bg_color.a > 0) {
            if (layer->radius > 0) {
                backend_render_rounded_rect(&layer->rect, layer->bg_color, layer->radius);
            } else {
                backend_render_fill_rect(&layer->rect, layer->bg_color);
            }

        }else{
            // 默认渲染方式
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
        render_layer(layer->children[i]);
    }

       // 递归渲染子图层
    if(layer->sub!=NULL){
        render_layer(layer->sub);
    }

        // 渲染滚动条
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
    int content_height = 0;
    for (int i = 0; i < layer->child_count; i++) {
        content_height += layer->children[i]->rect.h;
        if (i > 0) content_height += spacing;
    }
    
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
#include "layer.h"
#include "render.h"
#include "component_registry.h"
#include "animate.h"
#include "perf/perf.h"
#include <limits.h>
#include <math.h>

extern int yui_inspect_mode_enabled;
extern int yui_inspect_show_bounds;
extern int yui_inspect_show_info;

// ====================== 资源加载器 ======================
void load_textures(Layer* root) {
    if (root->type==IMAGE&& strlen(root->source) > 0) {
        // 检查是否为 data URI (base64)
        if (strncmp(root->source, "data:image/", 11) == 0) {
            // 查找 base64 标记
            const char* base64_marker = "base64,";
            char* base64_pos = strstr(root->source, base64_marker);
            if (base64_pos) {
                // 跳过 base64 标记
                const char* base64_data = base64_pos + strlen(base64_marker);
                size_t data_len = strlen(base64_data);
                
                printf("Loading image from base64 data URI, length: %zu\n", data_len);
                root->texture = backend_load_texture_from_base64(base64_data, data_len);
                
                if (!root->texture) {
                    printf("Failed to load texture from base64 data\n");
                }
            } else {
                printf("Unsupported data URI format (not base64)\n");
            }
        } else {
            // 修改为使用image支持多种格式
            char path[MAX_PATH];
            
            // 检查是否为绝对路径（以 '/' 开头，Unix/Linux/macOS）
            if (root->source[0] == '/') {
                // 使用绝对路径
                snprintf(path, sizeof(path), "%s", root->source);
            } else {
                // 使用相对路径，拼接 assets 路径
                snprintf(path, sizeof(path), "%s/%s", root->assets->path, root->source);
            }

            root->texture=backend_load_texture(path);
        }
    }
}

// 递归为所有图层加载字体（backend 按 path+size+weight 缓存 TTF_Font，多图层共享同一指针）
void load_all_fonts(Layer* layer) {
    int i;

    if (!layer) return;
    
    if (layer->font) {
        int needs_load = 1;

        if (layer->font->default_font != NULL) {
            if ((uintptr_t)layer->font->default_font != 0xbebebebebebebebeULL) {
                needs_load = 0;
            } else {
                layer->font->default_font = NULL;
            }
        }

        if (needs_load) {
            char font_path[MAX_PATH];
            
            if (layer->font->path[0] == '/') {
                snprintf(font_path, sizeof(font_path), "%s", layer->font->path);
            } else if (layer->assets && layer->assets->path[0] != '\0') {
                snprintf(font_path, sizeof(font_path), "%s/%s", layer->assets->path, layer->font->path);
            } else {
                snprintf(font_path, sizeof(font_path), "%s", layer->font->path);
            }
            
            if (layer->font->size == 0) {
                layer->font->size = 16;
            }
            if (strlen(layer->font->weight) == 0) {
                strcpy(layer->font->weight, "normal");
            }
            
            DFont* font = backend_load_font_with_weight(font_path, layer->font->size, layer->font->weight);
            if (font) {
                layer->font->default_font = font;
            } else {
                printf("error: failed to load font for layer '%s': %s (size: %d, weight: %s)\n",
                       layer->id, font_path, layer->font->size, layer->font->weight);
            }
        }
    }
    
    if (layer->children) {
        for (i = 0; i < layer->child_count; i++) {
            load_all_fonts(layer->children[i]);
        }
    }
    
    if (layer->sub) {
        load_all_fonts(layer->sub);
    }

    if (layer->item_template) {
        load_all_fonts(layer->item_template);
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
    
    // 检查字体路径是否为绝对路径
    if (root->font->path[0] == '/') {
        // 使用绝对路径
        snprintf(font_path, sizeof(font_path), "%s", root->font->path);
    } else if(root->assets){
        // 使用相对路径，拼接 assets 路径
        snprintf(font_path, sizeof(font_path), "%s/%s", root->assets->path, root->font->path);
    } else{
        // 直接使用字体路径
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
    if (!layer->font->default_font) {
        load_all_fonts(layer);
    }
    if (!layer->font->default_font) return NULL;
    
    Texture* texture= backend_render_texture(layer->font->default_font,text,color);
    
    return texture;
}

// ====================== 渲染管线 ======================
void render_layer(Layer* layer) {
    if (!layer) {
        printf("render_layer: layer is NULL\n");
        return;
    }
    if (layer->visible == IN_VISIBLE) {
        return;
    }

    int perf_on = perf_is_enabled();
    perf_layer_tree_enter(layer);

    uint64_t self_ns = 0;
    uint64_t t0 = perf_on ? perf_now_ns() : 0;

    layer_update_animation(layer);

    const YuiComponentOps* ops = yui_type_get_ops(layer->type);
    if (ops && (ops->flags & YUI_COMP_LVGL_WIDGET)) {
        if (layer->layout) {
            layer->layout(layer);
        }
    } else if (layer->render != NULL) {
        layer->render(layer);
    } else if (layer->type == VIEW) {
        if (layer->bg_color.a > 0) {
            if (layer->backdrop_filter) {
                backend_render_backdrop_filter(&layer->rect, layer->blur_radius, layer->saturation, layer->brightness);
            }

            if (layer->radius > 0) {
                backend_render_rounded_rect(&layer->rect, layer->bg_color, layer->radius);
            } else {
                backend_render_fill_rect(&layer->rect, layer->bg_color);
            }
        } else if (layer->backdrop_filter) {
            backend_render_backdrop_filter(&layer->rect, layer->blur_radius, layer->saturation, layer->brightness);
        }
    }

    if (perf_on) {
        self_ns += perf_now_ns() - t0;
    }

    Rect prev_clip;
    render_clip_start(layer, &prev_clip);

    for (int i = 0; i < layer->child_count; i++) {
        if (!layer->children) {
            printf("render_layer: layer->children is NULL for layer %s\n", layer->id);
            break;
        }
        if (!layer->children[i]) {
            printf("render_layer: layer->children[%d] is NULL for layer %s\n", i, layer->id);
            continue;
        }
        if (layer->children[i]->visible == IN_VISIBLE) {
            continue;
        }
        render_layer(layer->children[i]);
    }

    if (layer->sub != NULL) {
        render_layer(layer->sub);
    }

    if (perf_on) {
        t0 = perf_now_ns();
    }

    if ((layer->scrollable == 1 || layer->scrollable == 3) && layer->scrollbar_v && layer->scrollbar_v->visible) {
        render_vertical_scrollbar(layer);
    }

    if ((layer->scrollable == 2 || layer->scrollable == 3) && layer->scrollbar_h && layer->scrollbar_h->visible) {
        render_horizontal_scrollbar(layer);
    }

    if (layer->scrollable && layer->scrollbar && layer->scrollbar->visible) {
        render_scrollbar(layer);
    }

    if (perf_on) {
        self_ns += perf_now_ns() - t0;
        perf_layer_add_self_ns(layer, self_ns);
    }

    render_clip_end(layer, &prev_clip);

#if DEBUG_VIEW
    Texture* text_texture = render_text(layer, layer->id, (Color){strlen(layer->id) * 40 % 255, 0, 0, 255});
    Rect r = {layer->rect.x + 2, layer->rect.y, (int)strlen(layer->id) * 6, 12};
    backend_render_text_copy(text_texture, NULL, &r);
    backend_render_text_destroy(text_texture);
    backend_render_rect(&layer->rect, (Color){strlen(layer->id) * 40 % 255, 0, 0, 255});
#endif
}

static void render_layer_inspect(Layer* layer) {
    if (!layer) {
        return;
    }

    if (!(yui_inspect_mode_enabled || layer->inspect_enabled)) {
        return;
    }

    if (!(layer->inspect_show_bounds || layer->inspect_show_info ||
          yui_inspect_show_bounds || yui_inspect_show_info)) {
        return;
    }

    if (yui_inspect_show_bounds && layer->inspect_show_bounds &&
        layer->rect.w > 0 && layer->rect.h > 0) {
        Color bounds_color = {255, 0, 0, 255};
        backend_render_rect(&layer->rect, bounds_color);

        int corner_size = 4;
        Color corner_color = {255, 0, 0, 255};

        Rect corner1 = {layer->rect.x - corner_size / 2, layer->rect.y - corner_size / 2, corner_size, corner_size};
        backend_render_fill_rect(&corner1, corner_color);

        Rect corner2 = {layer->rect.x + layer->rect.w - corner_size / 2, layer->rect.y - corner_size / 2, corner_size, corner_size};
        backend_render_fill_rect(&corner2, corner_color);

        Rect corner3 = {layer->rect.x - corner_size / 2, layer->rect.y + layer->rect.h - corner_size / 2, corner_size, corner_size};
        backend_render_fill_rect(&corner3, corner_color);

        Rect corner4 = {layer->rect.x + layer->rect.w - corner_size / 2, layer->rect.y + layer->rect.h - corner_size / 2, corner_size, corner_size};
        backend_render_fill_rect(&corner4, corner_color);
    }

    if (yui_inspect_show_info && layer->inspect_show_info && strlen(layer->id) > 0) {
        char line1[128], line2[128], line3[128], line4[128];
        snprintf(line1, sizeof(line1), "ID: %s", layer->id);
        snprintf(line2, sizeof(line2), "Type: %s", yui_type_name(layer->type));
        snprintf(line3, sizeof(line3), "Pos: (%d,%d)", layer->rect.x, layer->rect.y);
        snprintf(line4, sizeof(line4), "Size: (%d,%d)", layer->rect.w, layer->rect.h);

        Color text_color = {255, 255, 255, 255};
        Texture* tex1 = render_text(layer, line1, text_color);
        Texture* tex2 = render_text(layer, line2, text_color);
        Texture* tex3 = render_text(layer, line3, text_color);
        Texture* tex4 = render_text(layer, line4, text_color);

        if (tex1 && tex2 && tex3 && tex4) {
            int w1, h1, w2, h2, w3, h3, w4, h4;
            backend_query_texture(tex1, NULL, NULL, &w1, &h1);
            backend_query_texture(tex2, NULL, NULL, &w2, &h2);
            backend_query_texture(tex3, NULL, NULL, &w3, &h3);
            backend_query_texture(tex4, NULL, NULL, &w4, &h4);

            int max_width = w1 > w2 ? (w1 > w3 ? (w1 > w4 ? w1 : w4) : (w3 > w4 ? w3 : w4))
                                    : (w2 > w3 ? (w2 > w4 ? w2 : w4) : (w3 > w4 ? w3 : w4));
            int total_height = h1 + h2 + h3 + h4;
            int line_spacing = 2;
            total_height += line_spacing * 3;

            int padding = 10;
            int info_width = max_width + padding * 2;
            int info_height = total_height + padding * 2;
            int info_x = layer->rect.x;
            int info_y = layer->rect.y;

            Rect info_bg = {info_x, info_y, info_width, info_height};
            Color bg_color = {0, 0, 0, 180};
            backend_render_fill_rect(&info_bg, bg_color);

            float info_scale = 0.8f;
            int current_y = info_y + padding;
            Rect rect1 = {info_x + padding, current_y, (int)(w1 * info_scale), (int)(h1 * info_scale)};
            backend_render_text_copy(tex1, NULL, &rect1);
            current_y += (int)(h1 * info_scale) + line_spacing;

            Rect rect2 = {info_x + padding, current_y, (int)(w2 * info_scale), (int)(h2 * info_scale)};
            backend_render_text_copy(tex2, NULL, &rect2);
            current_y += (int)(h2 * info_scale) + line_spacing;

            Rect rect3 = {info_x + padding, current_y, (int)(w3 * info_scale), (int)(h3 * info_scale)};
            backend_render_text_copy(tex3, NULL, &rect3);
            current_y += (int)(h3 * info_scale) + line_spacing;

            Rect rect4 = {info_x + padding, current_y, (int)(w4 * info_scale), (int)(h4 * info_scale)};
            backend_render_text_copy(tex4, NULL, &rect4);

            backend_render_text_destroy(tex1);
            backend_render_text_destroy(tex2);
            backend_render_text_destroy(tex3);
            backend_render_text_destroy(tex4);
        } else {
            if (tex1) backend_render_text_destroy(tex1);
            if (tex2) backend_render_text_destroy(tex2);
            if (tex3) backend_render_text_destroy(tex3);
            if (tex4) backend_render_text_destroy(tex4);
        }
    }
}

void render_inspect_overlay(Layer* layer) {
    if (!layer || layer->visible == IN_VISIBLE) {
        return;
    }

    if (!layer->parent) {
        backend_render_set_clip_rect(NULL);
    }

    render_layer_inspect(layer);

    for (int i = 0; i < layer->child_count; i++) {
        if (layer->children[i]) {
            render_inspect_overlay(layer->children[i]);
        }
    }

    if (layer->sub) {
        render_inspect_overlay(layer->sub);
    }
}

static void render_rect_intersect(Rect* out, const Rect* a, const Rect* b) {
    int left = (int)fmax((double)a->x, (double)b->x);
    int top = (int)fmax((double)a->y, (double)b->y);
    int right = (int)fmin((double)(a->x + a->w), (double)(b->x + b->w));
    int bottom = (int)fmin((double)(a->y + a->h), (double)(b->y + b->h));

    if (left < right && top < bottom) {
        out->x = left;
        out->y = top;
        out->w = right - left;
        out->h = bottom - top;
    } else {
        out->x = 0;
        out->y = 0;
        out->w = 0;
        out->h = 0;
    }
}

void render_clip_start(Layer* layer,Rect* prev_clip){
    backend_render_get_clip_rect(prev_clip);
    Rect clip_rect = layer->rect;
    Rect intersected;
    
    if (prev_clip->w > 0 && prev_clip->h > 0) {
        render_rect_intersect(&intersected, &clip_rect, prev_clip);
        clip_rect = intersected;
    }
    
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

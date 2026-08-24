#include "layer.h"
#include "render.h"
#include "component_registry.h"
#include "animate.h"
#include "perf/perf.h"
#include "util.h"
#include "layer_update.h"
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

extern int yui_inspect_mode_enabled;
extern int yui_inspect_show_bounds;
extern int yui_inspect_show_info;

static void render_rect_intersect(Rect* out, const Rect* a, const Rect* b);

// ====================== 资源加载器 ======================
void load_textures(Layer* root) {
    if (root->type==IMAGE && root->source && strlen(root->source) > 0) {
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
            char path[YUI_MAX_PATH];
            
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
            char font_path[YUI_MAX_PATH];
            
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
    char font_path[YUI_MAX_PATH];
    
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

void render_layer_background(Layer* layer, const Color* override_bg) {
    Color fill;
    Rect fill_rect;
    int fill_radius;
    if (!layer) return;

    if (layer->shadow.enabled && layer->shadow.color.a > 0) {
        backend_render_shadow(&layer->rect, layer->radius,
                              layer->shadow.offset_x, layer->shadow.offset_y,
                              layer->shadow.blur, layer->shadow.spread,
                              layer->shadow.color);
    }

    fill_rect = layer->rect;
    fill_radius = layer->radius;

    /* 先画边框外环，再在内缩区域填背景（保留渐变不被边框覆盖） */
    if (layer_border_visible(&layer->border)) {
        int bw = layer->border.width;
        if (layer->radius > 0) {
            backend_render_rounded_rect(&layer->rect, layer->border.color, layer->radius);
        } else {
            backend_render_fill_rect(&layer->rect, layer->border.color);
        }
        fill_rect.x += bw;
        fill_rect.y += bw;
        fill_rect.w -= bw * 2;
        fill_rect.h -= bw * 2;
        fill_radius = layer->radius - bw;
        if (fill_radius < 0) fill_radius = 0;
        if (fill_rect.w <= 0 || fill_rect.h <= 0) return;
    }

    if (layer->bg_gradient.enabled && layer->bg_gradient.count >= 2) {
        backend_render_rounded_gradient(&fill_rect, fill_radius,
                                        layer->bg_gradient.vertical,
                                        layer->bg_gradient.colors,
                                        layer->bg_gradient.count);
        return;
    }

    fill = override_bg ? *override_bg : layer->bg_color;
    if (fill.a == 0) return;

    if (fill_radius > 0) {
        backend_render_rounded_rect(&fill_rect, fill, fill_radius);
    } else {
        backend_render_fill_rect(&fill_rect, fill);
    }
}

// ====================== 渲染管线 ======================
/* 脏刷新模式：运行时由 backend_set_render_mode 切换。
 * FULL  模式：每帧清屏 + 全树渲染（SDL/移动端）。
 * DIRTY 模式：目标持久（直写 LCD），按层 dirty 标志跳过无变化子树。
 *
 * 每棵渲染树（窗口）持有独立 RenderCtx（root->render_ctx），
 * 多窗口各自渲染不串扰。s_rendered_once 等状态全部放 ctx 中。 */

/* 为图层所在树获取/创建渲染上下文。沿 parent 上溯到 root，取 root 的 ctx
 * （root->render_ctx 为 NULL 时分配）。传入 popup 层等树外图层时独立挂其自身。 */
RenderCtx* render_ctx_get(Layer* layer) {
    if (!layer) return NULL;
    while (layer->parent) layer = layer->parent;
    if (!layer->render_ctx) {
        layer->render_ctx = (RenderCtx*)calloc(1, sizeof(RenderCtx));
    }
    return layer->render_ctx;
}

/* 供 backend 或组件在销毁 root 树时释放渲染上下文。
 * 非 root 层继承父层 ctx，只解除引用不释放；root 层负责释放。 */
void render_ctx_free(Layer* layer) {
    if (!layer) return;
    if (layer->parent && layer->render_ctx == layer->parent->render_ctx) {
        layer->render_ctx = NULL;
        return;
    }
    if (layer->render_ctx) {
        free(layer->render_ctx);
        layer->render_ctx = NULL;
    }
}

void render_request_full_redraw(Layer* root) {
    RenderCtx* ctx = render_ctx_get(root);
    if (ctx) ctx->force_full_redraw = 1;
}

void render_request_redraw_rect(Layer* root, Rect r) {
    RenderCtx* ctx = render_ctx_get(root);
    if (ctx && ctx->redraw_rect_count < 4) {
        ctx->redraw_rects[ctx->redraw_rect_count++] = r;
    }
}

/* 层 rect 是否与任一局部重绘区域相交（相交则强制重绘该层） */
static int layer_intersects_redraw_rect(const RenderCtx* ctx, const Rect* layer_rect) {
    if (ctx->local_rect_active) {
        Rect inter;
        render_rect_intersect(&inter, layer_rect, &ctx->local_rect);
        return inter.w > 0 && inter.h > 0;
    }
    for (int i = 0; i < ctx->redraw_rect_count; i++) {
        Rect inter;
        render_rect_intersect(&inter, layer_rect, &ctx->redraw_rects[i]);
        if (inter.w > 0 && inter.h > 0) {
            return 1;
        }
    }
    return 0;
}

static int render_dirty_mode(void) {
    return backend_get_render_mode() == YUI_RENDER_MODE_DIRTY;
}

/* 该层是否有进行中的动画（需每帧推进并重绘）。
 * 仅 RUNNING：已完成/未启动的 infinite 描述不能挡住脏跳过。 */
static int layer_has_active_animation(const Layer* layer) {
    const Animation* a;
    if (!layer) return 0;
    a = layer->animation;
    return a && a->state == ANIMATION_STATE_RUNNING;
}

static void animating_ref_inc(Layer* layer) {
    for (; layer; layer = layer->parent) {
        layer->animating_ref++;
    }
}

static void animating_ref_dec(Layer* layer) {
    for (; layer; layer = layer->parent) {
        if (layer->animating_ref > 0) {
            layer->animating_ref--;
        }
    }
}

/* 该层绘制后是否可能覆盖并抹掉子层旧像素（不透明背景/阴影/边框/渐变）。
 * 若是，其子层必须重绘，不能脏跳过。 */
static int layer_paints_over_children(const Layer* layer) {
    if (!layer) return 0;
    if (layer->bg_color.a == 255) return 1;
    if (layer->bg_gradient.enabled) return 1;
    if (layer->shadow.enabled) return 1;
    if (layer_border_visible(&layer->border)) return 1;
    return 0;
}

static int s_rl_visit;
static int s_rl_skip;
static int s_rl_draw;
static unsigned s_rl_root_dirty;
static int s_rl_root_aref;
static char s_rl_draw_ids[8][24];
static int s_rl_draw_id_n;

void render_last_stats(int* visit, int* skip, int* draw, unsigned* root_dirty, int* root_aref) {
    if (visit) *visit = s_rl_visit;
    if (skip) *skip = s_rl_skip;
    if (draw) *draw = s_rl_draw;
    if (root_dirty) *root_dirty = s_rl_root_dirty;
    if (root_aref) *root_aref = s_rl_root_aref;
}

const char* render_last_draw_id(int i) {
    if (i < 0 || i >= s_rl_draw_id_n) return NULL;
    return s_rl_draw_ids[i];
}

static void render_note_draw(const Layer* layer) {
    s_rl_draw++;
    if (s_rl_draw_id_n >= 8 || !layer || !layer->id) return;
    snprintf(s_rl_draw_ids[s_rl_draw_id_n], sizeof(s_rl_draw_ids[0]), "%s", layer->id);
    s_rl_draw_id_n++;
}

static void render_layer_impl(Layer* layer, int force, RenderCtx* ctx) {
    if (!layer) {
        printf("render_layer: layer is NULL\n");
        return;
    }
    if (layer->visible == IN_VISIBLE) {
        return;
    }

    /* 脏刷新：DIRTY 模式下，首帧过后，非强制、无 dirty、无进行中动画的层跳过整棵子树。
     * 子孙有动画时 animating_ref>0，本层照常绘制（含背景和子层），不能只遍历不画，
     * 否则不透明父 View 会把同级 Button/文本盖掉或永远跳过。 */
    if (ctx->force_full_redraw) force = 1;
    if (ctx->local_rect_active) {
        /* 动画层跳过局部渲染：它由正常渲染阶段绘制当前位置，
         * 避免在旧位置被 force 重绘而把刚擦除的残留又填回去。 */
        if (layer_has_active_animation(layer)) {
            return;
        }
        if (!layer_intersects_redraw_rect(ctx, &layer->rect)) {
            return; /* 区域外：整棵子树跳过 */
        }
        force = 1; /* 区域内：强制重绘 */
    } else if (render_dirty_mode() && !force && ctx->rendered_once && layer->dirty_flags == DIRTY_NONE &&
        !layer_has_active_animation(layer) && layer->animating_ref <= 0) {
        s_rl_skip++;
        return;
    } else if (render_dirty_mode() && !force && ctx->rendered_once && layer->dirty_flags == DIRTY_NONE &&
        !layer_has_active_animation(layer) && layer->animating_ref > 0) {
        /* 仅子孙在动：不画本层背景，避免 keep-alive 把整屏不透明 View 每帧重刷。 */
        int i;
        s_rl_visit++;
        if (!layer->parent) {
            s_rl_root_dirty = layer->dirty_flags;
            s_rl_root_aref = layer->animating_ref;
        }
        layer_update_animation(layer);
        for (i = 0; i < layer->child_count; i++) {
            if (!layer->children || !layer->children[i]) {
                continue;
            }
            if (layer->children[i]->visible == IN_VISIBLE) {
                continue;
            }
            render_layer_impl(layer->children[i], 0, ctx);
        }
        if (layer->sub != NULL) {
            render_layer_impl(layer->sub, 0, ctx);
        }
        return;
    }

    s_rl_visit++;
    if (!layer->parent) {
        s_rl_root_dirty = layer->dirty_flags;
        s_rl_root_aref = layer->animating_ref;
    }

    /* Fully clipped layers must not render or replace the parent clip.
     * Layers with a custom render function may draw outside their own rect
     * (e.g. CONNECTOR draws bezier curves at absolute coords). */
    if (layer->render == NULL) {
        Rect parent_clip;
        backend_render_get_clip_rect(&parent_clip);
        if (parent_clip.w > 0 && parent_clip.h > 0) {
            Rect visible_rect;
            render_rect_intersect(&visible_rect, &layer->rect, &parent_clip);
            if (visible_rect.w <= 0 || visible_rect.h <= 0) {
                return;
            }
        }
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
        render_note_draw(layer);
        layer->render(layer);
    } else if (layer->type == VIEW) {
        if (layer->backdrop_filter) {
            backend_render_backdrop_filter(&layer->rect, layer->blur_radius, layer->saturation, layer->brightness);
        }
        /* 不透明 View 背景：只在几何/结构/自身颜色变化时重画。
         * 子孙 DIRTY_TEXT 会传播到本层，但不能因此把整块底再 SPI 一遍。 */
        int draw_cover_bg = 1;
        if (render_dirty_mode() && ctx->rendered_once && !ctx->force_full_redraw &&
            !ctx->local_rect_active) {
            draw_cover_bg = (layer->dirty_flags & (DIRTY_RECT | DIRTY_LAYOUT | DIRTY_LAYOUT_RECT |
                                                   DIRTY_CHILDREN | DIRTY_VISIBLE | DIRTY_COLOR | DIRTY_STYLE)) ? 1 : 0;
        }
        if (draw_cover_bg && (layer->bg_gradient.enabled || layer->bg_color.a > 0 ||
            layer->shadow.enabled || layer_border_visible(&layer->border))) {
            if (ctx->local_rect_active) {
                /* 局部重绘：背景 clip 到区域，只擦除区域内的旧像素 */
                Rect prev_clip_bg;
                if (render_clip_push(&ctx->local_rect, &prev_clip_bg)) {
                    render_note_draw(layer);
                    render_layer_background(layer, NULL);
                    render_clip_pop(&prev_clip_bg);
                }
            } else {
                render_note_draw(layer);
                render_layer_background(layer, NULL);
            }
        }
    }

    if (perf_on) {
        self_ns += perf_now_ns() - t0;
    }

    Rect prev_clip;
    if (!render_clip_start(layer, &prev_clip)) {
        return;
    }

    /* 只有本帧真正盖住了子层像素（重画了不透明底），或结构/几何脏，才强制子树。
     * DIRTY_TEXT 从子孙向上传播时，不能把同级 Button/整块表盘一起刷掉。 */
    int skip_cover_bg = 0;
    if (render_dirty_mode() && ctx->rendered_once && !ctx->force_full_redraw &&
        !ctx->local_rect_active) {
        skip_cover_bg = !(layer->dirty_flags & (DIRTY_RECT | DIRTY_LAYOUT | DIRTY_LAYOUT_RECT |
                                                DIRTY_CHILDREN | DIRTY_VISIBLE | DIRTY_COLOR | DIRTY_STYLE));
    }
    int painted_over = !skip_cover_bg && layer_paints_over_children(layer);
    /* DIRTY_VISIBLE：页面 show/hide 只标在该层和祖先上，子孙仍是 DIRTY_NONE。
     * 持久 FB 上还是上一页像素，必须强制子树重绘，否则启动器等 keepAlive 页空白。 */
    int force_children = force || painted_over ||
            (layer->dirty_flags & (DIRTY_RECT | DIRTY_LAYOUT | DIRTY_LAYOUT_RECT |
                                   DIRTY_CHILDREN | DIRTY_VISIBLE));

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
        render_layer_impl(layer->children[i], force_children, ctx);
    }

    if (layer->sub != NULL) {
        render_layer_impl(layer->sub, force_children, ctx);
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

/* 绘制完成：清除该层 dirty。局部渲染（render_layer_rect / 擦除旧位置）期间
 * 只重绘了区域内像素，不清 dirty——否则正常渲染阶段会因 dirty==0 跳过该层，
 * 区域外的内容（如刚打开的对话框主体）永远画不出来。 */
    if (!ctx->local_rect_active && layer->dirty_flags != DIRTY_NONE) {
        layer->dirty_flags = DIRTY_NONE;
    }
}

/* 公开入口：每帧由 backend_tick 调用，force=0 启用脏跳过。
 * 先对请求的局部区域逐个重绘（擦除 popup 移动后旧位置），再正常渲染。 */
void render_layer(Layer* layer) {
    RenderCtx* ctx = render_ctx_get(layer);
    if (!ctx) return;
    s_rl_visit = 0;
    s_rl_skip = 0;
    s_rl_draw = 0;
    s_rl_root_dirty = 0;
    s_rl_root_aref = 0;
    s_rl_draw_id_n = 0;
    ctx->force_full_redraw = 0; /* 请求只生效一帧 */
    for (int i = 0; i < ctx->redraw_rect_count; i++) {
        ctx->local_rect = ctx->redraw_rects[i];
        ctx->local_rect_active = 1;
        render_layer_impl(layer, 0, ctx);
    }
    ctx->local_rect_active = 0;
    ctx->redraw_rect_count = 0;
    render_layer_impl(layer, 0, ctx);
    ctx->rendered_once = 1;
}

/* 动画进入运行态：所在树 ctx 计数 +1（animation_start / resume 调用） */
void render_animation_started(Layer* layer) {
    RenderCtx* ctx;
    if (!layer || !layer->animation) return;
    if (layer->animation->ref_held) return;
    ctx = render_ctx_get(layer);
    if (ctx) {
        ctx->animation_count++;
    }
    animating_ref_inc(layer);
    layer->animation->ref_held = true;
}

/* 动画离开运行态：所在树 ctx 计数 -1（stop / pause / 完成 / 替换 / 层销毁调用，
 * 仅当动画处于 RUNNING 状态才回退，避免 COMPLETED 后又 stop 造成负数） */
void render_animation_released(Layer* layer) {
    RenderCtx* ctx;
    if (!layer || !layer->animation) return;
    if (!layer->animation->ref_held) return;
    if (layer->animation->state != ANIMATION_STATE_RUNNING) {
        layer->animation->ref_held = false;
        return;
    }
    ctx = render_ctx_get(layer);
    if (ctx && ctx->animation_count > 0) {
        ctx->animation_count--;
    }
    animating_ref_dec(layer);
    layer->animation->ref_held = false;
}

static void render_sync_animation_refs_walk(Layer* layer) {
    int i;
    if (!layer) return;
    if (layer->animation && layer->animation->state == ANIMATION_STATE_RUNNING) {
        if (layer_is_effectively_visible(layer)) {
            render_animation_started(layer);
        } else {
            render_animation_released(layer);
        }
    }
    if (layer->children) {
        for (i = 0; i < layer->child_count; i++) {
            render_sync_animation_refs_walk(layer->children[i]);
        }
    }
    if (layer->sub) {
        render_sync_animation_refs_walk(layer->sub);
    }
}

void render_sync_animation_refs(Layer* layer) {
    render_sync_animation_refs_walk(layer);
}

/* 局部渲染：只绘制 layer 树中与 rect 相交的层。区域外子树整棵跳过，
 * root 背景 clip 到区域擦除旧像素。用于移动/增删后的局部刷新。 */
void render_layer_rect(Layer* layer, Rect rect) {
    if (!layer) return;
    RenderCtx* ctx = render_ctx_get(layer);
    if (!ctx) return;
    ctx->local_rect = rect;
    ctx->local_rect_active = 1;
    render_layer_impl(layer, 0, ctx);
    ctx->local_rect_active = 0;
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

int render_clip_push(const Rect* local, Rect* prev_out) {
    Rect clip;
    Rect intersected;

    if (!local || !prev_out) {
        return 0;
    }

    backend_render_get_clip_rect(prev_out);
    clip = *local;

    if (prev_out->w > 0 && prev_out->h > 0) {
        render_rect_intersect(&intersected, &clip, prev_out);
        clip = intersected;
    }

    /* Empty must not call set_clip — that disables clipping and causes bleed. */
    if (clip.w <= 0 || clip.h <= 0) {
        return 0;
    }

    backend_render_set_clip_rect(&clip);
    return 1;
}

void render_clip_pop(const Rect* prev) {
    if (!prev || prev->w <= 0 || prev->h <= 0) {
        backend_render_set_clip_rect(NULL);
        return;
    }
    Rect restore = *prev;
    backend_render_set_clip_rect(&restore);
}

int render_clip_start(Layer* layer, Rect* prev_clip) {
    return render_clip_push(&layer->rect, prev_clip);
}

void render_clip_end(Layer* layer, Rect* prev_clip) {
    (void)layer;
    render_clip_pop(prev_clip);
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
        Color track_color = layer->scrollbar_v->track_color.a
            ? layer->scrollbar_v->track_color
            : (Color){100, 100, 100, 50};
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
        Color track_color = layer->scrollbar_h->track_color.a
            ? layer->scrollbar_h->track_color
            : (Color){100, 100, 100, 50};
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

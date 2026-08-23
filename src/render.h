#ifndef YUI_RENDER_H
#define YUI_RENDER_H


#include "layer.h"
#include "backend.h"



void render_layer(Layer* layer);
void render_inspect_overlay(Layer* layer);
void load_textures(Layer* root);
void load_font(Layer* root);
void load_all_fonts(Layer* layer);

/* 渲染上下文：每棵渲染树（窗口）独立一份，挂在 root->render_ctx。
 * 子层经继承共享同树上下文；多窗口各自渲染不串扰。 */
typedef struct RenderCtx {
    int rendered_once;         /* 首帧全量渲染后启用脏跳过（每树独立） */
    int force_full_redraw;     /* 请求下一帧全量重绘 */
    int redraw_rect_count;     /* 局部重绘区域数 */
    Rect redraw_rects[4];      /* popup 移动等局部擦除区域（屏幕坐标） */
    int local_rect_active;     /* 正在做局部渲染 */
    Rect local_rect;           /* 局部渲染区域（屏幕坐标） */
    int animation_count;       /* 本树运行中动画数（start/暂停/完成/停时维护，渲染时不扫描） */
} RenderCtx;

/* 为 root 树获取/创建渲染上下文（root->render_ctx 为 NULL 时分配） */
RenderCtx* render_ctx_get(Layer* root);

/* 释放 root 树渲染上下文（销毁 root 层时调用） */
void render_ctx_free(Layer* root);

/* 请求下一帧强制全量重绘（root 背景重绘擦除子层/弹出层旧位置像素）。
 * 用于 popup 等 root 树外图层移动后，避免持久目标（canvas/LCD）残留拖影。 */
void render_request_full_redraw(Layer* root);

/* 请求下一帧局部重绘：只重绘与该区域相交的层（root 背景 clip 到区域擦除）。
 * popup 移动后传入旧位置 rect，比全量重绘开销小。 */
void render_request_redraw_rect(Layer* root, Rect r);

/* 局部渲染：只绘制 layer 树中与 rect 相交的层（用于移动/增删后局部刷新）。
 * 不相交子树跳过，root 背景 clip 到区域擦除旧像素。 */
void render_layer_rect(Layer* layer, Rect rect);

/* 动画进入运行态：所在树 ctx 计数 +1（animation_start / resume 调用） */
void render_animation_started(Layer* layer);
/* 动画离开运行态：所在树 ctx 计数 -1（stop / pause / 完成 / 替换 / 层销毁调用，
 * 仅当动画处于 RUNNING 状态才回退） */
void render_animation_released(Layer* layer);

// 添加滚动条渲染函数声明
void render_scrollbar(Layer* layer);
void render_vertical_scrollbar(Layer* layer);
void render_horizontal_scrollbar(Layer* layer);
int render_clip_start(Layer* layer,Rect* prev_clip);
void render_clip_end(Layer* layer,Rect* prev_clip);

/* Nested clip for components: always intersect with current parent clip.
 * push returns 0 if fully clipped (clip unchanged — skip drawing).
 * pop restores exactly (never intersect). */
int render_clip_push(const Rect* local, Rect* prev_out);
void render_clip_pop(const Rect* prev);

Texture* render_text(Layer* layer,const char* text, Color color);

/* 绘制图层阴影 + 背景（纯色或渐变）。override_bg 非空时覆盖 layer->bg_color */
void render_layer_background(Layer* layer, const Color* override_bg);

/* 上一帧 render_layer 统计：visit=进入绘制的层，skip=脏跳过的子树根，draw=真正画了背景/组件的层 */
void render_last_stats(int* visit, int* skip, int* draw, unsigned* root_dirty, int* root_aref);
const char* render_last_draw_id(int i);

#endif
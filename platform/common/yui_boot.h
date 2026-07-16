#ifndef YUI_BOOT_H
#define YUI_BOOT_H

struct Layer;

#ifdef __cplusplus
extern "C" {
#endif

int yui_init(const char* json_path, const char* assets_dir);
void yui_resize(int width, int height);
void yui_tick(void);
void yui_on_touch(int pointer_id, int x, int y, int phase);
void yui_shutdown(void);
struct Layer* yui_get_root(void);

/* platform 在 yui_init 前传入原生绘图表面（Android: ANativeWindow*, iOS: CAMetalLayer*） */
void yui_set_native_surface(void* surface);

#ifdef __cplusplus
}
#endif

#endif

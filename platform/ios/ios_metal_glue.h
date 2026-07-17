#ifndef IOS_METAL_GLUE_H
#define IOS_METAL_GLUE_H

#ifdef __cplusplus
extern "C" {
#endif

void ios_metal_init(void* metal_layer);
void ios_metal_shutdown(void);
void ios_metal_resize(int width, int height);
void ios_metal_clear(unsigned char r, unsigned char g, unsigned char b, unsigned char a);
void ios_metal_draw_rect(float x, float y, float w, float h,
                         unsigned char r, unsigned char g, unsigned char b, unsigned char a);
void ios_metal_present(void);
int ios_metal_ready(void);

#ifdef __cplusplus
}
#endif

#endif

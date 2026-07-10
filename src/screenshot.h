#ifndef YUI_SCREENSHOT_H
#define YUI_SCREENSHOT_H

// 保存 RGBA 像素为 PNG；stride 为每行字节数
int screenshot_save_png(const char* path, const unsigned char* rgba, int w, int h, int stride);

// 为文件路径创建父目录（如 app/shopping/capture）
int screenshot_mkdir_for_file(const char* path);

#endif

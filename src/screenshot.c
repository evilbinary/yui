#include "screenshot.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../lib/stb/stb_image_write.h"

static int screenshot_write_png(const char* filename, int w, int h, int comp, const void* data, int stride) {
    return stbi_write_png(filename, w, h, comp, data, stride) ? 0 : -1;
}

int screenshot_save_png(const char* path, const unsigned char* rgba, int w, int h, int stride) {
    if (!path || !rgba || w <= 0 || h <= 0 || stride <= 0) {
        return -1;
    }
    if (screenshot_mkdir_for_file(path) != 0) {
        return -2;
    }
    if (screenshot_write_png(path, w, h, 4, rgba, stride) != 0) {
        printf("screenshot: failed to write %s\n", path);
        return -3;
    }
    printf("screenshot: saved %s (%dx%d)\n", path, w, h);
    return 0;
}

int screenshot_mkdir_for_file(const char* path) {
    if (!path || !path[0]) {
        return -1;
    }

    char dir[512];
    strncpy(dir, path, sizeof(dir) - 1);
    dir[sizeof(dir) - 1] = '\0';

    char* slash = strrchr(dir, '/');
    char* backslash = strrchr(dir, '\\');
    char* sep = slash;
    if (backslash && (!slash || backslash > slash)) {
        sep = backslash;
    }
    if (!sep) {
        return 0;
    }
    *sep = '\0';
    if (dir[0] == '\0') {
        return 0;
    }

    for (char* p = dir + 1; *p; p++) {
        if (*p == '/' || *p == '\\') {
            char saved = *p;
            *p = '\0';
#ifdef _WIN32
            if (_mkdir(dir) != 0 && errno != EEXIST) {
#else
            if (mkdir(dir, 0755) != 0 && errno != EEXIST) {
#endif
                /* 忽略非致命错误 */
            }
            *p = saved;
        }
    }

#ifdef _WIN32
    if (_mkdir(dir) != 0 && errno != EEXIST) {
#else
    if (mkdir(dir, 0755) != 0 && errno != EEXIST) {
#endif
        return -1;
    }
    return 0;
}

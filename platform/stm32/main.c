/*
 * YUI STM32 宿主壳（示例入口，F7 系列：LTDC + DMA2D + SDRAM）
 *
 * 集成方式（推荐 CubeIDE/Keil 工程）：
 *   1. 用 STM32CubeMX 生成工程（LTDC、DMA2D、SDRAM、触摸 GPIO 配置），
 *      保留其生成的 Drivers/、ltdc.c、dma2d.c、sdram.c、touch.c 与
 *      SystemClock_Config()。
 *   2. 先编译 yui 核心库：  ya -p stm32
 *      （产物 build/stm32/None/libyui.a，含 backend_stm32.c + backend_embed_font.c）
 *   3. 把本 main.c 加入工程，链接 libyui.a / libcjson.a / libtsm.a 以及
 *      STM32 HAL/BSP 库。
 *
 * 也可以试： ya -p stm32 -b yui-stm32（需在工程 include 路径中提供 HAL/BSP）。
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "backend.h"
#include "layer.h"
#include "layout.h"
#include "render.h"
#include "popup_manager.h"
#include "cJSON.h"

#ifdef STM32F7xx
#include "stm32f7xx_hal.h"
#endif

/* CubeMX 生成的系统时钟配置（用户工程提供） */
extern void SystemClock_Config(void);

/* 简单 UI 描述（也可改为从文件系统加载 JSON） */
static const char s_ui_json[] =
    "{"
    "  \"type\": \"container\","
    "  \"text\": \"YUI STM32\","
    "  \"layout\": \"vertical\","
    "  \"style\": {\"bg-color\": \"#102040\", \"color\": \"#ffffff\", \"font-size\": 20},"
    "  \"children\": ["
    "    {\"type\": \"label\", \"text\": \"Hello YUI\","
    "     \"style\": {\"color\": \"#ffd700\", \"font-size\": 24}}"
    "  ]"
    "}";

int main(void) {
    cJSON* json;
    Layer* ui_root;
    DFont* font;

    /* HAL 初始化（CubeMX 生成的时钟/外设配置） */
    HAL_Init();
    SystemClock_Config();

    /* 初始化后端（framebuffer + 触摸）+ 弹层管理 */
    backend_init();
    popup_manager_init();

    /* 加载字体（从文件系统或烧录到外部 Flash 的字体，按实际调整） */
    font = backend_load_font("font.ttf", 16);

    /* 构建 UI */
    json = cJSON_Parse(s_ui_json);
    ui_root = layer_create_from_json(json, NULL);
    cJSON_Delete(json);
    if (!ui_root) {
        backend_quit();
        return -1;
    }

    if (font) {
        ui_root->font = (Font*)malloc(sizeof(Font));
        memset(ui_root->font, 0, sizeof(Font));
        ui_root->font->default_font = font;
        ui_root->font->size = 16;
        strcpy(ui_root->font->path, "font.ttf");
    }
    ui_root->assets = (Assets*)malloc(sizeof(Assets));
    strcpy(ui_root->assets->path, "assets");

    int w, h;
    backend_get_windowsize(&w, &h);
    if (ui_root->rect.w <= 0) ui_root->rect.w = w;
    if (ui_root->rect.h <= 0) ui_root->rect.h = h;

    load_all_fonts(ui_root);
    load_textures(ui_root);
    layout_layer(ui_root);

    /* 主循环（内部不返回） */
    backend_run(ui_root);
    backend_quit();
    return 0;
}

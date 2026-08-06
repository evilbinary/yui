# YUI ESP32 宿主壳组件
# 编译入口 main.c + ESP32 后端，并链接 ya 编译的 yui 核心库。
#
# 前置：
#   ya -p esp32 -a esp32c3
# （产物在仓库根 build/esp32/esp32c3/None/ 下）

set(IDF_SKIP_DEFAULT_COMPONENTS ON)
set(CONFIG_SDKCONFIG_AUTO_SAVE OFF)
set(CONFIG_SDKCONFIG_AUTO_GEN OFF)
set(YUI_ROOT ${CMAKE_CURRENT_LIST_DIR}/../../..)

idf_component_register(
    SRCS "main.c" "${YUI_ROOT}/src/backend/backend_esp32.c" "${YUI_ROOT}/lib/socket/socket.c"
    INCLUDE_DIRS
        "${YUI_ROOT}/src"
        "${YUI_ROOT}/src/backend"
        "${YUI_ROOT}/lib/stb"
        "${YUI_ROOT}/lib/cjson"
        "${YUI_ROOT}/lib/jsmodule"
        "${YUI_ROOT}/lib/jsmodule-mquickjs"
        "${YUI_ROOT}/lib/socket"
    REQUIRES esp_lcd esp_lcd_touch esp_lcd_qemu_rgb esp_timer driver nvs_flash spi_flash spiffs lwip
)

# 嵌入式模式：ytype.h 使用 YuiTexture/YuiFont，不依赖 SDL
target_compile_definitions(${COMPONENT_LIB} PRIVATE YUI_BACKEND_EMBEDDED)

# 必须与 ya.py 的核心库编译参数一致，否则 sizeof(Layer)/sizeof(Font) 不匹配
# ytype.h 中 YUI_MAX_PATH 默认 1024，但 ya.py 编译核心库时用 256
target_compile_definitions(${COMPONENT_LIB} PRIVATE
    YUI_MAX_PATH=256
    YUI_PATH_MAX=256
    MAX_JS_EVENTS=64
    MAX_C_EVENT_HANDLERS=32
    YUI_LAYER_ID_MAX=32
    YUI_LAYER_VARIANT_MAX=32
    YUI_LIFECYCLE_NAME_MAX=32
    JS_MEM_POOL_SIZE=84*1024
)

# QEMU 构建：跳过真实 LCD/触摸（host 设 YUI_ESP32_QEMU=1）
if(DEFINED ENV{YUI_ESP32_QEMU})
    target_compile_definitions(${COMPONENT_LIB} PRIVATE YUI_ESP32_QEMU=1)
    message(STATUS "YUI: QEMU build (software framebuffer only)")
endif()

# 链接 ya 编译的 yui 核心库（按实际 set-target 调整 arch 目录）
set(YUI_BUILD_DIR ${YUI_ROOT}/build/esp32/esp32c3/None)
target_link_libraries(${COMPONENT_LIB} PRIVATE
    # esp32 不链接 libsocket.a（桌面库，内部裸引用 POSIX socket 符号）。
    # socket 后端由本组件编译的 lib/socket/socket.c 提供（IDF 环境吃到
    # lwip 头宏，无裸符号）；--undefined=_socket 强制链接器提前拉入它，
    # 使 libjsmodule.a 中的 _socket/_connect/... 引用得以解析。
    -Wl,--undefined=_socket
    ${YUI_BUILD_DIR}/libyui.a
    ${YUI_BUILD_DIR}/libcjson.a
    ${YUI_BUILD_DIR}/libtsm.a
    # JS 模块：jsmodule-mqjs(mquickjs 引擎) 与 mquickjs 归档内部成员互相引用
    # （mquickjs.o 定义 JS_* 而 mqjs_std.o 引用，单趟扫描顺序依赖），
    # 用 --start-group 允许多趟解析。
    -Wl,--start-group
    ${YUI_BUILD_DIR}/libjsmodule-mquickjs.a
    ${YUI_BUILD_DIR}/libmquickjs.a
    -Wl,--end-group
    m
)

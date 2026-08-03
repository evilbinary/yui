# YUI ESP32 宿主壳组件
# 编译入口 main.c + ESP32 后端，并链接 ya 编译的 yui 核心库。
#
# 前置：
#   ya -p esp32 -a esp32c3
# （产物在仓库根 build/esp32/esp32c3/None/ 下）

set(YUI_ROOT ${CMAKE_CURRENT_LIST_DIR}/../../..)

idf_component_register(
    SRCS "main.c" "${YUI_ROOT}/src/backend/backend_esp32.c"
    INCLUDE_DIRS
        "${YUI_ROOT}/src"
        "${YUI_ROOT}/src/backend"
        "${YUI_ROOT}/lib/stb"
        "${YUI_ROOT}/lib/cjson"
    REQUIRES esp_lcd esp_lcd_touch esp_timer driver nvs_flash spi_flash
)

# 嵌入式模式：ytype.h 使用 YuiTexture/YuiFont，不依赖 SDL
target_compile_definitions(${COMPONENT_LIB} PRIVATE YUI_BACKEND_EMBEDDED)

# 链接 ya 编译的 yui 核心库（按实际 set-target 调整 arch 目录）
set(YUI_BUILD_DIR ${YUI_ROOT}/build/esp32/esp32c3/None)
target_link_libraries(${COMPONENT_LIB} PRIVATE
    ${YUI_BUILD_DIR}/libyui.a
    ${YUI_BUILD_DIR}/libcjson.a
    ${YUI_BUILD_DIR}/libtsm.a
    m
)

cmake_minimum_required(VERSION 3.22)

# Required for Android 15+ devices/emulators with 16KB page size.
set(ANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES ON)

project(yui_android)

if(NOT DEFINED YUI_REPO_ROOT)
    set(YUI_REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../../../..")
endif()
get_filename_component(YUI_REPO_ROOT "${YUI_REPO_ROOT}" ABSOLUTE)
string(REPLACE "\\" "/" YUI_REPO_ROOT "${YUI_REPO_ROOT}")

set(YUI_PREBUILT "${YUI_REPO_ROOT}/third_party/yui-prebuilt/android/${ANDROID_ABI}")

function(import_yui_static name filename)
    add_library(${name} STATIC IMPORTED)
    set_target_properties(${name} PROPERTIES
        IMPORTED_LOCATION "${YUI_PREBUILT}/${filename}")
endfunction()

import_yui_static(yui "libyui.a")
import_yui_static(cjson "libcjson.a")
import_yui_static(yaml "libyaml.a")
import_yui_static(yaml2json "libyaml2json.a")
import_yui_static(quickjs "libquickjs.a")
import_yui_static(jsmodule "libjsmodule-quickjs.a")
import_yui_static(socket "libsocket.a")

add_library(yui_android_jni SHARED
    "${YUI_REPO_ROOT}/platform/android/jni/yui_bridge.cpp"
    "${YUI_REPO_ROOT}/platform/common/yui_boot.c"
)

target_include_directories(yui_android_jni PRIVATE
    "${YUI_REPO_ROOT}/platform/common"
    "${YUI_REPO_ROOT}/src"
    "${YUI_REPO_ROOT}/lib/cjson"
    "${YUI_REPO_ROOT}/lib/yaml2json"
    "${YUI_REPO_ROOT}/lib/jsmodule"
    "${YUI_REPO_ROOT}/lib/jsmodule-quickjs"
)

target_compile_definitions(yui_android_jni PRIVATE HAS_JS_MODULE YUI_BACKEND_MOBILE)

find_library(log-lib log)
find_library(android-lib android)
find_library(egl-lib EGL)
find_library(gles-lib GLESv2)

target_link_options(yui_android_jni PRIVATE
    "-Wl,-z,max-page-size=16384"
)

target_link_libraries(yui_android_jni
    yui
    jsmodule
    quickjs
    socket
    yaml2json
    yaml
    cjson
    ${log-lib}
    ${android-lib}
    ${egl-lib}
    ${gles-lib}
)

cmake_minimum_required(VERSION 3.22)

if(NOT IOS_SDK)
    set(IOS_SDK "iphoneos" CACHE STRING "iphoneos or iphonesimulator")
endif()

if(NOT IOS_ARCH)
    if(IOS_SDK STREQUAL "iphonesimulator")
        set(IOS_ARCH "arm64" CACHE STRING "arm64 or x86_64")
    else()
        set(IOS_ARCH "arm64" CACHE STRING "iOS device arch")
    endif()
endif()

set(CMAKE_SYSTEM_NAME iOS)
set(CMAKE_OSX_DEPLOYMENT_TARGET "15.0" CACHE STRING "Minimum iOS version")
set(CMAKE_OSX_ARCHITECTURES "${IOS_ARCH}")
set(CMAKE_OSX_SYSROOT "${IOS_SDK}")

project(YuiApp LANGUAGES C CXX Swift OBJCXX)

set(YUI_REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/../..")
set(YUI_PREBUILT "${YUI_REPO_ROOT}/third_party/yui-prebuilt/ios/${IOS_SDK}/${IOS_ARCH}")

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

set(SWIFT_OBJC_BRIDGING_HEADER "${CMAKE_CURRENT_LIST_DIR}/YuiApp/YuiBridgingHeader.h")

add_executable(YuiApp MACOSX_BUNDLE
    YuiApp/YuiApp.swift
    YuiApp/YuiViewController.swift
    YuiBridge.mm
    ios_metal_glue.mm
    ../common/yui_boot.c
)

set_source_files_properties(YuiApp/YuiApp.swift YuiApp/YuiViewController.swift PROPERTIES
    Swift_OBJC_BRIDGING_HEADER "${SWIFT_OBJC_BRIDGING_HEADER}")

target_include_directories(YuiApp PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}"
    "${YUI_REPO_ROOT}/platform/common"
    "${YUI_REPO_ROOT}/src"
    "${YUI_REPO_ROOT}/lib/cjson"
    "${YUI_REPO_ROOT}/lib/yaml2json"
    "${YUI_REPO_ROOT}/lib/jsmodule"
    "${YUI_REPO_ROOT}/lib/jsmodule-quickjs"
)

target_compile_definitions(YuiApp PRIVATE HAS_JS_MODULE YUI_BACKEND_MOBILE)

set_target_properties(YuiApp PROPERTIES
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_LIST_DIR}/YuiApp/Info.plist"
    XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "com.yui.app"
    XCODE_ATTRIBUTE_SWIFT_VERSION "5.0"
    XCODE_ATTRIBUTE_CLANG_ENABLE_MODULES "YES"
    XCODE_ATTRIBUTE_ALWAYS_EMBED_SWIFT_STANDARD_LIBRARIES "YES"
)

target_link_libraries(YuiApp PRIVATE
    yui
    jsmodule
    quickjs
    socket
    yaml2json
    yaml
    cjson
    "-framework Metal"
    "-framework MetalKit"
    "-framework UIKit"
    "-framework QuartzCore"
    "-framework Foundation"
)

file(GLOB_RECURSE YUI_APP_RESOURCES
    "${YUI_REPO_ROOT}/app/assets/*"
    "${YUI_REPO_ROOT}/app/lib/*"
    "${YUI_REPO_ROOT}/app/watch-os/*"
    "${YUI_REPO_ROOT}/app/tests/login.json"
)

foreach(resource ${YUI_APP_RESOURCES})
    file(RELATIVE_PATH rel "${YUI_REPO_ROOT}" "${resource}")
    set_source_files_properties("${resource}" PROPERTIES MACOSX_PACKAGE_LOCATION "Resources/${rel}")
    target_sources(YuiApp PRIVATE "${resource}")
endforeach()

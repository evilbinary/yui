# YUI Developer Guide

## 目录
- [环境搭建](#环境搭建)
- [项目结构](#项目结构)
- [开发工作流](#开发工作流)
- [调试技巧](#调试技巧)
- [性能优化](#性能优化)
- [代码规范](#代码规范)
- [测试指南](#测试指南)

## 环境搭建

### 开发环境要求

**操作系统支持**：
- Windows 10/11 (推荐使用 MSYS2)
- macOS 10.15+
- Ubuntu 20.04+/Debian 11+

**必备工具**：
- Python 3.7+ (用于构建系统)
- Git
- C 编译器 (GCC/Clang)
- CMake (可选，用于某些依赖)

### Windows 环境配置

1. **安装 MSYS2**：
   ```bash
   # 从 https://www.msys2.org/ 下载并安装
   ```

2. **更新包管理器**：
   ```bash
   pacman -Syu
   pacman -Su
   ```

3. **安装依赖**：
   ```bash
   pacman -S mingw-w64-x86_64-gcc
   pacman -S mingw-w64-x86_64-SDL2
   pacman -S mingw-w64-x86_64-SDL2_ttf
   pacman -S mingw-w64-x86_64-SDL2_image
   pacman -S mingw-w64-x86_64-cjson
   pacman -S mingw-w64-x86_64-dlfcn
   pacman -S mingw-w64-x86_64-make
   pacman -S mingw-w64-x86_64-pkg-config
   ```

4. **设置环境变量**：
   ```powershell
   $env:Path = "C:\msys64\mingw64\bin;C:\msys64\usr\bin;" + $env:Path
   ```

### macOS 环境配置

1. **安装 Homebrew**：
   ```bash
   /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
   ```

2. **安装依赖**：
   ```bash
   brew install sdl2 sdl2_ttf sdl2_image cjson
   brew install python3
   ```

### Linux 环境配置

```bash
# Ubuntu/Debian
sudo apt-get update
sudo apt-get install build-essential
sudo apt-get install libsdl2-dev libsdl2-ttf-dev libsdl2-image-dev libcjson-dev
sudo apt-get install python3 python3-pip

# Fedora/RHEL
sudo dnf install gcc SDL2-devel SDL2_ttf-devel SDL2_image-devel cjson-devel
sudo dnf install python3 python3-pip
```

## 项目结构

```
yui/
├── app/                    # 应用程序目录
│   ├── assets/            # 资源文件（图片、字体）
│   ├── js/               # JavaScript 脚本
│   ├── lib/              # 库文件
│   └── *.json            # UI 配置文件
├── docs/                 # 文档目录
├── lib/                  # 第三方库
│   ├── cjson/           # cJSON 库
│   ├── jsmodule/        # JavaScript 模块
│   ├── quickjs/         # QuickJS 引擎
│   └── mquickjs/        # mquickjs 引擎
├── src/                  # 源代码目录
│   ├── components/      # 组件实现
│   ├── *.c             # 核心源文件
│   ├── *.h             # 头文件
│   └── main.c          # 主入口
├── tests/               # 测试文件
├── Makefile            # Make 构建文件
├── ya.py               # Python 构建脚本
└── README.md           # 项目说明
```

### 核心模块说明

| 模块 | 文件 | 功能 |
|------|------|------|
| 图层系统 | `layer.c/h` | UI 层级管理和渲染 |
| 渲染系统 | `render.c/h` | 图形渲染管线 |
| 事件系统 | `event.c/h` | 用户输入处理 |
| 布局系统 | `layout.c/h` | 自动布局管理 |
| 动画系统 | `animate.c/h` | 动画效果实现 |
| 主题系统 | `theme.c/h` | 主题管理和应用 |
| 后端抽象 | `backend.c/h` | 平台相关接口 |

## 开发工作流

### 1. 克隆和初始化

```bash
git clone https://github.com/evilbinary/YUI.git
cd YUI
git submodule update --init --recursive
```

### 2. 构建项目

```bash
# 使用 Python 构建脚本
python ya.py build

# 或者使用 Make
make

# 清理构建
python ya.py clean
# 或
make clean
```

### 3. 运行应用

```bash
# 运行默认应用
python ya.py run yui

# 运行指定配置
python ya.py run -c app/test.json

# 调试模式
python ya.py run --debug
```

### 4. 开发循环

```bash
# 1. 修改代码
# 2. 重新构建
python ya.py -b target name

# 3. 运行测试
python ya.py -r target name

# 4. 验证功能
python ya.py -r target name
```

## 调试技巧

### 日志系统

YUI 内置日志系统，可通过不同级别输出调试信息：

```c
#include "util.h"

// 不同级别的日志
LOG_DEBUG("调试信息: %d", value);
LOG_INFO("普通信息");
LOG_WARN("警告信息");
LOG_ERROR("错误信息");
```

### 调试工具

#### 1. Inspector 模式

启用 UI 检查器查看组件结构：

```javascript
// 在 JS 中启用
YUI.inspect.enable();

// 为特定组件启用
YUI.inspect.setLayer("componentId", true);

// 显示边界框
YUI.inspect.setShowBounds(true);

// 显示详细信息
YUI.inspect.setShowInfo(true);
```

#### 2. 内存调试

```bash
# 使用 Valgrind 检测内存泄漏 (Linux/macOS)
valgrind --leak-check=full ./yui

# Windows 下使用 Dr. Memory
drmemory -- ./yui.exe
```

#### 3. 性能分析

```bash
# 使用 gprof (需要编译时添加 -pg 选项)
./yui
gprof yui gmon.out > analysis.txt
```

### 调试配置

在 `app.json` 中添加调试配置：

```json
{
    "type": "main",
    "debug": {
        "enable_inspector": true,
        "show_fps": true,
        "log_level": "debug"
    }
}
```

## 性能优化

### 渲染优化

1. **使用脏矩形渲染**：
   ```c
   // 只重绘变化的区域
   layer_set_dirty(layer, true);
   ```

2. **纹理缓存**：
   ```c
   // 重用相同纹理避免重复加载
   Texture* texture = texture_cache_get("image.png");
   ```

3. **批处理绘制**：
   ```c
   // 合并相似的绘制操作
   batch_begin();
   // 多个绘制调用
   batch_end();
   ```

### 内存优化

1. **及时释放资源**：
   ```c
   layer_destroy(layer);
   texture_free(texture);
   ```

### 动画优化

1. **使用合适的帧率**：
   ```c
   // 通常 60 FPS 足够
   animation_set_fps(animation, 60);
   ```

2. **避免复杂计算**：
   ```c
   // 预计算复杂表达式
   static float cached_values[100];
   ```

## 代码规范

### C 代码规范

#### 命名约定

```c
// 类型定义 - 驼峰命名
typedef struct Layer Layer;
typedef struct ButtonComponent ButtonComponent;

// 函数名 - 下划线分隔
void layer_create(void);
void button_component_init(ButtonComponent* btn);

// 变量名 - 下划线分隔
int button_width;
char* layer_name;

// 常量 - 全大写
#define MAX_LAYERS 100
#define DEFAULT_FONT_SIZE 16
```

#### 文件结构

```c
// header.h
#ifndef HEADER_H
#define HEADER_H

// 包含保护
#include <stdio.h>

// 前向声明
struct Layer;

// 类型定义
typedef struct MyStruct MyStruct;

// 函数声明
MyStruct* my_struct_create(void);
void my_struct_destroy(MyStruct* s);

#endif // HEADER_H

// source.c
#include "header.h"
#include "other_header.h"

// 静态函数声明
static void helper_function(void);

// 全局变量定义
static int global_counter = 0;

// 函数实现
MyStruct* my_struct_create(void) {
    MyStruct* s = malloc(sizeof(MyStruct));
    if (!s) return NULL;
    
    // 初始化
    s->field = 0;
    return s;
}
```

#### 错误处理

```c
// 返回错误码
int function_with_error_handling(void) {
    if (condition) {
        LOG_ERROR("Error condition occurred");
        return -1;
    }
    return 0;
}

// 使用断言进行调试
void critical_function(void* ptr) {
    assert(ptr != NULL);
    // 函数逻辑
}
```

### JSON 配置规范

```json
{
    "id": "unique_identifier",
    "type": "ComponentType",
    "position": [x, y],
    "size": [width, height],
    "style": {
        "color": "#RRGGBB",
        "fontSize": 16
    },
    "events": {
        "onClick": "handlerFunction"
    },
    "children": []
}
```

### JavaScript 规范

```javascript
// 使用严格模式
'use strict';

// 函数命名 - 驼峰命名
function handleClickEvent() {
    // 函数体
}

// 变量声明
const CONSTANT_VALUE = 100;
let mutableVariable = 0;

// 错误处理
try {
    YUI.update(jsonData);
} catch (error) {
    YUI.log("Update failed:", error.message);
}
```

## 测试指南

### 单元测试

创建测试文件 `tests/test_my_feature.c`：

```c
#include "my_feature.h"
#include <assert.h>

void test_basic_functionality(void) {
    MyStruct* obj = my_struct_create();
    assert(obj != NULL);
    
    int result = my_function(obj, 42);
    assert(result == expected_value);
    
    my_struct_destroy(obj);
}

int main(void) {
    test_basic_functionality();
    printf("All tests passed!\n");
    return 0;
}
```

### 集成测试

使用现有的测试应用：

```bash
# 运行特定测试
python ya.py run -c app/tests/test-button.json

# 运行所有测试
./run_tests.sh
```

### 自动化测试脚本

创建 `test_runner.py`：

```python
#!/usr/bin/env python3
import subprocess
import sys

def run_test(config_file):
    try:
        result = subprocess.run([
            'python', 'ya.py', 'run', '-c', config_file
        ], timeout=30, capture_output=True, text=True)
        
        if result.returncode == 0:
            print(f"✅ {config_file}: PASSED")
            return True
        else:
            print(f"❌ {config_file}: FAILED")
            print(result.stderr)
            return False
    except subprocess.TimeoutExpired:
        print(f"⏰ {config_file}: TIMEOUT")
        return False

def main():
    test_configs = [
        'app/tests/test-button.json',
        'app/tests/test-layout.json',
        'app/tests/test-animation.json'
    ]
    
    passed = 0
    total = len(test_configs)
    
    for config in test_configs:
        if run_test(config):
            passed += 1
    
    print(f"\nResults: {passed}/{total} tests passed")
    sys.exit(0 if passed == total else 1)

if __name__ == '__main__':
    main()
```

## 常见问题解答

### Q: 构建失败，找不到 SDL2 头文件？

A: 确保 SDL2 正确安装并设置了 PKG_CONFIG_PATH：
```bash
export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:$PKG_CONFIG_PATH
```

### Q: 运行时报错找不到动态库？

A: 设置库路径：
```bash
# Linux
export LD_LIBRARY_PATH=./lib:$LD_LIBRARY_PATH

# macOS  
export DYLD_LIBRARY_PATH=./lib:$DYLD_LIBRARY_PATH

# Windows
set PATH=%PATH%;.\lib
```

### Q: 如何调试 JavaScript 代码？

A: 使用 YUI.inspect 工具或者在 JS 代码中添加日志：
```javascript
YUI.log("Debug info:", variable);
```

---

*持续更新中...*
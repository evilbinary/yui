# Contribution Guidelines

感谢您对 YUI 项目的兴趣！我们欢迎任何形式的贡献，包括但不限于代码提交、文档改进、bug 报告和功能建议。

## 目录
- [行为准则](#行为准则)
- [如何贡献](#如何贡献)
- [开发流程](#开发流程)
- [代码规范](#代码规范)
- [提交规范](#提交规范)
- [文档规范](#文档规范)
- [测试要求](#测试要求)

## 行为准则

### 我们的价值观

1. **尊重他人** - 对所有贡献者保持礼貌和专业
2. **开放包容** - 欢迎不同的观点和技术背景
3. **协作共赢** - 通过合作创造更好的解决方案
4. **持续学习** - 乐于分享知识，共同成长

### 不可接受的行为

- 使用侮辱性语言或人身攻击
- 发布歧视性内容
- 扰乱社区讨论
- 泄露他人隐私信息
- 提交恶意代码

## 如何贡献

### 1. 报告 Bug

在提交 bug 报告前，请：

- 搜索现有 issues，避免重复报告
- 使用最新版本测试
- 提供详细的重现步骤
- 包含环境信息（操作系统、编译器版本等）

**Bug 报告模板**：
```markdown
**描述问题**
简明扼要地描述 bug 是什么

**重现步骤**
1. 打开 '...'
2. 点击 '....'
3. 滚动到 '....'
4. 看到错误

**期望行为**
清楚描述你期望发生什么

**截图**
如果适用，添加截图帮助解释问题

**环境信息**
- OS: [例如 Windows 10]
- YUI 版本: [例如 v1.2.3]
- 编译器: [例如 GCC 9.3.0]

**附加信息**
任何其他相关信息
```

### 2. 提出功能建议

好的功能建议应该包括：

- 清晰的问题陈述
- 解决方案描述
- 替代方案考虑
- 使用场景说明

### 3. 代码贡献

#### 开发前准备

1. Fork 项目仓库
2. 克隆到本地：
   ```bash
   git clone https://github.com/your-username/YUI.git
   cd YUI
   ```
3. 添加上游仓库：
   ```bash
   git remote add upstream https://github.com/evilbinary/YUI.git
   ```

#### 开发流程

1. **创建特性分支**：
   ```bash
   git checkout -b feature/your-feature-name
   # 或修复分支
   git checkout -b fix/issue-description
   ```

2. **编写代码**：
   - 遵循代码规范
   - 添加必要的注释
   - 编写单元测试

3. **测试代码**：
   ```bash
   # 构建项目
   python ya.py -b xxx
   
   # 运行测试
   python ya.py test
   
   # 运行特定测试
   python ya.py -r  test xx
   ```

4. **提交更改**：
   ```bash
   git add .
   git commit -m "feat: add new feature description"
   git push origin feature/your-feature-name
   ```

5. **创建 Pull Request**

## 开发流程

### 分支策略

```
main (稳定版本)
├── develop (开发分支)
│   ├── feature/new-feature
│   ├── fix/bug-fix
│   └── hotfix/critical-issue
└── release/v1.x.x (发布候选)
```

### 版本管理

我们遵循 [语义化版本控制](https://semver.org/lang/zh-CN/)：

- **MAJOR**: 不兼容的 API 修改
- **MINOR**: 向后兼容的功能性新增
- **PATCH**: 向后兼容的问题修正

### Pull Request 流程

1. **PR 提交要求**：
   - 标题使用 conventional commits 格式
   - 描述清楚变更内容和影响
   - 关联相关的 issues
   - 通过所有 CI 检查

2. **代码审查**：
   - 至少需要一位维护者批准
   - 解决所有评论和建议
   - 确保测试通过

3. **合并条件**：
   - 所有 CI 检查通过
   - 代码审查获得批准
   - 符合代码规范要求

## 代码规范

### C 语言规范

#### 命名约定

```c
// 类型定义 - 驼峰命名法
typedef struct Layer Layer;
typedef struct ButtonComponent ButtonComponent;
typedef enum LayerType LayerType;

// 函数名 - 下划线命名法
void layer_create(void);
void button_component_init(ButtonComponent* btn);
int util_string_compare(const char* str1, const char* str2);

// 变量名 - 下划线命名法
int button_width;
char* layer_name;
bool is_visible;

// 常量 - 全大写加下划线
#define MAX_LAYERS 100
#define DEFAULT_FONT_SIZE 16
#define INVALID_ID (-1)
```

#### 文件组织

```c
// header.h - 头文件模板
#ifndef HEADER_H
#define HEADER_H

// 系统头文件
#include <stdio.h>
#include <stdlib.h>

// 项目头文件
#include "common.h"

// 前向声明
struct Layer;

// 类型定义
typedef struct MyStruct MyStruct;
typedef enum MyEnum MyEnum;

// 常量定义
#define MY_CONSTANT 42

// 函数声明
MyStruct* my_struct_create(void);
void my_struct_destroy(MyStruct* s);
int my_struct_process(MyStruct* s, int param);

#endif // HEADER_H

// source.c - 源文件模板
#include "header.h"
#include "other_header.h"

// 静态函数声明（如果需要）
static void helper_function(int param);
static bool validate_input(const char* input);

// 全局变量定义
static int global_counter = 0;
static const char* module_name = "MyModule";

// 函数实现
MyStruct* my_struct_create(void) {
    MyStruct* s = malloc(sizeof(MyStruct));
    if (!s) {
        LOG_ERROR("Failed to allocate MyStruct");
        return NULL;
    }
    
    // 初始化成员
    s->field1 = 0;
    s->field2 = NULL;
    s->initialized = false;
    
    return s;
}

void my_struct_destroy(MyStruct* s) {
    if (!s) return;
    
    // 清理资源
    if (s->field2) {
        free(s->field2);
    }
    
    free(s);
}

// 静态函数实现
static void helper_function(int param) {
    // 实现细节
}
```

#### 错误处理

```c
// 返回错误码的方式
int function_that_can_fail(void) {
    if (!condition) {
        LOG_ERROR("Precondition failed");
        return -1;  // 或定义的错误码
    }
    
    // 执行操作
    return 0;  // 成功
}

// 使用枚举定义错误码
typedef enum {
    ERROR_NONE = 0,
    ERROR_INVALID_PARAM,
    ERROR_OUT_OF_MEMORY,
    ERROR_FILE_NOT_FOUND
} ErrorCode;

ErrorCode process_data(const char* data) {
    if (!data) {
        return ERROR_INVALID_PARAM;
    }
    
    char* buffer = malloc(strlen(data) + 1);
    if (!buffer) {
        return ERROR_OUT_OF_MEMORY;
    }
    
    // 处理数据...
    free(buffer);
    return ERROR_NONE;
}
```

#### 内存管理

```c
// RAII 风格的资源管理
typedef struct Resource {
    void* data;
    size_t size;
} Resource;

Resource* resource_create(size_t size) {
    Resource* res = malloc(sizeof(Resource));
    if (!res) return NULL;
    
    res->data = malloc(size);
    if (!res->data) {
        free(res);
        return NULL;
    }
    
    res->size = size;
    return res;
}

void resource_destroy(Resource* res) {
    if (!res) return;
    if (res->data) free(res->data);
    free(res);
}
```

### JavaScript 规范

```javascript
// 使用严格模式
'use strict';

// 常量和变量声明
const CONSTANT_VALUE = 100;
let mutableVariable = 0;
var legacyVariable; // 尽量避免使用

// 函数声明 - 驼峰命名法
function handleClickEvent(elementId) {
    // 函数体
    return true;
}

// 箭头函数
const processData = (data) => {
    return data.map(item => item.value);
};

// 对象字面量
const config = {
    apiUrl: 'https://api.example.com',
    timeout: 5000,
    retries: 3
};

// 错误处理
try {
    const result = YUI.update(jsonData);
    if (!result) {
        throw new Error('Update failed');
    }
} catch (error) {
    YUI.log('Error:', error.message);
    // 处理错误
}
```

### JSON 配置规范

```json
{
    "id": "unique_identifier",
    "type": "ComponentType",
    "position": [0, 0],
    "size": [100, 50],
    "style": {
        "color": "#FF0000",
        "fontSize": 16,
        "borderRadius": 4
    },
    "events": {
        "onClick": "handleClick",
        "onHover": "handleHover"
    },
    "children": [],
    "metadata": {
        "version": "1.0.0",
        "author": "Developer Name"
    }
}
```

## 提交规范

我们使用 [Conventional Commits](https://www.conventionalcommits.org/) 规范：

### 提交类型

| 类型 | 说明 | 示例 |
|------|------|------|
| feat | 新功能 | `feat: add dropdown component` |
| fix | 修复 bug | `fix: resolve memory leak in layer system` |
| docs | 文档更新 | `docs: update API documentation` |
| style | 代码格式调整 | `style: format code with clang-format` |
| refactor | 代码重构 | `refactor: optimize event dispatch system` |
| perf | 性能优化 | `perf: improve rendering performance` |
| test | 测试相关 | `test: add unit tests for layout manager` |
| chore | 构建/工具变更 | `chore: update build scripts` |
| ci | CI 配置变更 | `ci: add GitHub Actions workflow` |

### 提交消息格式

```
<type>(<scope>): <subject>

<body>

<footer>
```

### 示例

```bash
# 简单提交
git commit -m "feat(button): add hover effect support"

# 详细提交
git commit -m "fix(render): resolve memory leak in texture cache

- Fixed issue where textures were not properly released
- Added automatic cleanup mechanism
- Improved memory usage tracking

Fixes #123"
```

## 文档规范

### 文档结构

所有文档应包含以下部分：

1. **概述** - 简要介绍文档内容
2. **目录** - 方便导航的目录（较长文档）
3. **主要内容** - 详细的说明和示例
4. **参考资料** - 相关链接和进一步阅读

### Markdown 规范

```markdown
# 一级标题

## 二级标题

### 三级标题

**粗体文本** 和 *斜体文本*

- 无序列表项
- 另一个列表项

1. 有序列表项
2. 第二项

[链接文本](https://example.com)

`行内代码`

```c
// 代码块
int main() {
    return 0;
}
```

> 引用块内容

| 表格标题1 | 表格标题2 |
|-----------|-----------|
| 内容1     | 内容2     |
```

### API 文档格式

```c
/**
 * @brief 简短描述函数功能
 * 
 * 详细描述函数的作用、使用场景和注意事项
 * 
 * @param param1 参数1的描述
 * @param param2 参数2的描述
 * @return 返回值描述
 * 
 * @example
 * ```c
 * int result = example_function(42, "test");
 * ```
 */
int example_function(int param1, const char* param2);
```

## 测试要求

### 测试覆盖范围

每个功能都应该有相应的测试：

1. **单元测试** - 测试单个函数或组件
2. **集成测试** - 测试多个组件协同工作
3. **回归测试** - 确保修复不会引入新问题

### 测试文件组织

```
tests/
├── unit/                 # 单元测试
│   ├── test_layer.c
│   ├── test_render.c
│   └── test_event.c
├── integration/          # 集成测试
│   ├── test_layout_components.json
│   └── test_animation_system.json
└── regression/           # 回归测试
    ├── test_bug_fixes.json
    └── test_memory_leaks.c
```

### 测试编写规范

```c
// test_example.c
#include "example.h"
#include <assert.h>
#include <stdio.h>

// 测试用例命名：test_[模块]_[功能]_[场景]
void test_layer_create_success(void) {
    Layer* layer = layer_create(LAYER_VIEW);
    
    assert(layer != NULL);
    assert(layer->type == LAYER_VIEW);
    assert(layer->visible == true);
    
    layer_destroy(layer);
    printf("✓ test_layer_create_success passed\n");
}

void test_layer_create_invalid_type(void) {
    Layer* layer = layer_create(-1);
    
    assert(layer == NULL);
    
    printf("✓ test_layer_create_invalid_type passed\n");
}

// 测试套件入口
int main(void) {
    printf("Running layer tests...\n");
    
    test_layer_create_success();
    test_layer_create_invalid_type();
    
    printf("All tests passed!\n");
    return 0;
}
```

### JSON 测试配置

```json
{
    "id": "test_button_click",
    "type": "TestSuite",
    "description": "测试按钮点击功能",
    "setup": {
        "actions": [
            {"type": "create", "component": "Button", "id": "testBtn"}
        ]
    },
    "tests": [
        {
            "name": "按钮点击事件触发",
            "actions": [
                {"type": "click", "target": "testBtn"}
            ],
            "expectations": [
                {"property": "clicked", "value": true}
            ]
        }
    ],
    "teardown": {
        "actions": [
            {"type": "destroy", "target": "testBtn"}
        ]
    }
}
```

## 社区参与

### 讨论渠道

- **GitHub Issues** - Bug 报告和功能讨论
- **GitHub Discussions** - 一般性讨论和技术交流
- **Pull Requests** - 代码审查和反馈

### 贡献认可

我们会定期更新贡献者名单，在以下地方致谢：

- README.md 贡献者列表
- 发布日志
- 项目网站

## 许可证

通过向该项目贡献代码，您同意您的贡献将根据项目的 MIT 许可证进行许可。

---

如果您有任何疑问，请随时在 GitHub Issues 中提问！

Happy coding! 🎉
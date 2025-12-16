# Dialog Component 对话框组件

## 概述

Dialog组件是基于Popup系统开发的模态和非模态对话框组件，支持多种对话框类型和自定义样式。

## 功能特性

- 🎨 **多种对话框类型**: 信息、警告、错误、问题、自定义
- 🖱️ **完整的鼠标交互**: 点击按钮、悬停效果
- ⌨️ **键盘支持**: 回车确认、ESC取消、Tab切换
- 🎯 **模态/非模态**: 支持模态和非模态对话框
- 🎨 **自定义样式**: 可自定义颜色、大小等外观
- 📱 **DPI缩放**: 支持高DPI显示
- 🔧 **JSON配置**: 支持通过JSON创建对话框

## 基本用法

### 1. 创建对话框组件

```c
#include "dialog_component.h"

// 创建对话框组件
Layer* dialog_layer = layer_create(root_layer, 0, 0, 400, 200);
DialogComponent* dialog = dialog_component_create(dialog_layer);

// 设置基本属性
dialog_component_set_title(dialog, "提示");
dialog_component_set_message(dialog, "操作已完成！");
dialog_component_set_type(dialog, DIALOG_TYPE_INFO);
```

### 2. 添加按钮

```c
// 添加按钮
dialog_component_add_button(dialog, "确定", on_ok_callback, user_data, 1, 0);
dialog_component_add_button(dialog, "取消", on_cancel_callback, user_data, 0, 1);
```

### 3. 显示对话框

```c
// 显示对话框（屏幕居中）
int screen_w = 800, screen_h = 600;
dialog_component_show(dialog, (screen_w - 400) / 2, (screen_h - 200) / 2);
```

### 4. 回调函数

```c
void on_ok_callback(DialogComponent* dialog, void* user_data) {
    printf("用户点击了确定\n");
}

void on_cancel_callback(DialogComponent* dialog, void* user_data) {
    printf("用户点击了取消\n");
}

// 对话框关闭回调
dialog->on_close = on_dialog_close;

void on_dialog_close(DialogComponent* dialog, int button_index) {
    printf("对话框关闭，选择的按钮索引: %d\n", button_index);
}
```

## 对话框类型

### DIALOG_TYPE_INFO
- **用途**: 显示一般信息
- **颜色**: 蓝色主题
- **示例**: 操作成功提示

### DIALOG_TYPE_WARNING  
- **用途**: 显示警告信息
- **颜色**: 橙色主题
- **示例**: 确认删除操作

### DIALOG_TYPE_ERROR
- **用途**: 显示错误信息
- **颜色**: 红色主题
- **示例**: 操作失败提示

### DIALOG_TYPE_QUESTION
- **用途**: 询问用户选择
- **颜色**: 绿色主题
- **示例**: 确认操作

### DIALOG_TYPE_CUSTOM
- **用途**: 自定义样式
- **颜色**: 可自定义
- **示例**: 特殊用途对话框

## JSON配置

### 基本配置

```json
{
  "type": "info",
  "title": "标题",
  "message": "消息内容",
  "modal": true,
  "buttons": [
    {
      "text": "OK",
      "default": true
    },
    {
      "text": "Cancel", 
      "cancel": true
    }
  ],
  "style": {
    "titleColor": "#4682B4",
    "textColor": "#333333",
    "bgColor": "#FFFFFF",
    "borderColor": "#CCCCCC",
    "buttonColor": "#4682B4",
    "buttonHoverColor": "#5F9FD8",
    "buttonTextColor": "#FFFFFF"
  }
}
```

### 完整示例

查看 `app/test-dialog.json` 文件获取完整的配置示例。

## API参考

### 核心函数

```c
// 创建和销毁
DialogComponent* dialog_component_create(Layer* layer);
void dialog_component_destroy(DialogComponent* component);

// 基本设置
void dialog_component_set_title(DialogComponent* component, const char* title);
void dialog_component_set_message(DialogComponent* component, const char* message);
void dialog_component_set_type(DialogComponent* component, DialogType type);
void dialog_component_set_modal(DialogComponent* component, int modal);

// 按钮管理
void dialog_component_add_button(DialogComponent* component, const char* text, 
                                 void (*callback)(DialogComponent* dialog, void* user_data),
                                 void* user_data, int is_default, int is_cancel);
void dialog_component_clear_buttons(DialogComponent* component);

// 显示控制
bool dialog_component_show(DialogComponent* component, int x, int y);
void dialog_component_hide(DialogComponent* component);
bool dialog_component_is_opened(DialogComponent* component);

// 样式设置
void dialog_component_set_colors(DialogComponent* component, Color title, Color text, 
                                Color bg, Color border, Color button, 
                                Color button_hover, Color button_text);

// JSON创建
DialogComponent* dialog_component_create_from_json(Layer* layer, cJSON* json);
```

### 事件回调

```c
// 按钮回调
typedef void (*DialogButtonCallback)(DialogComponent* dialog, void* user_data);

// 对话框关闭回调
void (*on_close)(DialogComponent* dialog, int button_index);

// 对话框显示回调  
void (*on_show)(DialogComponent* dialog);
```

## 键盘快捷键

| 按键 | 功能 |
|------|------|
| Enter | 触发默认按钮（标记为default的按钮） |
| ESC | 触发取消按钮（标记为cancel的按钮）或直接关闭 |
| Tab | 在按钮间切换选择 |

## 注意事项

1. **内存管理**: 使用`dialog_component_destroy()`释放组件资源
2. **Popup集成**: 对话框基于Popup系统，需要确保PopupManager已初始化
3. **线程安全**: 当前实现不是线程安全的，请在主线程中使用
4. **文本长度**: 标题最大255字符，消息最大1023字符
5. **DPI缩放**: 组件会自动适应系统DPI设置

## 示例应用

运行测试应用查看dialog组件的实际效果：

```bash
ya -b yui
./build/yui app/test-dialog.json
```

这将显示一个包含多个按钮的测试界面，每个按钮触发不同类型的对话框。

## 扩展开发

### 自定义按钮行为

```c
void custom_button_handler(DialogComponent* dialog, void* user_data) {
    // 自定义按钮处理逻辑
    printf("自定义按钮被点击\n");
    
    // 可以在这里执行特定操作
    // 例如：保存数据、发送网络请求等
    
    // 不需要手动关闭对话框，按钮点击后会自动关闭
}
```

### 动态更新内容

```c
void update_dialog_content(DialogComponent* dialog, const char* new_title, const char* new_message) {
    dialog_component_set_title(dialog, new_title);
    dialog_component_set_message(dialog, new_message);
    
    // 如果对话框已打开，需要重新渲染
    if (dialog_component_is_opened(dialog)) {
        // 强制重新渲染
        backend_render_present();
    }
}
```

## 故障排除

### 常见问题

1. **对话框不显示**
   - 检查PopupManager是否已初始化
   - 确认传入的坐标在屏幕范围内
   - 验证layer是否有效

2. **按钮点击无响应**
   - 检查回调函数是否正确设置
   - 确认按钮的坐标计算是否正确
   - 验证事件处理是否正常

3. **样式不生效**
   - 检查颜色格式是否正确（支持十六进制和RGB）
   - 确认渲染顺序是否正确
   - 验证透明度设置

## 版本历史

- **v1.0.0**: 初始版本，基本对话框功能
- 支持多种对话框类型
- 完整的鼠标和键盘交互
- JSON配置支持
- 自定义样式
# Dialog组件在JSON中的使用方法

## 概述

Dialog组件现在已经完全集成到YUI框架的layer系统中，可以通过JSON配置文件直接定义和使用对话框。

## JSON配置结构

### 基本语法

```json
{
  "type": "dialog",
  "name": "dialog_id",
  "title": "对话框标题",
  "message": "对话框消息内容",
  "type": "info",           // 对话框类型
  "modal": true,            // 是否模态
  "buttons": [              // 按钮配置
    {
      "text": "确定",
      "default": true        // 是否为默认按钮（回车触发）
    },
    {
      "text": "取消",
      "cancel": true         // 是否为取消按钮（ESC触发）
    }
  ],
  "style": {                // 样式配置
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

## 完整示例

### 1. 在app.json中配置对话框

```json
{
  "window": {
    "title": "Dialog测试应用",
    "width": 800,
    "height": 600,
    "backgroundColor": "#f0f0f0"
  },
  "layers": [
    // 触发按钮
    {
      "type": "button",
      "name": "showDialogBtn",
      "rect": {"x": 50, "y": 50, "width": 150, "height": 40},
      "text": "显示对话框",
      "onClick": "showDialog"
    },
    
    // 信息对话框
    {
      "type": "dialog",
      "name": "infoDialog",
      "title": "信息",
      "message": "这是一个信息对话框示例。",
      "type": "info",
      "modal": true,
      "buttons": [
        {
          "text": "确定",
          "default": true
        }
      ]
    },
    
    // 确认对话框
    {
      "type": "dialog",
      "name": "confirmDialog",
      "title": "确认操作",
      "message": "您确定要执行此操作吗？",
      "type": "question",
      "modal": true,
      "buttons": [
        {
          "text": "是",
          "default": true
        },
        {
          "text": "否",
          "cancel": true
        }
      ]
    }
  ]
}
```

### 2. 在代码中显示对话框

```c
// 显示对话框的示例代码
void show_dialog_handler(void* user_data) {
    // 获取dialog组件
    Layer* dialog_layer = find_layer_by_id("infoDialog");
    if (dialog_layer && dialog_layer->component) {
        DialogComponent* dialog = (DialogComponent*)dialog_layer->component;
        
        // 居中显示对话框
        int screen_w = 800, screen_h = 600;
        int dialog_w = 400, dialog_h = 200;
        int x = (screen_w - dialog_w) / 2;
        int y = (screen_h - dialog_h) / 2;
        
        dialog_component_show(dialog, x, y);
    }
}

void show_confirm_dialog_handler(void* user_data) {
    Layer* dialog_layer = find_layer_by_id("confirmDialog");
    if (dialog_layer && dialog_layer->component) {
        DialogComponent* dialog = (DialogComponent*)dialog_layer->component;
        
        // 设置关闭回调
        dialog->on_close = confirm_dialog_close_handler;
        
        // 居中显示
        int screen_w = 800, screen_h = 600;
        int dialog_w = 400, dialog_h = 200;
        int x = (screen_w - dialog_w) / 2;
        int y = (screen_h - dialog_h) / 2;
        
        dialog_component_show(dialog, x, y);
    }
}

// 对话框关闭回调
void confirm_dialog_close_handler(DialogComponent* dialog, int button_index) {
    if (button_index == 0) {
        printf("用户选择了"是"\n");
        // 执行确认操作
    } else {
        printf("用户选择了"否"\n");
        // 取消操作
    }
}
```

## 对话框类型详解

### 1. 信息对话框 (info)
```json
{
  "type": "dialog",
  "type": "info",
  "title": "信息",
  "message": "操作已完成！",
  "buttons": [{"text": "确定", "default": true}]
}
```
- **用途**: 显示一般信息
- **默认颜色**: 蓝色主题
- **典型场景**: 操作成功提示、通知信息

### 2. 警告对话框 (warning)
```json
{
  "type": "dialog",
  "type": "warning",
  "title": "警告",
  "message": "此操作可能导致数据丢失！",
  "buttons": [
    {"text": "继续", "default": true},
    {"text": "取消", "cancel": true}
  ]
}
```
- **用途**: 警告潜在问题
- **默认颜色**: 橙色主题
- **典型场景**: 删除确认、修改重要设置

### 3. 错误对话框 (error)
```json
{
  "type": "dialog",
  "type": "error",
  "title": "错误",
  "message": "无法连接到服务器！",
  "buttons": [{"text": "确定", "default": true}]
}
```
- **用途**: 显示错误信息
- **默认颜色**: 红色主题
- **典型场景**: 操作失败、系统错误

### 4. 问题对话框 (question)
```json
{
  "type": "dialog",
  "type": "question",
  "title": "确认",
  "message": "是否保存更改？",
  "buttons": [
    {"text": "是", "default": true},
    {"text": "否", "cancel": true}
  ]
}
```
- **用途**: 询问用户选择
- **默认颜色**: 绿色主题
- **典型场景**: 保存确认、删除确认

### 5. 自定义对话框 (custom)
```json
{
  "type": "dialog",
  "type": "custom",
  "title": "自定义",
  "message": "这是一个自定义样式的对话框",
  "modal": false,
  "buttons": [
    {"text": "选项1", "default": true},
    {"text": "选项2"},
    {"text": "关闭", "cancel": true}
  ],
  "style": {
    "titleColor": "#9370DB",
    "textColor": "#2C3E50",
    "bgColor": "#F8F9FA",
    "borderColor": "#9370DB",
    "buttonColor": "#9370DB",
    "buttonHoverColor": "#A688DB",
    "buttonTextColor": "#FFFFFF"
  }
}
```
- **用途**: 完全自定义的对话框
- **颜色**: 可通过style完全自定义
- **典型场景**: 特殊用途、品牌定制

## 样式配置

### 颜色属性

| 属性 | 说明 | 默认值 |
|------|------|--------|
| titleColor | 标题颜色 | 依赖类型 |
| textColor | 文本颜色 | #333333 |
| bgColor | 背景颜色 | #FFFFFF |
| borderColor | 边框颜色 | #CCCCCC |
| buttonColor | 按钮颜色 | #4682B4 |
| buttonHoverColor | 按钮悬停颜色 | #5F9FD8 |
| buttonTextColor | 按钮文本颜色 | #FFFFFF |

### 示例样式配置

```json
"style": {
  "titleColor": "#2C3E50",        // 深灰色标题
  "textColor": "#34495E",         // 中灰色文本
  "bgColor": "#ECF0F1",           // 浅灰色背景
  "borderColor": "#BDC3C7",       // 边框颜色
  "buttonColor": "#3498DB",       // 蓝色按钮
  "buttonHoverColor": "#2980B9",  // 深蓝色悬停
  "buttonTextColor": "#FFFFFF"     // 白色按钮文字
}
```

## 按钮配置

### 按钮属性

| 属性 | 类型 | 说明 |
|------|------|------|
| text | string | 按钮显示文本 |
| default | boolean | 是否为默认按钮（回车触发） |
| cancel | boolean | 是否为取消按钮（ESC触发） |

### 按钮示例

```json
"buttons": [
  {
    "text": "确定",
    "default": true
  },
  {
    "text": "取消", 
    "cancel": true
  },
  {
    "text": "稍后提醒"
  }
]
```

## 事件处理

### 显示对话框

```c
// 通过layer ID查找对话框
Layer* dialog_layer = find_layer_by_id("dialog_id");
if (dialog_layer && dialog_layer->component) {
    DialogComponent* dialog = (DialogComponent*)dialog_layer->component;
    
    // 设置位置并显示
    dialog_component_show(dialog, x, y);
}
```

### 处理按钮点击

```c
// 设置对话框关闭回调
dialog->on_close = dialog_closed_handler;

void dialog_closed_handler(DialogComponent* dialog, int button_index) {
    switch (button_index) {
        case 0:
            printf("第一个按钮被点击\n");
            break;
        case 1:
            printf("第二个按钮被点击\n");
            break;
        case -1:
            printf("对话框被关闭（无按钮点击）\n");
            break;
    }
}
```

## 动态控制

### 运行时修改内容

```c
// 修改对话框标题和消息
dialog_component_set_title(dialog, "新标题");
dialog_component_set_message(dialog, "新消息内容");

// 修改对话框类型
dialog_component_set_type(dialog, DIALOG_TYPE_WARNING);

// 动态添加按钮
dialog_component_clear_buttons(dialog);
dialog_component_add_button(dialog, "确定", ok_handler, NULL, 1, 0);
dialog_component_add_button(dialog, "取消", cancel_handler, NULL, 0, 1);
```

### 检查对话框状态

```c
if (dialog_component_is_opened(dialog)) {
    printf("对话框当前处于打开状态\n");
}
```

## 键盘快捷键

- **Enter**: 触发默认按钮（标记为"default"的按钮）
- **ESC**: 触发取消按钮（标记为"cancel"的按钮）或直接关闭对话框
- **Tab**: 在按钮间循环切换选择

## 最佳实践

1. **合理使用模态对话框**: 对于重要操作使用模态对话框，对于提示信息可使用非模态对话框

2. **清晰的按钮文本**: 使用动词开头的明确按钮文本，如"确定删除"而不是简单的"确定"

3. **合理的默认选择**: 为危险的默认操作设置取消按钮为默认按钮

4. **一致的颜色主题**: 在同一应用中保持对话框颜色的一致性

5. **适当的错误处理**: 始终检查对话框组件是否成功创建和显示

## 故障排除

### 常见问题

1. **对话框不显示**
   - 检查JSON中的"type"字段是否为"dialog"
   - 确认layer ID唯一且正确
   - 验证坐标是否在屏幕范围内

2. **按钮无响应**
   - 检查按钮配置是否正确
   - 确认事件处理函数已正确设置
   - 验证对话框是否获得焦点

3. **样式不生效**
   - 检查颜色格式是否正确（十六进制格式）
   - 确认style对象语法正确
   - 验证属性名称拼写

通过以上配置和方法，你可以在YUI应用中灵活地使用dialog组件来实现各种用户交互场景。
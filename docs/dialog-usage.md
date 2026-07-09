# Dialog 组件使用说明

## 概述

Dialog 已集成到 YUI 的 Layer 体系中，可在 JSON 中声明对话框，通过 `YUI.show()` / `YUI.hide()` 或 C API 显示与关闭。

未配置 `visible: true` 时，对话框默认隐藏，适合作为「关于」「确认」等按需弹出的窗口。

## JSON 基本结构

```json
{
  "id": "aboutDialog",
  "type": "Dialog",
  "title": "关于 YUI",
  "message": "对话框正文，支持 \\n 换行。",
  "dialogType": "info",
  "modal": true,
  "movable": true,
  "fontSize": 12,
  "size": [360, 240],
  "buttons": [
    { "text": "确定", "default": true }
  ],
  "style": {
    "titleColor": "#89B4FA",
    "textColor": "#CDD6F4",
    "bgColor": "#1E1E2E",
    "borderColor": "#45475A",
    "buttonColor": "#45475A",
    "buttonHoverColor": "#585B70",
    "buttonTextColor": "#CDD6F4",
    "buttonWidth": 80,
    "buttonHeight": 32,
    "buttonSpacing": 20,
    "buttonAreaBottom": 50
  }
}
```

### 顶层属性

| 属性 | 类型 | 说明 | 默认值 |
|------|------|------|--------|
| `id` | string | 图层 ID，供 `YUI.show("id")` 查找 | — |
| `type` | string | 固定为 `"Dialog"` | — |
| `title` | string | 标题 | 空 |
| `message` | string | 正文，支持 `\n` 换行与自动折行 | 空 |
| `dialogType` | string | 对话框语义类型，见下文 | `info` |
| `modal` | boolean | 是否模态（打开时阻断主界面点击） | `true` |
| `movable` | boolean | 是否允许拖动标题栏移动 | `true` |
| `visible` | boolean | 是否初始可见；未配置时默认隐藏 | 隐藏 |
| `fontSize` | number | 标题、正文、按钮文字字号 | 继承父层或 `16` |
| `size` | [w, h] | 对话框宽高 | 自动估算 |
| `position` | [x, y] | 模板图层位置（弹出时仍默认居中） | `[0, 0]` |
| `buttons` | array | 底部按钮列表 | 空 |

> **注意**：语义类型请用 `dialogType`（`info` / `warning` / `error` / `question` / `custom`），不要与图层 `type: "Dialog"` 混淆。

## 完整示例（DB Editor）

`app/db/db.json`：

```json
{
  "id": "aboutDialog",
  "type": "Dialog",
  "title": "关于 YUI",
  "message": "YUI - Ya Ya User Interface\n\n一款面向 AI 应用的轻量级 GUI 框架……",
  "dialogType": "info",
  "modal": true,
  "fontSize": 12,
  "size": [360, 240],
  "buttons": [
    {
      "text": "确定",
      "default": true,
      "style": {
        "bgColor": "#313244",
        "hoverColor": "#45475A",
        "textColor": "#F38BA8",
        "width": 72,
        "height": 28
      }
    }
  ],
  "style": {
    "titleColor": "#89B4FA",
    "textColor": "#CDD6F4",
    "bgColor": "#1E1E2E",
    "borderColor": "#45475A",
    "buttonColor": "#45475A",
    "buttonHoverColor": "#585B70",
    "buttonTextColor": "#CDD6F4"
  }
}
```

`app/db/db.js`：

```javascript
function onAbout() {
    YUI.show("aboutDialog");
}
```

## 显示与隐藏

### JavaScript（推荐）

```javascript
YUI.show("aboutDialog");   // 居中弹出
YUI.hide("aboutDialog");   // 关闭
```

也可通过图层属性：

```javascript
var dlg = yui.find("aboutDialog");
if (dlg) dlg.visible = true;   // 等价于 show
```

### C API

```c
Layer* layer = find_layer_by_id(root, "aboutDialog");
if (layer) {
    layer_show(layer);   // 内部调用 dialog 的 set_visible，居中弹出
    layer_hide(layer);
}

// 或直接操作组件
DialogComponent* dialog = (DialogComponent*)layer->component;
dialog_component_show(dialog, x, y);
dialog_component_hide(dialog);
```

## dialogType 类型

| dialogType | 用途 | 标题默认色调 |
|------------|------|--------------|
| `info` | 一般信息提示 | 蓝色 |
| `warning` | 警告 | 橙色 |
| `error` | 错误 | 红色 |
| `question` | 确认 / 询问 | 绿色 |
| `custom` | 完全依赖 `style` 配色 | 自定义 |

示例：

```json
{
  "id": "confirmDialog",
  "type": "Dialog",
  "title": "确认删除",
  "message": "此操作不可撤销，是否继续？",
  "dialogType": "question",
  "modal": true,
  "buttons": [
    { "text": "删除", "default": true },
    { "text": "取消", "cancel": true }
  ]
}
```

## 样式配置（style）

### 对话框颜色

| 属性 | 说明 | 默认值 |
|------|------|--------|
| `titleColor` | 标题颜色 | 随 dialogType |
| `textColor` | 正文颜色 | `#505050` 系 |
| `bgColor` | 背景色 | `#FFFFFF` |
| `borderColor` | 边框色 | `#CCCCCC` |
| `buttonColor` | 按钮默认背景 | `#4682B4` |
| `buttonHoverColor` | 按钮悬停 / 选中背景 | `#5F9FD8` |
| `buttonTextColor` | 按钮文字颜色 | `#FFFFFF` |

### 按钮区域布局（对话框级默认）

| 属性 | 说明 | 默认值 |
|------|------|--------|
| `buttonWidth` | 按钮默认宽度（px） | `80` |
| `buttonHeight` | 按钮默认高度（px） | `35` |
| `buttonSpacing` | 按钮间距（px） | `20` |
| `buttonAreaBottom` | 按钮区距对话框底边（px） | `50` |

未单独配置的按钮继承以上默认值。

## 按钮配置（buttons）

### 通用属性

| 属性 | 类型 | 说明 |
|------|------|------|
| `text` | string | 按钮文字 |
| `default` | boolean | 默认按钮，**Enter** 触发 |
| `cancel` | boolean | 取消按钮，**Esc** 优先触发 |
| `style` | object | 单个按钮样式覆盖（可选） |

### 单按钮样式（buttons[].style）

| 属性 | 说明 |
|------|------|
| `width` / `height` | 按钮尺寸，未设置则用对话框默认 |
| `bgColor` 或 `color` | 背景色 |
| `hoverColor` | 悬停 / 选中背景色 |
| `textColor` | 文字颜色 |

不同宽度的按钮会按总宽度自动居中排列。

```json
"buttons": [
  {
    "text": "确定",
    "default": true,
    "style": {
      "bgColor": "#45475A",
      "hoverColor": "#585B70",
      "textColor": "#CDD6F4",
      "width": 80,
      "height": 32
    }
  },
  {
    "text": "取消",
    "cancel": true,
    "style": {
      "bgColor": "#313244",
      "hoverColor": "#45475A",
      "textColor": "#F38BA8",
      "width": 72,
      "height": 28
    }
  }
]
```

## 长文本与滚动

正文超出标题与按钮之间的区域时：

- 自动换行（含 UTF-8 字内折行）
- 支持 `\n` 手动换行
- 右侧显示**垂直滚动条**
- **鼠标滚轮**滚动（约 20px/格）
- 可**拖动滚动条**滑块，或点击轨道翻页

消息区会为滚动条预留右侧空间，避免文字与滑块重叠。

## 拖动移动

- 默认 `movable: true`
- 在**标题栏区域**按住左键拖动即可移动
- 位置限制在窗口内，不会拖出屏幕
- 关闭后再次 `show` 会重新居中，不记忆上次位置

禁用拖动：

```json
{ "movable": false }
```

## 键盘快捷键

| 按键 | 行为 |
|------|------|
| **Enter** | 触发 `default: true` 的按钮；若无则触发第一个按钮 |
| **Esc** | 触发 `cancel: true` 的按钮；若无则直接关闭 |
| **Tab** | 在按钮间循环切换悬停选中 |

按钮点击在**鼠标释放**时生效（与拖动、滚动条区分）。

## 运行时修改（C API）

```c
dialog_component_set_title(dialog, "新标题");
dialog_component_set_message(dialog, "新消息");
dialog_component_set_type(dialog, DIALOG_TYPE_WARNING);
dialog_component_set_modal(dialog, 1);

dialog_component_clear_buttons(dialog);
dialog_component_add_button(dialog, "确定", NULL, NULL, 1, 0);
dialog_component_add_button(dialog, "取消", NULL, NULL, 0, 1);

if (dialog_component_is_opened(dialog)) {
    /* 已打开 */
}
```

关闭回调：

```c
dialog->on_close = on_dialog_close;

void on_dialog_close(DialogComponent* dialog, int button_index) {
    /* button_index: 0,1,... 为按钮下标；-1 表示无按钮关闭 */
}
```

## 最佳实践

1. 按需弹出类对话框不要设 `visible: true`，用 `YUI.show(id)` 控制。
2. 危险操作用 `dialogType: "question"` 或 `"warning"`，并把 `cancel` 放在更符合习惯的按钮上。
3. 长说明用 `\n` 分段；内容过长时依赖内置滚动，可适当增大 `size` 高度。
4. 按钮文字保持简短；需要强调某个按钮时用 `buttons[].style` 单独配色。
5. 模态对话框用于必须用户确认的场景；纯提示可考虑 `modal: false`（仍可通过拖动查看背后内容）。

## 故障排除

| 现象 | 排查 |
|------|------|
| 对话框不出现 | 检查 `id` 是否与 `YUI.show` 一致；是否误设 `visible: false` 且未调用 show |
| 启动就显示 | 去掉 `visible: true`，或确认未在 `onLoad` 里自动 show |
| 按钮点不动 | 模态层是否被其他 popup 挡住；是否需在释放鼠标时点击 |
| 文字显示不全 | 增大 `size` 高度，或使用滚轮 / 滚动条查看全文 |
| 拖不动 | 确认 `movable` 未设为 `false`；在标题栏区域按下拖动 |
| 样式无效 | 颜色使用 `#RRGGBB`；按钮级样式写在 `buttons[].style` 内 |

## 相关文件

- 组件实现：`src/components/dialog_component.c`
- 示例配置：`app/db/db.json`（`aboutDialog`）
- 示例脚本：`app/db/db.js`（`onAbout`）

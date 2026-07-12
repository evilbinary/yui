# YUI Theme System - 主题系统使用文档

## 概述

YUI 主题系统通过 JSON 配置文件统一管理应用样式，支持**类型选择器**、**复合选择器**（`Type.modifier`）和 **ID 选择器**，可在运行时切换亮/暗色及桌面/手机端主题。

## 核心特性

- **类型选择器**：`Button`、`Label`、`View` 等组件默认样式
- **复合选择器**：`Button.primary`、`Label.title`（类型 + variant 修饰符）
- **ID 选择器**：`#btn_logo`、`#btn_logo.primary`（App 级个别覆盖）
- **多 modifier 叠加**：图层可带多个 variant，多条 `Type.modifier` 规则依次叠加
- **平台主题**：`dark.json` / `light.json`（桌面）与 `dark-mobile.json` / `light-mobile.json`（手机）
- **热切换**：`Theme.setCurrent` + `Theme.apply()` 运行时切换

## 文件结构

```
app/lib/
├── theme.js                 # 主题 JavaScript API
└── themes/
    ├── dark.json            # 暗色（桌面端）
    ├── light.json           # 亮色（桌面端）
    ├── dark-mobile.json     # 暗色（手机端）
    └── light-mobile.json    # 亮色（手机端）

app/shopping/
├── app.json                 # 图层：type + variant，不写颜色
└── app.js                   # 加载主题、切换亮/暗
```

旧版示例 `app/dark-theme.json`、`app/light-theme.json` 仍可用于简单 Demo，新项目请使用 `app/lib/themes/`。

## 快速开始

### 1. 图层 JSON（标 variant，不写颜色）

```json
{
  "id": "btn_login",
  "type": "Button",
  "variant": "primary",
  "text": "登录",
  "size": [48, 36],
  "events": { "onClick": "@goLogin" }
}
```

多个 modifier（方案 A，叠加匹配）：

```json
{
  "id": "btn_theme",
  "type": "Button",
  "variants": ["tertiary", "compact"],
  "text": "暗"
}
```

也支持字符串（空格/逗号分隔）：

```json
{ "variant": "primary block" }
```

### 2. 主题 JSON

```json
{
  "name": "dark",
  "version": "1.0",
  "description": "通用暗色主题",
  "styles": [
    { "selector": "Button", "style": { "bgColor": "#1A2030", "color": "#F2F4F8", "fontSize": 14, "borderRadius": 12 } },
    { "selector": "Button.primary", "style": { "bgColor": "#7C6CFF", "color": "#FFFFFF", "fontWeight": "bold" } },
    { "selector": "Button.compact", "style": { "fontSize": 12, "borderRadius": 8 } },
    { "selector": "Label.title", "style": { "color": "#FFFFFF", "fontSize": 22, "fontWeight": "bold" } },
    { "selector": "#btn_logo.primary", "style": { "bgColor": "#6B5CE7" } }
  ]
}
```

### 3. 应用内加载（shopping 示例）

```javascript
var themeMode = "dark";
var themePlatform = "mobile";  // "mobile" | "desktop"

function initThemes() {
    var suffix = themePlatform === "mobile" ? "-mobile" : "";
    Theme.load("app/lib/themes/dark" + suffix + ".json", "dark");
    Theme.load("app/lib/themes/light" + suffix + ".json", "light");
    Theme.setCurrent(themeMode);
    Theme.apply();
}

function toggleTheme() {
    themeMode = themeMode === "dark" ? "light" : "dark";
    Theme.setCurrent(themeMode);
    Theme.apply();
}
```

在 `app.json` 的 `js` 数组中引入 `../lib/theme.js`，在 `onAppLoad` 中调用 `initThemes()`。

动态挂载的页面（如商品详情）在 `onShow` 后需再次调用 `Theme.apply()`。

## 选择器规则

| 选择器 | 含义 | 示例 |
|--------|------|------|
| `Type` | 该类型所有组件 | `Button` |
| `Type.modifier` | 类型 + variant 含 modifier | `Button.primary` |
| `Type.a.b` | 同时包含 a、b 两个 modifier | `Button.primary.compact` |
| `#id` | 指定 id | `#btn_logo` |
| `#id.modifier` | 指定 id + variant | `#btn_logo.primary` |

### 优先级（低 → 高，后者覆盖前者）

```
Type  →  Type.modifier  →  #id  →  #id.modifier
```

同一图层会命中**所有**匹配规则并叠加；同名字段以后应用的规则为准（规则在 JSON 中越靠后，应用时优先级越高）。

### 通用 modifier 词汇（lib 主题已定义）

**Button：** `primary` `secondary` `tertiary` `danger` `compact` `block` `tone-a` `tone-b` `tone-c`

**Label：** `title` `heading` `subtitle` `body` `caption` `overline` `link` `price` `badge`

**View：** `surface` `elevated` `toolbar` `card` `panel` `divider`

App 可在自己的主题覆盖文件里增加 `#id.modifier` 规则，无需改 lib。

## 平台主题

| 文件 | 适用场景 |
|------|----------|
| `dark.json` / `light.json` | 桌面端，较大字号与圆角 |
| `dark-mobile.json` / `light-mobile.json` | 手机端（如 390px 宽），更紧凑的字号与密度 |

两套主题**配色体系一致**，差异主要在 `fontSize`、`borderRadius` 等尺寸 token。手机端 shopping 默认加载 `-mobile` 后缀文件。

## 主题文件格式

```json
{
  "name": "dark",
  "version": "1.0",
  "description": "主题描述",
  "styles": [
    {
      "selector": "Button.primary",
      "style": { "bgColor": "#7C6CFF", "color": "#FFFFFF" }
    }
  ]
}
```

`styles` 数组也支持字段名 `rules`（与 `styles` 等价）。

### 支持的样式属性

| 属性 | 类型 | 引擎支持 | 说明 |
|------|------|----------|------|
| `color` | string | ✅ | 文字颜色 |
| `bgColor` | string | ✅ | 背景色，支持 `#RRGGBB`、`transparent` |
| `fontSize` | number | ✅ | 字号（px），apply 后重新加载字体 |
| `borderRadius` | number | ✅ | 圆角（映射到 `layer.radius`） |
| `spacing` | number | ✅ | 布局子项间距 |
| `padding` | array | ✅ | `[top, right, bottom, left]` |
| `opacity` | number | ✅ | 0–255 |
| `fontWeight` | string | ⚠️ | 已解析，渲染侧待完善 |
| `borderWidth` | number | ⚠️ | 已解析，合并到图层待完善 |
| `borderColor` | string | ⚠️ | 已解析，合并到图层待完善 |

图层 JSON 里的 `style` 仍可用于布局相关或引擎尚未主题化的属性；**颜色与字号应交给主题**。

## JavaScript API

### 核心方法

```javascript
Theme.load(themePath, themeName);   // 加载主题文件，themeName 为逻辑名如 "dark"
Theme.setCurrent(themeName);          // 设置当前主题
Theme.apply();                        // 应用到图层树（等同 YUI.themeApplyToTree）
Theme.getCurrent();                   // 当前主题信息
Theme.getLoadedThemes();              // 已加载主题名列表
Theme.switch(themePath, themeName);   // 加载 + 设当前 + apply
```

### 原生桥接（C 层）

```javascript
YUI.themeLoad(path);        // 加载，返回 { success, name, version }
YUI.themeSetCurrent(name);  // 设置当前
YUI.themeApplyToTree();     // 应用到 g_layer_root
```

### 事件

```javascript
Theme.on('themeLoaded', fn);
Theme.on('themeChanged', fn);
Theme.on('themeApplied', fn);
Theme.on('themeUnloaded', fn);
```

## 完整示例：shopping

**图层**（`app/shopping/app.json`）只声明结构与 variant：

```json
{
  "id": "home_title",
  "type": "Label",
  "variant": "title",
  "text": "发现精品好物"
}
```

**主题**（`app/lib/themes/dark-mobile.json`）定义外观：

```json
{ "selector": "Label.title", "style": { "color": "#FFFFFF", "fontSize": 17, "fontWeight": "bold" } }
```

**切换亮/暗**：顶栏按钮调用 `toggleTheme()`，无需改 JSON。

## 常见问题

### 主题样式不生效？

1. 是否调用了 `Theme.apply()`（动态页面还需在 `onShow` 后再 apply）
2. 图层是否设置了对应的 `variant` / `variants`
3. 选择器是否写对：`Button.primary`，不是 `Button.variant-primary`
4. 主题文件路径是否相对于进程工作目录（一般为仓库根目录 `app/lib/themes/...`）

### 手机与桌面看起来一样？

确认 `themePlatform === "mobile"` 且加载的是 `dark-mobile.json`；修改主题后需重新运行 playground。`fontSize` 在 `Theme.apply()` 时会触发字体重新加载。

### 如何只做 App 级覆盖？

在 App 目录增加 `themes/dark.json`（仅含 `#id.modifier` 规则），加载在 lib 主题之后合并（或后续使用 `Theme.extend` API）。目前可先使用 `#id.modifier` 写入 App 专属主题文件并单独 load。

## 更新日志

### v1.1.0
- 复合选择器 `Type.modifier`、`#id.modifier`
- 图层 `variant` / `variants` 多 modifier 支持
- 平台主题 `dark-mobile.json`、`light-mobile.json`
- `fontSize` 主题合并与字体热重载
- shopping 接入示例

### v1.0.0
- 初始版本：ID / 类型选择器、加载切换、事件系统

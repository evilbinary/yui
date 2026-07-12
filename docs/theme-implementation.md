# YUI 主题系统实现说明

## 架构概览

```
app/lib/theme.js          JS API（Theme.load / setCurrent / apply）
        ↓ YUI.themeLoad / themeSetCurrent / themeApplyToTree
lib/jsmodule-quickjs/     QuickJS 桥接
        ↓
src/theme_manager.c       单例管理、遍历图层树 apply
src/theme.c               选择器匹配、样式合并
src/layer.c               解析 variant / variants 字段
```

## 文件结构

### C 实现

```
src/
├── theme.h               ThemeRule、选择器类型枚举
├── theme.c               规则解析、theme_merge_style、复合选择器匹配
├── theme_manager.h
├── theme_manager.c       加载、setCurrent、apply_to_tree、字体 invalidate
└── layer.c               JSON 解析 variant；创建图层时可选 apply 主题
```

### 主题资源

```
app/lib/themes/
├── dark.json             桌面暗色
├── light.json            桌面亮色
├── dark-mobile.json      手机暗色
└── light-mobile.json     手机亮色
```

### JavaScript

```
app/lib/theme.js          Theme 全局对象，封装 YUI C API
```

## 选择器类型

```c
typedef enum {
    THEME_SELECTOR_ID,        // #componentId
    THEME_SELECTOR_TYPE,    // Button, Label, View ...
    THEME_SELECTOR_COMPOUND // Button.primary, #id.primary
} ThemeSelectorType;
```

### 复合选择器匹配逻辑

- `Button.primary`：`layer.type == Button` 且 variant 列表含 `primary`
- `Button.primary.compact`：同时含 `primary` 与 `compact`
- `#btn_logo.primary`：`layer.id == btn_logo` 且含 `primary`

variant 在图层上存为**单个字符串**（最长 128 字符），多个 modifier 以空格连接：

```c
char variant[128];  // 例如 "primary compact"
```

JSON 解析（`layer.c`）：

- `"variants": ["primary", "compact"]` → 存为 `"primary compact"`
- `"variant": "primary, compact"` → 原样存入，匹配时按空格/逗号/分号分词

## 样式合并（theme_merge_style）

| 属性 | 行为 |
|------|------|
| `color` | 写入 `layer->color`，标记 DIRTY_COLOR |
| `bgColor` | 写入 `layer->bg_color`（`a > 0` 才应用；透明用 `#00000000`） |
| `borderRadius` | 写入 `layer->radius` |
| `fontSize` | 写入 `layer->font->size`；若 font 与父共享则拷贝独立 Font；清空 `default_font` 后 mark DIRTY_TEXT |
| `spacing` / `padding` | 写入 `layout_manager` |
| `borderColor` / `borderWidth` | 规则已解析，合并到 Layer **待实现** |

## 应用流程

```
Theme.apply()
  → theme_manager_apply_to_tree(root)
      → 递归 theme_apply_to_layer（遍历规则链表，匹配则 merge）
      → theme_invalidate_fonts(root)   // 清空 default_font
      → load_all_fonts(root)           // 按新 fontSize 重新加载
```

图层创建时（`layer.c`）若已有 current_theme，也会调用一次 `theme_apply_to_layer`。

规则链表：**后加载的规则在链表头部，遍历从头部开始**；JSON 文件中**靠后的规则最后应用，优先级更高**。

## JavaScript API 映射

| Theme.js | YUI (C) |
|----------|---------|
| `Theme.load(path, name)` | `YUI.themeLoad(path)` |
| `Theme.setCurrent(name)` | `YUI.themeSetCurrent(name)` |
| `Theme.apply()` | `YUI.themeApplyToTree()` |

`themeLoad` 也接受 JSON 字符串（以 `{` 开头）。

## 平台主题约定

- 文件名：`{mode}-mobile.json`，`mode` 为 `dark` | `light`
- JSON 内 `"name"` 仍为 `dark` / `light`，便于 `setCurrent("dark")` 不变
- 桌面与手机共享同一套 modifier 命名，仅尺寸 token 不同

| Token | 桌面 dark | 手机 dark-mobile |
|-------|-----------|------------------|
| Label | 14 | 12 |
| Label.title | 22 | 17 |
| Label.price | 20 | 16 |
| Button.borderRadius | 12 | 8 |
| View.card.borderRadius | 16 | 10 |

## shopping 集成要点

```javascript
// app/shopping/app.js
var themePlatform = "mobile";
var suffix = themePlatform === "mobile" ? "-mobile" : "";
Theme.load("app/lib/themes/dark" + suffix + ".json", "dark");
```

```json
// app/shopping/app.json — 无 inline 颜色 style
{ "type": "Button", "variant": "primary", "text": "登录" }
```

```javascript
// app/shopping/page/product.js — 动态路由
function onProductShow() {
    Theme.apply();
}
```

## 测试

```bash
ya -b playground -r
.\build\None\None\None\playground.exe .\app\shopping\app.json
```

## 已知限制

1. `fontWeight` 写入规则但未驱动字体重新选 weight 文件
2. 主题 `borderColor` / `borderWidth` 未写入 Layer，边框仍需 JSON inline 或后续扩展
3. `Theme.extend`（lib 主题 + App 覆盖合并）尚未实现，需加载两份完整主题或重复 modifier 定义
4. variant **不继承**父图层，每个控件需自行声明

## 更新日志

### v1.1.0
- `THEME_SELECTOR_COMPOUND`：`Type.modifier`、`#id.modifier`
- Layer `variant[128]`，支持 `variants` 数组
- `theme_invalidate_fonts` + `load_all_fonts` 于 apply 之后
- `fontSize` 主题合并
- `app/lib/themes/*` 通用主题与 mobile 变体

### v1.0.0
- 基础 ID / TYPE 选择器
- theme_manager 单例
- theme.js 封装

## 相关文件

- 使用文档：[theme.md](./theme.md)
- 核心 C：`src/theme.c`, `src/theme_manager.c`
- JS 库：`app/lib/theme.js`
- 参考 App：`app/shopping/`

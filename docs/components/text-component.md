# Text Component 文本组件

## 概述

Text 组件用于显示和编辑文本，支持单行/多行输入、滚动、行号、选区高亮，以及 JSON / SQL / Markdown 语法高亮。

## 功能特性

- 单行与多行编辑（`multiline`）
- 可编辑 / 只读（`editable`）
- 占位符、最大长度限制
- 垂直滚动（`scrollable`）
- 行号显示（`showLineNumbers`）
- 选区与光标颜色自定义
- 语法高亮：`json`、`sql`、`markdown`（别名 `md`）
- `onChange` 事件

## 基本用法

### 单行输入框

```json
{
  "id": "username",
  "type": "Text",
  "text": "",
  "placeholder": "请输入用户名",
  "size": [200, 36],
  "editable": true,
  "multiline": false,
  "style": {
    "color": "#333333",
    "fontSize": 14,
    "padding": [8, 8, 8, 8]
  }
}
```

### 多行代码编辑器

```json
{
  "id": "codeEditor",
  "type": "Text",
  "text": "",
  "size": [600, 400],
  "multiline": true,
  "editable": true,
  "scrollable": 1,
  "showLineNumbers": true,
  "lineNumberWidth": 40,
  "syntaxHighlight": "json",
  "selectionColor": "#6BAF52CC",
  "style": {
    "bgColor": "#2b2b2b",
    "color": "#f8f8f2",
    "fontSize": 14,
    "fontFamily": "monospace",
    "padding": [10, 10, 10, 10],
    "cursorColor": "#89B4FA"
  },
  "events": {
    "onChange": "@onCodeChange"
  }
}
```

## 语法高亮

在 Text 组件上设置 `syntaxHighlight` 即可启用对应语言的语法着色。未设置或设为未知值时不启用高亮。

### 支持的语言

| 值 | 说明 |
|----|------|
| `"json"` | JSON 语法高亮 |
| `"sql"` | SQL 语法高亮 |
| `"markdown"` | Markdown 语法高亮 |
| `"md"` | 同 `"markdown"` |

### 自定义颜色

通过 `syntaxColors` 覆盖默认配色。颜色值支持 `#RRGGBB` 或 `#RRGGBBAA` 格式。

```json
{
  "id": "sqlEditor",
  "type": "Text",
  "syntaxHighlight": "sql",
  "syntaxColors": {
    "keyword": "#CBA6F7",
    "string": "#A6E3A1",
    "number": "#FAB387",
    "comment": "#6C7086",
    "punctuation": "#89DCEB"
  },
  "style": {
    "color": "#CDD6F4"
  }
}
```

### syntaxColors 可用键名

| 键名 | 用途 |
|------|------|
| `default` | 默认文本颜色 |
| `key` | JSON 键名；Markdown 代码块、链接 URL |
| `string` | 字符串；Markdown 斜体 |
| `number` | 数字 |
| `keyword` | 关键字；Markdown 标题、粗体 |
| `punctuation` | 标点与运算符；Markdown 列表标记、分隔线 |
| `comment` | 注释；Markdown 引用、删除线 |
| `code` | 同 `key`，用于 Markdown 行内/围栏代码 |
| `heading` | 同 `keyword`，用于 Markdown 标题 |
| `link` | 同 `key`，用于 Markdown 链接 |

未指定的键使用内置默认配色（深色主题友好）。

### JSON 高亮规则

- 字符串、键名（引号后紧跟 `:` 的字符串）
- 数字
- 关键字：`true`、`false`、`null`
- 标点：`{ } [ ] : ,`

### SQL 高亮规则

- 关键字（`SELECT`、`FROM`、`WHERE` 等）
- 字符串（`'...'`、`"..."`、`` `...` ``）
- 数字
- 注释：`--`、`#`、`/* */`
- 运算符与标点

### Markdown 高亮规则

| 语法 | 着色 |
|------|------|
| `# 标题` | heading |
| `> 引用` | comment |
| ` ``` ` 围栏代码 / 4 空格缩进 | code |
| `---` 分隔线 | punctuation |
| `- * +` 无序列表、`1.` 有序列表 | 标记 punctuation |
| `` `inline` `` | code |
| `**bold**` / `__bold__` | keyword |
| `*italic*` / `_italic_` | string |
| `~~strike~~` | comment |
| `[text](url)` / `![alt](url)` | 分段着色 |
| `<!-- comment -->` | comment |

中文等 UTF-8 文本按完整字符渲染，不会被拆成乱码。

### 已知限制

- 分词按**行**进行，跨行的围栏代码块（` ``` `）中间行不会整体标为 code
- SQL / Markdown 关键字列表为常用子集，非完整语言规范

## 属性说明

### 通用属性

继承 [JSON 格式规范](../json-format-spec.md#通用属性) 中的 `id`、`type`、`size`、`style`、`events` 等。

### Text 特有属性

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `text` | string | `""` | 文本内容 |
| `placeholder` | string | — | 占位提示文字 |
| `maxLength` | number | — | 最大字符数 |
| `multiline` | boolean | `false` | 是否多行 |
| `editable` | boolean | `true` | 是否可编辑 |
| `scrollable` | number | — | 滚动模式，`1` 为垂直滚动 |
| `showLineNumbers` | boolean | `false` | 是否显示行号（需 `multiline: true`） |
| `lineNumberWidth` | number | — | 行号区域宽度（像素） |
| `selectionColor` | string | — | 选区背景色 |
| `syntaxHighlight` | string | — | 语法高亮语言 |
| `syntaxColors` | object | — | 语法高亮配色 |

### style 中常用项

| 属性 | 说明 |
|------|------|
| `color` | 正文默认颜色（也是语法高亮的 `default` 基色） |
| `bgColor` | 背景色 |
| `fontSize` | 字号 |
| `fontFamily` | 字体，代码编辑建议 `monospace` |
| `padding` | 内边距 `[上, 右, 下, 左]` |
| `cursorColor` | 光标颜色 |

## 事件

### onChange

文本内容变化时触发。配置方式：

```json
{
  "events": {
    "onChange": "@onTextChange"
  }
}
```

`@` 前缀可选，会映射到 JS 中同名函数。详见 [事件系统](../event.md)。

## JavaScript API

```javascript
YUI.setText("codeEditor", '{"name": "yui"}');
var content = YUI.getText("codeEditor");
```

详见 [YUI JavaScript API](../yui-js-api.md)。

## 示例

完整示例可参考：

- `app/playground/json-editor.json` — JSON 编辑器（`syntaxHighlight: "json"`）
- `app/db/db.json` — Markdown 编辑器（`syntaxHighlight: "markdown"`）

## 常见问题

**语法高亮不生效？**

- 确认 `syntaxHighlight` 拼写正确（`json` / `sql` / `markdown` / `md`）
- 确认组件已设置 `text` 或用户输入了内容

**多行内容无法滚动？**

- 设置 `"scrollable": 1`
- 确认 `multiline` 为 `true`，且组件高度小于内容高度

**行号不显示？**

- 需同时设置 `"multiline": true` 和 `"showLineNumbers": true`

# YUI JavaScript API 文档

## 概述

YUI 提供了多引擎支持的 JavaScript API，包括 mquickjs、QuickJS 和 Mario 三种 JavaScript 引擎的绑定。

## JavaScript 全局对象

### YUI 对象

YUI 对象是所有 YUI 原生 API 的命名空间。

#### YUI Core API (所有引擎通用)

| 方法 | 参数 | 返回值 | 描述 | 支持引擎 |
|------|------|--------|------|---------|
| `YUI.log(...)` | `...args` | `undefined` | 打印日志到控制台 | 全部 |
| `YUI.setText(layerId, text)` | `layerId: string`, `text: string` | `undefined` | 设置图层文本内容 | 全部 |
| `YUI.getText(layerId)` | `layerId: string` | `string` | 获取图层文本内容 | 全部 |
| `YUI.setBgColor(layerId, color)` | `layerId: string`, `color: string` | `undefined` | 设置图层背景色 (#RRGGBB) | 全部 |
| `YUI.hide(layerId)` | `layerId: string` | `undefined` | 隐藏图层 | 全部 |
| `YUI.show(layerId)` | `layerId: string` | `undefined` | 显示图层 | 全部 |
| `YUI.renderFromJson(layerId, json)` | `layerId: string`, `json: string` | `number` | 从JSON渲染图层树 | 全部 |
| `YUI.update(jsonString)` | `jsonString: string\|object` | `number` | JSON增量更新 | 全部 |
| `YUI.themeLoad(path)` | `path: string` | `object\|number` | 加载主题文件 | 全部 |
| `YUI.themeSetCurrent(name)` | `name: string` | `boolean\|number` | 设置当前主题 | 全部 |
| `YUI.themeUnload(name)` | `name: string` | `boolean\|number` | 卸载主题 | 全部 |
| `YUI.themeApplyToTree()` | - | `boolean\|number` | 应用主题到图层树 | 全部 |

#### Socket API (mquickjs & QuickJS)

Socket API 在 mquickjs 和 QuickJS 引擎中可用。

| 方法 | 参数 | 返回值 | 描述 |
|------|------|--------|------|
| `Socket.socket(type)` | `type: number` | `number` | 创建socket |
| `Socket.close(fd)` | `fd: number` | `number` | 关闭socket |
| `Socket.shutdown(fd)` | `fd: number` | `number` | 关闭socket连接 |
| `Socket.connect(fd, host, port, timeout)` | `fd: number`, `host: string`, `port: number`, `timeout: number` | `number` | 连接到服务器 |
| `Socket.bind(fd, host, port)` | `fd: number`, `host: string`, `port: number` | `number` | 绑定socket |
| `Socket.listen(fd, backlog)` | `fd: number`, `backlog: number` | `number` | 监听连接 |
| `Socket.accept(fd)` | `fd: number` | `number` | 接受连接 |
| `Socket.getsockname(fd)` | `fd: number` | `{ip: string, port: number}\|number` | 获取本地地址 |
| `Socket.getpeername(fd)` | `fd: number` | `{ip: string, port: number}\|number` | 获取对端地址 |
| `Socket.socketpair(domain, type, protocol)` | `domain: number`, `type: number`, `protocol: number` | `number[]\|number` | 创建socket对 |
| `Socket.setsockopt(fd, level, option_name, option_value, option_len)` | `fd: number`, `level: number`, `option_name: number`, `option_value: any`, `option_len: number` | `number` | 设置socket选项 |
| `Socket.getsockopt(fd, level, option_name, option_len)` | `fd: number`, `level: number`, `option_name: number`, `option_len: number` | `any\|number` | 获取socket选项 |
| `Socket.send(fd, data, flags)` | `fd: number`, `data: string`, `flags: number` | `number` | 发送数据 |
| `Socket.recv(fd, len, flags)` | `fd: number`, `len: number`, `flags: number` | `string\|number` | 接收数据 |
| `Socket.sendto(fd, data, flags, host, port)` | `fd: number`, `data: string`, `flags: number`, `host: string`, `port: number` | `number` | 发送数据到指定地址 |
| `Socket.recvfrom(fd, len, flags)` | `fd: number`, `len: number`, `flags: number` | `{data: string, ip: string, port: number}\|number` | 从任意地址接收数据 |
| `Socket.inet_addr(ip)` | `ip: string` | `number` | IP地址转换 |
| `Socket.ntohl(value)` | `value: number` | `number` | 网络字节序转换 |
| `Socket.make_sockaddr_in(ip, port)` | `ip: string`, `port: number` | `{ptr: number, size: number}` | 创建sockaddr结构 |

#### 标准库函数 (mquickjs)

mquickjs 引擎提供了完整的 JavaScript 标准库：

**全局函数**
- `print(...args)` - 打印输出
- `gc()` - 垃圾回收
- `load(filename)` - 加载并执行JS文件
- `setTimeout(func, delay)` - 设置定时器
- `clearTimeout(id)` - 清除定时器
- `parseInt(str, radix)` - 字符串转整数
- `parseFloat(str)` - 字符串转浮点数
- `eval(code)` - 执行代码
- `isNaN(value)` - 检查是否为NaN
- `isFinite(value)` - 检查是否为有限数

**Object 对象**
- `Object.defineProperty(obj, prop, descriptor)`
- `Object.getPrototypeOf(obj)`
- `Object.setPrototypeOf(obj, proto)`
- `Object.create(proto)`
- `Object.keys(obj)`
- `obj.hasOwnProperty(prop)`
- `obj.toString()`

**Function 对象**
- `func.call(thisArg, ...args)`
- `func.apply(thisArg, argsArray)`
- `func.bind(thisArg, ...args)`
- `func.toString()`
- `func.length` (属性)

**Number 对象**
- `Number.parseInt(str, radix)`
- `Number.parseFloat(str)`
- `Number.MAX_VALUE`
- `Number.MIN_VALUE`
- `num.toExponential(fractionDigits)`
- `num.toFixed(digits)`
- `num.toPrecision(precision)`
- `num.toString(radix)`

**String 对象**
- `str.length` (属性)
- `str.charAt(index)`
- `str.charCodeAt(index)`
- `str.codePointAt(pos)`
- `str.slice(start, end)`
- `str.substring(start, end)`
- `str.concat(...strings)`
- `str.indexOf(search)`
- `str.lastIndexOf(search)`
- `str.match(regexp)`
- `str.replace(search, replacement)`
- `str.replaceAll(search, replacement)`
- `str.search(regexp)`
- `str.split(separator, limit)`
- `str.toLowerCase()`
- `str.toUpperCase()`
- `str.trim()`
- `str.trimStart()`
- `str.trimEnd()`
- `str.toString()`
- `str.repeat(count)`

**Array 对象**
- `Array.isArray(value)`
- `arr.length` (属性)
- `arr.concat(...arrays)`
- `arr.push(...items)`
- `arr.pop()`
- `arr.join(separator)`
- `arr.toString()`
- `arr.reverse()`
- `arr.shift()`
- `arr.slice(start, end)`
- `arr.splice(start, deleteCount, ...items)`
- `arr.unshift(...items)`
- `arr.forEach(callback)`
- `arr.map(callback)`
- `arr.filter(callback)`
- `arr.every(callback)`
- `arr.some(callback)`
- `arr.reduce(callback, initialValue)`
- `arr.reduceRight(callback, initialValue)`
- `arr.sort(compareFunc)`

**Math 对象**
- `Math.abs(x)`, `Math.acos(x)`, `Math.asin(x)`, `Math.atan(x)`
- `Math.atan2(y, x)`, `Math.ceil(x)`, `Math.cos(x)`, `Math.exp(x)`
- `Math.floor(x)`, `Math.log(x)`, `Math.max(...values)`, `Math.min(...values)`
- `Math.pow(x, y)`, `Math.random()`, `Math.round(x)`, `Math.sin(x)`
- `Math.sqrt(x)`, `Math.tan(x)`, `Math.imul(a, b)`, `Math.clz32(x)`

**JSON 对象**
- `JSON.parse(text, reviver)`
- `JSON.stringify(value, replacer, space)`

**Date 对象**
- `Date.now()` - 返回当前时间戳

**console 对象**
- `console.log(...args)` - 控制台输出

**performance 对象**
- `performance.now()` - 返回高精度时间戳

### 兼容性全局函数

以下函数在 QuickJS 引擎中也可作为全局函数直接调用：

- `setText(layerId, text)`
- `getText(layerId)`
- `setBgColor(layerId, color)`
- `hide(layerId)`
- `show(layerId)`

## C Native 接口

### 主题管理器 API (src/theme_manager.h)

```c
// 获取主题管理器单例
ThemeManager* theme_manager_get_instance(void);

// 销毁主题管理器
void theme_manager_destroy(void);

// 加载主题文件
Theme* theme_manager_load_theme(const char* theme_path);

// 设置当前主题
int theme_manager_set_current(const char* theme_name);

// 获取当前主题
Theme* theme_manager_get_current(void);

// 获取主题（按名称）
Theme* theme_manager_get_theme(const char* theme_name);

// 卸载主题
void theme_manager_unload_theme(const char* theme_name);

// 应用主题到单个图层
void theme_manager_apply_to_layer(Layer* layer, const char* id, const char* type);

// 应用主题到图层树
void theme_manager_apply_to_tree(Layer* root);
```

### 主题 API (src/theme.h)

```c
// 创建主题对象
Theme* theme_create(const char* name, const char* version);

// 销毁主题对象
void theme_destroy(Theme* theme);

// 从JSON文件加载主题
Theme* theme_load_from_file(const char* json_path);

// 从JSON对象加载主题
Theme* theme_load_from_json(cJSON* json);

// 添加规则到主题
void theme_add_rule(Theme* theme, ThemeRule* rule);

// 从JSON创建规则
ThemeRule* theme_rule_create_from_json(cJSON* json);

// 销毁规则
void theme_rule_destroy(ThemeRule* rule);

// 应用主题样式到图层
void theme_apply_to_layer(Theme* theme, Layer* layer, const char* id, const char* type);

// 合并样式
void theme_merge_style(ThemeRule* rule, Layer* layer);

// 解析选择器类型
ThemeSelectorType theme_parse_selector_type(const char* selector);
```

### 图层管理 API (src/layer.h)

```c
// 查找图层
Layer* find_layer_by_id(Layer* root, const char* id);

// 从JSON字符串解析图层
Layer* parse_layer_from_string(const char* json_str, Layer* parent);

// 销毁图层
void destroy_layer(Layer* layer);

// 布局图层
void layout_layer(Layer* layer);

// 加载所有字体
void load_all_fonts(Layer* layer);

// 设置图层文本
void layer_set_text(Layer* layer, const char* text);

// 获取图层文本
const char* layer_get_text(const Layer* layer);

// 设置图层事件
void layer_set_event(Layer* layer, EventType event_type, EventHandler handler);
```

### 图层更新 API (src/layer_update.h)

```c
// JSON增量更新
int yui_update(Layer* root, const char* update_json);

// 解析颜色字符串
int parse_color_string(const char* color_str, Color* color);
```

### 图层属性 API (src/layer_properties.h)

```c
// 从JSON设置单个属性
int layer_set_property_from_json(Layer* layer, const char* key, cJSON* value, int is_creating);

// 从JSON对象批量设置属性
int layer_set_properties_from_json(Layer* layer, cJSON* json, int is_creating);

// 从JSON数组创建子图层
int layer_set_children_from_json(Layer* layer, cJSON* children_array);
```

### 渲染 API (src/render.h)

```c
// 渲染图层树
void render_layer(Layer* layer);

// 渲染所有图层
void render_all_layers(Layer* root);
```

### 布局 API (src/layout.h)

```c
// 计算图层布局
void calculate_layout(Layer* layer);
```

### 动画 API (src/animate.h)

```c
// 更新动画
void animate_update(Layer* root, uint32_t delta_time);

// 添加动画
Animation* animate_add(Layer* layer, AnimationType type, float from, float to, uint32_t duration);
```

## 引擎特定实现差异

### mquickjs (lib/jsmodule/yui_stdlib.c)

- 使用 `JS_CFUNC_DEF` 宏注册函数
- 函数参数使用 `JSValue *argv`
- 返回 `JSValue` 类型
- 字符串转换使用 `JS_ToCString(ctx, value, &buf)`
- 不支持 `JS_FreeCString`

### QuickJS (lib/jsmodule-quickjs/js_module.c)

- 使用 `JS_NewCFunction` 创建函数
- 函数参数使用 `JSValueConst *argv`
- 返回 `JSValue` 类型
- 字符串转换使用 `JS_ToCStringLen(ctx, &len, value)`
- 需要使用 `JS_FreeCString(ctx, str)` 释放字符串

### Mario (lib/jsmodule-mario/js_module.c)

- 使用 `vm_reg_native` 注册函数
- 函数签名：`var_t* func(vm_t* vm, var_t* env, void* data)`
- 使用 `get_func_arg_str(env, index)` 获取字符串参数
- 返回 `var_t*` 类型（使用 `var_new_int`, `var_new_str`, `var_new_null` 等创建）
- 不支持复杂的对象操作，返回简单类型

## 使用示例

### 主题管理示例

```javascript
// 加载主题
var result = YUI.themeLoad('app/themes/dark.json');
if (result && result.success) {
    console.log('Loaded theme: ' + result.name);
    
    // 设置为当前主题
    if (YUI.themeSetCurrent(result.name)) {
        console.log('Theme activated');
        
        // 应用到UI
        if (YUI.themeApplyToTree()) {
            console.log('Theme applied to UI');
        }
    }
}

// 卸载主题
YUI.themeUnload('old-theme');
```

### 图层操作示例

```javascript
// 设置文本
YUI.setText('button1', 'Click Me');

// 获取文本
var text = YUI.getText('label1');

// 设置背景色
YUI.setBgColor('panel1', '#FF0000');

// 显示/隐藏
YUI.hide('loading');
YUI.show('content');

// 从JSON渲染
var json = '{"type": "Label", "text": "Hello", "style": {"width": 100}}';
YUI.renderFromJson('container', json);

// JSON增量更新
var update = {
    "target": "button1",
    "style": {"bgColor": "#00FF00", "text": "Updated"}
};
YUI.update(JSON.stringify(update));
```

### Socket 示例 (mquickjs/QuickJS)

```javascript
// 创建TCP socket
var fd = Socket.socket(Socket.TCP);

// 连接到服务器
if (Socket.connect(fd, '127.0.0.1', 8080, 5000) === 0) {
    console.log('Connected');
    
    // 发送数据
    Socket.send(fd, 'Hello Server', 0);
    
    // 接收数据
    var data = Socket.recv(fd, 1024, 0);
    console.log('Received: ' + data);
    
    // 关闭socket
    Socket.close(fd);
}
```

## 编译和构建

### 生成 YUI stdlib 头文件

```bash
ya -c yui-stdlib-host && ya -b yui-stdlib-host
```

### 构建 playground

```bash
make playground
```

### 构建其他目标

```bash
make main          # 主程序
make mqjs          # mquickjs 版本
make run           # 运行主程序
make clean         # 清理构建文件
```

## 注意事项

1. **引擎兼容性**：部分 API 只在特定引擎中可用，Socket API 仅在 mquickjs 和 QuickJS 中可用
2. **内存管理**：QuickJS 需要手动释放字符串，mquickjs 和 Mario 不需要
3. **错误处理**：不同引擎的错误返回值类型不同，mquickjs/Mario 返回整数代码，QuickJS 返回布尔值
4. **异步支持**：目前只支持同步 API，异步操作需要使用定时器模拟

## 调试技巧

1. 使用 `YUI.log()` 或 `console.log()` 输出调试信息
2. 检查返回值判断操作是否成功
3. 使用 `JSON.stringify()` 将对象转为字符串输出
4. 在 C 代码中添加 `printf` 调试原生函数调用
5. 使用 `make clean` 清理后重新构建确保代码更新生效

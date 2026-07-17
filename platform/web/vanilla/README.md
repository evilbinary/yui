# Web platform (vanilla)

纯 HTML + WASM 宿主，引擎由 `ya -p em` 交叉编译，产物自动拷到 `public/yui/`。

## 1. 编译 WASM

```bash
ya -p em -m release -b yui-web.html
```

或 Makefile：

```bash
make web
```

成功后生成：

```text
platform/web/vanilla/yui/
  playground.js
  playground.wasm
  playground.data    # Emscripten preload（若启用）
```

## 2. 本地预览

```bash
cd platform/web/vanilla
python -m http.server 8080
```

浏览器打开 `http://localhost:8080/`，默认加载 `app/tests/login.json`。

URL 参数：

- `?json=app/watch-os/app.json`
- `?assets=app/assets`

## 3. 目录

```text
platform/web/vanilla/
  index.html          页面壳 + canvas
  yui-loader.js       加载 WASM、传 argv
  main.c              入口（yui_boot）
  yui/                ya 构建产出（gitignore）
```

## 4. 与 app/playground.html 的关系

`app/ya.py` 里原有 `playground.html` 等目标保留，用于直接 emcc 调试。  
`yui-web.html` 走 `platform/common/yui_boot.c`，与 PC / 移动端统一启动流程。

# YUI Theme System - 主题系统使用文档

## 概述

YUI主题系统允许您通过JSON配置文件统一管理应用的主题样式，支持按组件ID和类型批量设置样式属性。

## 核心特性

- ✅ **ID选择器**：针对特定组件ID设置样式（如 `#button1`）
- ✅ **类型选择器**：针对所有同类型组件设置样式（如 `Button`）
- ✅ **优先级机制**：ID选择器 > 类型选择器 > 组件默认样式
- ✅ **热切换**：运行时动态切换主题
- ✅ **本地存储**：自动保存用户主题偏好

## 文件结构

```
app/
├── lib/
│   └── theme.js          # 主题系统JavaScript库
├── dark-theme.json       # 深色主题示例
├── light-theme.json      # 浅色主题示例
└── your-app.json         # 你的应用界面配置
```

## 快速开始

### 1. 创建主题文件

创建 `dark-theme.json`：

```json
{
  "name": "dark-theme",
  "version": "1.0",
  "styles": [
    {
      "selector": "Button",
      "style": {
        "bgColor": "#16A085",
        "color": "#ffffff",
        "fontSize": 24,
        "borderRadius": 8
      }
    },
    {
      "selector": "#submitBtn",
      "style": {
        "bgColor": "#3498DB",
        "fontSize": 16
      }
    }
  ]
}
```

### 2. 在应用中使用

#### 方法1：自动加载并切换

```javascript
// 加载并应用主题
await Theme.switch('app/dark-theme.json', 'dark-theme');
```

#### 方法2：分步操作

```javascript
// 1. 加载主题
await Theme.load('app/dark-theme.json', 'dark-theme');

// 2. 设置为当前主题
Theme.setCurrent('dark-theme');

// 3. 应用到UI
Theme.apply();
```

### 3. 创建主题切换按钮

```javascript
// 创建切换按钮
const toggleBtn = Theme.ThemeSwitcher.createButton();
document.body.appendChild(toggleBtn);

// 点击按钮会显示主题选择器
```

## API 参考

### 核心方法

#### `Theme.load(themePath, themeName)`
加载主题文件。

- **参数**：
  - `themePath` (string): 主题JSON文件路径
  - `themeName` (string, optional): 主题名称
- **返回**：`Promise<Object>` 主题信息
- **示例**：
  ```js
  await Theme.load('app/dark-theme.json', 'dark-theme');
  ```

#### `Theme.setCurrent(themeName)`
设置当前主题。

- **参数**：
  - `themeName` (string): 主题名称
- **返回**：`boolean` 是否成功
- **示例**：
  ```js
  Theme.setCurrent('dark-theme');
  ```

#### `Theme.apply()`
应用当前主题到UI。

- **返回**：`boolean` 是否成功
- **示例**：
  ```js
  Theme.apply();
  ```

#### `Theme.switch(themePath, themeName)`
加载并切换主题（快捷方法）。

- **参数**：
  - `themePath` (string): 主题文件路径
  - `themeName` (string): 主题名称
- **返回**：`Promise<Object>` 主题信息
- **示例**：
  ```js
  await Theme.switch('app/dark-theme.json', 'dark-theme');
  ```

### 查询方法

#### `Theme.getCurrent()`
获取当前主题信息。

- **返回**：`Object|null` 主题信息
- **示例**：
  ```js
  const current = Theme.getCurrent();
  console.log(current.name); // 'dark-theme'
  ```

#### `Theme.getLoadedThemes()`
获取所有已加载的主题列表。

- **返回**：`Array<string>` 主题名称数组
- **示例**：
  ```js
  const themes = Theme.getLoadedThemes();
  // ['dark-theme', 'light-theme']
  ```

#### `Theme.isLoaded(themeName)`
检查主题是否已加载。

- **参数**：
  - `themeName` (string): 主题名称
- **返回**：`boolean`
- **示例**：
  ```js
  if (Theme.isLoaded('dark-theme')) {
      Theme.setCurrent('dark-theme');
  }
  ```

### 本地存储

#### `Theme.saveToStorage(storageKey)`
保存当前主题到本地存储。

- **参数**：
  - `storageKey` (string, optional): 存储key，默认 `'app-theme'`
- **示例**：
  ```js
  Theme.saveToStorage(); // 保存到 localStorage['app-theme']
  ```

#### `Theme.restoreFromStorage(storageKey)`
从本地存储恢复主题设置。

- **参数**：
  - `storageKey` (string, optional): 存储key，默认 `'app-theme'`
- **返回**：`Promise<Object|null>` 主题信息
- **示例**：
  ```js
  // 应用启动时恢复主题
  await Theme.restoreFromStorage();
  ```

### 事件系统

#### `Theme.on(event, callback)`
添加事件监听器。

- **事件类型**：
  - `themeLoaded`: 主题加载完成
  - `themeChanged`: 主题切换
  - `themeApplied`: 主题应用到UI
  - `themeUnloaded`: 主题卸载
- **示例**：
  ```js
  Theme.on('themeChanged', (data) => {
      console.log(`Switched from ${data.oldTheme} to ${data.newTheme}`);
  });
  ```

#### `Theme.off(event, callback)`
移除事件监听器。

- **示例**：
  ```js
  Theme.off('themeChanged', callback);
  ```

### UI组件

#### `Theme.ThemeSwitcher.createButton(options)`
创建主题切换按钮。

- **参数**：
  - `options` (object, optional): 配置选项
    - `position`: 定位方式，默认 `'fixed'`
    - `top`: 上边距，默认 `'20px'`
    - `right`: 右边距，默认 `'20px'`
    - `zIndex`: z-index，默认 `1000`
- **返回**：`HTMLElement` 按钮元素
- **示例**：
  ```js
  const btn = Theme.ThemeSwitcher.createButton({
      top: '30px',
      right: '30px'
  });
  document.body.appendChild(btn);
  ```

## 主题文件格式

### 基本结构

```json
{
  "name": "theme-name",
  "version": "1.0",
  "description": "主题描述",
  "styles": [
    {
      "selector": "#componentId",     // ID选择器（优先级高）
      "style": { ... }
    },
    {
      "selector": "ComponentType",    // 类型选择器（优先级中）
      "style": { ... }
    }
  ]
}
```

### 支持的样式属性

| 属性 | 类型 | 说明 | 示例 |
|------|------|------|------|
| `color` | string | 文字颜色（支持#RRGGBB、#RRGGBBAA、rgb()、rgba()） | `"#ffffff"` |
| `bgColor` | string | 背景颜色 | `"#2C3E50"` |
| `fontSize` | number | 字体大小（px） | `24` |
| `fontWeight` | string | 字体粗细 | `"bold"` |
| `borderRadius` | number | 圆角半径（px） | `8` |
| `borderWidth` | number | 边框宽度（px） | `2` |
| `borderColor` | string | 边框颜色 | `"#000000"` |
| `spacing` | number | 子组件间距（px） | `15` |
| `padding` | array | 内边距 `[top, right, bottom, left]` | `[20, 20, 20, 20]` |
| `opacity` | number | 透明度（0-255） | `255` |

### 选择器优先级

优先级从高到低：

1. **ID选择器**：`#componentId`（最高优先级）
2. **类型选择器**：`ComponentType`（中优先级）
3. **组件默认样式**（最低优先级）

**注意**：组件JSON中的style属性会覆盖主题样式。

## 完整示例

### 1. 创建应用界面

`app.json`:

```json
{
  "id": "mainView",
  "type": "View",
  "children": [
    {
      "id": "title",
      "type": "Label",
      "text": "欢迎使用"
    },
    {
      "id": "submitBtn",
      "type": "Button",
      "text": "提交"
    }
  ]
}
```

### 2. 创建深色主题

`dark-theme.json`:

```json
{
  "name": "dark-theme",
  "version": "1.0",
  "styles": [
    {
      "selector": "View",
      "style": {
        "bgColor": "#1a1a1a",
        "padding": [20, 20, 20, 20]
      }
    },
    {
      "selector": "Label",
      "style": {
        "color": "#ffffff",
        "fontSize": 20
      }
    },
    {
      "selector": "Button",
      "style": {
        "bgColor": "#3498db",
        "color": "#ffffff",
        "borderRadius": 8
      }
    },
    {
      "selector": "#submitBtn",
      "style": {
        "bgColor": "#2ecc71"
      }
    }
  ]
}
```

### 3. 应用启动脚本

```javascript
// 初始化主题系统
async function initApp() {
    try {
        // 从本地存储恢复主题
        await Theme.restoreFromStorage();
        
        // 如果没有保存的主题，加载默认主题
        if (!Theme.getCurrent()) {
            await Theme.switch('app/dark-theme.json', 'dark-theme');
        }
        
        // 创建主题切换按钮
        const toggleBtn = Theme.ThemeSwitcher.createButton();
        document.body.appendChild(toggleBtn);
        
        console.log('App initialized with theme:', Theme.getCurrent().name);
    } catch (error) {
        console.error('Failed to initialize theme:', error);
    }
}

// 监听主题切换事件
Theme.on('themeChanged', (data) => {
    console.log(`Theme changed: ${data.oldTheme} → ${data.newTheme}`);
    
    // 保存用户偏好
    Theme.saveToStorage();
    
    // 更新UI状态
    updateUIState();
});

// 应用启动
if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', initApp);
} else {
    initApp();
}
```

## 高级用法

### 动态主题生成

```javascript
// 根据用户偏好动态生成主题
function createCustomTheme(primaryColor, secondaryColor) {
    const themeData = {
        name: 'custom-theme',
        version: '1.0',
        styles: [
            {
                selector: 'Button',
                style: {
                    bgColor: primaryColor,
                    color: '#ffffff'
                }
            },
            {
                selector: '#accent',
                style: {
                    bgColor: secondaryColor
                }
            }
        ]
    };
    
    // 保存为JSON文件（实际项目中需要后端支持）
    const themePath = `app/custom-themes/${Date.now()}.json`;
    // ... 保存逻辑
    
    return Theme.switch(themePath, 'custom-theme');
}
```

### 主题预设

```javascript
// 定义主题预设
const THEME_PRESETS = {
    'dark': {
        name: '深色主题',
        path: 'app/dark-theme.json',
        icon: '🌙'
    },
    'light': {
        name: '浅色主题', 
        path: 'app/light-theme.json',
        icon: '☀️'
    }
};

// 批量加载预设
async function loadThemePresets() {
    const promises = Object.keys(THEME_PRESETS).map(key => {
        const preset = THEME_PRESETS[key];
        return Theme.load(preset.path, key)
            .then(() => console.log(`Loaded: ${preset.name}`))
            .catch(err => console.warn(`Failed to load ${key}:`, err));
    });
    
    await Promise.all(promises);
}
```

## 注意事项

1. **C函数实现**：需要在C代码中实现 `_themeLoad`、`_themeSetCurrent`、`_themeUnload`、`_themeApplyToTree` 四个函数
2. **样式优先级**：组件JSON中的style属性会覆盖主题样式
3. **性能**：主题切换会重新渲染所有组件，避免频繁切换
4. **错误处理**：加载失败时提供回退主题

## 常见问题

### Q: 主题样式不生效？
A: 检查：
- 主题文件路径是否正确
- 选择器格式是否正确（ID选择器以 `#` 开头）
- 主题是否已加载并设置为当前主题
- 组件JSON中的style是否覆盖了主题样式

### Q: 如何调试主题系统？
A: 启用控制台日志：
```javascript
Theme.on('themeLoaded', data => console.log('Loaded:', data));
Theme.on('themeChanged', data => console.log('Changed:', data));
Theme.on('themeApplied', data => console.log('Applied:', data));
```

### Q: 如何扩展主题功能？
A: 通过事件系统监听主题变化，自定义处理逻辑：
```javascript
Theme.on('themeChanged', (data) => {
    // 自定义逻辑
    updateCustomComponents(data.newTheme);
});
```

## 更新日志

### v1.0.0
- ✅ 初始版本
- ✅ 支持ID和类型选择器
- ✅ 主题加载、切换、应用
- ✅ 本地存储支持
- ✅ 事件系统
- ✅ UI切换组件

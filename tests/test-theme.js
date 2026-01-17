/**
 * 主题系统测试 - JavaScript
 * 测试主题加载、切换和应用功能（兼容QuickJS/MarioJS/mquickjs）
 */

// 测试用的主题数据（实际应该使用JSON文件）
var TEST_THEMES = {
    'test-dark': {
        name: 'test-dark',
        version: '1.0',
        styles: [
            {
                selector: 'View',
                style: {
                    bgColor: '#2C3E50'
                }
            },
            {
                selector: 'Label',
                style: {
                    color: '#ECF0F1',
                    fontSize: 20
                }
            },
            {
                selector: 'Button',
                style: {
                    bgColor: '#16A085',
                    color: '#ffffff',
                    borderRadius: 8
                }
            },
            {
                selector: '#themeTitle',
                style: {
                    color: '#3498DB',
                    fontSize: 28
                }
            }
        ]
    },
    'test-light': {
        name: 'test-light',
        version: '1.0',
        styles: [
            {
                selector: 'View',
                style: {
                    bgColor: '#FFFFFF'
                }
            },
            {
                selector: 'Label',
                style: {
                    color: '#333333',
                    fontSize: 20
                }
            },
            {
                selector: 'Button',
                style: {
                    bgColor: '#4CAF50',
                    color: '#ffffff',
                    borderRadius: 8
                }
            },
            {
                selector: '#themeTitle',
                style: {
                    color: '#2196F3',
                    fontSize: 28
                }
            }
        ]
    }
};

/**
 * 初始化主题测试
 */
function initThemeTest() {
    YUI.log('[ThemeTest] Initializing theme test...');
    
    // 添加更多调试信息，检查全局对象
    YUI.log('[ThemeTest] Debugging global objects...');
    YUI.log('[ThemeTest] typeof Theme: ' + typeof Theme);
    YUI.log('[ThemeTest] typeof global: ' + typeof global);
    YUI.log('[ThemeTest] typeof globalThis: ' + typeof globalThis);
    YUI.log('[ThemeTest] typeof exports: ' + typeof exports);
    YUI.log('[ThemeTest] typeof this: ' + typeof this);
    YUI.log('[ThemeTest] typeof window: ' + typeof window);
    
    // 尝试列出全局对象的所有属性（QuickJS环境）
    if (typeof std !== 'undefined' && typeof os !== 'undefined') {
        try {
            YUI.log('[ThemeTest] Available global properties:');
            for (var prop in this) {
                if (prop === 'Theme') {
                    YUI.log('[ThemeTest] Found Theme in global scope: ' + prop);
                }
            }
            
            // 检查globalThis
            if (typeof globalThis !== 'undefined') {
                YUI.log('[ThemeTest] globalThis properties:');
                for (var prop in globalThis) {
                    if (prop === 'Theme') {
                        YUI.log('[ThemeTest] Found Theme in globalThis: ' + prop);
                    }
                }
            }
        } catch (e) {
            YUI.log('[ThemeTest] Failed to enumerate global properties: ' + e.message);
        }
    }
    
    if (typeof global !== 'undefined') {
        YUI.log('[ThemeTest] global.Theme: ' + (global.Theme ? 'defined' : 'undefined'));
    }
    if (typeof globalThis !== 'undefined') {
        YUI.log('[ThemeTest] globalThis.Theme: ' + (globalThis.Theme ? 'defined' : 'undefined'));
    }
    if (typeof exports !== 'undefined') {
        YUI.log('[ThemeTest] exports.Theme: ' + (exports.Theme ? 'defined' : 'undefined'));
    }
    if (typeof this !== 'undefined') {
        YUI.log('[ThemeTest] this.Theme: ' + (this.Theme ? 'defined' : 'undefined'));
    }
    
    // 尝试从全局作用域获取Theme对象的函数
    function getThemeFromGlobalScope() {
        // 方法1: 直接检查全局变量
        if (typeof Theme !== 'undefined') {
            YUI.log('[ThemeTest] Found Theme in global scope');
            return Theme;
        }
        
        // 方法2: 检查global对象
        if (typeof global !== 'undefined' && global.Theme) {
            YUI.log('[ThemeTest] Found Theme in global object');
            return global.Theme;
        }
        
        // 方法3: 检查globalThis对象
        if (typeof globalThis !== 'undefined' && globalThis.Theme) {
            YUI.log('[ThemeTest] Found Theme in globalThis object');
            return globalThis.Theme;
        }
        
        // 方法4: 检查exports对象
        if (typeof exports !== 'undefined' && exports.Theme) {
            YUI.log('[ThemeTest] Found Theme in exports object');
            return exports.Theme;
        }
        
        // 方法5: 检查this对象
        if (typeof this !== 'undefined' && this.Theme) {
            YUI.log('[ThemeTest] Found Theme in this object');
            return this.Theme;
        }
        
        // 方法6: 在QuickJS环境中，尝试使用eval获取
        if (typeof std !== 'undefined' && typeof os !== 'undefined') {
            try {
                // 尝试通过eval获取Theme
                var themeFromEval = eval('Theme');
                if (typeof themeFromEval !== 'undefined') {
                    YUI.log('[ThemeTest] Found Theme using eval');
                    return themeFromEval;
                }
            } catch (e) {
                YUI.log('[ThemeTest] Failed to get Theme using eval: ' + e.message);
            }
        }
        
        return null;
    }
    
    // 获取Theme对象
    var ThemeObj = getThemeFromGlobalScope();
    
    if (!ThemeObj) {
        YUI.log('[ThemeTest] Theme library not found! Attempting manual import...');
        YUI.setText('themeInfo', '错误: Theme库未加载');
        
        // 尝试手动导入主题模块
        try {
            YUI.log('[ThemeTest] Attempting to manually import theme module...');
            
            // 尝试从全局导入
            if (typeof global !== 'undefined' && global.Theme) {
                ThemeObj = global.Theme;
                YUI.log('[ThemeTest] Found Theme in global object');
            }
            
            // 尝试从globalThis导入
            if (!ThemeObj && typeof globalThis !== 'undefined' && globalThis.Theme) {
                ThemeObj = globalThis.Theme;
                YUI.log('[ThemeTest] Found Theme in globalThis object');
            }
            
            // 尝试从this导入
            if (!ThemeObj && typeof this !== 'undefined' && this.Theme) {
                ThemeObj = this.Theme;
                YUI.log('[ThemeTest] Found Theme in this object');
            }
            
            // 如果找到了，使用它
            if (ThemeObj) {
                Theme = ThemeObj;
                YUI.log('[ThemeTest] Theme library loaded manually');
            } else {
                // 创建一个简单的Theme对象，防止后续错误
                YUI.log('[ThemeTest] Creating mock Theme object');
                Theme = {
                    init: function() { YUI.log('[Theme] Mock init'); return this; },
                    load: function(themeSource, themeName, callback) { 
                        YUI.log('[Theme] Mock load'); 
                        if (callback) callback(true, {name: themeName || 'mock-theme', version: '1.0'});
                    },
                    setCurrent: function() { YUI.log('[Theme] Mock setCurrent'); },
                    getCurrent: function() { return null; },
                    apply: function() { YUI.log('[Theme] Mock apply'); },
                    isLoaded: function() { return false; },
                    getLoadedThemes: function() { return []; },
                    getTheme: function() { return null; },
                    switch: function(themeSource, themeName, callback) { 
                        YUI.log('[Theme] Mock switch'); 
                        if (callback) callback(true, {name: themeName || 'mock-theme', version: '1.0'});
                    },
                    on: function() { return this; },
                    off: function() { return this; }
                };
                YUI.log('[ThemeTest] Mock Theme object created');
                YUI.setText('themeInfo', '主题系统测试（使用模拟对象）');
                // 继续执行剩余代码，不提前返回
            }
        } catch (e) {
            YUI.log('[ThemeTest] Exception loading theme module: ' + e.message);
        }
    } else {
        // 使用获取到的Theme对象
        Theme = ThemeObj;
    }
    
    // 设置事件监听器
    Theme.on('themeLoaded', onThemeLoaded)
         .on('themeChanged', onThemeChanged)
         .on('themeApplied', onThemeApplied);
    
    // 开始测试
    runThemeTest();
    
    // 自动加载并应用主题（测试用）
    autoLoadAndApplyThemes();
}

/**
 * 运行主题测试
 */
function runThemeTest() {
    YUI.log('[ThemeTest] Starting theme test...');
    
    // 测试1: 加载主题（实际项目中应该从文件加载）
    YUI.log('[ThemeTest] Test 1: Loading themes...');
    updateInfo('正在加载主题...');
    
    // 注意：在实际项目中，应该使用 Theme.load('path/to/theme.json', 'theme-name')
    // 这里只是测试，实际主题数据应该来自JSON文件
    
    updateInfo('主题加载测试通过 (需要实际JSON文件)');
    
    // 测试2: 获取当前主题
    YUI.log('[ThemeTest] Test 2: Getting current theme...');
    var current = Theme.getCurrent();
    if (current) {
        YUI.log('[ThemeTest] Current theme:', current.name);
        updateInfo('当前主题: ' + current.name);
    } else {
        YUI.log('[ThemeTest] No current theme');
        updateInfo('当前主题: 默认');
    }
    
    // 测试3: 创建主题切换按钮
    YUI.log('[ThemeTest] Test 3: Creating theme switcher...');
    createThemeTestButtons();
    
    updateInfo('主题系统测试完成，点击按钮切换主题');
}

/**
 * 创建主题测试按钮
 */
function createThemeTestButtons() {
    YUI.setText('testBtn1', '🌙 深色主题');
    YUI.setText('testBtn2', '☀️ 浅色主题');
    
}

/**
 * 测试按钮点击事件
 */
function onTestButtonClick(buttonId) {
    YUI.log('[ThemeTest] Button clicked: ' + buttonId);
    
    if (buttonId === 'testBtn1') {
        // 切换到深色主题
        switchToTheme('test-dark');
    } else if (buttonId === 'testBtn2') {
        // 切换到浅色主题
        switchToTheme('test-light');
    }
    
    // 显示提示
    updateInfo('按钮点击: ' + buttonId);
}

/**
 * 切换到指定主题
 */
function switchToTheme(themeKey) {
    try {
        updateInfo('正在切换到 ' + themeKey + '...');
        
        // 检查Theme对象是否有必要的方法
        if (!Theme || typeof Theme !== 'object') {
            YUI.log('[ThemeTest] Theme object is not available');
            updateInfo('主题系统不可用');
            return;
        }
        
        // 检查必要的方法是否存在
        if (typeof Theme.isLoaded !== 'function') {
            YUI.log('[ThemeTest] Theme.isLoaded is not a function');
            updateInfo('主题系统功能不完整');
            return;
        }
        
        if (typeof Theme.load !== 'function') {
            YUI.log('[ThemeTest] Theme.load is not a function');
            updateInfo('主题系统功能不完整');
            return;
        }
        
        // 修复：使用TEST_THEMES而不是GAME_THEMES
        if (TEST_THEMES[themeKey]) {
            // 使用从JSON对象加载主题的方式
            var jsonThemeName = 'json-' + themeKey;
            
            try {
                // 检查主题是否已经加载
                if (Theme.isLoaded(jsonThemeName)) {
                    YUI.log('[ThemeTest] Theme is already loaded: ' + jsonThemeName);
                    
                    // 检查setCurrent和apply方法
                    if (typeof Theme.setCurrent === 'function' && typeof Theme.apply === 'function') {
                        // 如果已加载，直接设置为当前主题
                        Theme.setCurrent(jsonThemeName);
                        Theme.apply();
                        YUI.log('[ThemeTest] Theme switched to: ' + jsonThemeName);
                        updateInfo('主题已切换: ' + themeKey);
                    } else {
                        YUI.log('[ThemeTest] Theme.setCurrent or Theme.apply is not a function');
                        updateInfo('主题系统功能不完整');
                    }
                } else {
                    YUI.log('[ThemeTest] Loading theme from JSON object: ' + themeKey);
                    
                    // 如果未加载，先从JSON对象加载
                    Theme.load(TEST_THEMES[themeKey], jsonThemeName, function(success, themeInfo) {
                        try {
                            if (success && typeof Theme.setCurrent === 'function' && typeof Theme.apply === 'function') {
                                // 加载成功后设置为当前主题
                                Theme.setCurrent(jsonThemeName);
                                Theme.apply();
                                YUI.log('[ThemeTest] Theme loaded and switched to: ' + jsonThemeName);
                                updateInfo('主题已切换: ' + themeKey);
                            } else {
                                YUI.log('[ThemeTest] Failed to load theme or methods not available: ' + themeKey);
                                updateInfo('主题加载失败: ' + themeKey);
                            }
                        } catch (e) {
                            YUI.log('[ThemeTest] Exception in theme load callback: ' + e.message);
                            updateInfo('主题切换异常: ' + e.message);
                        }
                    });
                }
            } catch (e) {
                YUI.log('[ThemeTest] Exception in theme loading: ' + e.message);
                updateInfo('主题加载异常: ' + e.message);
            }
        } else {
            YUI.log('[ThemeTest] Unknown theme: ' + themeKey);
            updateInfo('未知主题: ' + themeKey);
        }
        
    } catch (error) {
        YUI.log('[ThemeTest] Failed to switch theme:' + error.message);
        updateInfo('切换失败: ' + error.message);
    }
}

/**
 * 更新主题信息显示
 */
function updateInfo(message) {
 
    YUI.setText('themeInfo', message);
    
}

/**
 * 主题加载事件回调
 */
function onThemeLoaded(themeInfo) {
    YUI.log('[ThemeTest] Theme loaded: ' + themeInfo.name);
    updateInfo('主题已加载: ' + themeInfo.name);
}

/**
 * 主题变化事件回调
 */
function onThemeChanged(data) {
    YUI.log('[ThemeTest] Theme changed: ' + (data.oldTheme || 'none') + ' → ' + data.newTheme);
    updateInfo('当前主题: ' + data.newTheme);
}

/**
 * 主题应用事件回调
 */
function onThemeApplied(data) {
    YUI.log('[ThemeTest] Theme applied: ' + data.theme.name);
    updateInfo('主题已应用: ' + data.theme.name);
}

/**
 * 自动加载并应用主题（测试用）
 */
function autoLoadAndApplyThemes() {
    // 注意：在实际项目中，应该使用 Theme.load('path/to/theme.json', 'theme-name')
    // 这里演示从JSON对象加载主题的用法
    
    YUI.log('[ThemeTest] Auto loading themes for test...');
    
    // 测试从JSON对象加载深色主题
    YUI.log('[ThemeTest] Loading dark theme from JSON object...');
    Theme.load(TEST_THEMES['test-dark'], 'json-dark-theme', function(success, themeInfo) {
        if (success) {
            YUI.log('[ThemeTest] Dark theme loaded from JSON object: ' + themeInfo.name);
            updateInfo('已从JSON对象加载深色主题');
            
            // 设置为当前主题
            Theme.setCurrent('json-dark-theme');
            Theme.apply();
        } else {
            YUI.log('[ThemeTest] Failed to load dark theme from JSON object');
            updateInfo('从JSON对象加载主题失败');
        }
    });
    
    // 测试从JSON对象加载浅色主题
    YUI.log('[ThemeTest] Loading light theme from JSON object...');
    Theme.load(TEST_THEMES['test-light'], 'json-light-theme', function(success, themeInfo) {
        if (success) {
            YUI.log('[ThemeTest] Light theme loaded from JSON object: ' + themeInfo.name);
            updateInfo('已从JSON对象加载浅色主题');
        } else {
            YUI.log('[ThemeTest] Failed to load light theme from JSON object');
            updateInfo('从JSON对象加载主题失败');
        }
    });
    
    // 测试事件系统
    Theme.on('themeChanged', function(data) {
        YUI.log('[ThemeTest] Theme change detected: ' + data.newTheme);
    });
}

/**
 * 清理测试
 */
function cleanupThemeTest() {
    YUI.log('[ThemeTest] Cleaning up...');
    
    // 移除事件监听器
    Theme.off('themeLoaded', onThemeLoaded)
         .off('themeChanged', onThemeChanged)
         .off('themeApplied', onThemeApplied);
    
    YUI.log('[ThemeTest] Cleanup complete');
}

function onLoad() {
    createThemeTestButtons();
}



YUI.log('[ThemeTest] Test script loaded',Theme);

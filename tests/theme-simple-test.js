/**
 * 简单的主题系统测试
 * 直接在文件中定义Theme对象，避免作用域问题
 */

// 直接定义Theme对象
var Theme = {
    // 已加载的主题
    loadedThemes: {},
    
    // 当前主题
    currentTheme: null,
    
    // 初始化
    init: function() {
        YUI.log('[Theme] Theme library initialized');
        return this;
    },
    
    // 加载主题
    load: function(themeSource, themeName, callback) {
        YUI.log('[Theme] Loading theme: ' + themeName);
        
        // 模拟加载成功
        this.loadedThemes[themeName] = {
            name: themeName,
            version: '1.0',
            loadedAt: Date.now()
        };
        
        if (callback) callback(true, this.loadedThemes[themeName]);
    },
    
    // 设置当前主题
    setCurrent: function(themeName) {
        if (!this.loadedThemes[themeName]) {
            YUI.log('[Theme] Theme not loaded: ' + themeName);
            return false;
        }
        
        this.currentTheme = themeName;
        YUI.log('[Theme] Current theme set to: ' + themeName);
        return true;
    },
    
    // 应用当前主题
    apply: function() {
        if (!this.currentTheme) {
            YUI.log('[Theme] No current theme to apply');
            return false;
        }
        
        YUI.log('[Theme] Applied theme: ' + this.currentTheme);
        return true;
    },
    
    // 获取当前主题
    getCurrent: function() {
        return this.currentTheme ? this.loadedThemes[this.currentTheme] : null;
    },
    
    // 检查主题是否已加载
    isLoaded: function(themeName) {
        return !!this.loadedThemes[themeName];
    },
    
    // 事件系统（简化版）
    on: function(event, callback) {
        return this;
    },
    
    off: function(event, callback) {
        return this;
    }
};

// 测试用的主题数据
var TEST_THEMES = {
    'test-dark': {
        name: 'test-dark',
        version: '1.0'
    },
    'test-light': {
        name: 'test-light',
        version: '1.0'
    }
};

// 初始化测试
function initThemeTest() {
    YUI.log('[ThemeTest] Initializing theme test...');
    YUI.log('[ThemeTest] typeof Theme: ' + typeof Theme);
    
    if (typeof Theme === 'undefined') {
        YUI.log('[ThemeTest] ERROR: Theme is not defined');
        YUI.setText('themeInfo', '错误: Theme对象未定义');
        return;
    }
    
    // 初始化Theme系统
    Theme.init();
    
    // 测试加载主题
    Theme.load(TEST_THEMES['test-dark'], 'test-dark', function(success, themeInfo) {
        if (success) {
            YUI.log('[ThemeTest] Dark theme loaded: ' + themeInfo.name);
            YUI.setText('themeInfo', '深色主题已加载');
            
            // 设置为当前主题
            Theme.setCurrent('test-dark');
            Theme.apply();
        } else {
            YUI.log('[ThemeTest] Failed to load dark theme');
            YUI.setText('themeInfo', '主题加载失败');
        }
    });
    
    // 创建测试按钮
    YUI.setText('testBtn1', '🌙 深色主题');
    YUI.setText('testBtn2', '☀️ 浅色主题');
    
    YUI.log('[ThemeTest] Test initialized');
}

// 按钮点击处理
function onTestButtonClick(buttonId) {
    YUI.log('[ThemeTest] Button clicked: ' + buttonId);
    
    if (buttonId === 'testBtn1') {
        Theme.load(TEST_THEMES['test-dark'], 'test-dark', function(success, themeInfo) {
            if (success) {
                Theme.setCurrent('test-dark');
                Theme.apply();
                YUI.setText('themeInfo', '已切换到深色主题');
            }
        });
    } else if (buttonId === 'testBtn2') {
        Theme.load(TEST_THEMES['test-light'], 'test-light', function(success, themeInfo) {
            if (success) {
                Theme.setCurrent('test-light');
                Theme.apply();
                YUI.setText('themeInfo', '已切换到浅色主题');
            }
        });
    }
}

// 延迟初始化
function delayedInit() {
    YUI.log('[ThemeTest] Delayed initialization triggered');
    
    // 在QuickJS环境中使用sleep函数延迟
    try {
        if (typeof std !== 'undefined' && typeof os !== 'undefined' && typeof os.sleep === 'function') {
            os.sleep(100);
            YUI.log('[ThemeTest] Executing delayed initialization after sleep...');
            initThemeTest();
        } else {
            YUI.log('[ThemeTest] Executing delayed initialization...');
            initThemeTest();
        }
    } catch (e) {
        YUI.log('[ThemeTest] Sleep failed, executing directly...');
        initThemeTest();
    }
}

YUI.log('[ThemeTest] Simple test script loaded');
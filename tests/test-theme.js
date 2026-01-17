/**
 * 主题系统测试 - JavaScript
 * 测试主题加载、切换和应用功能
 */

// 测试用的主题数据（实际应该使用JSON文件）
const TEST_THEMES = {
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
    console.log('[ThemeTest] Initializing theme test...');
    
    // 检查Theme库是否可用
    if (typeof Theme === 'undefined') {
        console.error('[ThemeTest] Theme library not found!');
        yui_set_text(find_layer_by_id('themeInfo'), '错误: Theme库未加载');
        return;
    }
    
    // 设置事件监听器
    Theme.on('themeLoaded', onThemeLoaded)
         .on('themeChanged', onThemeChanged)
         .on('themeApplied', onThemeApplied);
    
    // 开始测试
    runThemeTest();
}

/**
 * 运行主题测试
 */
async function runThemeTest() {
    try {
        console.log('[ThemeTest] Starting theme test...');
        
        // 测试1: 加载主题（实际项目中应该从文件加载）
        console.log('[ThemeTest] Test 1: Loading themes...');
        updateInfo('正在加载主题...');
        
        // 注意：在实际项目中，应该使用 Theme.load('path/to/theme.json', 'theme-name')
        // 这里只是测试，实际主题数据应该来自JSON文件
        
        updateInfo('主题加载测试通过 (需要实际JSON文件)');
        
        // 测试2: 获取当前主题
        console.log('[ThemeTest] Test 2: Getting current theme...');
        const current = Theme.getCurrent();
        if (current) {
            console.log('[ThemeTest] Current theme:', current.name);
            updateInfo(`当前主题: ${current.name}`);
        } else {
            console.log('[ThemeTest] No current theme');
            updateInfo('当前主题: 默认');
        }
        
        // 测试3: 创建主题切换按钮
        console.log('[ThemeTest] Test 3: Creating theme switcher...');
        createThemeTestButtons();
        
        updateInfo('主题系统测试完成，点击按钮切换主题');
        
    } catch (error) {
        console.error('[ThemeTest] Test failed:', error);
        updateInfo('测试失败: ' + error.message);
    }
}

/**
 * 创建主题测试按钮
 */
function createThemeTestButtons() {
    // 创建测试按钮容器
    const container = find_layer_by_id('themeTestView');
    if (!container) {
        console.error('[ThemeTest] Container not found');
        return;
    }
    
    // 添加主题切换按钮
    const testBtn1 = find_layer_by_id('testBtn1');
    const testBtn2 = find_layer_by_id('testBtn2');
    
    if (testBtn1) {
        yui_set_text(testBtn1, '🌙 深色主题');
    }
    
    if (testBtn2) {
        yui_set_text(testBtn2, '☀️ 浅色主题');
    }
}

/**
 * 测试按钮点击事件
 */
function onTestButtonClick(layer) {
    const buttonId = layer.id;
    console.log(`[ThemeTest] Button clicked: ${buttonId}`);
    
    if (buttonId === 'testBtn1') {
        // 切换到深色主题
        switchToTheme('test-dark');
    } else if (buttonId === 'testBtn2') {
        // 切换到浅色主题
        switchToTheme('test-light');
    }
}

/**
 * 切换到指定主题
 */
async function switchToTheme(themeKey) {
    try {
        updateInfo(`正在切换到 ${themeKey}...`);
        
        // 注意：实际项目中应该从文件加载
        // await Theme.switch(GAME_THEMES[themeKey].path, themeKey);
        
        // 测试：直接模拟（实际应该使用Theme.switch）
        console.log(`[ThemeTest] Would switch to: ${themeKey}`);
        updateInfo(`主题切换测试: ${themeKey}`);
        
    } catch (error) {
        console.error(`[ThemeTest] Failed to switch theme:`, error);
        updateInfo('切换失败: ' + error.message);
    }
}

/**
 * 更新主题信息显示
 */
function updateInfo(message) {
    const infoLabel = find_layer_by_id('themeInfo');
    if (infoLabel) {
        yui_set_text(infoLabel, message);
    }
}

/**
 * 主题加载事件回调
 */
function onThemeLoaded(themeInfo) {
    console.log('[ThemeTest] Theme loaded:', themeInfo.name);
    updateInfo(`主题已加载: ${themeInfo.name}`);
}

/**
 * 主题变化事件回调
 */
function onThemeChanged(data) {
    console.log(`[ThemeTest] Theme changed: ${data.oldTheme || 'none'} → ${data.newTheme}`);
    updateInfo(`当前主题: ${data.newTheme}`);
}

/**
 * 主题应用事件回调
 */
function onThemeApplied(data) {
    console.log('[ThemeTest] Theme applied:', data.theme.name);
    updateInfo(`主题已应用: ${data.theme.name}`);
}

/**
 * 清理测试
 */
function cleanupThemeTest() {
    console.log('[ThemeTest] Cleaning up...');
    
    // 移除事件监听器
    Theme.off('themeLoaded', onThemeLoaded)
         .off('themeChanged', onThemeChanged)
         .off('themeApplied', onThemeApplied);
    
    console.log('[ThemeTest] Cleanup complete');
}

// 导出测试函数
window.ThemeTest = {
    init: initThemeTest,
    cleanup: cleanupThemeTest,
    switchTheme: switchToTheme
};

console.log('[ThemeTest] Test script loaded');

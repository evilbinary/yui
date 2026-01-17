/**
 * 主题系统集成示例 - 记忆游戏
 * 
 * @description 演示如何在实际应用中使用主题系统
 * @usage 在记忆游戏中引入此脚本即可启用主题切换功能
 */

// 主题配置
const GAME_THEMES = {
    'dark-theme': {
        name: '深色主题',
        path: 'app/dark-theme.json',
        icon: '🌙',
        description: '深色模式，护眼舒适'
    },
    'light-theme': {
        name: '浅色主题',
        path: 'app/light-theme.json',
        icon: '☀️',
        description: '明亮清晰，经典风格'
    }
};

/**
 * 初始化游戏主题系统
 */
async function initGameThemes() {
    console.log('[GameThemes] Initializing...');
    
    try {
        // 批量加载所有主题
        const loadPromises = Object.keys(GAME_THEMES).map(async themeKey => {
            const themeInfo = GAME_THEMES[themeKey];
            try {
                await Theme.load(themeInfo.path, themeKey);
                console.log(`[GameThemes] Loaded: ${themeInfo.name}`);
                return { key: themeKey, success: true };
            } catch (error) {
                console.warn(`[GameThemes] Failed to load ${themeKey}:`, error);
                return { key: themeKey, success: false, error };
            }
        });
        
        const results = await Promise.all(loadPromises);
        const successfullyLoaded = results.filter(r => r.success);
        
        console.log(`[GameThemes] ${successfullyLoaded.length}/${results.length} themes loaded`);
        
        if (successfullyLoaded.length === 0) {
            console.error('[GameThemes] No themes loaded, theme system disabled');
            return false;
        }
        
        // 从本地存储获取用户偏好
        const savedTheme = localStorage.getItem('memory-game-theme');
        const availableThemes = successfullyLoaded.map(r => r.key);
        const defaultTheme = availableThemes.includes(savedTheme) ? savedTheme : availableThemes[0];
        
        // 应用主题
        if (Theme.setCurrent(defaultTheme)) {
            Theme.apply();
            console.log(`[GameThemes] Applied default theme: ${defaultTheme}`);
        }
        
        // 创建主题切换UI
        createThemeSwitcherUI();
        
        // 监听主题变化
        Theme.on('themeChanged', handleThemeChanged);
        
        return true;
        
    } catch (error) {
        console.error('[GameThemes] Initialization failed:', error);
        return false;
    }
}

/**
 * 创建主题切换UI
 */
function createThemeSwitcherUI() {
    // 避免重复创建
    if (document.getElementById('gameThemeToggle')) {
        return;
    }
    
    // 创建切换按钮
    const toggleBtn = Theme.ThemeSwitcher.createButton({
        top: '20px',
        right: '20px'
    });
    toggleBtn.id = 'gameThemeToggle';
    toggleBtn.title = '切换主题';
    
    document.body.appendChild(toggleBtn);
    console.log('[GameThemes] Theme switcher UI created');
}

/**
 * 处理主题变化事件
 */
function handleThemeChanged(data) {
    console.log(`[GameThemes] Theme changed: ${data.oldTheme || 'none'} → ${data.newTheme}`);
    
    // 保存用户偏好
    localStorage.setItem('memory-game-theme', data.newTheme);
    
    // 显示切换提示
    showThemeSwitchNotification(data.newTheme);
    
    // 可以在这里添加其他自定义逻辑
    // 例如：更新游戏状态、重新渲染特定组件等
}

/**
 * 显示主题切换提示
 */
function showThemeSwitchNotification(themeKey) {
    const themeInfo = GAME_THEMES[themeKey];
    if (!themeInfo) return;
    
    // 移除已存在的提示
    const existing = document.getElementById('themeNotification');
    if (existing) {
        existing.remove();
    }
    
    const notification = document.createElement('div');
    notification.id = 'themeNotification';
    notification.style.cssText = `
        position: fixed;
        top: 80px;
        right: 20px;
        background: #4CAF50;
        color: white;
        padding: 12px 20px;
        border-radius: 8px;
        box-shadow: 0 4px 12px rgba(0,0,0,0.15);
        font-size: 14px;
        z-index: 1002;
        animation: themeNotificationSlideIn 0.3s ease;
    `;
    
    notification.innerHTML = `
        <div style="display: flex; align-items: center; gap: 8px;">
            <span style="font-size: 18px;">${themeInfo.icon}</span>
            <span>已切换到 ${themeInfo.name}</span>
        </div>
    `;
    
    // 添加动画样式
    if (!document.getElementById('themeNotificationStyle')) {
        const style = document.createElement('style');
        style.id = 'themeNotificationStyle';
        style.textContent = `
            @keyframes themeNotificationSlideIn {
                from { transform: translateX(100%); opacity: 0; }
                to { transform: translateX(0); opacity: 1; }
            }
            @keyframes themeNotificationSlideOut {
                from { transform: translateX(0); opacity: 1; }
                to { transform: translateX(100%); opacity: 0; }
            }
        `;
        document.head.appendChild(style);
    }
    
    document.body.appendChild(notification);
    
    // 3秒后自动消失
    setTimeout(() => {
        notification.style.animation = 'themeNotificationSlideOut 0.3s ease';
        setTimeout(() => {
            if (notification.parentNode) {
                notification.remove();
            }
        }, 300);
    }, 3000);
}

/**
 * 手动切换主题（供游戏逻辑调用）
 */
function switchGameTheme(themeKey) {
    if (!GAME_THEMES[themeKey]) {
        console.error(`[GameThemes] Unknown theme: ${themeKey}`);
        return false;
    }
    
    if (!Theme.isLoaded(themeKey)) {
        console.error(`[GameThemes] Theme not loaded: ${themeKey}`);
        return false;
    }
    
    if (Theme.setCurrent(themeKey)) {
        Theme.apply();
        return true;
    }
    
    return false;
}

/**
 * 获取可用的主题列表
 */
function getAvailableThemes() {
    const loadedThemes = Theme.getLoadedThemes();
    return loadedThemes.map(key => ({
        key: key,
        ...GAME_THEMES[key]
    })).filter(theme => theme.name);
}

/**
 * 主题系统初始化入口
 */
function initThemeSystem() {
    // 等待DOM和Theme库加载完成
    const checkAndInit = () => {
        if (typeof Theme !== 'undefined' && document.body) {
            initGameThemes();
        } else {
            setTimeout(checkAndInit, 100);
        }
    };
    
    checkAndInit();
}

// 导出函数供外部使用
window.GameThemes = {
    init: initThemeSystem,
    switchTheme: switchGameTheme,
    getAvailableThemes: getAvailableThemes,
    GAME_THEMES: GAME_THEMES
};

console.log('[GameThemes] Module loaded');

// 自动初始化（可选）
// 如果不想自动初始化，可以注释掉下面这行，手动调用 GameThemes.init()
if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', initThemeSystem);
} else {
    initThemeSystem();
}

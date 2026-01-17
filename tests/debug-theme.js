/**
 * 主题系统调试脚本
 * 用于检查Theme对象是否正确导出
 */

// 确保全局对象可用
var global = (typeof globalThis !== 'undefined') ? globalThis :
             (typeof global !== 'undefined') ? global :
             (typeof this !== 'undefined') ? this : {};

// 调试函数
function debugTheme() {
    console.log('=== Theme System Debug ===');
    console.log('typeof Theme:', typeof Theme);
    console.log('typeof global:', typeof global);
    console.log('typeof globalThis:', typeof globalThis);
    console.log('typeof exports:', typeof exports);
    console.log('typeof this:', typeof this);
    console.log('typeof window:', typeof window);
    
    // 检查各种对象中的Theme
    if (typeof global !== 'undefined') {
        console.log('global.Theme:', global.Theme ? 'defined' : 'undefined');
    }
    if (typeof globalThis !== 'undefined') {
        console.log('globalThis.Theme:', globalThis.Theme ? 'defined' : 'undefined');
    }
    if (typeof exports !== 'undefined') {
        console.log('exports.Theme:', exports.Theme ? 'defined' : 'undefined');
    }
    if (typeof this !== 'undefined') {
        console.log('this.Theme:', this.Theme ? 'defined' : 'undefined');
    }
    if (typeof window !== 'undefined') {
        console.log('window.Theme:', window.Theme ? 'defined' : 'undefined');
    }
    
    // 列出全局对象的所有属性
    if (typeof std !== 'undefined' && typeof os !== 'undefined') {
        try {
            console.log('Available global properties:');
            for (var prop in this) {
                if (prop.indexOf('Theme') === 0) {
                    console.log('Found theme-related property:', prop);
                }
            }
            
            // 检查globalThis
            if (typeof globalThis !== 'undefined') {
                console.log('globalThis properties:');
                for (var prop in globalThis) {
                    if (prop.indexOf('Theme') === 0) {
                        console.log('Found theme-related property in globalThis:', prop);
                    }
                }
            }
        } catch (e) {
            console.log('Failed to enumerate global properties:', e.message);
        }
    }
    
    console.log('=== End Debug ===');
}

// 延迟执行调试
if (typeof std !== 'undefined' && typeof os !== 'undefined' && typeof os.sleep === 'function') {
    // 在QuickJS环境中使用sleep函数延迟
    os.sleep(100);
    debugTheme();
} else {
    // 其他环境直接执行
    debugTheme();
}
/**
 * 第二个脚本文件 - 尝试访问第一个文件中定义的变量
 */

// 延迟执行，确保第一个脚本已加载
function delayedCheck() {
    YUI.log('[Second Script] 开始检查变量...');
    
    // 检查各种变量
    YUI.log('[Second Script] typeof globalTestVar: ' + typeof globalTestVar);
    YUI.log('[Second Script] typeof testObject: ' + typeof testObject);
    
    if (typeof globalTestVar !== 'undefined') {
        YUI.log('[Second Script] globalTestVar value: ' + globalTestVar);
        YUI.setText('infoLabel', '找到变量: ' + globalTestVar);
    } else {
        YUI.log('[Second Script] globalTestVar 未找到');
    }
    
    if (typeof testObject !== 'undefined') {
        YUI.log('[Second Script] testObject.message: ' + testObject.message);
        YUI.setText('infoLabel', '找到对象: ' + testObject.message);
    } else {
        YUI.log('[Second Script] testObject 未找到');
    }
    
    // 检查不同方式设置的全局变量
    YUI.log('[Second Script] typeof fromFirstFile: ' + typeof fromFirstFile);
    if (typeof fromFirstFile !== 'undefined') {
        YUI.log('[Second Script] fromFirstFile value: ' + fromFirstFile);
    }
    
    if (typeof globalThis !== 'undefined') {
        YUI.log('[Second Script] typeof globalThis.fromFirstFile: ' + typeof globalThis.fromFirstFile);
        if (typeof globalThis.fromFirstFile !== 'undefined') {
            YUI.log('[Second Script] globalThis.fromFirstFile value: ' + globalThis.fromFirstFile);
        }
    }
    
    if (typeof global !== 'undefined') {
        YUI.log('[Second Script] typeof global.fromFirstFile: ' + typeof global.fromFirstFile);
        if (typeof global.fromFirstFile !== 'undefined') {
            YUI.log('[Second Script] global.fromFirstFile value: ' + global.fromFirstFile);
        }
    }
    
    if (typeof window !== 'undefined') {
        YUI.log('[Second Script] typeof window.fromFirstFile: ' + typeof window.fromFirstFile);
        if (typeof window.fromFirstFile !== 'undefined') {
            YUI.log('[Second Script] window.fromFirstFile value: ' + window.fromFirstFile);
        }
    }
    
    // 在QuickJS环境中检查特殊方式
    if (typeof std !== 'undefined' && typeof os !== 'undefined') {
        YUI.log('[Second Script] typeof this.fromFirstFileThis: ' + typeof this.fromFirstFileThis);
        if (typeof this.fromFirstFileThis !== 'undefined') {
            YUI.log('[Second Script] this.fromFirstFileThis value: ' + this.fromFirstFileThis);
        }
        
        YUI.log('[Second Script] typeof fromFirstFileEval: ' + typeof fromFirstFileEval);
        if (typeof fromFirstFileEval !== 'undefined') {
            YUI.log('[Second Script] fromFirstFileEval value: ' + fromFirstFileEval);
        }
        
        YUI.log('[Second Script] typeof fromFirstFileFunc: ' + typeof fromFirstFileFunc);
        if (typeof fromFirstFileFunc !== 'undefined') {
            YUI.log('[Second Script] fromFirstFileFunc value: ' + fromFirstFileFunc);
        }
    }
}

// 在QuickJS环境中使用sleep延迟
if (typeof std !== 'undefined' && typeof os !== 'undefined' && typeof os.sleep === 'function') {
    os.sleep(100);
    delayedCheck();
} else {
    delayedCheck();
}

YUI.log('[Second Script] 脚本已加载');
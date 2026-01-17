/**
 * YUI Theme System - JavaScript API Library
 * 
 * @version 1.0.0
 * @description 主题系统JavaScript API库，提供主题加载、切换和应用功能
 * 
 * @example
 * // 基本用法（兼容QuickJS/MarioJS/mquickjs）
 * // 加载并应用主题
 * Theme.load('app/dark-theme.json', 'dark-theme', function(success, themeInfo) {
 *     if (success) {
 *         Theme.setCurrent('dark-theme');
 *         Theme.apply();
 *     }
 * });
 * 
 * // 获取当前主题
 * var current = Theme.getCurrent();
 * if (current) {
 *     print('Current theme: ' + current.name);
 * }
 * 
 * // 事件监听
 * Theme.on('themeChanged', function(data) {
 *     print('Theme changed: ' + data.oldTheme + ' → ' + data.newTheme);
 * });
 * 
 * // 初始化
 * Theme.init();
 */

(function(global) {
    'use strict';

    // ==================== 兼容性检测 ====================
    
    // 检测运行环境
    var isQuickJS = typeof std !== 'undefined' && typeof os !== 'undefined';
    var isMarioJS = typeof global !== 'undefined' && typeof global.mario !== 'undefined';
    var isBrowser = typeof window !== 'undefined' && typeof document !== 'undefined';
    
    // 引擎名称（用于调试）
    var engineName = isQuickJS ? 'QuickJS' : (isMarioJS ? 'MarioJS' : (isBrowser ? 'Browser' : 'Unknown'));
    
    // 打印函数（兼容不同引擎）
    var log = function(msg) {
        if (isQuickJS) {
            std.puts('[Theme] ' + msg + '\n');
        } else if (typeof console !== 'undefined' && console.log) {
            console.log('[Theme] ' + msg);
        } else if (typeof print !== 'undefined') {
            print('[Theme] ' + msg);
        }
    };
    
    // 错误打印
    var error = function(msg) {
        if (isQuickJS) {
            std.printf('[Theme ERROR] %s\n', msg);
        } else if (typeof console !== 'undefined' && console.error) {
            console.error('[Theme] ' + msg);
        } else if (typeof print !== 'undefined') {
            print('[Theme ERROR] ' + msg);
        }
    };

    // ==================== 核心主题管理器 ====================
    
    /**
     * 主题管理器 - 内部使用
     * 管理主题加载、切换和应用
     */
    var ThemeManager = {
        // 当前主题名称
        currentTheme: null,
        
        // 已加载的主题列表
        loadedThemes: {},
        
        // 事件监听器
        listeners: {
            'themeLoaded': [],
            'themeChanged': [],
            'themeApplied': [],
            'themeUnloaded': []
        },
        
        /**
         * 触发事件
         */
        emit: function(event, data) {
            if (this.listeners[event]) {
                for (var i = 0; i < this.listeners[event].length; i++) {
                    try {
                        this.listeners[event][i](data);
                    } catch (e) {
                        error('Event handler error: ' + e.message);
                    }
                }
            }
        },
        
        /**
         * 添加事件监听器
         */
        on: function(event, callback) {
            if (this.listeners[event]) {
                this.listeners[event].push(callback);
            }
            return this;
        },
        
        /**
         * 移除事件监听器
         */
        off: function(event, callback) {
            if (this.listeners[event]) {
                var index = -1;
                for (var i = 0; i < this.listeners[event].length; i++) {
                    if (this.listeners[event][i] === callback) {
                        index = i;
                        break;
                    }
                }
                if (index > -1) {
                    this.listeners[event].splice(index, 1);
                }
            }
            return this;
        }
    };

    // ==================== 兼容性层 ====================

    /**
     * localStorage的polyfill（用于QuickJS/MarioJS）
     */
    var Storage = {
        data: {},
        
        getItem: function(key) {
            if (isBrowser && typeof localStorage !== 'undefined') {
                return localStorage.getItem(key);
            }
            return this.data[key] || null;
        },
        
        setItem: function(key, value) {
            if (isBrowser && typeof localStorage !== 'undefined') {
                localStorage.setItem(key, value);
            } else {
                this.data[key] = String(value);
            }
        },
        
        removeItem: function(key) {
            if (isBrowser && typeof localStorage !== 'undefined') {
                localStorage.removeItem(key);
            } else {
                delete this.data[key];
            }
        },
        
        clear: function() {
            if (isBrowser && typeof localStorage !== 'undefined') {
                localStorage.clear();
            } else {
                this.data = {};
            }
        }
    };

    // ==================== 公开API ====================

    var Theme = {
        /**
         * 加载主题文件
         * @param {string} themePath - 主题文件路径
         * @param {string} themeName - 主题名称（可选）
         * @param {Function} callback - 回调函数 function(success, themeInfo)
         */
        load: function(themePath, themeName, callback) {
            // 参数处理 - themeName是可选的
            if (typeof themeName === 'function') {
                callback = themeName;
                themeName = null;
            }
            
            // 参数验证
            if (!themePath) {
                if (callback) callback(false, null);
                return;
            }
            
            // 检查是否已加载
            if (themeName && ThemeManager.loadedThemes[themeName]) {
                log('[Theme] Theme \'' + themeName + '\' already loaded');
                if (callback) callback(true, ThemeManager.loadedThemes[themeName]);
                return;
            }
            
            try {
                // 调用C函数加载主题
                var result = _themeLoad(themePath);
                
                if (result && result.success) {
                    var loadedThemeName = themeName || result.name;
var themeInfo = {
                            name: loadedThemeName,
                            version: result.version,
                            path: themePath,
                            loadedAt: Date.now()
                        };
                    
                    ThemeManager.loadedThemes[loadedThemeName] = themeInfo;
                    ThemeManager.emit('themeLoaded', themeInfo);
                    
                    log('[Theme] Loaded: ' + loadedThemeName + ' (v' + result.version + ')');
                    if (callback) callback(true, themeInfo);
                } else {
                    error('Failed to load theme: ' + themePath);
                    if (callback) callback(false, result);
                }
            } catch (e) {
                error('Exception while loading theme: ' + e.message);
                if (callback) callback(false, null);
            }
        },
        
        /**
         * 设置当前主题
         * @param {string} themeName - 主题名称
         * @returns {boolean} - 是否成功
         */
        setCurrent: function(themeName) {
            // 参数验证
            if (!themeName) {
                error('Theme name is required');
                return false;
            }
            
            // 检查是否已加载
            if (!ThemeManager.loadedThemes[themeName]) {
                error('[Theme] Theme \'' + themeName + '\' not loaded. Call Theme.load() first.');
                return false;
            }
            
            try {
                // 调用C函数设置当前主题
                var result = _themeSetCurrent(themeName);
                
                if (result) {
                    var oldTheme = ThemeManager.currentTheme;
                    ThemeManager.currentTheme = themeName;
                    
                    ThemeManager.emit('themeChanged', {
                        oldTheme: oldTheme,
                        newTheme: themeName,
                        theme: ThemeManager.loadedThemes[themeName]
                    });
                    
                    log('[Theme] Current theme set to: ' + themeName);
                    return true;
                } else {
                    error('[Theme] Failed to set current theme: ' + themeName);
                    return false;
                }
            } catch (e) {
                error('[Theme] Exception while setting current theme: ' + e.message);
                return false;
            }
        },
        
        /**
         * 获取当前主题
         * @returns {Object|null} - 当前主题信息
         */
        getCurrent: function() {
            if (!ThemeManager.currentTheme) {
                return null;
            }
            
            return ThemeManager.loadedThemes[ThemeManager.currentTheme] || null;
        },
        
        /**
         * 获取已加载的主题列表
         * @returns {Array} - 主题名称数组
         */
        getLoadedThemes: function() {
            var themes = [];
            for (var key in ThemeManager.loadedThemes) {
                if (ThemeManager.loadedThemes.hasOwnProperty(key)) {
                    themes.push(key);
                }
            }
            return themes;
        },
        
        /**
         * 检查主题是否已加载
         * @param {string} themeName - 主题名称
         * @returns {boolean} - 是否已加载
         */
        isLoaded: function(themeName) {
            return !!ThemeManager.loadedThemes[themeName];
        },
        
        /**
         * 获取主题信息
         * @param {string} themeName - 主题名称
         * @returns {Object|null} - 主题信息
         */
        getTheme: function(themeName) {
            return ThemeManager.loadedThemes[themeName] || null;
        },
        
        /**
         * 卸载主题
         * @param {string} themeName - 主题名称
         * @returns {boolean} - 是否成功
         */
        unload: function(themeName) {
            // 参数验证
            if (!themeName) {
                error('[Theme] Theme name is required');
                return false;
            }
            
            // 不能卸载当前主题
            if (ThemeManager.currentTheme === themeName) {
                error('[Theme] Cannot unload current theme \'' + themeName + '\'. Switch to another theme first.');
                return false;
            }
            
            try {
                // 调用C函数卸载主题
                _themeUnload(themeName);
                
                var themeInfo = ThemeManager.loadedThemes[themeName];
                delete ThemeManager.loadedThemes[themeName];
                
                ThemeManager.emit('themeUnloaded', themeInfo);
                
                log('[Theme] Unloaded: ' + themeName);
                return true;
            } catch (e) {
                error('[Theme] Exception while unloading theme: ' + e.message);
                return false;
            }
        },
        
        /**
         * 应用当前主题到UI（刷新所有组件）
         * @returns {boolean} - 是否成功
         */
        apply: function() {
            if (!ThemeManager.currentTheme) {
                error('[Theme] No current theme set');
                return false;
            }
            
            try {
                // 调用C函数应用主题到图层树
                var result = _themeApplyToTree();
                
                if (result) {
                    ThemeManager.emit('themeApplied', {
                        theme: ThemeManager.loadedThemes[ThemeManager.currentTheme]
                    });
                    
                    log('[Theme] Applied: ' + ThemeManager.currentTheme);
                    return true;
                } else {
                    error('[Theme] Failed to apply theme to UI');
                    return false;
                }
            } catch (e) {
                error('[Theme] Exception while applying theme: ' + e.message);
                return false;
            }
        },
        
        /**
         * 切换主题（加载并应用）
         * @param {string} themePath - 主题文件路径  
         * @param {string} themeName - 主题名称
         * @param {Function} callback - 回调函数 function(success, themeInfo)
         */
        switch: function(themePath, themeName, callback) {
            if (typeof themeName === 'function') {
                callback = themeName;
                themeName = null;
            }
            
            this.load(themePath, themeName, function(success, themeInfo) {
                if (!success) {
                    if (callback) callback(false, themeInfo);
                    return;
                }
                
                var result = Theme.setCurrent(themeInfo.name);
                if (result) {
                    Theme.apply();
                    if (callback) callback(true, themeInfo);
                } else {
                    if (callback) callback(false, null);
                }
            });
        },
        
        /**
         * 从本地存储恢复主题设置
         * @param {string} storageKey - 本地存储的key
         * @param {Function} callback - 回调函数 function(success, themeInfo)
         */
        restoreFromStorage: function(storageKey, callback) {
            if (typeof storageKey === 'function') {
                callback = storageKey;
                storageKey = 'app-theme';
            }
            
            var savedTheme = Storage.getItem(storageKey || 'app-theme');
            if (savedTheme && this.loadedThemes[savedTheme]) {
                this.setCurrent(savedTheme);
                this.apply();
                log('[Theme] Restored from storage: ' + savedTheme);
                if (callback) callback(true, this.getCurrent());
            } else {
                if (callback) callback(false, null);
            }
        },
        
        /**
         * 保存当前主题到本地存储
         * @param {string} storageKey - 本地存储的key
         */
        saveToStorage: function(storageKey) {
            if (ThemeManager.currentTheme) {
                Storage.setItem(storageKey || 'app-theme', ThemeManager.currentTheme);
                log('[Theme] Saved to storage: ' + ThemeManager.currentTheme);
            }
        },
        
        /**
         * 添加事件监听器
         * @param {string} event - 事件类型: themeLoaded, themeChanged, themeApplied, themeUnloaded
         * @param {Function} callback - 回调函数
         * @returns {Theme} - 返回this以支持链式调用
         */
        on: function(event, callback) {
            ThemeManager.on(event, callback);
            return this;
        },
        
        /**
         * 移除事件监听器
         * @param {string} event - 事件类型
         * @param {Function} callback - 回调函数
         * @returns {Theme} - 返回this以支持链式调用
         */
        off: function(event, callback) {
            ThemeManager.off(event, callback);
            return this;
        }
    };

    // ==================== C函数绑定（占位符） ====================

    /**
     * 加载主题文件（C实现）
     * @param {string} themePath - 主题文件路径
     * @returns {Object} - {success: boolean, name: string, version: string}
     */
    function _themeLoad(themePath) {
        // 注意：此函数需要在C代码中实现
        // 这里返回失败，确保调用者知道需要C实现
        error('_themeLoad is not implemented. Please implement in C code.');
        return {
            success: false,
            name: '',
            version: '',
            error: 'Not implemented in C'
        };
    }

    /**
     * 设置当前主题（C实现）
     * @param {string} themeName - 主题名称
     * @returns {boolean} - 是否成功
     */
    function _themeSetCurrent(themeName) {
        // 注意：此函数需要在C代码中实现
        error('_themeSetCurrent is not implemented. Please implement in C code.');
        return false;
    }

    /**
     * 卸载主题（C实现）
     * @param {string} themeName - 主题名称
     */
    function _themeUnload(themeName) {
        // 注意：此函数需要在C代码中实现
        error('_themeUnload is not implemented. Please implement in C code.');
    }

    /**
     * 应用主题到图层树（C实现）
     * @returns {boolean} - 是否成功
     */
    function _themeApplyToTree() {
        // 注意：此函数需要在C代码中实现
        error('_themeApplyToTree is not implemented. Please implement in C code.');
        return false;
    }

    // ==================== 初始化 ====================

    // 自动初始化（如果支持）
    if (document.readyState === 'loading') {
        document.addEventListener('DOMContentLoaded', () => {
            console.log('[Theme] Library initialized');
        });
    } else {
        console.log('[Theme] Library initialized');
    }

    // ==================== 初始化 ====================

    // 初始化函数
    Theme.init = function() {
        log('Theme library initialized (' + engineName + ')');
        return Theme;
    };

    // ==================== 导出 ====================

    // 导出到全局（兼容不同环境）
    if (typeof global !== 'undefined') {
        // MarioJS / Node.js
        global.Theme = Theme;
    } else if (typeof window !== 'undefined') {
        // Browser
        window.Theme = Theme;
    } else {
        // QuickJS / 其他
        if (typeof this !== 'undefined') {
            this.Theme = Theme;
        }
    }

    // 兼容CommonJS（如果支持）
    if (typeof module !== 'undefined' && module.exports) {
        module.exports = Theme;
    }

    // 兼容AMD（如果支持）
    if (typeof define === 'function' && define.amd) {
        define(function() { return Theme; });
    }

    // 返回Theme对象
    return Theme;

})(typeof window !== 'undefined' ? window : 
   (typeof global !== 'undefined' ? global : this));

// YUI Playground API Client
// API 客户端示例，演示如何调用增量和全量更新接口

var APIClient = {
    // API 基础配置
    baseURL: 'http://localhost:5000',
    
    // 发送消息（增量更新模式）
    sendMessageIncremental: function(message, jsonConfig, callback) {
        YUI.log("APIClient: Sending incremental message...");
        
        var url = this.baseURL + '/api/message/incremental';
        var data = {
            message: message,
            json: jsonConfig
        };
        
        this._postJSON(url, data, function(response) {
            if (response.status === 'success') {
                YUI.log("APIClient: Incremental update successful");
                YUI.log("APIClient: Updates: " + JSON.stringify(response.updates));
                
                // 应用增量更新到UI
                if (callback && typeof callback === 'function') {
                    callback(response);
                }
            } else {
                YUI.log("APIClient: Incremental update failed - " + (response.error || 'Unknown error'));
            }
        });
    },
    
    // 发送消息（全量更新模式）
    sendMessageFull: function(message, jsonConfig, callback) {
        YUI.log("APIClient: Sending full message...");
        
        var url = this.baseURL + '/api/message/full';
        var data = {
            message: message,
            json: jsonConfig
        };
        
        this._postJSON(url, data, function(response) {
            if (response.status === 'success') {
                YUI.log("APIClient: Full update successful");
                YUI.log("APIClient: UI State: " + JSON.stringify(response.ui_state));
                
                // 应用全量更新到UI
                if (callback && typeof callback === 'function') {
                    callback(response);
                }
            } else {
                YUI.log("APIClient: Full update failed - " + (response.error || 'Unknown error'));
            }
        });
    },
    
    // 获取消息历史
    getHistory: function(limit, callback) {
        YUI.log("APIClient: Getting message history...");
        
        var url = this.baseURL + '/api/history?limit=' + (limit || 50);
        
        this._getJSON(url, function(response) {
            if (response.status === 'success') {
                YUI.log("APIClient: Got " + response.messages.length + " messages");
                if (callback && typeof callback === 'function') {
                    callback(response.messages);
                }
            } else {
                YUI.log("APIClient: Failed to get history");
            }
        });
    },
    
    // 获取当前UI状态
    getState: function(callback) {
        YUI.log("APIClient: Getting UI state...");
        
        var url = this.baseURL + '/api/state';
        
        this._getJSON(url, function(response) {
            if (response.status === 'success') {
                YUI.log("APIClient: Got UI state");
                if (callback && typeof callback === 'function') {
                    callback(response.ui_state);
                }
            } else {
                YUI.log("APIClient: Failed to get state");
            }
        });
    },
    
    // 重置状态
    resetState: function(callback) {
        YUI.log("APIClient: Resetting state...");
        
        var url = this.baseURL + '/api/reset';
        
        this._postJSON(url, {}, function(response) {
            if (response.status === 'success') {
                YUI.log("APIClient: State reset successfully");
                if (callback && typeof callback === 'function') {
                    callback(response);
                }
            } else {
                YUI.log("APIClient: Failed to reset state");
            }
        });
    },
    
    // 内部辅助函数：POST请求
    _postJSON: function(url, data, callback) {
        YUI.log("APIClient: POST " + url);
        
        // 使用 http.js 模块的 http_post 函数
        if (typeof http_post !== 'undefined') {
            try {
                var options = {
                    headers: {
                        'Content-Type': 'application/json'
                    },
                    contentType: 'application/json',
                    timeout: 5000
                };
                
                var response = http_post(url, JSON.stringify(data), options);
                YUI.log("APIClient: Response status: " + response.status);
                
                if (response.status >= 200 && response.status < 300) {
                    var parsedResponse = JSON.parse(response.body);
                    if (callback && typeof callback === 'function') {
                        callback(parsedResponse);
                    }
                } else {
                    YUI.log("APIClient: HTTP error - " + response.status);
                    callback({status: 'error', error: 'HTTP ' + response.status});
                }
            } catch (e) {
                YUI.log("APIClient: Request failed - " + e.message);
                // 回退到模拟响应
                var mockResponse = APIClient._getMockResponse(url, data);
                if (callback && typeof callback === 'function') {
                    callback(mockResponse);
                }
            }
        } else {
            YUI.log("APIClient: http_post not available");
            // 回退到模拟响应
            var mockResponse = APIClient._getMockResponse(url, data);
            if (callback && typeof callback === 'function') {
                callback(mockResponse);
            }
        }
    },
    
    // 内部辅助函数：GET请求
    _getJSON: function(url, callback) {
        YUI.log("APIClient: GET " + url);
        
        // 使用 http.js 模块的 http_get 函数
        if (typeof http_get !== 'undefined') {
            try {
                var options = {
                    timeout: 5000
                };
                
                var response = http_get(url, options);
                YUI.log("APIClient: Response status: " + response.status);
                
                if (response.status >= 200 && response.status < 300) {
                    var parsedResponse = JSON.parse(response.body);
                    if (callback && typeof callback === 'function') {
                        callback(parsedResponse);
                    }
                } else {
                    YUI.log("APIClient: HTTP error - " + response.status);
                    callback({status: 'error', error: 'HTTP ' + response.status});
                }
            } catch (e) {
                YUI.log("APIClient: Request failed - " + e.message);
                // 回退到模拟响应
                var mockResponse = APIClient._getMockResponse(url, null);
                if (callback && typeof callback === 'function') {
                    callback(mockResponse);
                }
            }
        } else {
            YUI.log("APIClient: http_get not available");
            // 回退到模拟响应
            var mockResponse = APIClient._getMockResponse(url, null);
            if (callback && typeof callback === 'function') {
                callback(mockResponse);
            }
        }
    },
    
    // 模拟响应（用于测试）
    _getMockResponse: function(url, data) {
        if (url.includes('/api/message/incremental')) {
            return {
                status: 'success',
                message_id: 1,
                updates: [
                    {"target": "statusLabel", "change": {"text": "状态：模拟增量更新", "color": "#2196f3"}},
                    {"target": "item1", "change": {"text": "模拟更新 - " + (data.message || ''), "bgColor": "#4caf50"}}
                ],
                timestamp: new Date().toISOString()
            };
        } else if (url.includes('/api/message/full')) {
            return {
                status: 'success',
                message_id: 2,
                ui_state: {
                    "statusLabel": {"text": "状态：模拟全量更新", "color": "#673ab7"},
                    "item1": {"text": "全量更新 - " + (data.message || ''), "bgColor": "#ff9800"},
                    "item2": {"text": "项目 2", "bgColor": "#f0f0f0"},
                    "item3": {"text": "项目 3", "bgColor": "#f0f0f0"}
                },
                changes: [],
                timestamp: new Date().toISOString()
            };
        } else if (url.includes('/api/history')) {
            return {
                status: 'success',
                messages: [
                    {"id": 1, "message": "测试消息1", "timestamp": new Date().toISOString(), "type": "incremental"},
                    {"id": 2, "message": "测试消息2", "timestamp": new Date().toISOString(), "type": "full"}
                ],
                total_count: 2
            };
        } else if (url.includes('/api/state')) {
            return {
                status: 'success',
                ui_state: {
                    "statusLabel": {"text": "状态：模拟状态", "color": "#333333"},
                    "item1": {"text": "模拟项目 1", "bgColor": "#f0f0f0"},
                    "item2": {"text": "模拟项目 2", "bgColor": "#f0f0f0"},
                    "item3": {"text": "模拟项目 3", "bgColor": "#f0f0f0"}
                }
            };
        } else if (url.includes('/api/reset')) {
            return {
                status: 'success',
                message: '模拟状态已重置'
            };
        }
        return {status: 'error', error: 'Unknown endpoint'};
    }
};

// 使用示例函数
function demoIncrementalUpdate() {
    YUI.log("Demo: 增量更新示例");
    
    var jsonText = YUI.getText("jsonEditor");
    var json = null;
    try {
        json = JSON.parse(jsonText);
    } catch (e) {
        YUI.log("Demo: JSON无效");
        return;
    }
    
    APIClient.sendMessageIncremental("批量更新", json, function(response) {
        YUI.log("Demo: 收到增量更新响应");
        
        // 将更新转换为 YUI.update() 格式
        var updateString = JSON.stringify(response.updates);
        YUI.update(updateString);
        
        YUI.setText("previewLabel", "增量更新已应用！\\n" + JSON.stringify(response, null, 2));
    });
}

function demoFullUpdate() {
    YUI.log("Demo: 全量更新示例");
    
    var jsonText = YUI.getText("jsonEditor");
    var json = null;
    try {
        json = JSON.parse(jsonText);
    } catch (e) {
        YUI.log("Demo: JSON无效");
        return;
    }
    
    APIClient.sendMessageFull("全量更新测试", json, function(response) {
        YUI.log("Demo: 收到全量更新响应");
        
        // 将完整状态转换为 YUI.update() 格式
        // 这里需要将 ui_state 转换为 YUI.update 能理解的格式
        var fullUpdate = [];
        for (var key in response.ui_state) {
            fullUpdate.push({
                "target": key,
                "change": response.ui_state[key]
            });
        }
        
        var updateString = JSON.stringify(fullUpdate);
        YUI.update(updateString);
        
        YUI.setText("previewLabel", "全量更新已应用！\\n" + JSON.stringify(response, null, 2));
    });
}

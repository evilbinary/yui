/**
 * Photo AI API client
 * 对接 e:\workspace\sheep\cmd\photo HTTP 服务
 *
 * 默认地址: http://127.0.0.1:8082
 * 全部走异步 HTTP，避免同步请求卡死 UI
 */

var PhotoAPI = {
    baseURL: "http://127.0.0.1:8082",

    setBaseURL: function(url) {
        if (!url) return;
        url = String(url).replace(/\/+$/, "");
        this.baseURL = url;
    },

    getBaseURL: function() {
        return this.baseURL;
    },

    resolveURL: function(path) {
        if (!path) return "";
        if (path.indexOf("http://") === 0 || path.indexOf("https://") === 0) {
            return path;
        }
        if (path.charAt(0) !== "/") {
            path = "/" + path;
        }
        return this.baseURL + path;
    },

    health: function(callback) {
        this._getJSON(this.baseURL + "/api/health", callback, 5000);
    },

    listTools: function(callback) {
        this._getJSON(this.baseURL + "/api/tools", callback, 8000);
    },

    listSessions: function(callback) {
        this._getJSON(this.baseURL + "/api/sessions", callback, 8000);
    },

    getSession: function(sessionId, callback) {
        var url = this.baseURL + "/api/sessions/" + encodeURIComponent(sessionId);
        this._getJSON(url, callback, 8000);
    },

    /**
     * 异步聊天（非流式）
     * body: { message, session_id, files?, stream:false }
     */
    chatSync: function(body, callback) {
        var payload = body || {};
        payload.stream = false;
        this._postJSON(this.baseURL + "/api/chat", payload, callback, 180000);
    },

    /**
     * SSE 流式聊天（异步）
     * handlers: { onEvent(type, data), onDone(data), onError(err) }
     */
    chatStream: function(body, handlers) {
        handlers = handlers || {};
        var url = this.baseURL + "/api/chat";
        var payload = body || {};
        payload.stream = true;

        if (typeof http_post_sse_async === "undefined") {
            if (handlers.onError) {
                handlers.onError("SSE async unavailable: load app/lib/http.js");
            }
            return;
        }

        var streamHandlers = {
            onEvent: function(eventType, data) {
                if (handlers.onEvent) {
                    handlers.onEvent(eventType, data);
                }
                if (eventType === "done" && handlers.onDone) {
                    handlers.onDone(data || {});
                }
                if (eventType === "error" && handlers.onError) {
                    var msg = data;
                    if (data && typeof data === "object" && data.error) {
                        msg = data.error;
                    }
                    handlers.onError(String(msg));
                }
            },
            onDone: function(data) {
                if (handlers.onDone) {
                    handlers.onDone(data || {});
                }
            },
            onError: function(err) {
                if (handlers.onError) {
                    handlers.onError(err);
                }
            }
        };

        http_post_sse_async(url, JSON.stringify(payload), streamHandlers, {
            headers: { "Content-Type": "application/json" },
            contentType: "application/json",
            timeout: 300000
        });
    },

    _getJSON: function(url, callback, timeout) {
        YUI.log("PhotoAPI GET async " + url);
        if (typeof http_get_async === "undefined") {
            if (callback) callback({ error: "http_get_async unavailable" });
            return;
        }
        http_get_async(url, function(err, response) {
            if (err || !response) {
                if (callback) callback({ error: err || "empty response" });
                return;
            }
            if (response.status >= 200 && response.status < 300) {
                var parsed = {};
                try {
                    parsed = JSON.parse(response.body);
                } catch (e) {
                    parsed = { raw: response.body };
                }
                if (callback) callback(parsed);
            } else if (callback) {
                callback({ error: "HTTP " + response.status, body: response.body });
            }
        }, { timeout: timeout || 8000 });
    },

    _postJSON: function(url, data, callback, timeout) {
        YUI.log("PhotoAPI POST async " + url);
        if (typeof http_post_async === "undefined") {
            if (callback) callback({ error: "http_post_async unavailable" });
            return;
        }
        http_post_async(url, JSON.stringify(data), function(err, response) {
            if (err || !response) {
                if (callback) callback({ error: err || "empty response" });
                return;
            }
            if (response.status >= 200 && response.status < 300) {
                var parsed = {};
                try {
                    parsed = JSON.parse(response.body);
                } catch (e) {
                    parsed = { raw: response.body };
                }
                if (callback) callback(parsed);
            } else {
                var errBody = response.body;
                try {
                    errBody = JSON.parse(response.body);
                } catch (e2) {}
                if (callback) {
                    callback({
                        error: (errBody && errBody.error) ? errBody.error : ("HTTP " + response.status),
                        body: errBody
                    });
                }
            }
        }, {
            timeout: timeout || 60000,
            headers: { "Content-Type": "application/json" },
            contentType: "application/json"
        });
    }
};

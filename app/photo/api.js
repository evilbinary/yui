/**
 * Photo AI API client
 * 对接 e:\workspace\sheep\cmd\photo HTTP 服务
 *
 * 默认地址: http://127.0.0.1:8082
 * 主要接口:
 *   GET  /api/health
 *   GET  /api/tools
 *   GET  /api/sessions
 *   POST /api/chat   { message, session_id, stream, files }
 *   POST /api/upload multipart file
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
        var url = this.baseURL + "/api/health";
        this._getJSON(url, callback, 5000);
    },

    listTools: function(callback) {
        var url = this.baseURL + "/api/tools";
        this._getJSON(url, callback, 8000);
    },

    listSessions: function(callback) {
        var url = this.baseURL + "/api/sessions";
        this._getJSON(url, callback, 8000);
    },

    getSession: function(sessionId, callback) {
        var url = this.baseURL + "/api/sessions/" + encodeURIComponent(sessionId);
        this._getJSON(url, callback, 8000);
    },

    /**
     * 同步聊天（非流式）
     * body: { message, session_id, files?, stream:false }
     */
    chatSync: function(body, callback) {
        var url = this.baseURL + "/api/chat";
        var payload = body || {};
        payload.stream = false;
        this._postJSON(url, payload, callback, 180000);
    },

    /**
     * SSE 流式聊天
     * handlers: { onEvent(type, data), onDone(data), onError(err) }
     */
    chatStream: function(body, handlers) {
        handlers = handlers || {};
        var url = this.baseURL + "/api/chat";
        var payload = body || {};
        payload.stream = true;

        if (typeof http_post_sse_async === "undefined" && typeof http_post_sse === "undefined") {
            if (handlers.onError) {
                handlers.onError("SSE unavailable: load app/lib/http.js");
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

        var options = {
            headers: { "Content-Type": "application/json" },
            contentType: "application/json",
            timeout: 300000
        };

        if (typeof http_post_sse_async !== "undefined") {
            http_post_sse_async(url, JSON.stringify(payload), streamHandlers, options);
        } else {
            http_post_sse(url, JSON.stringify(payload), streamHandlers, options);
        }
    },

    _getJSON: function(url, callback, timeout) {
        YUI.log("PhotoAPI GET " + url);
        if (typeof http_get === "undefined") {
            if (callback) callback({ error: "http_get unavailable" });
            return;
        }
        try {
            var response = http_get(url, { timeout: timeout || 8000 });
            if (response.status >= 200 && response.status < 300) {
                var parsed = {};
                try {
                    parsed = JSON.parse(response.body);
                } catch (e) {
                    parsed = { raw: response.body };
                }
                if (callback) callback(parsed);
            } else {
                if (callback) {
                    callback({ error: "HTTP " + response.status, body: response.body });
                }
            }
        } catch (e) {
            YUI.log("PhotoAPI GET failed: " + e.message);
            if (callback) callback({ error: e.message || String(e) });
        }
    },

    _postJSON: function(url, data, callback, timeout) {
        YUI.log("PhotoAPI POST " + url);
        if (typeof http_post === "undefined") {
            if (callback) callback({ error: "http_post unavailable" });
            return;
        }
        try {
            var response = http_post(url, JSON.stringify(data), {
                timeout: timeout || 60000,
                headers: { "Content-Type": "application/json" },
                contentType: "application/json"
            });
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
        } catch (e) {
            YUI.log("PhotoAPI POST failed: " + e.message);
            if (callback) callback({ error: e.message || String(e) });
        }
    }
};

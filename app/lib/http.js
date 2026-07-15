// HTTP 客户端实现，基于 Socket API

function utf8_byte_length(str) {
    var len = 0;
    for (var i = 0; i < str.length; i++) {
        var c = str.charCodeAt(i);
        if (c < 0x80) {
            len += 1;
        } else if (c < 0x800) {
            len += 2;
        } else if (c >= 0xD800 && c < 0xDC00) {
            len += 4;
            i++;
        } else {
            len += 3;
        }
    }
    return len;
}

/**
 * HTTP GET 请求
 * @param {string} url - URL地址，例如 "http://example.com:8080/path"
 * @param {object} options - 可选参数 {headers: {...}, timeout: 5000}
 * @returns {object} 响应对象 {status: number, headers: object, body: string}
 */
function http_get(url, options) {
    options = options || {};
    var timeout = options.timeout || 5000;
    var headers = options.headers || {};
    
    // 解析URL
    var urlParts = parse_url(url);
    if (!urlParts) {
        throw new Error("Invalid URL: " + url);
    }
    
    var host = urlParts.host;
    var port = urlParts.port || 80;
    var path = urlParts.path || "/";
    
    YUI.log("HTTP GET: " + url);
    
    // 创建TCP socket
    var sock = Socket.socket(Socket.TCP);
    if (sock < 0) {
        throw new Error("Failed to create socket");
    }
    
    try {
        // 连接到服务器
        var connectResult = Socket.connect(sock, host, port, timeout);
        if (connectResult !== 0) {
            throw new Error("Failed to connect to " + host + ":" + port);
        }
        
        // 构建HTTP请求
        var requestLines = [];
        requestLines.push("GET " + path + " HTTP/1.1");
        requestLines.push("Host: " + host);
        requestLines.push("User-Agent: YUI-HTTP-Client/1.0");
        requestLines.push("Connection: close");
        
        // 添加自定义headers
        for (var key in headers) {
            requestLines.push(key + ": " + headers[key]);
        }
        
        requestLines.push("");
        requestLines.push("");
        
        var request = requestLines.join("\r\n");
        YUI.log("HTTP Request:\n" + request);
        
        // 发送请求
        var sent = Socket.send(sock, request, 0);
        if (sent < 0) {
            throw new Error("Failed to send request");
        }
        
        // 接收响应
        var response = "";
        var buffer;
        while (true) {
            buffer = Socket.recv(sock, 0, 4096);
            if (typeof buffer === 'string' && buffer.length > 0) {
                response += buffer;
            } else {
                break;
            }
        }
        
        YUI.log("HTTP Response length: " + response.length);
        
        // 解析响应
        return parse_http_response(response);
        
    } finally {
        // 关闭socket
        Socket.close(sock);
    }
}

/**
 * HTTP POST 请求
 * @param {string} url - URL地址
 * @param {string} data - POST数据
 * @param {object} options - 可选参数 {headers: {...}, timeout: 5000, contentType: "application/x-www-form-urlencoded"}
 * @returns {object} 响应对象 {status: number, headers: object, body: string}
 */
function http_post(url, data, options) {
    options = options || {};
    var timeout = options.timeout || 5000;
    var headers = options.headers || {};
    var contentType = options.contentType || "application/x-www-form-urlencoded";
    
    // 解析URL
    var urlParts = parse_url(url);
    if (!urlParts) {
        throw new Error("Invalid URL: " + url);
    }
    
    var host = urlParts.host;
    var port = urlParts.port || 80;
    var path = urlParts.path || "/";
    
    YUI.log("HTTP POST: " + url);
    
    // 创建TCP socket
    var sock = Socket.socket(Socket.TCP);
    if (sock < 0) {
        throw new Error("Failed to create socket");
    }
    
    try {
        // 连接到服务器
        var connectResult = Socket.connect(sock, host, port, timeout);
        if (connectResult !== 0) {
            throw new Error("Failed to connect to " + host + ":" + port);
        }
        
        // 确保data是字符串
        if (typeof data !== 'string') {
            data = JSON.stringify(data);
        }
        
        // 构建HTTP请求
        var requestLines = [];
        requestLines.push("POST " + path + " HTTP/1.1");
        requestLines.push("Host: " + host);
        requestLines.push("User-Agent: YUI-HTTP-Client/1.0");
        requestLines.push("Content-Type: " + contentType);
        requestLines.push("Content-Length: " + utf8_byte_length(data));
        requestLines.push("Connection: close");
        
        // 添加自定义headers
        for (var key in headers) {
            requestLines.push(key + ": " + headers[key]);
        }
        
        requestLines.push("");
        requestLines.push(data);
        
        var request = requestLines.join("\r\n");
        YUI.log("HTTP Request:\n" + request);
        
        // 发送请求
        var sent = Socket.send(sock, request, 0);
        if (sent < 0) {
            throw new Error("Failed to send request");
        }
        
        // 接收响应
        var response = "";
        var buffer;
        while (true) {
            buffer = Socket.recv(sock, 0, 4096);
            if (typeof buffer === 'string' && buffer.length > 0) {
                response += buffer;
            } else {
                break;
            }
        }
        
        YUI.log("HTTP Response length: " + response.length);
        
        // 解析响应
        return parse_http_response(response);
        
    } finally {
        // 关闭socket
        Socket.close(sock);
    }
}

/**
 * 解析URL
 */
function parse_url(url) {
    var match = url.match(/^http:\/\/([^\/]+)(\/.*)?$/);
    if (!match) {
        return null;
    }
    
    var hostPort = match[1];
    var path = match[2] || "/";
    
    var colonIdx = hostPort.indexOf(":");
    if (colonIdx > 0) {
        return {
            host: hostPort.substring(0, colonIdx),
            port: parseInt(hostPort.substring(colonIdx + 1)),
            path: path
        };
    } else {
        return {
            host: hostPort,
            port: 80,
            path: path
        };
    }
}

/**
 * 解析HTTP响应
 */
function parse_http_response(response) {
    if (!response || response.length === 0) {
        return {
            status: 0,
            headers: {},
            body: ""
        };
    }
    
    // 分离响应头和正文
    var parts = response.split("\r\n\r\n");
    var headerPart = parts[0];
    var bodyPart = parts.length > 1 ? parts[1] : "";
    
    // 解析状态行
    var lines = headerPart.split("\r\n");
    var statusLine = lines[0];
    var statusMatch = statusLine.match(/HTTP\/\d+\.\d+\s+(\d+)/);
    var status = statusMatch ? parseInt(statusMatch[1]) : 0;
    
    // 解析headers
    var headers = {};
    for (var i = 1; i < lines.length; i++) {
        var line = lines[i];
        var colonIdx = line.indexOf(":");
        if (colonIdx > 0) {
            var key = line.substring(0, colonIdx).trim();
            var value = line.substring(colonIdx + 1).trim();
            headers[key] = value;
        }
    }
    
    return {
        status: status,
        headers: headers,
        body: bodyPart
    };
}

/**
 * HTTP POST SSE 流式请求
 * @param {string} url
 * @param {string} data - POST body
 * @param {object} handlers - { onToken, onUpdate, onDone, onError, onEvent }
 * @param {object} options - { headers, timeout, contentType }
 * @returns {number} 0 成功，-1 失败
 */
function http_post_sse(url, data, handlers, options) {
    handlers = handlers || {};
    options = options || {};
    var timeout = options.timeout || 120000;
    var headers = options.headers || {};
    var contentType = options.contentType || "application/json";

    var urlParts = parse_url(url);
    if (!urlParts) {
        if (handlers.onError) {
            handlers.onError("Invalid URL: " + url);
        }
        return -1;
    }

    var host = urlParts.host;
    var port = urlParts.port || 80;
    var path = urlParts.path || "/";

    YUI.log("HTTP POST SSE: " + url);

    var sock = Socket.socket(Socket.TCP);
    if (sock < 0) {
        if (handlers.onError) {
            handlers.onError("Failed to create socket");
        }
        return -1;
    }

    if (typeof data !== 'string') {
        data = JSON.stringify(data);
    }

    var requestLines = [];
    requestLines.push("POST " + path + " HTTP/1.1");
    requestLines.push("Host: " + host);
    requestLines.push("User-Agent: YUI-HTTP-Client/1.0");
    requestLines.push("Content-Type: " + contentType);
    requestLines.push("Accept: text/event-stream");
    requestLines.push("Content-Length: " + utf8_byte_length(data));
    requestLines.push("Connection: close");

    for (var key in headers) {
        requestLines.push(key + ": " + headers[key]);
    }

    requestLines.push("");
    requestLines.push(data);

    var request = requestLines.join("\r\n");
    var raw = "";
    var headersParsed = false;
    var status = 0;
    var eventType = "message";
    var dataLines = [];
    var startTime = Date.now();

    function dispatchEvent() {
        if (dataLines.length === 0) {
            return;
        }
        var payloadText = dataLines.join("\n");
        var payload = payloadText;
        try {
            payload = JSON.parse(payloadText);
        } catch (e) {
            payload = payloadText;
        }

        if (handlers.onEvent) {
            handlers.onEvent(eventType, payload);
        }
        if (eventType === "token" && handlers.onToken) {
            handlers.onToken(payload);
        } else if (eventType === "update" && handlers.onUpdate) {
            handlers.onUpdate(payload);
        } else if (eventType === "done" && handlers.onDone) {
            handlers.onDone(payload);
        } else if (eventType === "error" && handlers.onError) {
            if (typeof payload === "object" && payload.error) {
                handlers.onError(payload.error);
            } else {
                handlers.onError(String(payload));
            }
        }

        eventType = "message";
        dataLines = [];
    }

    function processSseBuffer() {
        while (true) {
            var newlineIdx = raw.indexOf("\n");
            if (newlineIdx < 0) {
                break;
            }

            var line = raw.substring(0, newlineIdx);
            raw = raw.substring(newlineIdx + 1);
            if (line.length > 0 && line.charAt(line.length - 1) === '\r') {
                line = line.substring(0, line.length - 1);
            }

            if (line === "") {
                dispatchEvent();
                continue;
            }

            if (line.indexOf("event:") === 0) {
                eventType = line.substring(6);
                if (eventType.charAt(0) === ' ') {
                    eventType = eventType.substring(1);
                }
            } else if (line.indexOf("data:") === 0) {
                var dataPart = line.substring(5);
                if (dataPart.charAt(0) === ' ') {
                    dataPart = dataPart.substring(1);
                }
                dataLines.push(dataPart);
            }
        }
    }

    try {
        var connectResult = Socket.connect(sock, host, port, timeout);
        if (connectResult !== 0) {
            if (handlers.onError) {
                handlers.onError("Failed to connect to " + host + ":" + port);
            }
            return -1;
        }

        var sent = Socket.send(sock, request, 0);
        if (sent < 0) {
            if (handlers.onError) {
                handlers.onError("Failed to send request");
            }
            return -1;
        }

        while (true) {
            if (Date.now() - startTime > timeout) {
                if (handlers.onError) {
                    handlers.onError("SSE timeout");
                }
                return -1;
            }

            var buffer = Socket.recv(sock, 0, 4096);
            if (typeof buffer !== 'string' || buffer.length === 0) {
                break;
            }

            if (!headersParsed) {
                raw += buffer;
                var sep = raw.indexOf("\r\n\r\n");
                if (sep < 0) {
                    continue;
                }

                var headerBlock = raw.substring(0, sep);
                raw = raw.substring(sep + 4);
                var parsed = parse_http_response(headerBlock + "\r\n\r\n");
                status = parsed.status;
                headersParsed = true;

                if (status < 200 || status >= 300) {
                    if (handlers.onError) {
                        handlers.onError("HTTP " + status);
                    }
                    return -1;
                }

                processSseBuffer();
            } else {
                raw += buffer;
                processSseBuffer();
            }
        }

        if (dataLines.length > 0) {
            dispatchEvent();
        }

        return 0;
    } catch (e) {
        if (handlers.onError) {
            handlers.onError(e.message || String(e));
        }
        return -1;
    } finally {
        Socket.close(sock);
    }
}

/**
 * HTTP POST SSE 流式请求（非阻塞，不卡 UI）
 * @returns {number} 0 已启动，-1 启动失败
 */
function http_post_sse_async(url, data, handlers, options) {
    handlers = handlers || {};
    options = options || {};
    var timeout = options.timeout || 120000;
    var headers = options.headers || {};
    var contentType = options.contentType || "application/json";

    var urlParts = parse_url(url);
    if (!urlParts) {
        if (handlers.onError) {
            handlers.onError("Invalid URL: " + url);
        }
        return -1;
    }

    var host = urlParts.host;
    var port = urlParts.port || 80;
    var path = urlParts.path || "/";

    var sock = Socket.socket(Socket.TCP);
    if (sock < 0) {
        if (handlers.onError) {
            handlers.onError("Failed to create socket");
        }
        return -1;
    }

    if (typeof data !== 'string') {
        data = JSON.stringify(data);
    }

    var requestLines = [];
    requestLines.push("POST " + path + " HTTP/1.1");
    requestLines.push("Host: " + host);
    requestLines.push("User-Agent: YUI-HTTP-Client/1.0");
    requestLines.push("Content-Type: " + contentType);
    requestLines.push("Accept: text/event-stream");
    requestLines.push("Content-Length: " + utf8_byte_length(data));
    requestLines.push("Connection: close");
    for (var key in headers) {
        requestLines.push(key + ": " + headers[key]);
    }
    requestLines.push("");
    requestLines.push(data);
    var request = requestLines.join("\r\n");

    var raw = "";
    var headersParsed = false;
    var status = 0;
    var eventType = "message";
    var dataLines = [];
    var startTime = Date.now();
    var finished = false;

    function finish(err) {
        if (finished) {
            return;
        }
        finished = true;
        Socket.close(sock);
        if (err && handlers.onError) {
            handlers.onError(err);
        }
    }

    function dispatchEvent() {
        if (dataLines.length === 0) {
            return;
        }
        var payloadText = dataLines.join("\n");
        var payload = payloadText;
        try {
            payload = JSON.parse(payloadText);
        } catch (e) {
            payload = payloadText;
        }
        if (handlers.onEvent) {
            handlers.onEvent(eventType, payload);
        }
        if (eventType === "token" && handlers.onToken) {
            handlers.onToken(payload);
        } else if (eventType === "update" && handlers.onUpdate) {
            handlers.onUpdate(payload);
        } else if (eventType === "done" && handlers.onDone) {
            handlers.onDone(payload);
        } else if (eventType === "error" && handlers.onError) {
            if (typeof payload === "object" && payload.error) {
                handlers.onError(payload.error);
            } else {
                handlers.onError(String(payload));
            }
        }
        eventType = "message";
        dataLines = [];
    }

    function processSseBuffer() {
        while (true) {
            var newlineIdx = raw.indexOf("\n");
            if (newlineIdx < 0) {
                break;
            }
            var line = raw.substring(0, newlineIdx);
            raw = raw.substring(newlineIdx + 1);
            if (line.length > 0 && line.charAt(line.length - 1) === '\r') {
                line = line.substring(0, line.length - 1);
            }
            if (line === "") {
                dispatchEvent();
                continue;
            }
            if (line.indexOf("event:") === 0) {
                eventType = line.substring(6);
                if (eventType.charAt(0) === ' ') {
                    eventType = eventType.substring(1);
                }
            } else if (line.indexOf("data:") === 0) {
                var dataPart = line.substring(5);
                if (dataPart.charAt(0) === ' ') {
                    dataPart = dataPart.substring(1);
                }
                dataLines.push(dataPart);
            }
        }
    }

    function onData(buffer) {
        if (!headersParsed) {
            raw += buffer;
            var sep = raw.indexOf("\r\n\r\n");
            if (sep < 0) {
                return;
            }
            var headerBlock = raw.substring(0, sep);
            raw = raw.substring(sep + 4);
            var parsed = parse_http_response(headerBlock + "\r\n\r\n");
            status = parsed.status;
            headersParsed = true;
            if (status < 200 || status >= 300) {
                finish("HTTP " + status);
                return;
            }
            processSseBuffer();
        } else {
            raw += buffer;
            processSseBuffer();
        }
    }

    function poll() {
        if (finished) {
            return;
        }
        if (Date.now() - startTime > timeout) {
            finish("SSE timeout");
            return;
        }
        var buffer = Socket.recv(sock, 0, 4096);
        if (typeof buffer === 'number') {
            if (buffer < 0) {
                setTimeout(poll, 16);
                return;
            }
            if (dataLines.length > 0) {
                dispatchEvent();
            }
            finish(null);
            return;
        }
        if (buffer.length === 0) {
            if (dataLines.length > 0) {
                dispatchEvent();
            }
            finish(null);
            return;
        }
        onData(buffer);
        setTimeout(poll, 0);
    }

    try {
        if (Socket.connect(sock, host, port, timeout) !== 0) {
            finish("Failed to connect to " + host + ":" + port);
            return -1;
        }
        if (Socket.send(sock, request, 0) < 0) {
            finish("Failed to send request");
            return -1;
        }
        if (Socket.setNonBlocking(sock, 1) !== 0) {
            finish("Failed to set non-blocking");
            return -1;
        }
        setTimeout(poll, 0);
        return 0;
    } catch (e) {
        finish(e.message || String(e));
        return -1;
    }
}

// 导出函数（在YUI环境中，这些函数会自动成为全局函数）
YUI.log("HTTP module loaded");

// 测试函数
function test_http() {
    YUI.log("Starting HTTP tests...");
    
    // 测试GET请求
    try {
        YUI.log("Testing GET request...");
        var response = http_get("http://httpbin.org/get");
        YUI.log("GET Response status: " + response.status);
        YUI.log("GET Response body length: " + response.body.length);
    } catch (e) {
        YUI.log("GET request failed: " + e.message);
    }
    
    // 测试POST请求
    try {
        YUI.log("Testing POST request...");
        var postData = "name=test&value=123";
        var response = http_post("http://httpbin.org/post", postData);
        YUI.log("POST Response status: " + response.status);
        YUI.log("POST Response body length: " + response.body.length);
    } catch (e1) {
        YUI.log("POST request failed: " + e1.message);
    }
    
    YUI.log("HTTP tests completed");
}

// 运行测试（可选）
// test_http();

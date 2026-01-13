// HTTP 客户端实现，基于 Socket API
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
    
    YUI.log("HTTP GET: " + url,timeout);
    
    // 创建TCP socket
    var sock = Socket.socket(Socket.TCP);
    if (sock < 0) {
        throw new Error("Failed to create socket error:" + sock);
    }
    
    try {
        // 连接到服务器
        var connectResult = Socket.connect(sock, host, port, timeout);
        if (connectResult !== 0) {
            throw new Error("Failed to connect to " + host + ":" + port+ " code:"+connectResult);
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
            YUI.log('buffer=>',buffer);
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
    
    YUI.log("HTTP POST: " + url, timeout);
    
    // 创建TCP socket
    var sock = Socket.socket(Socket.TCP);
    if (sock < 0) {
        throw new Error("Failed to create socket");
    }
    
    try {
        // 连接到服务器
        var connectResult = Socket.connect(sock, host, port, timeout);
        if (connectResult !== 0) {
            throw new Error("Failed to connect to " + host + ":" + port+ " code:"+connectResult);
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
        requestLines.push("Content-Length: " + data.length);
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
    
    YUI.log("HTTP tests completed");
}

// 测试1: 解析URL
YUI.log("=== 开始 HTTP 测试 ===");
YUI.log("测试1: 解析URL");
var url1 = parse_url("http://example.com:8080/path");
YUI.log("URL1: host=" + url1.host + ", port=" + url1.port + ", path=" + url1.path);

var url2 = parse_url("http://httpbin.org/get");
YUI.log("URL2: host=" + url2.host + ", port=" + url2.port + ", path=" + url2.path);

// 测试2: HTTP GET 请求
YUI.log("\n测试2: HTTP GET 请求");
try {
    var response = http_get("http://httpbin.org/get");
    YUI.log("GET 响应状态: " + response.status);
    YUI.log("GET 响应头 Content-Length: " + (response.headers["Content-Length"] || "N/A"));
    YUI.log("GET 响应体长度: " + response.body.length);
    
    // 打印响应体的前100个字符
    if (response.body.length > 0) {
        var preview = response.body;
        YUI.log("GET 响应体预览: " + preview + "...");
    }
} catch (e) {
    YUI.log("GET 请求失败: " + e.message);
}

// 测试3: HTTP POST 请求
YUI.log("\n测试3: HTTP POST 请求");
try {
    var postData = "name=YUI&value=awesome";
    var response = http_post("http://httpbin.org/post", postData);
    YUI.log("POST 响应状态: " + response.status);
    YUI.log("POST 响应体长度: " + response.body.length);
    
    // 打印响应体的前100个字符
    if (response.body.length > 0) {
        var preview = response.body;
        YUI.log("POST 响应体预览: " + preview + "...");
    }
} catch (ex) {
    YUI.log("POST 请求失败: " + ex.message);
}

YUI.log("\n=== HTTP 测试完成 ===");

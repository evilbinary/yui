// HTTP 客户端实现，基于 Socket API
// 注意：当前Socket实现不支持DNS解析，需要使用IP地址

/**
 * HTTP GET 请求
 * @param {string} url - URL地址，例如 "http://192.168.1.1:8080/path"
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

// 测试1: 解析URL
YUI.log("=== 开始 HTTP 测试 ===");
YUI.log("测试1: 解析URL");
var url1 = parse_url("http://192.168.1.100:8080/path");
YUI.log("URL1: host=" + url1.host + ", port=" + url1.port + ", path=" + url1.path);

// 重要提示：当前Socket实现不支持DNS解析，必须使用IP地址
// 以下测试使用示例IP地址，请根据实际情况修改

YUI.log("\n=== 重要提示 ===");
YUI.log("当前Socket实现不支持DNS解析，必须使用IP地址");
YUI.log("请修改以下测试IP地址为您实际可用的服务器IP");
YUI.log("示例：http://192.168.1.100:8080/test");
YUI.log("=== 重要提示 ===\n");

// 示例测试（使用回环地址，应该可以连接）
YUI.log("测试2: HTTP GET 到本地回环地址");
try {
    // 127.0.0.1 是回环地址，即使没有网络也应该可以连接（如果本地有服务）
    // 但这个测试可能会失败，因为本地可能没有运行HTTP服务
    var response = http_get("http://127.0.0.1:9999/");
    YUI.log("GET 响应状态: " + response.status);
} catch (e) {
    YUI.log("GET 请求失败: " + e.message);
    YUI.log("这是正常的，因为本地可能没有运行HTTP服务");
}

YUI.log("\n=== HTTP 测试完成 ===");
YUI.log("要在实际环境中使用，请：");
YUI.log("1. 确保使用IP地址而不是域名");
YUI.log("2. 确保目标服务器可访问");
YUI.log("3. 根据需要修改超时设置");

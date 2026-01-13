// 加载 HTTP 模块	
import { http_get, http_post, parse_url }from './http.js';

YUI.log("=== 开始 HTTP 测试 ===");

// 测试1: 解析URL
YUI.log("测试1: 解析URL");
var url1 = parse_url("http://example.com:8080/path");
YUI.log("URL1: host=" + url1.host + ", port=" + url1.port + ", path=" + url1.path);

var url2 = parse_url("http://httpbin.org/get");
YUI.log("URL2: host=" + url2.host + ", port=" + url2.port + ", path=" + url2.path);

// 测试2: HTTP GET 请求（注意：需要网络连接）
YUI.log("\n测试2: HTTP GET 请求");
try {
    var response = http_get("http://httpbin.org/get");
    YUI.log("GET 响应状态: " + response.status);
    YUI.log("GET 响应头 Content-Length: " + (response.headers["Content-Length"] || "N/A"));
    YUI.log("GET 响应体长度: " + response.body.length);
    
    // 打印响应体的前100个字符
    if (response.body.length > 0) {
        var preview = response.body.substring(0, 100);
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
        var preview = response.body.substring(0, 100);
        YUI.log("POST 响应体预览: " + preview + "...");
    }
} catch (e) {
    YUI.log("POST 请求失败: " + e.message);
}

// 测试4: HTTP GET 带自定义 headers
YUI.log("\n测试4: HTTP GET 带自定义 headers");
try {
    var options = {
        headers: {
            "User-Agent": "YUI-HTTP-Client-Test/1.0",
            "X-Custom-Header": "test-value"
        }
    };
    var response = http_get("http://httpbin.org/headers", options);
    YUI.log("GET 带headers 响应状态: " + response.status);
    
    if (response.body.length > 0) {
        // 响应中包含我们发送的headers
        YUI.log("GET 带headers 响应包含 'YUI-HTTP-Client': " + 
               (response.body.indexOf("YUI-HTTP-Client") >= 0 ? "是" : "否"));
    }
} catch (e) {
    YUI.log("GET 带headers 请求失败: " + e.message);
}

YUI.log("\n=== HTTP 测试完成 ===");

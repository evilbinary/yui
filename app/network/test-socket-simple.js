// 测试 Socket API
YUI.log("Testing Socket API...");

// 测试创建 socket
var sock = Socket.socket(Socket.TCP);
YUI.log("Created socket with fd: " + sock);

// 测试关闭 socket
Socket.close(sock);
YUI.log("Socket closed");

// 测试 inet_addr
var ip = Socket.inet_addr("127.0.0.1");
YUI.log("IP address (numeric): " + ip);

// 测试 ntohl
var host_long = Socket.ntohl(ip);
YUI.log("Host long: " + host_long);

// 测试 make_sockaddr_in
var sockaddr = Socket.make_sockaddr_in(2, ip, 8080);
YUI.log("Created sockaddr_in: ptr=" + sockaddr.ptr + ", size=" + sockaddr.size);

YUI.log("Socket API tests completed!");
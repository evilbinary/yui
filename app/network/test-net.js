YUI.log("test net start==");

var sock = Socket.socket(Socket.TCP);

YUI.log("test net",sock)

// 绑定到本地地址
Socket.bind(sock, "127.0.0.1", 8080);

YUI.log("bind net",sock)


// 监听连接
Socket.listen(sock, 5);

YUI.log("listen net",sock)

// 接受连接
var client = Socket.accept(sock);

// 发送数据
Socket.send(client, "Hello World", 0);

// 接收数据
var data = Socket.recv(client, 0, 1024);

// 获取本地地址
var localAddr = Socket.getsockname(sock);
YUI.log("Local IP: " + localAddr.ip + ", Port: " + localAddr.port);
# socket
```js
// 创建TCP套接字
var sock = new Socket.Socket(Socket.TCP);

// 绑定到本地地址
Socket.bind(sock, "127.0.0.1", 8080);

// 监听连接
Socket.listen(sock, 5);

// 接受连接
var client = Socket.accept(sock);

// 发送数据
Socket.send(client, "Hello World", 0);

// 接收数据
var data = Socket.recv(client, 0, 1024);

// 获取本地地址
var localAddr = Socket.getsockname(sock);
console.log("Local IP: " + localAddr.ip + ", Port: " + localAddr.port);

```
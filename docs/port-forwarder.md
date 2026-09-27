# Port Forwarder

轻量级 TCP 端口转发守护进程，内嵌网页管理界面。可在浏览器里管理多条转发规则，无需手写配置文件。

## 特性

- 纯 TCP 转发（暂未加密与鉴权）
- 内嵌网页管理界面，零外部资源
- 规则管理的 REST API
- 单静态文件，可交叉编译到 Linux 与 Windows

## 启动

```sh
./bin/port-forwarder-linux-amd64 server --port 28774
```

默认管理界面监听 `28774`。转发客户端：

```sh
./bin/port-forwarder-linux-amd64 client
```

## 从源码构建

需要 Go 1.25 或更高版本。

```sh
make all
```

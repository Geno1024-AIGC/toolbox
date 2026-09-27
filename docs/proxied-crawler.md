# Proxied Crawler

一个行为类似正向代理的爬虫：把浏览器 / 客户端的 HTTP 代理指向它之后，它会拦截路过的 HTTP(S) 请求，把每个响应的正文本体（不含响应头）按 URL 存成文件。

- `http://example.com/a/b?c` 的响应 → `results/http/example.com/a/b?c`
- `https://example.com/path` 的响应 → `results/https/example.com/path`（HTTPS 走 MITM，需安装本项目自签的根 CA）
- 不关心请求方法与请求体，只抓响应。

## 特性

- 单文件原生可执行程序，无运行时依赖（纯 Go + 标准库编译）。
- 内置网页 GUI：代理状态面板 + 按路径折叠 / 展开的结果树 + 文件预览。
- 一键交叉编译出 Linux 与 Windows 两个平台的可执行文件。

## 启动

```sh
./bin/proxied-crawler-linux-amd64
```

默认参数：代理端口 `8080`，GUI 端口 `8088`，结果目录 `results`，CA 目录 `certs`。所有参数均可用命令行 flag 覆盖。

# Toolbox

统一管理同目录下各个自研项目的入口。一份 manifest 登记一个工具，桌面版与网页版共用同一套核心。

## 功能

- 工具列表，按分类分组，标注类型（服务 / 网页应用 / 桌面应用 / Android 应用 / 文档）
- 托管型工具一键启停，异步 TCP 健康探针，pid 与状态实时可见
- 每工具一份说明文档，网页版内直接查看
- 桌面版额外提供系统托盘菜单与桌面通知

## 构建

依赖 Qt 6（Core / Network / Widgets）：

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
```

产出两个可执行文件：

| 文件 | 说明 |
| --- | --- |
| `build/toolboxd` | 命令行入口与无头服务 |
| `build/toolbox` | 桌面 GUI 入口 |

## 使用

```sh
# 桌面版（工具列表 + 系统托盘 + 通知）
./build/toolbox

# 无头服务，供局域网浏览器访问（默认 0.0.0.0:29811）
./build/toolboxd --daemon

# 单次操作
./build/toolboxd list
./build/toolboxd start <id>
./build/toolboxd stop <id>
```

两个入口都支持 `--root DIR` 指定仓库根目录（默认为当前工作目录）。

## 登记新工具

在 `manifests/` 下新建一个 JSON 文件，字段说明见下：

| 字段 | 含义 |
| --- | --- |
| `id` | 唯一标识，命令行操作用它 |
| `name` | 显示名称 |
| `category` | 分类，用于分组 |
| `type` | `service` / `webapp` / `desktop` / `android` / `document` |
| `summary` | 一句话简介 |
| `repo` | 工具目录，相对本仓库根目录 |
| `workdir` | 工作目录，默认为 `repo` |
| `command` | 启动命令 argv；`argv[0]` 相对 `workdir`，也可用绝对路径 |
| `port` | 网页端口 / 健康检查端口 |
| `healthPath` | 健康检查路径，默认 `/` |
| `docs` | 说明文档路径，相对本仓库根目录 |
| `note` | 额外提示，显示在界面上 |
| `managed` | 默认为 `true`，表示由 Toolbox 托管进程并可停止 |

托管型工具（`managed: true`）必须提供 `command`；桌面与移动端应用一般为 `managed: false`，以分离方式启动。

## HTTP 接口

| 路径 | 说明 |
| --- | --- |
| `GET /api/ping` | 存活探测 |
| `GET /api/tools` | 工具列表与状态 |
| `GET /api/tools/<id>` | 单个工具状态 |
| `POST /api/tools/<id>/start` | 启动 |
| `POST /api/tools/<id>/stop` | 停止 |
| `GET /api/tools/<id>/logs` | 日志尾部 |
| `GET /` | 内嵌网页版界面 |
| `GET /docs/<file>.md` | 工具说明文档 |

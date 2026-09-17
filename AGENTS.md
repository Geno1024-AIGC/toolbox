# AGENTS.md — 项目规范（权威，供 AI 代理遵守）

以下规则约束本仓库每一次编码会话，请务必遵守，无需再次提醒。

## 提交规则
- **一项一个 commit**。不要把无关改动揉进同一个 commit。每个独立事项完成后立即提交。
- 提交信息格式：`<bra><action><ket> <summary>`。
    - <action> 为动词，例如 Update、Typo、Refactor 等。
    - <braket> 为表示事件大小的括号对，大事件用大括号 {}，小事件用圆括号 ()，中事件用方括号 []。
    - <summary> 为一句简短的英文描述修改内容。
- 除非用户明确要求，绝不 amend、绝不强推、绝不改写已推送历史。
- 每个 commit 只包含本意要包含的文件，绝不提交密钥等敏感信息。

## 构建与验证
- 构建命令（Ubuntu）：
  ```sh
  cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
  cmake --build build -j
  ```
- 推送前必须构建成功（exit 0）。若失败，先修复错误再继续。
- 若项目内提供 lint/类型检查，可运行。

## 代码库事实
- C++/Qt6 桌面 + 网页双端 Toolbox。
  - 核心库 `toolboxcore`（QtCore + QtNetwork）：工具模型、manifest 加载、极简 HTTP Server（QTcpServer）、进程管理、REST API。
  - CLI 入口 `toolboxd`：纯服务模式，默认监听 `0.0.0.0:29811`，`--daemon` / `--gui` 二选一入口。
  - GUI 入口 `toolbox-gui`：Qt Widgets 桌面版（工具列表 + 系统托盘 + 通知），进程内直接使用 `toolboxcore`，同时内嵌启动 HTTP 服务供局域网网页版访问。
  - 内置 Web UI（qrc 嵌入）为 vanilla JS，无第三方依赖、无前端构建步骤。
- 工具登记：`manifests/*.json` 手动维护，每个工具一个文件；`dir` 相对本仓库根目录。
- 文档：`docs/*.md`，供网页版 `/docs/<file>` 与桌面版读取展示。
- 本仓库默认分支 `master`，origin 为 `git@github.com:Geno1024-AIGC/toolbox.git`。
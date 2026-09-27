# scheduled-backup

一个基于 Git 的轻量桌面备份客户端（Windows / Linux）。首次使用选择 GitHub 或 Gitee，创建一个私有仓库作为备份目标；之后程序会定时执行 `git add` → `git commit` → `git push`，把指定文件夹安全地备份到远端，并支持系统托盘后台运行。

## 技术选型

- Qt 6（Widgets + Network）+ C++17 + CMake
- 原生 GUI，无浏览器内核，单可执行文件（静态链接 Qt 后可单文件分发）
- Git 操作：调用系统 `git` 命令行，GitHub API 与 Gitee API 走 HTTPS

## 构建

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

构建产物位于 `build/scheduled-backup`。

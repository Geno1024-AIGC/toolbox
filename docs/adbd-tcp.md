# adbd-tcp

Android 端的 ADB over TCP 工具：通过 Wi-Fi 远程调试 Android 设备，无需插 USB 线。

- 包名 / applicationId：`com.geno1024.adbtcp`
- 技术栈：Kotlin + Gradle KTS

## 构建与安装

```sh
./gradlew assembleDebug
adb install -r app/build/outputs/apk/debug/adbd-tcp-v1.0-debug.apk
```

在 Linux 主机侧配合使用 Toolbox 中的 `port-forwarder`，把 5555 端口转发到无线网络中的设备。

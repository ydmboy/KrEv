<<<<<<< HEAD
# codexKr-was-created-using-OpenAi-s-Codex.
A free, powerful, multi-purpose tool that helps you monitor system resources, debug software and detect malware. Brought to you by Winsider Seminars &amp; Solutions, Inc. @ http://www.windows-internals.com
=======
# KrEverything

这是一个最小可编译的 Windows 内核驱动 Hello World 示例，适用于 `Visual Studio 2019 + WDK`。

## 功能

- 驱动加载时输出：`KrEverything: Hello from kernel mode.`
- 驱动卸载时输出：`KrEverything: unload`

输出通过 `DbgPrintEx` 进入内核调试输出，可用 `DbgView` 或 WinDbg 查看。

## 编译环境

- Visual Studio 2019
- Windows 10 WDK

如果没有安装 WDK，`WindowsKernelModeDriver10.0` 工具集不会出现，工程也无法编译。

## 打开和编译

1. 用 Visual Studio 2019 打开 `KrEverything.sln`
2. 选择 `Debug|x64` 或 `Release|x64`
3. 直接生成解决方案

生成成功后，驱动文件通常位于：

- `build\Debug\KrEverything.sys`
- `build\Release\KrEverything.sys`

## 代码入口

- `DriverEntry` 是驱动入口
- `KrEverythingUnload` 是卸载例程

源码文件：

- `KrEverything.c`

## 加载说明

要真正加载 `.sys`，系统需要满足驱动签名要求。开发测试时通常需要：

- 使用测试签名证书为驱动签名
- 或在测试环境开启 test signing

如果你下一步需要，我可以继续给你补：

- 一个可安装的 `.inf`
- VS2019 下的签名配置
- `sc create` / `sc start` 的加载脚本
- KMDF 版本的 Hello World
>>>>>>> origin/master

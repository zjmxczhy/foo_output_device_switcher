# foobar2000 无障碍增强

foobar2000 无障碍增强组件。

本组件会在 foobar2000 的 `播放` 菜单中添加播放设备、播放模式、播放列表相关子菜单，方便屏幕阅读器用户通过菜单和热键操作，并通过 Tolk 播报当前状态或切换结果。

## 功能

- 在 `播放 -> 播放设备` 中查看当前播放设备，或切换到上一个/下一个播放设备。
- 在 `播放 -> 播放模式` 中查看当前播放模式，或切换到上一个/下一个播放模式。
- 在 `播放 -> 播放列表` 中查看当前播放列表，或切换到上一个/下一个播放列表并播放。
- 按 foobar2000 返回的设备、播放模式、播放列表顺序循环切换。
- 使用 Tolk 播报当前状态和切换结果。
- 使用 `third_party\tolk-with-zdsr` 中的 Tolk 运行库，来源为 <https://github.com/boz700908/tolk>。

## 安装

从发布页下载：

```text
foo_output_device_switcher-0.2.1-x64.fb2k-component
foo_output_device_switcher-0.2.1-x86.fb2k-component
```

foobar2000 2.x 的 x64 版本请安装 `x64` 包；foobar2000 2.x 的 x86 版本请安装 `x86` 包。

foobar2000 1.5 / 1.6 是 32 位程序，请安装 `x86` 包。

在 foobar2000 中打开：

```text
文件 -> 参数选项 -> 组件
```

选择组件包安装，安装完成后重启 foobar2000。

## 构建

需要 Visual Studio 2022 Build Tools。

在仓库根目录运行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\build_output_device_switcher.ps1
```

构建输出：

```text
dist\foo_output_device_switcher-0.2.1-x64.fb2k-component
dist\foo_output_device_switcher-0.2.1-x86.fb2k-component
```

当前 SDK 目标为 foobar2000 1.5 / 1.6 兼容 API。1.4 及更早版本不作为正式支持目标。

## 说明

本仓库只维护 `foo_output_device_switcher` 组件源码，不包含朗读 LRC 歌词组件源码。组件显示名称为 `foobar2000 无障碍增强`，DLL 文件名和安装包文件名仍保留 `foo_output_device_switcher`，用于兼容既有安装和发布流程。

详细使用方法见：

- [Foobar2000无障碍增强使用方法.md](Foobar2000无障碍增强使用方法.md)
- [Foobar2000无障碍增强更新日志.md](Foobar2000无障碍增强更新日志.md)

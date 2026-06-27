# 播放设备切换

foobar2000 播放设备切换组件。

本组件会在 foobar2000 的 `播放` 菜单中添加 `播放设备` 子菜单，用于切换 foobar2000 当前使用的输出声卡。切换成功后，组件会通过 Tolk 直接播报新的设备名称。

## 功能

- 在 `播放 -> 播放设备` 中提供 `切换到上一个播放设备`。
- 在 `播放 -> 播放设备` 中提供 `切换到下一个播放设备`。
- 按 foobar2000 输出设备列表循环切换。
- 使用 Tolk 播报切换后的设备名称。
- 使用 `third_party\tolk-with-zdsr` 中的 Tolk 运行库。

## 安装

从发布页下载：

```text
foo_output_device_switcher.fb2k-component
foo_output_device_switcher-x64.fb2k-component
foo_output_device_switcher-fb2k-x86.fb2k-component
```

foobar2000 2.x 优先使用 `foo_output_device_switcher.fb2k-component` 或对应架构包。

foobar2000 1.5 / 1.6 请使用 `foo_output_device_switcher-fb2k-x86.fb2k-component`。foobar2000 1.x 是 32 位程序，需要安装 x86 包。

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
dist\foo_output_device_switcher.fb2k-component
dist\foo_output_device_switcher-x64.fb2k-component
dist\foo_output_device_switcher-fb2k-x86.fb2k-component
```

当前 SDK 目标为 foobar2000 1.5 / 1.6 兼容 API。1.4 及更早版本不作为正式支持目标。

## 说明

本仓库只维护 `foo_output_device_switcher` 播放设备切换组件，不包含朗读 LRC 歌词组件源码。

详细使用方法见：

- [播放设备切换组件使用方法.md](播放设备切换组件使用方法.md)
- [播放设备切换组件更新日志.md](播放设备切换组件更新日志.md)

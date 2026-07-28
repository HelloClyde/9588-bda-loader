# 更新日志

本项目遵循 [Semantic Versioning](https://semver.org/lang/zh-CN/)。

## [未发布]

## [1.2.1] - 2026-07-28

- trampoline 改为 Loader 的第一次堆分配，诊断日志和系统字库不再改变其
  分配顺序。
- 普通版和诊断版在释放图形及堆资源后统一等待 8 个固件 tick，再提交
  外层返回地址，避免日志文件操作偶然掩盖固件收尾时序。

## [1.2.0] - 2026-07-26

- 中文标题和分类改为运行时读取设备自带 `HZK_LIB.BIN`，恢复原机字形。
- 字库缺失、损坏或内存不足时显示 MsgBox 并安全退出。
- 移除仓库内嵌的 Noto 中文点阵字体表。
- 启动 trampoline 改为在 GUI 初始化前预留，避免复用刚释放的固件对象。
- 启动前先释放 Loader 资源，再提交外层返回地址。
- trampoline 改从 KSEG1 非缓存别名执行，消除普通版与诊断版之间的
  I-cache、时序和内存布局差异。

## [1.1.0] - 2026-07-26

- 支持 9688 V2.32 的 JZ4730/JZ4740 path-loader 布局。
- 固件诊断 profile 现在会区分 9588/9688 机型和芯片。
- 新增原始内核及 64-byte 包装内核的离线只读验证工具。

## [1.0.0] - 2026-07-25

- 首个开源版本。
- 提供黑色主题、3×3 九宫格、两行触摸分类 tabs 和上下翻页。
- 扫描并展示 BDA 标题、分类及固件原生图标。
- 支持触摸、方向键、Enter 启动和 Esc 退出。
- 使用逐像素 RGB565 合成，正确区分 `0xf81f` 色键与 `0x0000` VX 透明规则。
- 通过延迟 trampoline 安全启动第二个 BDA，不让目标覆盖仍在执行的 Loader。
- 支持 9588 V3.30 的 JZ4720、JZ4730、JZ4740 三套固件布局。
- 提供诊断构建、GitHub Actions 自动打包和 tag 自动发布。

[未发布]: https://github.com/HelloClyde/9588-bda-loader/compare/v1.2.1...HEAD
[1.2.1]: https://github.com/HelloClyde/9588-bda-loader/compare/v1.2.0...v1.2.1
[1.2.0]: https://github.com/HelloClyde/9588-bda-loader/compare/v1.1.0...v1.2.0
[1.1.0]: https://github.com/HelloClyde/9588-bda-loader/compare/v1.0.0...v1.1.0
[1.0.0]: https://github.com/HelloClyde/9588-bda-loader/releases/tag/v1.0.0

# 更新日志

本项目遵循 [Semantic Versioning](https://semver.org/lang/zh-CN/)。

## [未发布]

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

[未发布]: https://github.com/HelloClyde/9588-bda-loader/compare/v1.1.0...HEAD
[1.1.0]: https://github.com/HelloClyde/9588-bda-loader/compare/v1.0.0...v1.1.0
[1.0.0]: https://github.com/HelloClyde/9588-bda-loader/releases/tag/v1.0.0

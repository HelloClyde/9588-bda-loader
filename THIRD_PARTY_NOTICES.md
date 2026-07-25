# 第三方组件声明

## Noto Sans CJK

`src/small_title_font.h` 中的 12×12 中文位图由 **Noto Sans CJK SC Regular**
生成。

- 上游项目：<https://github.com/notofonts/noto-cjk>
- 生成来源 commit：`f8d157532fbfaeda587e826d4cd5b21a49186f7c`
- `NotoSansCJKsc-Regular.otf` SHA256：
  `2c76254f6fc379fddfce0a7e84fb5385bb135d3e399294f6eeb6680d0365b74b`
- 字体许可证：SIL Open Font License 1.1
- 本仓库不分发原始 OTF/TTC 字体文件
- 许可证全文：[docs/licenses/OFL-NotoSansCJK.txt](docs/licenses/OFL-NotoSansCJK.txt)

## bbk9588-bda-sdk

构建过程使用：

- <https://github.com/HelloClyde/bbk9588-bda-sdk>
- Apache License 2.0

打包器通过 `requirements-build.txt` 固定到明确 commit，本仓库不复制 SDK 源码。

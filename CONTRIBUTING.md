# 参与贡献

感谢你改进 9588 BDA Loader。

## 开发流程

1. Fork 仓库并从 `main` 创建分支。
2. 安装 `requirements-build.txt` 中固定版本的打包器。
3. 修改代码后同时构建普通版和诊断版：

   ```bash
   python build.py
   python build.py --diagnostic
   ```

4. 确认两个产物都通过 `bda_packer.validate`。
5. 提交 Pull Request，说明测试固件、芯片型号以及是否经过真机验证。

## 代码约定

- C 代码保持 freestanding，不依赖 libc 或操作系统头文件。
- 所有固件私有入口必须由运行时机器码校验保护，不能只按固定地址调用。
- 不要提交固件、系统资源、商业 BDA、设备序列号或包含隐私路径的日志。
- 修复真机问题时，优先附上诊断版 `BDALOAD.LOG` 的最小相关片段。

## 字体表

`src/small_title_font.h` 是从开放授权字体生成、可以直接构建的字体表。需要更换字体
或补充扩展 GBK 标题字符时，可使用：

```bash
python build.py --font /path/to/open-font.otf --apps /path/to/应用/程序
```

提交重新生成的字体表时，必须同时说明字体来源和许可证。请勿使用或提交设备的
`HZK_LIB.BIN`。

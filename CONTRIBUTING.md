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

## 系统字体

Loader 在运行时读取设备自带的 `A:\系统\数据\HZK_LIB.BIN`，仓库不保存字体内容。
修改字模索引、偏移或缓存逻辑时，请提供固件版本、文件 SHA256 和不含字体数据的验证
结果。请勿提交设备的 `HZK_LIB.BIN`、字模转储或由其生成的嵌入字体表。

# 更新日志

本项目遵循 [Semantic Versioning](https://semver.org/lang/zh-CN/)。

## [未发布]

- 真机连续启动测试确认 1.2.2 正式版的最小日志 I/O 只能偶然改变启动时间；
  V45 显式文件系统同步仍仅成功 4/10，排除卷同步为根因。
- V46 将共同等待延长到 600 ms 后仍然死机，排除单纯 GUI 定时器恢复过慢。
- 逐条还原固件 path-loader 收尾流程，确认 BDA 返回后会用固定参数 `0x190`
  重启外层 GUI 定时器；五套已支持固件均为 400 ms。
- 找到概率死机的结构性原因：旧实现修改了内层 path-loader 的保存返回地址，
  在外层菜单 caller 尚未执行 post-BDA trace、heap 检查及栈帧收尾时就启动目标。
- V47 改为修改完整菜单 caller 的保存父返回地址：第一层 path-loader 和菜单
  caller 先按固件原逻辑完整返回、弹出旧栈帧，再由无返回 handoff 重建一份原生
  `0x160` 字节 caller 栈帧，并从固件原有 prelaunch tail 启动目标。
- handoff 仍等待 24 个固件 tick（600 ms），确保 400 ms GUI 恢复事件完成。
- 移除 V45 的私有文件系统同步调用；正式版也不再创建或写入日志文件。
- 固件离线验证工具新增 GUI 恢复定时器，以及完整 caller prologue、保存寄存器、
  post-BDA helper、跳转目标和 epilogue 的机器码校验。
- 普通版和诊断版仍保持相同长度、地址布局和启动代码，只有日志输出开关一个
  数据字节不同。

## [1.2.2] - 2026-07-29

- 启动交接改为固件菜单调用者栈上的 tail stub，目标运行期间不再保留
  Loader heap trampoline。
- 对 PSX dynarec 等高内存、堆执行应用恢复与系统菜单直启一致的堆布局和
  `a0/a1/a2/ra` 调用形态。
- 固件验证增加原生 caller frame、路径区和 tail-call 参数布局检查。
- 固定 8 tick 等待从 Loader 内部移到 tail stub，在第一层固件 path-loader
  完成 post-BDA 清理之后才开始计时，普通版和诊断版共用同一 gate。
- 从当前固件的系统菜单调用点动态解码并重放路径/运行时准备 helper 与固件
  trace helper，使延迟启动链和系统菜单直启保持同一前置状态。
- 诊断日志移到固定等待和原生 helper 之前；正式版与诊断版进入 path-loader
  前执行完全相同的最后两次固件调用，避免日志 I/O 再次掩盖状态差异。
- 固件验证工具同步检查两次前置 `jal`、路径参数和 trace 字符串装载形态。
- 正式版与诊断版统一编译诊断计时和代码路径，仅由一个运行时数据字节控制
  `BDALOAD.LOG` 输出；两个 BDA 的大小、代码地址、全局布局和栈帧完全一致。
- 真机一字节 A/B 测试确认 PSX 依赖固件文件写入/关闭的状态副作用，而非编译
  布局；正式版因此仅保留启动时 create/truncate/close 和清理后单行 marker 两个
  兼容 I/O 检查点，避免完整诊断版在列表操作期间频繁写盘。
- 诊断版继续写入完整日志；二者成品除输出开关外仍只有一个字节不同。

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

[未发布]: https://github.com/HelloClyde/9588-bda-loader/compare/v1.2.2...HEAD
[1.2.2]: https://github.com/HelloClyde/9588-bda-loader/compare/v1.2.1...v1.2.2
[1.2.1]: https://github.com/HelloClyde/9588-bda-loader/compare/v1.2.0...v1.2.1
[1.2.0]: https://github.com/HelloClyde/9588-bda-loader/compare/v1.1.0...v1.2.0
[1.1.0]: https://github.com/HelloClyde/9588-bda-loader/compare/v1.0.0...v1.1.0
[1.0.0]: https://github.com/HelloClyde/9588-bda-loader/releases/tag/v1.0.0

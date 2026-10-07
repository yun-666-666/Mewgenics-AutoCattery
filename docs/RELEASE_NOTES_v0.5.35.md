# AutoCattery v0.5.35 — 2026-10-07

本版为 **正式版 / Latest Release**，整合 v0.5.34 之后的最新 MOD 和游戏外繁育工坊。
玩家于2026-10-07确认当前MOD实际游玩正常，并授权将v0.5.35转为正式版。
源码、安装包和说明同步发布，源码包含此前留在工作树中的必要组件。

## 下载与升级

- `AutoCattery-v0.5.35-MOD-only.zip`：只含完整 MOD DLL、配置、Mewtator 数据补丁和文档，
  不含繁育工坊/运行环境；F10“后代遗传全七辅助”和“供食辅助”均保留，无需工坊即可使用。
- `AutoCattery-v0.5.35-Windows-x64.zip`：完整安装包，含 DLL、配置、Mewtator 数据补丁、
  文档及带独立 Python 运行环境的 `AutoCatteryWorkbench`。
- `AutoCattery-v0.5.35-Workbench.zip`：只更新游戏外繁育工坊，将整个文件夹放到
  `Mewgenics.exe` 同级，双击 `StartBreeding.cmd`。无需系统 Python。
- `AutoCattery-v0.5.35-source.zip`：当前完整项目源码及第三方源码依赖，供修改与构建。

玩家可先只安装MOD，再自行选择是否添加独立Workbench包；游戏内辅助不依赖工坊。

关闭游戏后按 [README](../README.md) 安装。DLL 位于游戏 `Mods/AutoCattery.dll`，
配置/日志位于 `Mods/AutoCattery/`；Mewtator 数据放入其 `mods/AutoCattery/` 并启用。
升级时保留自己的 `user_config.json` 和保护规则；不要用默认配置替换个人选择。
繁育工坊模拟/导入期间保持游戏关闭，确认后写回所选槽位并自动保留恢复副本。

## MOD 变化

- 修复房间身份读取，支持当前已确认的2–5房；管道中的待安置猫重新参与整理。
- 读取实际存在的性欲、性取向和族谱，Tink显示解锁不再阻断数量筛选与繁育。
- 新增数量上限（默认150）：可写模式下进House/新一天检查超额，显示10秒名单后
  顺序垃圾桶交付；Esc/F10/关闭取消本次访问，保护、固定房和未知状态猫保留。
- NPC交付按已确认进度和优先级选择接收方，复用逐只交付、取消与确认流程。
- F10提供后代遗传全七和供食辅助，游戏内默认关闭。辅助实际可写时可按遗传特性
  优先选种；否则基础属性优先。主动/被动/正变异各自有界评分，疾病和缺陷仍扣分。
- 繁育房不固定阁楼。排除禁止繁育效果，优先舒适大于-10的房，再按舒适、刺激、
  健康、变异等权合计选房；稀有家具和已安装合并家具MOD的强化效果计入。
- 配对/留种考虑COI与独立血系；原生繁育仍可能在同房猫间交叉选配，不保证指定父母。

## 游戏外繁育工坊

- 独立运行环境：Python 3.12.10、pefile 2024.8.26、Unicorn 2.1.4，含原始许可。
  只读取玩家本机游戏EXE/资源，不分发游戏文件、游戏资产或存档。
- 网页选择账户/槽位并模拟，预览新增/移出猫；玩家勾选并确认后自动写回原槽位，
  保留 `before-apply.sav`。源档变化、游戏运行或空结果时拒绝导入，重复确认不重复写入。
- 新猫使用游戏原生随机名，预览与导入一致；已有猫不重命名。
- 显示“实际安排 / 目标配对”。房间不足时多对可共用繁育房，仍留其他猫/幼猫房。
  资格和原生概率继续生效，目标4对不代表必定每日4窝。
- 网页两个辅助首次默认勾选，明确修改后跨重开/端口保留，独立于游戏内MOD设置。
- 原生模拟保留健康、打架、外来猫、供食等阶段。等价缓存减少开销，实验工具支持
  checkpoint恢复和 `--jobs` 有界独立进程并行，默认1。

## 验证范围与限制

本次发布只更新版本和文档并收齐现有实现，不重复已完成的长期模拟。

- v0.5.35 Release编译成功；7项针对CTest全部通过：House UI资源、数量筛选、
  逐只交付、自动数量管理、繁育特性/房间规划、清便及DLL加载（0.79秒）。
- 工坊写回8项测试通过，配对/HTTP偏好4项通过，完整网页脚本的偏好保存与显示通过。
- 本次编译有测试代码的窄化/变量遮蔽警告，未将结果描述为无警告构建。
- 随包运行环境导入与ZIP内容检查作为最后的发布检查；具体命令和结果记录在本地stage-40报告。

既有验证记录：

- 真实存档只读规划及家具/房间针对回归通过，动态繁育房预览与重复整理幂等通过。
- 6种子各两策略60日共720日对照：平均每出生后代主动效用+101.33%、被动+569.41%、
  变异+11.56%；出生736→447（-39.27%）。这是当前资源启发式，不能解释为实战强度
  或技能遗传概率提升；变异均值差小于其标准误，不宣称稳定统计提升。
- 长期辅助全七统计按辅助开关和已验证写入流程计算，非逐出生独立读回。
  模拟两对分房不等于游戏内六猫组完整验证。
- 同种子两日缓存/快速PE读取/回调优化通过事件、猫字节、RNG和完整状态普通内容等价
  对照；串行/双进程实验结果一致。短程提速不外推720日总耗时。
- 工坊确认导入、恢复副本、重复操作和游戏进程门控已在测试存档验证。
- 历史综合 `phase14_unit_tests` 有10条固定房名断言与后续动态房间规划不一致。
  本版沿用该已记录限制，不宣称完整suite通过，不在发布任务中改写旧测试。

玩家于2026-10-07确认当前MOD实际游玩正常，本次正式发布依据此玩家确认。
本次转正只修改发布状态与相关文档，沿用已完成的构建和测试结果。

## English

This stable release publishes the complete current MOD and standalone workbench since v0.5.34.
It includes population management with a cancellable preview, saved breeding data independent
of Tink display unlocks, optional offspring/food assistance, balanced inherited-trait scoring,
dynamic breeding-room selection and furniture multipliers. The workbench bundles its runtime,
native simulation, confirmed save application and recovery copy, native random names,
actual/target pairs and persistent assist preferences.

Install the MOD-only archive for the full in-game MOD, including both F10 offspring and food
assists, without the workbench/runtime. Optionally add the separate Workbench archive, or use
the Windows archive for both. Keep personal configuration/protection rules when upgrading. Keep the game closed
while simulating or importing, and confirm the result before writing the selected save.

Targeted build/tests and package checks accompany the release. Existing native-equivalence
and long-run evidence is retained; full aggregate test success is not claimed. On 2026-10-07,
the player confirmed normal actual play and authorized promotion to the formal Latest Release.
The scoring experiment improved average heuristic ability
and passive utility per offspring while reducing total births; mutation improvement is not
statistically established.

# Stage 23 当前报告

更新日期：2026-08-04

状态：v0.5.7 战后异常风暴修复后，玩家实测发现 F10 面板无文字且闪烁；
根定位回归和每帧重试已修复，v0.5.8 已部署，等待实机复测。

## 实机证据与根因

- 玩家结束一天时直接闪退；重新进行一局并从战斗返回 House 后仍持续卡顿。
- 最新 `chainloader.log` 达到约 1.2 GB；House UI 线程持续产生数百万次
  `0xC0000005`，主要地址为 `Mewgenics.exe+0x5A0E0/0x5A0E3`。
- 21:38 的崩溃报告为 `0xC0000374` 堆损坏；22:31 的结束一天过渡点之后进程
  直接卸载。两者都发生在异常风暴持续写盘和 UI 场景切换窗口内。
- 首轮修复只替换了面板和按钮的显式查找，没有覆盖当天推荐不可用时的循环：
  `ObserveRuntime` 每个 UI tick 都调用 `SetAvailable(false)`，该函数每次清空
  4 行推荐文字，而每行都通过 `MewUI_SetTextInSceneText` 扫描全体 House 根。
- 当前 House 有 4,249 个组件。一个不变的 false 状态因此被放大为每帧 4 次
  全场景扫描，解释了战后持续卡顿、1.2 GB 日志和堆损坏风险。
- v0.5.7 将全部 MOD UI 查找错误限定到游戏的 `HouseTest` 根。最新日志明确记录
  `AC18001 F10 panel nodes are missing from the House SWF`，推荐按钮和整理按钮也
  同时报告资源不可用；截图表现为只有面板边框、全部文字缺失并闪烁。
- 面板控制器此前在挂载失败后每个 UI tick 立刻重试，反复执行不可能成功的根
  查找和显隐操作，形成新的异常与闪烁路径。

## 本轮修复

- 推荐可用性同步改为幂等；同一状态连续轮询 100 次只调用视图一次。
- 推荐行的文字节点在挂载时与 MovieClip 一起缓存；显示和清空都直接写缓存
  节点，不再调用任何全场景文字查找。
- 移除错误的 `HouseTest` 类型限制，也移除对 4,249 个组件逐个读取类型名的
  虚函数调用。只在 UI 挂载阶段解析经验证、去重后的根，找到首个 MOD 资源根
  即停止；正常渲染不执行根扫描。
- F10 面板挂载失败后最多每 500 ms 重试一次，不再每帧检查或反复显隐。
- 保留场景 generation 边界的 `AbandonScene` 清理，不修改游戏原始文件、
  存档、玩家配置、历史日志或未跟踪工具目录。

## 文件与命令

- UI 根边界：`src/ui/mew_ui_scene_components.*`、
  `src/ui/mew_ui_safe_node_lookup.*`。
- 推荐状态与缓存：`src/ui/recommendation_marker_controller.cpp`、
  `src/ui/mew_ui_recommendation_marker_view.*` 及对应头文件。
- 回归测试：`tests/mew_ui_safe_node_lookup_tests.cpp`、
  `tests/recommendation_marker_controller_tests.cpp`。
- 面板节流：`src/ui/in_game_panel_controller.*`。
- 文档与版本：`CMakeLists.txt`、`assets/description.json`、`CHANGELOG.md`、
  README、当前状态、路线图和 v0.5.8 发布说明。
- Debug：`.\tools\build.ps1 -Configuration Debug`。
- Release：`.\tools\build.ps1 -Configuration Release`。
- 部署：`.\tools\deploy.ps1 -GameRoot <GAME_ROOT> -Configuration Release`。
- 安装校验：`.\tools\verify_install.ps1 -GameRoot <GAME_ROOT>`。

## 验证结果

- Debug 编译通过，CTest `4/4` 通过。
- 静态检查确认根查询只存在于 UI 挂载路径，正常渲染不扫描场景；推荐列表仅
  使用缓存文字节点，面板失败重试受 500 ms 节流保护。
- Release DLL 编译通过，DLL 加载 smoke 通过。按玩家“减少没必要的检查”要求，
  未重复编译与本次 UI 修复无关的 Release CLI/探针测试目标；逻辑单测已由 Debug
  同源目标覆盖。
- Release DLL 已部署；构建、dist 与安装 DLL 的 SHA-256 均为
  `20264503DFFEC0C344C57DF91C2C0E0EAFB7BF7F237348D3B0BC7D86F21500EC`。
- 安装校验通过；Mewtator 数据 MOD 为 v0.5.8，玩家既有
  `level_up.reroll_count=10` 保持不变，14 个职业数据一致。
- 玩家实机验证待完成。

## 风险与待验收

- 自动化不能证明真实游戏文字渲染、帧率或结束一天的场景销毁行为；玩家需要
  进入 House 打开 F10，确认文字正常且不闪烁，再完成一场战斗返回 House并
  结束一天，确认不卡顿且不闪退。
- 历史 `chainloader.log` 约 1.2 GB，本轮未擅自删除；新版本验证应以新增日志
  是否停止快速增长为准。
- 本阶段只修复 House UI 生命周期和性能，不扩展淘汰、分房或繁育功能。

本轮本地 commit：以最终回复中的提交哈希为准。

是否 push：否

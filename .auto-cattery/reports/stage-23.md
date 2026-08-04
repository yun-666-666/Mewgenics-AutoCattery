# Stage 23 当前报告

更新日期：2026-08-04

状态：首轮战后卡顿修复经玩家实测失败；剩余每帧推荐列表扫描已定位并修复，
Debug/Release 构建、测试、部署和安装校验通过，等待玩家复测。

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
- 首轮“校验所有可读根”的定位器仍会把普通组件根交给直接子节点查询；虽然
  比备用查询更轻，但仍会制造被 SEH 吞掉的访问异常。运行时证据显示当前 build
  只有一个 `HouseTest` 组件和一个根，故将 UI 根边界收紧到该实测所有者。

## 本轮修复

- 推荐可用性同步改为幂等；同一状态连续轮询 100 次只调用视图一次。
- 推荐行的文字节点在挂载时与 MovieClip 一起缓存；显示和清空都直接写缓存
  节点，不再调用任何全场景文字查找。
- House UI 根查找只检查当前 build 实测唯一的 `HouseTest` 组件；类型或节点
  不匹配时安全禁用 UI，不再扫描数千个普通组件根。
- 保留场景 generation 边界的 `AbandonScene` 清理，不修改游戏原始文件、
  存档、玩家配置、历史日志或未跟踪工具目录。

## 文件与命令

- UI 根边界：`src/ui/mew_ui_scene_components.*`、
  `src/ui/mew_ui_safe_node_lookup.*`。
- 推荐状态与缓存：`src/ui/recommendation_marker_controller.cpp`、
  `src/ui/mew_ui_recommendation_marker_view.*` 及对应头文件。
- 回归测试：`tests/mew_ui_safe_node_lookup_tests.cpp`、
  `tests/recommendation_marker_controller_tests.cpp`。
- 文档与版本：`CMakeLists.txt`、`assets/description.json`、`CHANGELOG.md`、
  README、当前状态、路线图和 v0.5.7 发布说明。
- Debug：`.\tools\build.ps1 -Configuration Debug`。
- Release：`.\tools\build.ps1 -Configuration Release`。
- 部署：`.\tools\deploy.ps1 -GameRoot <GAME_ROOT> -Configuration Release`。
- 安装校验：`.\tools\verify_install.ps1 -GameRoot <GAME_ROOT>`。

## 验证结果

- Debug 编译通过，CTest `4/4` 通过。
- 静态检查确认三个 House UI 视图不再调用 root-owned 或全场景节点查询；推荐
  列表仅使用缓存文字节点。
- Release 编译通过，CTest `4/4` 通过。
- Release DLL 已部署；构建、dist 与安装 DLL 的 SHA-256 均为
  `04272BC9E97EF2145409191C5B86FCB72311071E283E80F7FFAE7F86560F1334`。
- 安装校验通过；Mewtator 数据 MOD 为 v0.5.7，玩家既有
  `level_up.reroll_count=10` 保持不变，14 个职业数据一致。
- 玩家实机验证待完成。

## 风险与待验收

- 自动化不能证明真实游戏帧率或结束一天的场景销毁行为；玩家需要完成一场
  战斗返回 House，再结束一天，确认不卡顿且不闪退。
- 历史 `chainloader.log` 约 1.2 GB，本轮未擅自删除；新版本验证应以新增日志
  是否停止快速增长为准。
- 本阶段只修复 House UI 生命周期和性能，不扩展淘汰、分房或繁育功能。

本轮本地 commit：以最终回复中的提交哈希为准。

是否 push：否

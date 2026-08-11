# Stage 43 - 五房修正、按钮标签同步与家具属性升级分析

日期：2026-08-11
版本：v0.5.30
状态：实现、Debug/Release 自动验证、主存档五房字段修正、Release 部署和安装校验已完成；等待玩家实机确认第五房间、按钮标签和属性升级提示。

## 玩家问题与结论

- 主存档截图实际只有 4 个房间，阁楼下左侧仍为封板；原因是
  `properties.house_storage_upgrades=4`，不是画面识别错误。
- House 按钮在初次进入及从家具界面返回时短暂显示原版 `Clean Up!`，原因是原版在
  AutoCattery 首次改写后还会刷新按钮标签，旧实现只在 hover 等后续事件中恢复文案。
- 已摆满不代表家具属性组合最优。主存档有 257 件家具，其中 142 件已摆、115 件在
  仓库；旧分析只验证几何布局，没有比较后来获得的仓库家具属性。

## 实现

- 将当前第五房间映射补充为 `Floor2_Small`。
- House 按钮 attach、进入/退出家具模式后，以有限的指数帧间隔重同步当前状态标签，
  覆盖原版的延迟刷新窗口，不再依赖鼠标悬停恢复中文。
- 家具分析新增五属性无损升级匹配：Comfort、Stimulation、Health、Mutation、Appeal
  全部不降低且至少一项提高；已摆实例和仓库实例均只允许匹配一次，并汇总五属性净增。
- 发现升级候选时 UI 明确显示“发现更优属性组合；仓库替换仅预览”，不再错误显示
  “无需移动”。新增 `AC3910` 逐项升级日志和 `AC3911` 仓库/scene 匹配统计。
- 当前 House scene 真实枚举仍只有 142 个已摆 `FurniturePiece`，没有发现 115 件仓库
  家具对应的 grid-null piece；因此本阶段只提供属性替换分析预览，不执行未证明安全的
  原生仓库取放。

## 主存档与备份

- 修改前备份复用原文件：
  `C:\Users\wordy\AppData\Roaming\AutoCatteryData\save-backups\manual-main-five-room-20260811-152235\steamcampaign01.sav.bak`
- 备份 SHA-256：
  `C984ED6F57B9260C027F0671C257C6998B439D5AC02512DF7E4BF498705A9170`。
- 游戏退出后以单事务把主存档 `house_storage_upgrades` 从 4 改为 5；与修改前即时状态
  相比仅该字段变化。
- 修改后主存档 SHA-256：
  `2D4D931578D0D3AEF4E47C6AD150D5E9D926DAB6DF7AF5CBE631AC9D90C421D9`。
- 最终只读复核：`house_storage_upgrades=5`、`PRAGMA integrity_check=ok`，不存在
  `-wal` 或 `-shm`；备份仍在原位且 hash 不变。

## 文件

- 版本与阶段：`CMakeLists.txt`、`assets/description.json`、`CODEX_TASK.md`、
  `.auto-cattery/state.json`、本报告。
- 属性分析：`include/auto_cattery/furniture_analysis/domain.hpp`、
  `src/furniture_analysis/service.cpp`。
- 按钮标签：`include/auto_cattery/ui/house_button_controller.hpp`、
  `src/ui/house_button_controller.cpp`。
- 运行时桥接与仓库 probe：`src/ui/mew_ui_bridge.cpp`、
  `src/ui/runtime_house_state.hpp`、`src/ui/runtime_matched_save_snapshot_adapter.cpp`。
- 回归：`tests/furniture_analysis_service_tests.cpp`、
  `tests/house_button_controller_tests.cpp`。

## 构建、测试与部署

- `tools\build.ps1 -Configuration Debug`：成功；Debug 单元测试通过，CTest 4/4。
- `tools\build.ps1 -Configuration Release`：成功；Release CTest 4/4，Release 打包成功。
- `tools\deploy.ps1 -GameRoot D:\steam\steam\steamapps\common\Mewgenics -Configuration Release`：成功。
- `tools\verify_install.ps1 -GameRoot D:\steam\steam\steamapps\common\Mewgenics`：通过；
  x64 DLL、Mewjector、Mewtator data mod 和 14 个职业 20 次重投配置有效。
- build、dist、实际安装 DLL SHA-256 一致：
  `D7992A2F74ECC0386877BEDA7CA5CD0A309E961B7D11687FA31C51530E3FFBC5`。
- `git diff --check`：通过。

## 游戏验证状态、风险与未做范围

- Codex 未启动、进入或控制游戏。玩家仍需亲自确认：第五房间是否真正出现；初次进入
  House 及从家具界面返回后按钮是否直接显示中文；主存档点击开始分析后是否显示属性
  升级候选而非“无需移动”。
- 属性候选目前是只读分析预览。仓库家具的当前 build 原生取放对象和调用链尚未由真实
  scene 证据证明，因此没有自动替换仓库家具，也没有把候选加入原生 move 批次。
- 不自动移动猫、休息、结束一天、出征、组队或淘汰；没有 push。

本地提交：本报告与实现位于同一本地任务提交，最终 hash 见交付回复。
是否 push：否

# Stage 42 - 精确原生家具网格、阁楼承重链与实时重新分析

日期：2026-08-11
版本：v0.5.29
状态：实现、Debug/Release 自动验证、真实存档只读探针、DLL-only 部署和安装校验已完成；等待玩家 24 猫测试存档原生执行验收。

## 玩家问题与根因

- v0.5.28 把 Surface 当成可承重格，导致 `object_cinderblock1` 的 Support 落到
  `set_bone_sink` Surface 后被原生校验拒绝，随后批次回滚，因此玩家看到画面不动。
- 旧打包搜索只保留局部紧凑方案，没有主动保留“底层承重家具 -> 中层 Solid ->
  吊笼/小物”的多级依赖组合，所以即使截图上方有真实可利用空间，24 猫当前布局仍
  留下 `ceiling_cage`。
- Hitbox-only 墙挂物品因为 `Support=0` 被旧保护规则永久固定；当前原生规则已确认
  这类物品只要求目标格不冲突，不要求额外 Support。
- 显式重新分析沿用了本次家具界面的 completed-room lock，binding digest 也没有包含
  base/live grid 内容，玩家手动移动家具后可能继续看到上一次分析结论。

## 实现

- 全路径统一当前 build 原生 tile 语义：Hitbox/Solid/PoopLogic 写 grid；Surface
  不占位、不承重；Support 只接受房间或家具 Solid。
- 新增有限、确定性的承重链种子搜索：从无法直接落地的吊挂家具反向寻找可执行的
  Solid 提供链，再交给原有全屋 beam 完成剩余家具打包。该搜索不会接受循环悬空链，
  最终执行仍按原生可安装顺序逐件验证。
- 有效的当前完整布局会被识别并保留，避免自动放置完成后下一次分析把同一批家具
  搬出阁楼。
- Hitbox-only 墙挂家具按原生碰撞规则参与跨房间规划；24 猫的两件墙挂物不再留在
  `Floor1_Large`。
- 每次玩家显式点击“开始分析”都会清空旧房间锁并重新捕获当前家具坐标及房间
  base/live grid；digest 纳入网格内容。
- `deferred_furniture_count > 0` 时不再把目标房间标记为完成或加入锁定列表。
- 原生拒绝日志补充 item、来源/目标房间、起点/终点、status、signature、placement、
  commit、verify、rollback、SEH 和 RVA。

## 文件

- 原生网格捕获与拒绝诊断：
  `src/ui/mew_ui_furniture_move_adapter.[ch]`、`src/ui/runtime_house_state.hpp`、
  `src/ui/runtime_matched_save_snapshot_adapter.cpp`、`src/ui/mew_ui_bridge.cpp`。
- 分析绑定：`src/furniture_analysis/service.cpp`。
- 求解器：`include/auto_cattery/furniture_planning/layout_solver.hpp`、
  `src/furniture_planning/layout_solver.cpp`。
- 回归与只读探针：`tests/furniture_analysis_service_tests.cpp`、
  `tests/furniture_layout_solver_tests.cpp`、`tests/furniture_geometry_probe.cpp`。
- 版本与状态：`CMakeLists.txt`、`assets/description.json`、`CODEX_TASK.md`、
  `.auto-cattery/state.json`、本报告。

## 构建与测试

- `tools\build.ps1 -Configuration Debug`：成功；4/4 CTest 通过，包含单元测试、
  DLL load smoke、restore CLI smoke、save-lab CLI smoke。
- `tools\build.ps1 -Configuration Release`：成功；4/4 CTest 通过，同上。
- 新增确定性回归覆盖：
  - 多层 Solid 承重链可安装吊挂家具；
  - Hitbox-only 家具参与规划；
  - Surface 不提供 Support；
  - 相同 scene generation 下 live grid 改变会重新 Capture 并改变 binding digest；
  - 完成后的最新 live grid 再分析保持 0 moves。
- `git diff --check`：通过。

## 真实存档只读探针

探针只读打开 `.sav`，未写存档、未修改 Steam Cloud。

- 7 猫当前存档，`Floor1_Large=18x9;Attic=20x7`：Debug/Release 一致，
  `considered=10`、`deferred=0`、`unsupported=0`；重建最新 live grid 后
  `moves=0`、`kept=10`。
- 24 猫当前存档，
  `Floor1_Large=18x9;Floor1_Small=18x9;Attic=37x11`：Debug/Release 一致选择
  `Attic`，`considered=20`、`moves=36`、`deferred=0`、`unsupported=0`；
  `ceiling_cage` 和两件 `wallmounted_*` 都进入阁楼。计划终局 Support 分类为
  `room_solid=11`、`furniture_solid=26`、`surface_only=0`、`unsupported=0`；
  第二次分析为 `moves=0`、`kept=20`、`deferred=0`、`target=Attic`。
- 24 猫失败前存档：Debug/Release 一致选择 `Attic`，`considered=20`、
  `moves=35`、`deferred=0`；终局 `room_solid=10`、`furniture_solid=27`、
  `surface_only=0`、`unsupported=0`；第二次分析同样 0 moves、20 kept。
- 最多猫主存档只运行 Release：257 件总家具、142 件已放置、115 件仓库；
  `considered=142`、`no_space=142`、0 moves，未生成可执行写计划。

## 部署

- 部署前确认 `Mewgenics.exe` 未运行。
- 只复制 Release DLL 到实际加载位置，并同步 Mewtator data mod 的
  `description.json`；没有运行会改写职业配置的完整部署。
- build、dist、安装 DLL SHA-256 一致：
  `F35D2E8380233180992F9F0F40BD9A0FE7C1FE829BE1282A7341849C84DB1293`。
- `tools\verify_install.ps1 -GameRoot <game-root>`：通过；x64 DLL、Mewjector、
  Mewtator data mod 和 14 个职业 20 次重投配置均有效。

## 游戏验证状态、风险与未做范围

- 自动测试和只读探针证明方案符合当前解析出的原生网格语义，但没有在本轮中控制
  游戏 UI 执行 36 步；最终门禁仍是玩家在 24 猫测试存档点击自动放置、保存并完全
  退出重进。
- 玩家还需在同一次家具界面中手动移动一件家具，再点击“开始分析”，确认界面使用
  新位置而不是旧布局；失败时返回新的 `AC3900` 至 `AC3907` 日志和截图。
- 不从仓库取家具，不改变 scale/旋转，不自动移动猫、休息、结束一天、出征、组队
  或淘汰，不直接写 `.sav`。
- 没有 push。

本地提交：本报告与实现位于同一本地任务提交，最终 hash 见交付回复。
是否 push：否

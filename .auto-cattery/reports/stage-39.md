# Stage 39：同房间多家具自动布局与原生执行

更新日期：2026-08-09

状态：实现、聚焦构建与单元测试完成。玩家首次测试证明布局分析成功，但暴露出
“自动放置”按钮视觉启用后仍不接收点击；v0.5.21 已修复并部署，等待玩家重新
点击“自动放置”并保存重进验证。

## 本阶段玩家可见功能

- 家具界面右侧“开始分析”现在生成确定性同房间布局计划。
- 存在可执行移动时，左侧“自动放置”由禁用变为可点击。
- 点击后每个 UI tick 只移动一件家具，全部使用 Stage 38 已由玩家验证的原生
  移除、合法性校验、提交和读回路径。
- 执行前逐件核对稳定 key、item、room 和原坐标；原生拒绝、场景变化或实例被
  手动移动时停止剩余家具，已经成功提交的家具保留，玩家可重新分析继续。

## 玩家测试修复（v0.5.21）

- 玩家日志连续出现 `AC3901`（布局已生成，2 件待移动），但没有出现一次
  `AC3900`（自动放置点击），证明问题位于按钮输入而非布局或存档写入。
- 原因是按钮在等待分析时关闭了原生 Button 组件；后续只恢复可见状态不足以让
  部分 House UI 路径重新轮询该组件。
- 修复后，只要按钮仍显示就保持 Button 组件启用；是否允许点击仅由交互覆盖控制。
- 正式 Release 的分析与执行始终使用当前捕获到的房间、家具实例 key、item、坐标
  和几何目录，不写死 7 猫存档。Stage 38 的 `object_cattree1/key 5` F8 测试入口
  现仅编入 Debug，Release 不再包含该单存档测试行为。

## 布局算法

- 新增纯 `FurnitureLayoutSolver`，不调用写 adapter。
- 每个房间独立建立碰撞和家具占用；家具按实体占地从大到小、稳定 key 排序，
  候选坐标按底部优先、左到右枚举。
- 不猜测 Support 的全局语义；从家具当前合法位置记录每个非空 tile 对应的房间
  碰撞值，候选位置必须保持同一关系。
- Hitbox/Solid 参与家具间占用；Support 只约束房间支撑关系。
- 当前只移动至少含一个 Solid tile、`scale_x/scale_y=±1`、实例 key 有效、房间
  几何唯一、且没有与其他家具共享 saved 原点的独立实体家具。
- 墙面、小件、挂接对象、仓库家具、歧义 Attic 几何和无空间家具保持原位并计数。

## 当前 7 猫存档只读结果

- day 17，`Floor1_Large`，10 件已放置家具。
- 计划房间 1；可考虑 4；需移动 2；已紧凑 2；保持原位/不支持 6；无空间 0。
- 计划移动：
  - `object_cattree1`，key 5，`(4,-8) -> (-4,-8)`。
  - `object_cinderblock1`，key 3，`(4,-11) -> (-4,-11)`。
- 该结果由 Debug 与 Release `furniture_geometry_probe` 独立得到，未写存档。

## 文件

- 规划器：`include/auto_cattery/furniture_planning/layout_solver.hpp`、
  `src/furniture_planning/layout_solver.cpp`。
- 分析集成：`include/auto_cattery/furniture_analysis/domain.hpp`、
  `src/furniture_analysis/service.cpp`。
- UI/执行：`include/auto_cattery/ui/house_button_controller.hpp`、
  `include/auto_cattery/ui/mew_ui_bridge.hpp`、
  `src/ui/house_button_controller.cpp`、`src/ui/mew_ui_bridge.cpp`、
  `src/ui/mew_ui_house_button_view.cpp`。
- 测试与只读探针：`tests/furniture_layout_solver_tests.cpp`、
  `tests/furniture_geometry_probe.cpp`、`tests/house_button_controller_tests.cpp`、
  `tests/test_main.cpp`、`CMakeLists.txt`。
- 状态、版本与文档：`CODEX_TASK.md`、`.auto-cattery/state.json`、
  `assets/description.json`、`docs/furniture-auto-placement-design.md`、
  `docs/implementation-status.md` 和本报告。

## 构建与检查

- `git diff --check`：通过，仅有工作区既有 LF/CRLF 提示。
- Debug 聚焦构建 `auto_cattery_tests`、`AutoCattery`、
  `furniture_geometry_probe`、`dll_smoke_tests`：通过。
- Debug `auto_cattery_tests.exe`：通过。
- Debug DLL load smoke：通过。
- Debug 当前 7 猫只读布局探针：1 房、2 移动、2 已紧凑、6 保持原位。
- Release 聚焦构建上述目标：通过。
- Release `auto_cattery_tests.exe`：通过。
- v0.5.21 Release 聚焦构建 `AutoCattery`、`auto_cattery_tests`：通过。
- v0.5.21 Release `auto_cattery_tests.exe`：通过。
- Release DLL load smoke：通过。
- Release 当前 7 猫只读布局探针与 Debug 一致。
- Release DLL 已复制到 `dist/Release` 和游戏 `mods`；只同步 DLL 与
  `description.json`，没有运行会重写职业配置的完整部署脚本。
- v0.5.21 build、dist、安装 DLL SHA-256 一致：
  `AE1618B39D5D5BE0DDC20EAB02E7085B482B4607C93409373559633DF5B784C2`。
- `tools/verify_install.ps1`：通过；安装结构与 x64 DLL 有效，14 个职业重投次数
  保持用户当前值 20。

## 玩家验证步骤

1. 完全退出游戏后重新启动，进入任意受支持存档的家具摆放界面。
2. 点击右侧“开始分析”；存在安全移动时，左侧“自动放置”应变为可点击。7 猫
   测试档当前预期显示“需移动 2 件”。
3. 点击“自动放置”；应出现 `AC3900`，随后按计划逐件移动。7 猫测试档预期移动
   猫爬架 A 与煤渣砖，最后显示“自动放置完成”。
4. 保存、完全退出并重新进入，确认两件家具位置保持。
5. 再点击“开始分析”；稳定布局应显示无需移动或只报告仍被原生校验拒绝的剩余项。

失败时返回 `AC3901`–`AC3905` 日志和家具界面截图即可。

## 剩余范围

- 尚未按繁育、战斗、育幼、恢复等房间用途选择家具。
- 尚未把仓库家具放入房间，也未实现跨房间分配。
- 尚未移动墙面、天花板、Background、Anchor 或共享原点的挂接家具。
- 尚未改变旋转/scale，也未实现完整反向计划和一次性撤销。
- 当前完成度估计约 60%；剩余约 40% 集中在用途评分与家具分配、跨房间/仓库
  执行、完整预览/撤销和 1–5 房玩家验收。

## 交付记录

- 本地 commit：最终差异审查后创建；哈希由最终回复记录。
- 是否 push：否

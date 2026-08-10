# Stage 40 - 空目标跨房间紧凑布局与逐房间锁定

日期：2026-08-10
版本：v0.5.27
状态：Debug/Release 构建、单元测试、DLL smoke、7/24 猫真实存档只读探针、
DLL-only 部署与安装校验已完成；等待玩家实机验证跨房间动画、保存和重进。

## 完成功能

- 运行时枚举全部 `FurnitureGrid`，即使房间当前没有家具，也能读取 room、width、
  height 并作为布局目标。
- `FurnitureLayoutMove` 记录来源房间与目标房间；原生 gateway 使用目标 grid 执行
  `remove -> validate(piece,target_grid,false) -> commit(piece,target_grid)`。
- 自动布局使用 strict target，只提交计划 room/x/y，禁止 closest-valid fallback；
  native 读回房间或坐标不一致即恢复或停止。
- 目标布局从空基础碰撞网格重新求解，不再把当前手摆位置当作保留候选。目标函数
  依次比较放入数量、包围盒面积、最大边/周长、地板 Support 数和稳定坐标/key。
- 拆装模拟先移走上层依赖，再移动底座，最后安装；临时位置可使用任一未锁定且
  已验证房间，但不能占用任何最终 Solid/Support 目标。
- 分析完成后，点击自动放置会重新捕获整屋 binding digest；任一家具在分析后发生
  变化都会使计划失效。
- 成功完成一个房间后将其加入当前家具界面会话锁。下一次分析不再把该房间作为
  家具来源、目标或临时缓冲区；离开家具界面或切换 scene generation 后清空锁。
- 目标房间无法容纳全部候选时，未选家具明确计入 deferred，留给下一批；墙面、
  天花板、未知 tile 与不可证明合法的家具固定不动。

## 设计审查结论

- 独立 `gpt-5.6-sol` max 审查全程只读，没有修改工作区文件。
- 审查指出并已修复：旧 `room_id` 接口残留、自动执行仍走非严格同房间路径、最大
  房间失败不尝试次选、无空间家具可能泄漏部分方案、临时点只搜索目标房间、分析
  后未整体复核家具快照。
- 求解器是有界、确定性的 best-found，不声称对任意大规模家具集合证明全局数学
  最优；只有完整几何与完整拆装模拟成功的候选才可执行。

## 聚焦测试

- 相同家具、不同初始坐标收敛到相同空目标终局。
- 空大阁楼可成为目标并从两个其他房间收集家具。
- 容量不足时只延后未选家具，不泄漏失败候选临时 moves。
- 成功房间加入锁后，下一批 moves 不包含锁定房间内的任何家具。
- 跨房间 move 序列逐步核对 from room/x/y，并最终写入 target room/x/y。
- 自动执行调用 strict target gateway；旧同房间 Debug 探针仍保留 closest-valid
  行为，不影响自动布局。

## 构建与测试

- Debug 与 Release 在同一构建阶段并发启动，聚焦目标：`AutoCattery`、
  `auto_cattery_tests`、`furniture_geometry_probe`、`dll_smoke_tests`。
- 两配置构建成功，stderr 均为空。
- Debug/Release `auto_cattery_tests.exe`：串行运行均通过；并发运行会因共享输入/时序
  状态互扰，因此测试保持串行，构建仍按要求同阶段并发。
- Debug/Release `dll_smoke_tests.exe`：连续三次加载、初始化、关闭、卸载均通过。
- `git diff --check`：通过，仅有仓库既有 LF/CRLF 提示。

## 真实存档只读探针

- 24 猫 day 32，运行时房间 `Floor1_Large=18x9`、`Attic=37x11`：
  Debug/Release 一致选择 `Attic`，20 件可考虑，30 个执行步骤，2 件固定保留，
  0 件延后，1 件不支持，`evacuation_blocked=0`、`installation_blocked=0`。
- 7 猫 day 17，运行时房间 `Floor1_Large=18x9`、`Attic=20x7`：
  Debug/Release 一致选择面积更大的 `Floor1_Large`，10 件可考虑，15 个执行步骤，
  2 件已在终局，0 延后，0 不支持，两个阻塞计数均为 0。
- 主存档 day 265 仅运行 Release 只读探针：257 件家具、142 件已放置、115 件仓库；
  当前有界求解未生成完整可执行批次，因此返回 0 moves。未执行移动、未写存档，
  也未把临时或部分方案发布给 UI。

## 部署与安装校验

- 游戏进程未运行时，仅复制 Release `AutoCattery.dll` 到 `dist/Release` 与实际加载
  位置 `Mods/AutoCattery.dll`；仅同步 `description.json`，未运行完整部署脚本，
  未改写用户职业配置。
- build、dist、安装 DLL SHA-256 一致：
  `5D38D685BBD3AAB07773CCD17C3D037710010F346F01B6EDB137AA56B2922C00`。
- `tools/verify_install.ps1 -GameRoot ...`：通过；DLL 结构、Mewtator data mod、x64
  架构有效，14 个职业当前重投次数保持 20。

## 文件

- 求解/分析：`include/auto_cattery/furniture_planning/layout_solver.hpp`、
  `src/furniture_planning/layout_solver.cpp`、
  `include/auto_cattery/furniture_analysis/domain.hpp`、
  `include/auto_cattery/furniture_analysis/service.hpp`、
  `src/furniture_analysis/service.cpp`。
- 原生 grid/gateway/UI：`src/ui/mew_ui_furniture_move_adapter.[ch]`、
  `src/ui/furniture_placement_gateway.[ch]pp`、`src/ui/runtime_house_state.hpp`、
  `src/ui/runtime_matched_save_snapshot_adapter.cpp`、
  `include/auto_cattery/ui/mew_ui_bridge.hpp`、`src/ui/mew_ui_bridge.cpp`、
  `src/ui/house_button_controller.cpp`。
- 测试：`tests/furniture_layout_solver_tests.cpp`、
  `tests/furniture_geometry_probe.cpp`、`tests/mew_ui_furniture_placement_tests.cpp`。
- 任务、版本、状态与文档：`CODEX_TASK.md`、`CMakeLists.txt`、
  `assets/description.json`、`.auto-cattery/state.json`、
  `docs/furniture-auto-placement-design.md`、`docs/implementation-status.md`。

## 风险与玩家验证

- 24 猫首次实机验证应确认家具确实进入大阁楼、没有落到邻近替代坐标，并在保存、
  完全退出和重进后保持。
- 完成阁楼后再次点击开始分析，应只分析剩余未锁定房间；日志和画面都不应出现从
  阁楼搬出家具。
- 主存档当前仍失败关闭，不应测试自动放置。
- 本阶段不从仓库取家具，不改变旋转/scale，不执行猫移动或日期推进。

本地提交：本报告与实现位于同一本地任务提交，最终 hash 见交付回复。
是否 push：否

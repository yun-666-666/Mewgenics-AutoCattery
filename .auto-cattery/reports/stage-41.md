# Stage 41 - 原生合法阁楼叠放与批次逆序回滚

日期：2026-08-10
版本：v0.5.28
状态：Debug/Release 构建、单元测试、DLL smoke、7/24 猫与主存档只读探针、
DLL-only 部署和安装校验已完成；等待玩家实机复测自动放置、回滚、保存和重进。

## 玩家问题与根因

- v0.5.27 的 24 猫执行不是正常完成：第 16 个提交后，游戏原生校验拒绝把
  `set_spider_dresser` 放到 `(5,-9)`，因为其 Hitbox 会覆盖
  `set_modern_couch` 的 Solid/Surface。
- 截图中的楼下横排是尚未完成的临时撤离区，不是求解器决定把家具最终分配到
  下层房间；执行在进入上层安装步骤前已经停止。
- 旧占用模型只约束 Solid/Support，错误地把家具腿或可点击区域之间的视觉空隙
  当作整件家具可嵌入空间，也没有在失败后恢复本批已成功提交的跨房间移动。

## 本阶段实现

- 布局占用同时记录 Hitbox、Solid、Support、Surface。
- Hitbox 不得覆盖另一件家具的 Solid/Surface；Solid/Surface 不得覆盖已有
  Hitbox、Solid 或 Surface。
- Support 必须唯一连接房间地面、家具 Solid 或家具 Surface；真正的承重点仍可
  用于向上叠放，不把家具上方合法孔位一概禁用。
- 紧凑包围盒包含 Hitbox 与 Surface，布局评分不再只看到 Solid/Support。
- 新增按真实 couch/dresser 网格形状构造的确定性回归，要求两件家具都进入阁楼，
  dresser 不能再嵌进 couch，且必须形成合法的上下层位置。
- 自动执行记录实际成功提交的 move 索引。后续步骤失败且场景仍安全时，按这些
  索引严格逆序恢复；场景已失效时不盲目调用原生移动。
- 回滚单独记录 `AC3907`，报告恢复数量/总提交数量；成功完成整批后清空跟踪。

## 构建与自动验证

- 原并发 Debug CMake PID 20880、Release CMake PID 22304 及约 19:47 启动的
  MSBuild 子进程均已结束；两份 stdout 均完成 `AutoCattery`、
  `auto_cattery_tests`、`furniture_geometry_probe`、`dll_smoke_tests`，stderr 为空。
- 构建监控 `monitor-autocattery-stage-41-build` 已在成功确认后删除，没有启动重复
  构建。
- 串行运行：
  - `build\Debug\auto_cattery_tests.exe`：通过。
  - `build\Release\auto_cattery_tests.exe`：通过。
  - `build\Debug\dll_smoke_tests.exe build\out\Debug\AutoCattery.dll`：通过。
  - `build\Release\dll_smoke_tests.exe build\out\Release\AutoCattery.dll`：通过。

## 真实存档只读探针

探针实际使用 `furniture_geometry_probe.exe <game-root> <save> <runtime-room-grids>`；
以下存档路径和账号目录不写入报告，全部以 SQLite read-only 打开，未执行家具移动。

- 7 猫当前存档，`Floor1_Large=18x9;Attic=20x7`：Debug/Release 一致选择
  `Floor1_Large`，`considered=10`、`moves=0`、`kept=10`、`unsupported=0`，三个
  blocked 计数均为 0。已完成的紧凑布局不会被新碰撞规则打散。
- 24 猫失败前副本，`Floor1_Large=18x9;Attic=37x11`：Debug/Release 一致选择
  `Attic`，`considered=20`、`moves=24`、`kept=2`、`deferred=0`、
  `unsupported=1`，三个 blocked 计数均为 0。
  - couch 最终目标 `(3,-9)`。
  - dresser 最终目标 `(10,-8)`，不再与 couch 同层重叠。
  - 一张床最终目标 `(3,-7)`，证明求解结果包含上层家具，而非只做横排。
- 24 猫当前部分执行存档：Debug/Release 一致仍选择 `Attic`，`considered=18`、
  `moves=27`、`kept=2`、`unsupported=3`，三个 blocked 计数均为 0；楼下临时家具
  出现在返回阁楼的最终安装步骤中，没有被当成已完成房间家具永久排除。
- 主存档只运行 Release 只读探针：`considered=91`、`moves=0`、`warehouse=115`，
  没有执行、保存或写入主存档。

## 部署与安装校验

- 部署前确认 `Mewgenics.exe` 未运行。
- 仅复制 `build\out\Release\AutoCattery.dll` 到 `dist\Release` 与游戏实际加载位置
  `Mods\AutoCattery.dll`，并同步 Mewtator data mod 的 `description.json`；没有运行
  会重写职业数据或用户配置的完整部署脚本。
- build、dist、安装 DLL SHA-256 一致：
  `47EB142996C6EF359555C8C34D757A12281D2629E5FCBFCF66D4731A95234D8F`。
- `tools\verify_install.ps1 -GameRoot ..`：通过；DLL PE/x64、Mewjector 文件、
  Mewtator data mod 和 14 个职业当前 20 次重投配置均有效。

## 文件

- 求解器：`src/furniture_planning/layout_solver.cpp`。
- 执行与回滚：`include/auto_cattery/ui/mew_ui_bridge.hpp`、
  `src/ui/mew_ui_bridge.cpp`。
- 回归与探针：`tests/furniture_layout_solver_tests.cpp`、
  `tests/furniture_geometry_probe.cpp`。
- 版本、任务与文档：`CMakeLists.txt`、`assets/description.json`、
  `CODEX_TASK.md`、`docs/furniture-auto-placement-design.md`、
  `docs/implementation-status.md`、`.auto-cattery/state.json`。

## 风险、玩家验收与阶段边界

- 自动探针证明新终局满足当前解析出的碰撞规则，但最终仍以游戏原生逐步校验为准。
- 玩家应先在 24 猫测试存档复测：阁楼优先、出现多层叠放、整批完成后楼下不残留
  临时家具、保存并完全退出重进后位置保持。
- 若任一步仍被原生拒绝，应确认日志出现 `AC3907`，且已提交家具按逆序恢复；把该
  段日志和失败前后截图交回即可继续定位。
- 本阶段不从仓库取家具，不旋转或缩放家具，不自动移动猫、休息、结束一天、出征、
  组队或淘汰，也不直接写 `.sav`。

本地提交：本报告与实现位于同一本地任务提交，最终 hash 见交付回复。
是否 push：否

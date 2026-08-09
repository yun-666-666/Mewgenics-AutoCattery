# Stage 39：同房间多家具自动布局与原生执行

更新日期：2026-08-10

状态：v0.5.24 整体重排、叠放重新组合和安全拆装已完成 Release 聚焦构建、
单元测试、当前真实存档只读探针与部署；等待玩家执行、保存和重进验证。

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

## 原生目标回退（v0.5.22）

- 玩家复测出现 `AC3900`，随后 `AC3905` 报告第一件 `object_cattree1` 的规划坐标
  被原生校验拒绝，证明按钮链路已经修复，剩余问题是纯布局模型不能完整替代游戏
  的动态放置规则。
- 执行器现在只移除家具一次：先验证规划坐标；若被拒绝，则从该目标沿原位置方向
  生成稳定候选，并同时测试路径单元的上下左右相邻格，提交第一个由游戏原生验证
  接受的坐标。所有候选均被拒绝时才恢复原位置并停止。
- 候选搜索不使用固定存档、家具 item、实例 key 或房间 ID；原生校验继续作为最终
  碰撞与合法性裁决。
- 后续房间用途规划采用严格字典序：特殊安全硬限制之后，先最大化合法放入的家具
  数量；只有数量相同才比较属性组合收益。

## 实时重分析与叠放保护（v0.5.23）

- 玩家实测确认 v0.5.22 已能移动家具，但移动 `object_cinderblock1` 时会留下其上方
  小家具；该小家具随后与玩家选中的其他家具共用选择和移动轨迹。根因是布局器只
  检查 Hitbox/Solid 占用，没有把 Surface/Support 上的重叠家具视为依赖组合。
- 现在任意两个家具的非空 placement tile 在当前房间坐标相交时，两者都不进入
  Stage 39 可移动集合。在 Anchor、Support、Surface 依赖执行完整实现前，底座与
  上层小家具不会再被拆开移动。
- 玩家同一家具界面内手动移动后再次分析仍得到首次结果，日志出现旧 `from` 坐标、
  `already_placed` 或执行前坐标变化。根因是分析只覆盖了实时猫和房间，家具仍读取
  磁盘存档坐标。
- 每次“开始分析”现在重新枚举当前 House 的 FurniturePiece，以稳定 key 和 item
  匹配所选存档，并覆盖 room、saved x/y、scale。重复 key、item 不符、枚举截断或
  当前已放置家具覆盖不完整时失败关闭，不生成可执行旧计划。
- 该逻辑不包含任何特定存档、家具 key、家具 item 或固定坐标；512 件上限仅是防止
  场景枚举失控的安全边界，正常结果仍按当前运行时实例逐一匹配。

## 整体重排与安全重新叠放（v0.5.24）

- v0.5.23 的“相交组合保持原位”只作为临时防损坏措施；已有叠放不再被视为最优
  或受保护布局。求解器现在会清空全部可移动家具的旧占用后，从房间与家具锚点
  重新组合目标布局，因此相同家具集合不再因初始位置不同陷入不同局部结果。
- 当前资源与公开家具编辑器说明交叉验证：Hitbox 只提供可点击区域，不阻挡家具；
  Solid 占用格同时可作为连接锚点；每个 Support 格必须唯一连接房间值 2 锚点或
  另一件家具的 Solid 格。现有桌面、小家具、猫爬架组合按该依赖关系参与求解。
- 严格目标顺序为：先保证全部碰撞、Support 数量和锚点合法；再最大化放入家具
  数量；数量相同时优先减少房间基础锚点占用并压缩包围空间；同房间家具属性总和
  不随坐标改变，属性组合收益保留给后续跨房间分配层作为下一层比较条件。
- 执行计划先按当前依赖从上层到下层拆开需要重排的组合，把家具放到原生合法的
  房间临时锚点；随后按目标依赖从底座到上层重新安装。临时位置不足或最终依赖
  无法按合法顺序完成时，整个房间不生成部分执行计划。

## 布局算法

- 纯 `FurnitureLayoutSolver` 不调用写 adapter；每个房间独立求解。
- Solid 唯一占用格并提供家具锚点；Support 唯一消费房间或家具锚点；Hitbox 不参与
  占用。只含 Hitbox 的海报类家具保留当前合法位置且不挤占实体容量。
- 对多种稳定家具顺序执行有界 beam 搜索，所有待移动家具不再以旧位置预占空间。
  方案先比较放入数量，再比较房间基础锚点使用量、包围面积、水平/垂直跨度和稳定
  坐标顺序。
- 目标依赖图只能引用已经放置的房间或家具锚点，因此不会生成循环叠放；执行阶段
  使用先拆后装的临时落点计划，不直接拖动仍带有上层家具的底座。
- `scale_x/scale_y=±1`、实例 key、家具网格和房间几何仍是执行前提；仓库、未知
  tile、未知 scale 和无可证明合法计划的房间失败关闭。

## 当前 7 猫存档只读结果

- day 17，`Floor1_Large`，10 件已放置家具。
- 计划房间 1；可考虑 10；执行步骤 13；已在最终位置 2；不支持 0；无空间 0。
- 13 步包含 5 件上层/底座家具的临时拆放和 8 件家具的最终重装；现有桌子、
  猫爬架、小垃圾桶、电话与图钉组合均允许重新选择更紧凑的连接关系。
- Release `furniture_geometry_probe` 对当前真实存档只读运行得到上述结果，未写存档。

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
- v0.5.22 Release 聚焦构建 `AutoCattery`、`auto_cattery_tests`：通过。
- v0.5.22 Release `auto_cattery_tests.exe`：通过；候选路径包含规划目标、向原位置
  收敛的合法回退点，且不会把当前原坐标作为伪移动目标。
- v0.5.23 Release 聚焦构建 `AutoCattery`、`auto_cattery_tests`：通过。
- v0.5.23 `auto_cattery_tests.exe`：通过；覆盖实时家具坐标重复刷新、item 身份不符、
  重复 key、已放置家具缺失，以及底座 Surface 与上层家具 Solid 相交时双方不移动。
- Release DLL load smoke：通过。
- Release 当前 7 猫只读布局探针与 Debug 一致。
- Release DLL 已复制到 `dist/Release` 和游戏 `mods`；只同步 DLL 与
  `description.json`，没有运行会重写职业配置的完整部署脚本。
- v0.5.22 build、dist、安装 DLL SHA-256 一致：
  `5B772F8A5C00B3653D8D14C02E1288C685B0B0FE33FD7624743C96DFC860B8AB`。
- v0.5.23 仅同步 Release DLL 与 `description.json`；build、dist、安装 DLL SHA-256
  一致：`CEC6F6124D2645E919F805184B6581A41991BDF24FDFA600AD7F5363EEB01365`。
- v0.5.24 Release 聚焦构建 `AutoCattery`、`auto_cattery_tests`、
  `furniture_geometry_probe`：通过；`auto_cattery_tests.exe`：通过。
- v0.5.24 当前真实存档只读探针：1 房、10 件可考虑、13 步、2 件已在最终位置、
  0 不支持、0 无空间；覆盖不同初始位置收敛、桌面小家具重新组合和先拆后装顺序。
- v0.5.24 仅部署 Release DLL 与 `description.json`；build 与安装 DLL SHA-256
  一致：`E6845FA6A307483DFEC32F36286167B243A4A1E824E2F9EEF4DBFD25CB920D82`。
- `tools/verify_install.ps1`：通过；安装结构与 x64 DLL 有效，14 个职业重投次数
  保持用户当前值 20。

## 玩家验证步骤

1. 完全退出游戏后重新启动，进入任意受支持存档的家具摆放界面。
2. 点击右侧“开始分析”；当前测试存档应显示全部 10 件家具均被考虑，并生成包含
   临时拆放与最终重装的计划，而不是把已有叠放计为不支持。
3. 点击“自动放置”；应出现 `AC3900`，随后先移开上层小家具，再移动底座，最后
   把小家具装到新的最优锚点。任何一步原生拒绝都应停止，不能留下共同选中状态。
4. 不退出家具界面，手动移动任意独立家具后再次点击“开始分析”；日志应出现
   `AC14319`，新预览必须使用该家具的当前坐标，而不是首次分析坐标。
5. 执行新预览并保存、完全退出、重新进入，确认家具位置保持且可继续重新分析。

失败时返回 `AC3901`–`AC3905` 日志和家具界面截图即可。

## 剩余范围

- 尚未按繁育、战斗、育幼、恢复等房间用途选择家具。
- 尚未把仓库家具放入房间，也未实现跨房间分配。
- Support 位于上方或侧面的家具已进入同一通用房间锚点模型，但吊灯、墙面等方向
  仍等待玩家实机样本验证；只含 Hitbox 的 Background/海报保持原位。
- 尚未改变旋转/scale，也未实现完整反向计划和一次性撤销。
- 当前完成度估计约 60%；剩余约 40% 集中在用途评分与家具分配、跨房间/仓库
  执行、完整预览/撤销和 1–5 房玩家验收。

## 交付记录

- 本地 commit：本次提交；最终哈希由最终回复记录。
- 是否 push：否

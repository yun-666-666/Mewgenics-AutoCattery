# Stage 39：同房间多家具自动布局与原生执行

更新日期：2026-08-09

状态：实现、聚焦构建与单元测试完成。v0.5.21 玩家复测确认点击已经进入执行，
但首个理论目标被原生校验拒绝；v0.5.22 已加入原生合法坐标回退并部署，等待玩家
重新执行、保存和重进验证。

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
- `tools/verify_install.ps1`：通过；安装结构与 x64 DLL 有效，14 个职业重投次数
  保持用户当前值 20。

## 玩家验证步骤

1. 完全退出游戏后重新启动，进入任意受支持存档的家具摆放界面。
2. 点击右侧“开始分析”；存在安全移动时，左侧“自动放置”应变为可点击。与上层
   小家具网格相交的煤渣砖底座不应再进入移动计划。
3. 点击“自动放置”；应出现 `AC3900`，随后只移动独立家具。底座及其上方小家具均
   应保持原位，小家具不应再跟随之后选中的其他家具。
4. 不退出家具界面，手动移动任意独立家具后再次点击“开始分析”；日志应出现
   `AC14319`，新预览必须使用该家具的当前坐标，而不是首次分析坐标。
5. 执行新预览并保存、完全退出、重新进入，确认家具位置保持且可继续重新分析。

失败时返回 `AC3901`–`AC3905` 日志和家具界面截图即可。

## 剩余范围

- 尚未按繁育、战斗、育幼、恢复等房间用途选择家具。
- 尚未把仓库家具放入房间，也未实现跨房间分配。
- 尚未移动墙面、天花板、Background、Anchor 或共享原点的挂接家具。
- 尚未改变旋转/scale，也未实现完整反向计划和一次性撤销。
- 当前完成度估计约 60%；剩余约 40% 集中在用途评分与家具分配、跨房间/仓库
  执行、完整预览/撤销和 1–5 房玩家验收。

## 交付记录

- 本地 commit：本次提交；最终哈希由最终回复记录。
- 是否 push：否

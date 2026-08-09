# Stage 38：单件家具同房间原生移动

更新日期：2026-08-09

状态：实现、Debug/Release 聚焦构建、单元测试、DLL load smoke、Release 部署、
哈希和安装校验完成；玩家 F8 画面移动与保存后重进测试已验收。

## 玩家输入与本阶段决策

- 已读取玩家生成的三份 F7 报告：`203425`、`203535`、`203545`。
- 三份报告中 Furniture UI 与 House scene 根均达到 96 节点上限；变化对象分别为
  `65/59`、`59/49`、`42/32`。
- 广泛对象图被猫、物理、UI、ragdoll 和音频占满，没有直接采到
  `FurniturePiece`。当前 scene 组件可以直接枚举，7 猫运行时已有 10 个家具组件，
  因此停止扩展探针并直接实现原生移动。
- 玩家已经证明 `object_cattree1` 的 `(-6,-7)` 与 `(3,-9)` 都是同一房间内的
  合法位置，且手动移动只改变 saved x/y。

## 当前 build 原生证据

- `FurniturePiece` vtable RVA：`0xEDE690`。
- `FurnitureGrid` vtable RVA：`0xEF4C20`。
- 原生移除：`0x2EE3D0`。
- 原生合法性校验：`0x2EDE60(piece, grid, false)`，返回 `AL`。
- 原生提交：`0x2EE230(piece, grid)`；内部写 grid 占用、entry room、saved x/y
  和 scale。
- 目标 transform 使用当前 build 加载公式：
  `grid_world + saved + ceil(11.5 * scale)`，z 为 0。

## 实现

- 新增 build-specific C adapter：
  - 枚举 House scene 组件并只接受当前 vtable 的 `FurniturePiece`。
  - 读取 transform、当前 grid、entry、稳定 key、item、room、saved x/y 与 scale。
  - 以原生函数开头字节校验当前 build 后，执行移除、校验和提交。
  - 独立读回当前 grid 与 entry saved x/y；拒绝、异常或读回失败时恢复旧
    transform 并重新提交旧 grid。
- 新增通用 C++ `FurniturePlacementGateway`：按 item 与可选稳定 key 定位实例，
  接受明确的目标 saved x/y，不写死具体家具或测试坐标。
- F8 玩家测试入口定位 `object_cattree1`，优先稳定 key 5，在两个玩家已验证坐标
  间切换。成功、拒绝、异常和回滚结果使用 `AC3800`–`AC3802` 写日志并显示在
  家具界面。
- 原 debug F8 scene summary 改为 Shift+F8，避免与玩家移动测试冲突。
- MOD 版本更新为 `0.5.19`。

## 文件

- 原生 adapter：`src/ui/mew_ui_furniture_move_adapter.h`、
  `src/ui/mew_ui_furniture_move_adapter.c`。
- Gateway：`src/ui/furniture_placement_gateway.hpp`、
  `src/ui/furniture_placement_gateway.cpp`。
- UI 集成：`include/auto_cattery/ui/mew_ui_bridge.hpp`、
  `src/ui/mew_ui_bridge.cpp`。
- 测试：`tests/mew_ui_furniture_placement_tests.cpp`、`tests/test_main.cpp`、
  `CMakeLists.txt`。
- 状态与版本：`CODEX_TASK.md`、`docs/furniture-auto-placement-design.md`、
  `docs/implementation-status.md`、`.auto-cattery/state.json`、
  `assets/description.json` 和本报告。

## 构建与检查

- `git diff --check`：通过，仅有既有 LF/CRLF 提示。
- Debug 聚焦构建 `auto_cattery_tests` 与 `AutoCattery`：通过。
- Debug `auto_cattery_tests.exe`：通过。
- Debug `phase14_dll_load_smoke`：通过。
- Release 聚焦构建 `auto_cattery_tests` 与 `AutoCattery`：通过。
- Release `auto_cattery_tests.exe`：通过。
- Release `phase14_dll_load_smoke`：通过。
- Release DLL 已复制到 `dist/Release` 和游戏 `mods`；未运行会重写配置/职业数据
  的完整部署脚本。
- build、dist、安装 DLL SHA-256 一致：
  `5A220DF099091329FD48AFF2A331938FCCA3756604BB9C1BF93AD1C47C0B1194`。
- `tools/verify_install.ps1`：通过；安装结构与 x64 DLL 有效，14 个职业重投次数
  保持用户当前值 20。

## 玩家验证

- 玩家确认已完成本阶段测试。
- 返回截图显示 7 猫家具界面的猫爬架已由原位置移动到右侧目标网格，F8 原生移动
  的玩家可见结果成立。
- 按既定步骤完成保存、退出和重进测试，本阶段玩家验收通过。

## 剩余风险与后续范围

- 本阶段只处理已放置家具的同房间单件移动；跨房间 grid 选择、仓库家具、Anchor、
  Background、门口/斜顶/墙面/天花板完整合法性、用途评分、布局优化、批量预览与
  执行仍未完成。
- 正式“自动放置”按钮继续禁用，不直接写 `.sav`。

## 交付记录

- 本地 commit：最终差异审查后创建；哈希由最终回复记录。
- 是否 push：否

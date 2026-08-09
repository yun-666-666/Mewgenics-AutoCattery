# CODEX CURRENT TASK - STAGE 38 SINGLE FURNITURE NATIVE MOVEMENT

## Current objective

优先打通玩家可见的家具原生移动。读取玩家已经完成的三份 F7 报告和当前 build
静态证据，枚举 House scene 中的 `FurniturePiece`，定位 7 猫存档的
`object_cattree1`，通过游戏自己的移除、合法性校验和提交函数移动同一房间内的
一件家具。F8 是本阶段玩家测试入口；通用 gateway 接受明确的目标存档坐标，供
后续布局规划器复用。

## Player evidence accepted

- 玩家已生成三份报告：
  - `furniture-move-probe-20260809-203425.json`
  - `furniture-move-probe-20260809-203535.json`
  - `furniture-move-probe-20260809-203545.json`
- 三份报告的 UI/House 根均达到 96 节点上限；变化对象分别为
  `65/59`、`59/49`、`42/32`。广泛对象图被猫、物理、UI、音频和 ragdoll
  占满，因此未直接包含 `FurniturePiece`。
- 当前 scene 组件可直接枚举，7 猫运行时存在 10 个 `FurniturePiece`，所以对象图
  上限不再阻塞原生移动。
- 玩家手动移动同一 `object_cattree1` 时只改变 `x/y`，两个已验证坐标为
  `(-6,-7)` 与 `(3,-9)`；实例 key、item、room、z、flags、scale 和其他 9 件
  家具均不变。

## Confirmed current-build route

- `FurniturePiece` vtable RVA：`0xEDE690`。
- `FurnitureGrid` vtable RVA：`0xEF4C20`。
- `FurniturePiece+0x38`：transform；`+0x48`：当前 grid；`+0x2D8`：entry。
- entry：key `+0x00`、item `+0x08`、room `+0x30`、saved x/y `+0x50/+0x54`。
- transform：x/y/z `+0x80/+0x88/+0x90`、scale x/y `+0xB8/+0xC0`。
- 原生移除 RVA `0x2EE3D0`；合法性校验 RVA `0x2EDE60`；提交 RVA
  `0x2EE230`。
- 当前 build 加载坐标：
  `world = grid_world + saved + ceil(11.5 * scale)`，z 为 0。

## Stage 38 completion boundary

- 新增通用同房间 `FurniturePlacementGateway`：按 item 和可选稳定 key 定位实例，
  接受明确目标 saved x/y。
- 移动顺序固定为：快照旧状态 -> 原生移除 -> 写目标 transform -> 原生校验
  (`false`) -> 原生提交 -> 读回 grid 与 saved x/y。
- 校验拒绝、原生异常或提交读回失败时，恢复旧 transform 并重新提交旧 grid；
  F8 结果和回滚状态写入日志并显示在家具界面。
- 7 猫测试入口优先 key 5，在 `(-6,-7)` 与 `(3,-9)` 两个玩家已验证坐标间切换。
- Debug/Release 构建和聚焦单元测试通过；Release DLL 部署并校验；创建一个本地
  commit，不 push。

## Out of scope

- 本阶段不实现家具用途评分、跨房间分配、合法布局搜索或批量执行。
- 不直接写 `.sav`，不修改游戏原始文件，不实现家具旋转。
- 不自动结束一天、休息、出征、组队或淘汰猫。

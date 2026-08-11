# CODEX CURRENT TASK - STAGE 44 WAREHOUSE TAKE/PLACE EVIDENCE PROBE

## Current objective

在 Stage 43 已能显示家具五属性升级候选后，继续定位当前 build 从仓库抽屉取出家具并
放入房间的真实原生路径。先把公开的 107% 完成存档替换到原 8 猫测试槽，保留单份
可恢复备份；再把 F7 改为专用只读探针，记录玩家手动完成一次“仓库 -> 房间”操作
前后的家具实例和相关原生对象变化，为下一阶段实现可预览、可取消、可回滚的仓库
属性升级替换提供当前 build 证据。

## Accepted runtime and local evidence

- 玩家已确认 Stage 43 能成功显示 45 个属性升级候选；第五房间仍未解锁，玩家明确
  要求不再把该问题作为当前验收项。
- 主存档运行时只枚举到 142 个已摆 `FurniturePiece`；115 件仓库家具没有对应的
  grid-null scene piece，因此不能把现有 placed-piece move API 直接套到仓库实例。
- 当前可执行文件包含 `HouseInventory`、`FurnitureEditor`、
  `FurnitureClickHandler`、`FurniturePiece_PickedUpFromDrawer` 和
  `FurniturePiece_PlacedInRoom` 等当前 build 类型/事件名，但没有可靠函数签名或调用
  约定证据，不能仅凭字符串直接调用。
- 第三槽原测试档当前只读快照为 7 猫、2 房、day 17；它是此前 8 猫测试槽的后续
  状态。公开 107% 存档已通过 Defender、SQLite 完整性、项目 save-format 和 snapshot
  探针检查后替换该槽；主存档和第二槽未修改。

## Stage 44 completion boundary

- 第三槽替换前只创建一份备份，备份 hash 与原档一致；替换后目标 hash 与下载并
  解压的源存档一致，`PRAGMA integrity_check=ok`，不存在 WAL/SHM。
- F7 第一次采集必须记录当前 scene `FurniturePiece` 集合及
  `FurnitureBuildingUI`、`HouseInventory`、`FurnitureEditor`、
  `FurnitureClickHandler`、House scene manager 的有界对象图。
- 玩家手动从仓库抽屉取出一件家具、放进任一房间后第二次按 F7；报告必须列出前后
  scene piece 数量、完整性、出现/消失/位置变化的 stable key、item、room、坐标及
  grid 状态，同时保存上述五个根对象的差异。
- 报告不保存原始内存字节，不读取账号凭据，不调用任何未知仓库函数，不自动取出、
  放置或替换家具。
- 版本升级为 v0.5.31；Debug/Release 构建和现有 CTest 通过后 DLL-only 部署并验证
  安装；不 push。最终原生路径仍由玩家完成一次 F7 手动取放采样后继续验证。

## Player validation result

- 玩家生成了两份 schema 2 报告：`furniture-move-probe-20260811-183525.json` 和
  `furniture-move-probe-20260811-183547.json`；日志确认每份报告均来自一次完整的
  F7 前态采集、手动仓库取放和 F7 后态采集。
- 第一次操作的 scene `FurniturePiece` 完整枚举从 111 增至 112，唯一新增项为
  `set_junk_suspendedshelf`，stable key 474，房间 `Floor2_Large`，保存坐标
  `(-8,-6)`，grid 存在。
- 第二次操作的完整枚举从 112 增至 113，唯一新增项为 `small_bobble_spots`，
  stable key 467，房间 `Floor2_Large`，保存坐标 `(-5,-11)`，grid 存在。
- 两次采样都没有已摆 scene piece 消失或改变位置。`FurnitureBuildingUI`、
  `FurnitureClickHandler` 和 House scene manager 均记录到有界对象图差异；当前 build
  未找到独立的 `HouseInventory` 和 `FurnitureEditor` 组件，因此对应根以 0 节点明确
  记录，而不是伪造对象或签名。
- 当前证据证明手动从仓库取出时会创建带原存档 stable key 的新 scene piece，但仍未
  证明可安全调用的仓库取出函数签名或调用约定。Stage 44 只读证据探针验收完成；自动
  仓库取放和属性替换留给后续独立阶段。

## Safety boundary

- 不自动启动、进入或控制游戏；需要实机验证时只给玩家清晰测试步骤。
- 不自动移动猫、休息、结束一天、出征、组队或淘汰。
- 仓库原生取放路径未由当前 build 的真实采样证明前，不调用未知函数，不把属性候选
  加入可执行 move 批次，也不声明已经自动替换仓库家具。
- 不修改主存档和第二槽；第三槽旧测试档备份必须保留在原位。

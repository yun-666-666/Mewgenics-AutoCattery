# Stage 44 - 仓库家具手动取放证据探针

日期：2026-08-11
版本：v0.5.31
状态：实现、Debug/Release 自动验证、完成存档替换、Release 部署、安装校验和玩家两次
F7 手动仓库取放采样均已完成；本阶段验收通过。

## 本阶段目标与结论

- Stage 43 已由玩家确认能显示 45 个家具五属性升级候选；第五房间仍未解锁的问题按
  玩家要求移出当前范围，不再作为 Stage 44 验收项。
- 旧证据只看到已摆 `FurniturePiece`，没有看到仓库实例，不能把已验证的场景家具移动
  API 直接套到仓库家具。
- Stage 44 将 F7 改为专用只读探针，记录玩家手动完成一次“仓库抽屉 -> 房间”操作
  前后的 scene 家具集合和相关原生对象图。
- 玩家完成了两次独立采样。两次结果都显示 scene 家具完整枚举精确增加 1，并出现一件
  带存档 stable key、目标房间、保存坐标和有效 grid 的新 `FurniturePiece`。
- 该证据证明手动仓库取出会创建新的 scene piece，但没有证明可安全调用的仓库取出
  函数签名或调用约定。因此本阶段不实现自动仓库取放或属性替换。

## 公开完成存档与恢复点

- 来源：Nexus Mods “107% 完成存档”页面
  `https://www.nexusmods.com/mewgenics/mods/140`，通过 3DM 搬运页取得公开下载文件。
- 下载 ZIP SHA-256：
  `380172573D6B487AAA3E24095873ED57116F479CD83C1A6BF52546CF85923186`。
- 解压源存档 SHA-256：
  `E9B5C7B0F721B57CD19AACE4AE5346D35555A9BCBBC3D3251A2D03ECB3B007FC`。
- Microsoft Defender 自定义扫描未发现威胁；源档通过 SQLite、save-format 和 snapshot
  只读检查。
- 替换目标为第三槽（原 8 猫测试槽的后续状态）：
  `%APPDATA%\Glaiel Games\Mewgenics\<SteamID>\saves\steamcampaign03.sav`。
- 替换前只创建一份备份：
  `%APPDATA%\Glaiel Games\Mewgenics\<SteamID>\saves\backups\autocattery-pre-full-unlock-slot3-20260811-174355\steamcampaign03.sav.bak`。
- 原档及备份 SHA-256：
  `3C308D8EC72423F3CB2274141A2AEAEDC18A6D550E37E58DE8AC98C0B560B270`。
- 替换当时目标 hash 与解压源存档一致，`PRAGMA integrity_check=ok`，无 WAL/SHM；
  `house_storage_upgrades=7`，Future、The End、End of Time、Moon、Dimension X 和
  Butch Box 等完成字段均为 1。
- 玩家完成两次家具取放并保存后，当前第三槽 SHA-256 为
  `58246853CA1E0E6058C8C619CB7A467A4E7A3B0EA7A50AC771F23B4F78B709D7`；再次只读检查
  `PRAGMA integrity_check=ok`，无 WAL/SHM。唯一备份仍在原位且 hash 不变。

## 实现

- 第一次 F7 记录当前 scene `FurniturePiece` 集合，以及 `FurnitureBuildingUI`、
  `HouseInventory`、`FurnitureEditor`、`FurnitureClickHandler` 和 House scene manager
  的有界对象图。
- 第二次 F7 输出 schema 2 报告
  `warehouse_furniture_manual_take_place_delta`，包含前后 scene 家具数量和枚举完整性、
  出现/消失/位置变化的 stable key、item、room、保存坐标及 grid 状态，并保存五个
  对象根的差异。
- 报告不保存原始内存字节，不读取账号凭据，不调用未知仓库函数，也不自动移动家具。
- UI 提示明确要求玩家从仓库抽屉取出一件家具并放入房间，避免把普通已摆家具移动
  误当作本阶段采样。

## 玩家 F7 证据

- `furniture-move-probe-20260811-183525.json`：scene 家具完整枚举 `111 -> 112`；
  唯一新增 `set_junk_suspendedshelf`，stable key 474，`Floor2_Large`，坐标
  `(-8,-6)`，grid 存在；无消失项、无位置变化项。报告 SHA-256：
  `265CB3E2A72A651003957FE304FE16FA4B9E931993717852DE681A7BF65481D0`。
- `furniture-move-probe-20260811-183547.json`：scene 家具完整枚举 `112 -> 113`；
  唯一新增 `small_bobble_spots`，stable key 467，`Floor2_Large`，坐标 `(-5,-11)`，
  grid 存在；无消失项、无位置变化项。报告 SHA-256：
  `F2932912FD045D825BD1CD323C8A0DC79714C254235C3B48F958FADFF50525F9`。
- 两次报告都在 `FurnitureBuildingUI`、`FurnitureClickHandler` 和 House scene manager
  根记录到有界对象图变化；当前 build 未发现独立 `HouseInventory`、
  `FurnitureEditor` 组件，对应报告明确记录为 0 节点。
- 日志确认 MOD v0.5.31 正常加载，四次 F7 依次完成两组前态和后态采集；没有探针
  报错。玩家随后保存并完全退出游戏。

## 文件

- 阶段与版本：`CMakeLists.txt`、`assets/description.json`、`CODEX_TASK.md`、
  `.auto-cattery/state.json`、本报告。
- 探针控制与报告：`src/ui/furniture_move_probe_controller.cpp`、
  `src/ui/furniture_move_probe_controller.hpp`、`src/ui/furniture_move_probe_report.cpp`、
  `src/ui/furniture_move_probe_report.hpp`。
- MewUI 桥接：`src/ui/mew_ui_bridge.cpp`、`src/ui/mew_ui_furniture_move_probe.h`。
- 回归：`tests/mew_ui_furniture_move_probe_tests.cpp`。

## 构建、测试与部署

- `tools\build.ps1 -Configuration Debug`：成功；Debug CTest 4/4 通过。
- `tools\build.ps1 -Configuration Release`：成功；Release CTest 4/4 通过。
- `tools\deploy.ps1 -GameRoot D:\steam\steam\steamapps\common\Mewgenics -Configuration Release`：
  DLL-only 部署成功。
- `tools\verify_install.ps1 -GameRoot D:\steam\steam\steamapps\common\Mewgenics`：通过；
  14 个职业重投配置均为 20。
- build、dist 和实际加载位置 `Mewgenics\Mods\AutoCattery.dll` 的 SHA-256 一致：
  `ED747FD65C939137382136BB57839A62BDEFC0798A2A0E3381D3B4ADCE6C53D1`。

## 风险与未做范围

- 两份真实采样只证明玩家手动取出家具后会出现新的 scene piece；对象图变化较多且
  包含 UI/分配器噪声，不能据此猜测函数入口或调用约定。
- 完成存档在玩家取放后重新分析时出现 `runtime room identity is incomplete`；这是该
  公开存档的运行时房间身份与保存快照不完整匹配问题，不影响本阶段 F7 探针结果，
  也不在本阶段扩展处理。
- 未自动取出、放置或替换仓库家具，未把 Stage 43 属性候选加入执行批次。
- 未自动移动猫、休息、结束一天、出征、组队或淘汰；未修改主存档和第二槽。

本地提交：本报告与实现位于同一本地任务提交，最终 hash 见交付回复。
是否 push：否

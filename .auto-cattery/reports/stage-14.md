# Stage 14 当前报告

更新日期：2026-07-31

状态：离线备份/恢复、复制存档写入验证和当前 build 原生 MoveOnly 自动分房均已
通过玩家实机验证；真实淘汰仍未实现。

## 离线存档安全

- 复用 backup、journal、恢复包和事务边界。
- 备份执行稳定窗口、占用检查、WAL/SHM 拒绝、SHA-256、大小和 manifest
  校验。
- 同卷临时文件写入后通过 `ReplaceFileW` 原子替换。
- 恢复要求游戏关闭，并在覆盖前再次备份当前目标。
- 恢复后独立验证文件大小、SHA-256 和存档结构。
- 不控制或改写 Steam Cloud 文件。
- `AutoCatterySaveLab.exe` 支持隔离复制存档的单猫移动、读回和恢复验证。

## 当前 build 精确门

- `Mewgenics.exe` 大小：`21,981,184` bytes。
- SHA-256：
  `C3A41E436A93FA58CD386EC46DAD5C2A6F21A583D33C3A57A15A2604C726439E`。
- 不匹配时不启用当前原生移动入口。

## 已验证存档与房间数据

- 存档容器：SQLite。
- 猫 blob：LZ4。
- 稳定 CatId：SQLite 64-bit key。
- `house_state` 记录 CatId、房间字符串和三个坐标。
- 当前确认普通房间：
  - `Floor1_Large`
  - `Floor1_Small`
  - `Floor2_Large`
  - `Attic`
- `AdventureBox` 不作为普通自动分房目标。
- `voice_id` 当前样本可区分 `female...` 与 `male...`。

## 原生 House 移动证据

通过玩家手动拖动猫、运行时差分、当前 EXE 反汇编和受控探针确认：

- `HouseCat` 对象边界：`0x118`。
- `HouseCat` vtable RVA：`0xEF4F58`。
- CatId：`HouseCat+0x80`。
- 当前房间指针：`HouseCat+0xE8`。
- 原生房间移动 RVA：`0x2E7DB0`。
- 原生函数会从旧房间移除猫、向目标房间添加猫，并更新当前房间指针。
- Adapter 调用后再次读取 `HouseCat+0xE8`，只有等于目标房间才算提交。

## 当前 MoveOnly 工作流

1. House 场景稳定后刷新运行时猫数和房间组件数。
2. 每次点击预览前再次强制刷新。
3. 扫描全部存档候选，选择猫数与当前 House 一致的快照。
4. 根据实际 2、3、4 房集合建立房间能力。
5. 所有 House 猫按潜力评分，不以战斗状态、职业或受伤状态排除。
6. 读取存档家具归属与当前游戏家具属性；均衡人数后按实际房间属性集中培养
   高潜力猫，公母足够时每房至少一公一母。
7. 预览前捕获全体实时 CatId、组件和当前房间，用实时分布覆盖未保存的
   `house_state`。
8. 第一次点击生成移动计划。
9. 第二次点击前再次捕获全体实时房间；与预览不一致时旧预览失效并零移动。
10. 状态一致时，其余猫调用原生移动函数。

## 玩家实机验证

### 8 猫存档

- 运行时：8 猫、2 房。
- 存档候选猫数：`79,25,8,9`。
- 正确选择 8 猫快照。
- 计划移动 6 只。
- 原生提交 6 只。
- 重复执行提交 0 只。

### 25 猫存档

- 运行时：25 猫、2 房。
- 同一候选集合中正确选择 25 猫快照。
- 计划移动 10 只。
- 原生提交 10 只。
- 重复执行提交 0 只。

玩家确认两份存档均已正常自动分房。

2026-08-01 全房间均衡算法实机验证：

- 8 猫两房最终 `4/4`，首次提交 2 次，重复执行提交 0 次。
- 25 猫两房最终 `13/12`，首次提交 2 次，重复执行提交 0 次。
- 玩家随后在预览与执行之间手动搬猫，日志出现计划 2 次而实际提交 0、1、2
  次，证明旧预览仍依赖未保存的存档房间分布。
- 已实现实时房间覆盖与执行前全体映射校验。Debug/Release 5/5 测试通过并
  部署，新的手动搬猫路径待玩家复测。
- 16:17 的 8 猫测试在猫位于普通房间外时连续记录 `AC14318`；根因是实时
  CatId 和组件仍有效但房间指针为 null，旧采集器把它误判为整批不完整。
- 已把 null 房间建模为未分配来源，仍要求完整 CatId/组件，并只允许规划到
  当前 build 已验证的普通房间。自动化覆盖房外猫预览、均衡计划和陈旧校验。
- 旧版 25 猫首次 `15/10 -> 13/12` 提交 4 次、随后提交 0 次；当时具体猫
  组合仍受最少移动影响，本轮属性目标已替代该行为。
- 后续零移动预览原本会在确认时记录 `AC14300`；现已改为重新校验实时状态
  后成功提交 0 次，状态变化时仍按陈旧预览取消。
- 玩家在 25 猫三房档把 `9/8/8` 手动轮换为 `8/8/9` 后，旧规划因人数与
  潜力计数相同而停止；新版已改为具体猫的稳定属性目标，不再以最少移动为
  完成条件。
- C++ 实读主存档确认四房属性不同；25 猫档实读 `Floor1_Large` 为舒适 8、
  刺激 4、健康 2、变异 1，另外两房为 0。当前 `9/8/8` 目标因此改为
  `Attic 8 / Floor1_Large 9 / Floor1_Small 8`，并把潜力前列猫放入大房。

2026-07-30 的 6/10 次移动证据对应旧的固定阁楼潜力组算法；2026-08-01 的
`4/4`、`13/12` 和各提交 2 次才是当前均衡算法的实机证据。

## 自动化验证

- Debug/Release 构建成功。
- Debug/Release CTest：均为 5/5 通过。
- 新算法场景：25 猫两房 `13/12`、8 猫两房 `4/4`、10 猫四房
  `3/3/2/2`。
- 新增 25 猫三房属性场景：首次得到属性目标、`9/8/8 -> 8/8/9` 手动轮换
  后恢复、人数不变的跨房换猫后恢复，三项自动化均通过。
- 当前游戏 `resources.gpak` 家具目录测试通过（超过 600 项）；主存档和
  25 猫档的 C++ 房间属性探针均成功，且无家具定义缺失。
- 已知公母足够时各目标房保留一公一母；性别极少时不虚构缺失性别。
- 新增属性规划实现拆分为 69、168、174、171 行的单职责 `.cpp` 文件；
  家具目录与房间汇总读取分别为 163、102 行。
- 手动移动修复拆分为实时捕获、房间解析、快照覆盖和执行门文件；本轮新增或
  重写的实现文件均不超过 139 行。
- Debug/Release build 与 dist 产物由构建脚本完成哈希校验。
- `git diff --check` 在每个修复小步后执行。
- Release DLL 已部署，安装哈希与 dist 一致：
  `134B03A30A7758C68CF3921113D1A4D83F48AC4E9E9BA90F7301E6A552D1B339`。

## 当前限制

- 当前存档选择先按猫数匹配；两个存档猫数相同时仍需 CatId 集合级匹配。
- 2 房和 3 房旧均衡版本已由玩家确认通过；本轮属性感知的 3 房版本待复测。
- 三房测试存档已通过本机 `house_unlocks` 的两房/四房差分构造并完成 SQLite
  完整性与 25 猫快照读回；仍需进入游戏确认第三空房组件和实际执行。
- 房外猫路径已有自动化证据并部署，仍需玩家确认原生函数能从 null 来源移入
  普通房间，随后重复执行为 0。
- 尚未验证移动结果经游戏正常保存、退出并重进后的持久化。
- 已读取 Comfort、Stimulation、Health、Mutation，并按人数预测拥挤后的
  舒适度；尚未在 House UI 中直接显示这些数值。
- 手动搬猫失效门已有自动化证据，仍需玩家确认 `AC14316` 零移动和下一次
  预览采用实时房间。
- `default_soft_capacity=4` 不作为当前 MoveOnly 的游戏硬容量。
- 亲缘、性向、libido 和家具阻止繁育状态仍为未知，不启用自动配对。
- 实时移动尚未完整接回白名单、固定房间和 NoMove 规则。
- 真实淘汰、繁育配对、近亲检查和淘汰撤销尚未实现。

## 实际修改范围

- 快照属性：`snapshot/domain.hpp`、`furniture_attributes.hpp`、
  `furniture_catalog.cpp`、`furniture_room_attributes.cpp`、
  `save_database.cpp`、`save_snapshot_adapter.cpp`。
- 属性规划：`balanced_move_only_context.cpp`、`preferences.cpp`、
  `targets.cpp`、`assignment.cpp`、`planner.cpp`。
- 接线与绑定：`runtime_matched_save_snapshot_adapter.*`、
  `mew_ui_bridge.cpp`、`plan_sealer.cpp`。
- 测试：`deterministic_room_assignment_tests.cpp`、
  `furniture_attributes_tests.cpp`、`room_attribute_probe.cpp`。
- 当前 build HouseCat/房间探针与原生移动 adapter。
- 运行时移动 gateway。
- 当前存档自动刷新与候选匹配 adapter。
- MoveOnly 全房间人数、公母、具体猫和房间实际属性目标规划。
- House 按钮两次点击预览/执行状态。
- 当前 build 实时 House CatId/房间捕获、预览覆盖和旧预览失效门。
- 相关单元测试、运行日志和当前状态文档。

本轮命令：

- `.\tools\build.ps1 -Configuration Debug`：5/5 通过。
- `.\tools\build.ps1 -Configuration Release`：5/5 通过。
- `.\tools\deploy.ps1 -GameRoot 'D:\steam\steam\steamapps\common\Mewgenics'`。
- `.\tools\verify_install.ps1 -GameRoot 'D:\steam\steam\steamapps\common\Mewgenics'`：通过。

本轮本地 commit：以最终回复中的提交哈希为准。

是否 push：否

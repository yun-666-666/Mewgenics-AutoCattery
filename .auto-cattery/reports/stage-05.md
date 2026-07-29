阶段：05
状态：已完成（自动化、真实存档只读验证、部署及玩家游戏内验收全部通过）

真实数据证据：
- 当前 `.sav` 是 SQLite 数据库，`cats.key` 为稳定 64 位猫 ID。
- 测试存档的 `cats` 表有 30 条原始记录，但当前 `house_state` 只有 8 个
  猫 ID；它们来自同一个存档，不是多个存档合并。
- `properties.data` 中的 `current_day` 为真实游戏日。
- 猫数据为带原始大小头的 LZ4 block，解压后 magic 为 19。
- 实际属性为 STR/DEX/CON/INT/SPD/CHA/LCK 共 7 项，包含 genetic、
  heredity bonus、equipment bonus 三组数值。
- `files.house_state` 使用 64 位猫 ID、字符串房间 ID 和三个 double 坐标。
- 当前 SDK 与真实存档共同验证 9 个能力槽；后续字段身份未证实，因此
  class、age、relationships 保持 unavailable。
- 房间容量及未出现在 `house_state` 中的猫所在房间无法可靠证明，保持
  unavailable，不按坐标或文档猜测。

实现：
- 新增不可变 `CatSnapshot`、`RoomSnapshot`、`HouseSnapshot` 和能力矩阵。
- 新增边界校验：稳定 ID、重复 ID、缺失猫引用、多房间归属、猫房间一致性。
- 新增 bounds-checked LZ4、猫 blob 和 `house_state` 解析器。
- 新增 Windows 系统 SQLite 动态接口、只读连接和只读事务。
- 旧系统 SQLite 不认识游戏 schema 的 `STRICT` 尾部；只读连接启用
  connection-local `writable_schema` 兼容解析，数据库仍以 READONLY 打开。
- 新增 `.sav` 定位器：限定 Mewgenics 存档树、排除 backup/backups、
  选择最新 `.sav`。
- 家园按钮点击时同步拍摄一次快照，输出脱敏摘要；不写存档、不移动猫、
  不评分、不规划。
- 快照只保留出现在当前 `house_state` 中的家园猫；未证实状态的原始记录
  不再计入当前猫数量。日志字段改为 `house_cats`，避免混淆。

主要文件：
- `include/auto_cattery/snapshot/domain.hpp`
- `include/auto_cattery/snapshot/game_read_adapter.hpp`
- `include/auto_cattery/snapshot/save_snapshot_adapter.hpp`
- `include/auto_cattery/snapshot/detail/*.hpp`
- `src/snapshot/*.cpp`
- `include/auto_cattery/workflow/organize_workflow_facade.hpp`
- `src/workflow/organize_workflow_facade.cpp`
- `include/auto_cattery/ui/house_button_controller.hpp`
- `src/ui/house_button_controller.cpp`
- `src/ui/mew_ui_bridge.cpp`
- `tests/*snapshot*tests.cpp`
- `tests/cat_blob_parser_tests.cpp`
- `tests/house_state_parser_tests.cpp`
- `tests/lz4_block_tests.cpp`
- `tests/save_database_tests.cpp`
- `tests/save_locator_tests.cpp`
- `tests/win_sqlite_api_tests.cpp`
- `tests/organize_workflow_facade_tests.cpp`

构建与测试：
- Debug `phase05_unit_tests`：通过。
- Debug `phase05_dll_load_smoke`：通过。
- Release `phase05_unit_tests`：通过。
- Release `phase05_dll_load_smoke`：通过。
- DLL 导出与 x64 PE 检查：Debug/Release 均通过。
- 规模测试：空猫舍、1、100、500、1000 只猫通过。
- 当前真实存档只读探针：
  `house_cats=8 rooms=2 assigned=8 adventure=1 day=17 warnings=3 errors=0 stable_ids=1`。
- 未把存档、名字、猫 ID、Steam ID、完整个人路径或探针输出写入仓库。

部署：
- Release DLL 已部署到 Mewjector 非递归 mods 目录。
- `verify_install.ps1`：通过。
- dist 与已安装 DLL SHA-256：
  `2207E05285A5C2A990227E0998A084307C3802CDA9BA1A7A1A4833FB66E1C58E`。

游戏内验证：
- 2026-07-29 玩家通过 Mewtator 启动游戏并在真实 House 多次点击两个按钮。
- 初次验收会话记录 10 次 `AC5100`，旧标签显示 `cats=30`；玩家截图证明
  当前猫数为 8。核查确认 30 是同一存档 `cats` 表的原始记录数，8 才是
  `house_state` 中具有可靠房间归属的当前家园猫。
- 修正后真实存档端到端探针稳定输出 `house_cats=8`，玩家要求此修正通过后
  将阶段 05 保持为完成。
- 推荐标记按钮切换 5 次；两个按钮各挂载一次，并在离开 House 时各清理一次。
- 本次会话 WARN=0、ERROR=0，无重复挂载。
- 日志未出现 AutoCattery 写入、移动猫、修改队伍或日期的行为。

剩余风险：
- 当前活动存档槽没有已证实的运行时 ID；适配器只读选择最近修改的非备份
  `.sav`。该启发式可用于本阶段预览，但未来任何写入阶段必须先解决身份门禁。
- class、age、relationships、room capacities 尚未证实。
- 不在 `house_state` 中的原始猫记录状态尚未证实，因此从当前家园快照排除。
- 本阶段通过前禁止进入任何移动、淘汰或写入阶段。

后续阶段：
- 阶段 06 及评分、规划、写入均未实施。

本地 commit：阶段 05 完成提交（实际 SHA-1 见 Git HEAD 与最终回复）
是否 push：否

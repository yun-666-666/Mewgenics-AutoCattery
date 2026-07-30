阶段：14
状态：存档安全、离线恢复和当前 build 隔离副本单猫移动闭环完成；玩家实机验证及真实 MoveCat adapter 仍 blocked，工作流保持 PreviewOnly

本阶段实现：
- 复用 Stage 10 的 `BackupService`、journal、恢复包和事务边界。备份包含稳定
  窗口、占用检查、WAL/SHM 拒绝、流式 SHA-256、大小校验、脱敏来源 identity、
  manifest、verification.txt 和 staging 目录原子发布。
- 同卷临时文件验证与 `ReplaceFileW` 原子替换；恢复前确认游戏未运行并再次
  备份当前目标，恢复后独立校验大小/SHA-256，失败时自动回退预恢复备份。
- 不控制、删除、枚举或改写 Steam Cloud 文件。所有自动写入只作用于调用者
  显式提供、位于游戏目录和玩家存档目录之外的 `.sav` 测试副本。
- 新增当前 build 精确门：`Mewgenics.exe` 21,981,184 bytes，SHA-256
  `C3A41E436A93FA58CD386EC46DAD5C2A6F21A583D33C3A57A15A2604C726439E`。
- 新增 `AutoCatterySaveLab.exe` / `Test-AutoCatterySingleCatMove.ps1`。写入必须
  显式开发开关、关闭游戏、隔离路径、精确 build 和 SQLite 3.37+ runtime；
  Windows 自带 SQLite 不支持当前 `STRICT` schema，因此保持安全失败。

当前 build 的本地证据：
- 只读比较游戏创建的 `.savbackup` 证明 `house_state` 每条记录包含 CatId、房间
  字符串和三个 double 坐标；同猫出现过 `Floor1_Large -> AdventureBox` 及反向
  转换。正常模拟还会改变其他猫坐标，因此只改房间字符串不安全。
- 当前样本验证的普通房间仅为 `Floor1_Large`、`Floor1_Small`、
  `Floor2_Large`、`Attic`；测试写入只接受这四个已有值，拒绝空房间、
  `AdventureBox` 和未知房间。
- 静态 EXE 只读检查未找到签名可靠、可调用的运行时 `MoveCat` API。没有从
  字符串、文档或示例推断函数签名。
- 当前 79 条 placement、游戏日 249 的复制样本继续由既有只读链确认：容器为
  SQLite，`properties`/`cats`/`files` 可读，699 个 cat blob 为 LZ4，猫身份来自
  SQLite key，`house_state` 关联可重新解析。

隔离复制存档验证：
- 来源是游戏创建的 `.savbackup` 的逐字节副本，置于 `%TEMP%`；没有打开或写入
  活动 `.sav`、游戏文件、真实日志或 Cloud 数据。
- 选择两个不同的已验证普通房间，只修改索引 1 的 placement：复制索引 7 的
  完整房间与三坐标。SQLite 使用 `BEGIN IMMEDIATE`、DELETE journal、FULL
  synchronous，恰好更新一条 `files.key='house_state'`，并通过 integrity_check。
- 修改前创建并验证备份；同目录临时副本写完后重新解析，原文件哈希再次确认
  未变，再原子替换。独立只读 CLI 确认 79 条记录不变，索引 1 从
  `Floor1_Large` 变为 `Floor2_Large`，且没有 WAL/SHM/journal 残留。
- 恢复工具先备份修改后的目标，再恢复原备份；独立只读 CLI 确认索引 1 回到
  `Floor1_Large`，最终文件 SHA-256 与最初复制样本完全一致。
- Debug 直接 CLI 和 Release PowerShell 包装器各完成一次上述移动/读回/恢复。

测试覆盖：
- 备份损坏、manifest 损坏、哈希/大小不一致、保存中、文件占用、WAL/SHM、
  模拟无权限、原子替换失败、读回不一致及自动回退。
- 恢复时游戏运行、准备期间游戏启动、恢复前备份、重复恢复、重复 operation、
  catalog/path traversal/reparse 逃逸。
- build 哈希/大小不匹配、隔离路径与玩家/游戏目录重叠、未开启开发开关、未知
  房间、重复 CatId、非法坐标、SQLite 非单行更新和 sidecar 拒绝。

实际修改文件：
- 当前 build/隔离写服务：`include/auto_cattery/save_safety/game_build_gate.hpp`、
  `single_cat_move_test_service.hpp`、`test_copy_guard.hpp`、
  `test_copy_house_state_store.hpp` 及对应 `src/save_safety/*.cpp`。
- 存档格式边界：`include/auto_cattery/snapshot/house_state_writer.hpp`、
  `include/auto_cattery/snapshot/detail/win_sqlite_api.hpp`、
  `src/snapshot/house_state_writer.cpp`、`src/snapshot/win_sqlite_api.cpp`。
- 工具/构建：`tools/save_lab_main.cpp`、`tools/Test-AutoCatterySingleCatMove.ps1`、
  `tools/build.ps1`、`CMakeLists.txt`。
- 测试：`tests/game_build_gate_tests.cpp`、`house_state_writer_tests.cpp`、
  `single_cat_move_test_service_tests.cpp`、`test_copy_guard_tests.cpp`、
  `test_copy_house_state_store_tests.cpp`、`win_sqlite_api_tests.cpp`、`test_main.cpp`。
- 阶段记录：`.auto-cattery/state.json`、本报告。

执行验证：
- `tools/build.ps1 -Configuration Debug`：5/5 通过；unit、DLL smoke、settings
  validation、restore CLI smoke、save lab CLI smoke。
- `tools/build.ps1 -Configuration Release`：5/5 通过，同上。
- 当前 build 复制存档：Debug 与 Release 均完成备份 -> 单猫完整 placement 修改
  -> 原子替换 -> 独立读回 -> 恢复前备份 -> 恢复 -> 原哈希读回。
- `git diff --check`：完成前通过。

游戏验证状态：
- 尚未把修改后的副本放入玩家明确选择的废弃测试槽，也未进入游戏目视确认。
  因此不能宣称真实房间分配可用。
- `UnsupportedGameWriteAdapter`、批量房间分配和真实淘汰均未接通；工作流继续
  `PreviewOnly`。Stage 15/16 未实施。

剩余风险与 blocker：
- 当前只有离线复制存档字段写入证据，没有许可清楚且签名可靠的运行时 MoveCat
  API，也没有玩家完成“载入测试槽 -> 目视确认 -> 关闭游戏 -> 恢复 -> 再次
  目视确认”的证据。
- 游戏 build 或 schema 变化会被精确 build/SQLite/version/room whitelist 门直接
  拒绝；不得通过更新常量绕过，必须重新只读取证。

本地 commit：`00c061b0298cc244a4013ad9fbc4852bbf994f9b`（Stage 14 基础安全工具）；复制存档单猫验证见包含本报告的 Stage 14 continuation commit（HEAD）
是否 push：否

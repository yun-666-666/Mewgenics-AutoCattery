阶段：14
状态：存档安全与离线恢复工具完成；真实存档写入和房间移动仍 blocked，工作流保持 PreviewOnly

本阶段实现：
- 复用 Stage 10 的 `BackupService`、journal、恢复包和事务边界；没有复制或
  替换它们的职责。备份扩展为稳定窗口、独占写入检查、WAL/SHM 拒绝、流式
  SHA-256、大小校验、脱敏来源 identity、manifest、verification.txt 和 staging
  目录原子发布。
- manifest 不保存原存档路径、猫名或 CatId；包含 operation id、UTC、build/
  mod 版本、可选游戏日、来源路径哈希和 `original.savbak` 的大小/SHA-256。
- 新增同卷临时文件验证与 `ReplaceFileW` 原子替换边界；临时文件、替换失败
  和读回不一致都保留恢复证据。
- 新增离线恢复服务和 `AutoCatteryRestore.exe` / `Restore-AutoCatteryBackup.ps1`：
  通过调用者提供的完整游戏可执行文件路径检测进程；恢复前和原子替换前各
  检测一次；覆盖前再备份当前目标；替换后独立 SHA-256/大小读回，失败时用
  预恢复备份回退，回退失败才报告人工恢复。
- 不控制、删除、枚举或改写 Steam Cloud 文件。恢复只处理调用者显式提供的
  本地 `.sav` 目标，且拒绝路径穿越与 reparse-point 逃逸。
- 新增独立只读存档格式探针；不修改玩家存档、游戏原文件、真实日志或 Cloud。

当前 build 的本地只读证据：
- `save_format_probe` 对当前候选存档返回：`container=sqlite`，现有
  `properties`、`cats`、`files` 读取均成功；68 个 cats blob 为 LZ4；
  `house_state` 成功给出 25 条猫-房间关联；猫身份来自 SQLite key；游戏日 32。
- `snapshot_probe` 对同一读取链返回 25 个 house cats、25 个已关联、15 个
  combat available、1 个 dead；两次读取的 ID、评分、保护和 RoomPlan 都稳定，
  `executable_moves=0`。
- 这些证据只确认当前只读容器/表/压缩/身份/关联，不证明任何可写 room 字段、
  运行时移动 API、RestoreCat API 或在线 SQLite 一致写回。

测试覆盖：
- SHA-256 已知值与文件哈希；稳定窗口、保存中、文件占用、WAL、SHM。
- manifest 创建/损坏、脱敏来源路径、哈希/大小不一致、备份重复与路径穿越。
- 同卷原子替换、临时文件保留、模拟无权限/原子替换失败、读回不一致和自动
  预恢复副本回退。
- 恢复时游戏运行、准备期间游戏启动、重复恢复、恢复前备份、损坏 manifest
  catalog、路径穿越。
- 恢复测试使用临时目录、合成 SQLite `.sav` 与复制的测试副本；恢复后由既有
  只读 SQLite adapter 独立读回 `current_day=32`。

实际修改文件：
- 构建与交付：`CMakeLists.txt`、`tools/build.ps1`、
  `tools/Restore-AutoCatteryBackup.ps1`、`tools/restore_main.cpp`。
- 备份边界：`include/auto_cattery/execution/backup_service.hpp`、
  `src/execution/backup_service.cpp`、`include/auto_cattery/save_safety/*`、
  `src/save_safety/*`。
- 测试与探针：`tests/atomic_file_replace_tests.cpp`、
  `tests/backup_manifest_tests.cpp`、`tests/file_hash_tests.cpp`、
  `tests/restore_service_tests.cpp`、`tests/save_format_probe.cpp`、
  `tests/save_stability_tests.cpp`、`tests/test_main.cpp`。
- 阶段记录：`CODEX_TASK.md`、`.auto-cattery/state.json`、本报告。

执行验证：
- 初始 Debug 基线：`tools/build.ps1 -Configuration Debug`，Stage 13 的 3/3 通过。
- Stage 14 Debug：`tools/build.ps1 -Configuration Debug`，4/4 通过：unit、DLL
  smoke、settings editor validation、restore CLI smoke；最终 6.44 秒。
- Stage 14 Release：`tools/build.ps1 -Configuration Release`，4/4 通过；最终
  2.16 秒。
- `build/Debug/snapshot_probe.exe` 和 `build/Debug/save_format_probe.exe`：通过，
  只读运行。
- `git diff --check`：通过（仅 Git LF/CRLF 工作区提示）。
- 本阶段未使用网络资料；没有部署 DLL，没有写入玩家真实存档。

游戏验证状态：
- 未进入游戏执行真实移动、淘汰、恢复或批量房间分配；不需要为本阶段的备份/
  恢复单元验收修改玩家存档。
- 若后续要验证真实单猫移动，最短可恢复流程是：关闭游戏和 Steam Cloud 冲突
  处理界面；复制一个明确非主用 `.sav` 到隔离临时目录；在副本上创建/验证
  备份；仅在 build 专用写 adapter、字段读回和撤销证据齐全后执行一次移动；
  用独立只读 probe 验证；用恢复工具恢复副本并再次读回。当前不得执行此流程
  的移动步骤，因为写 adapter 仍 Unsupported。

剩余风险与 blocker：
- build identity 仍可能是 unknown。
- 尚无许可清楚、签名明确、可重复验证的 MoveCat/Cull/Restore 运行时 API；
  也没有能证明正确持久化的 room 写字段、SQLite backup/checkpoint 或在线一致
  快照证据。
- 因此未实现 `IDirectSaveEditor`、build 专用写 adapter、单猫真实移动、批量
  房间分配或真实淘汰；`UnsupportedGameWriteAdapter` 和 `PreviewOnly` 未改动。
- 未实施 Stage 15/16、自动组队、自动选猫、自动确认、自动休息、日期推进或
  自动出征。

本地 commit：f26dbe4（将在回填后 amend 为同一提交）
是否 push：否

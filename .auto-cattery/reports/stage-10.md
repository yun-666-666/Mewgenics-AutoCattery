阶段：10
状态：安全基础设施已完成；真实执行 Unsupported，Stage 11 仍 blocked

实现：
- 新增密封的 ApprovedExecutionPlan 与 ExecutionAuthorization。
- 规范化摘要覆盖快照内容、分类、RoomPlan、ProtectionDigest、场景
  generation、游戏日、存档身份和 build 身份。
- snapshot_id 只作捕获关联，不作唯一并发版本。
- Stage 9 的 `executable=false` 和执行权限没有被修改。
- 新增离线一致备份、匿名 journal、恢复说明包和原子发布边界。
- 新增注入式事务执行器、独立读回验证、失败停止、反向恢复与
  ManualRecoveryRequired。
- 新增 UnsupportedGameWriteAdapter；配置禁止启用真实写入和淘汰。
- 现有 House 按钮仍只预览，执行入口固定返回 PreviewOnly。

证据矩阵：
- 运行时移动 API：本地 SDK/公开源码未找到可验证签名。
- 淘汰/捐赠 API：未找到可验证签名。
- RestoreCat：未找到可验证签名，因此真实淘汰保持 Unsupported。
- 写入介质：无法证明应调用运行时函数、修改内存或修改存档。
- 存档容器：已确认 `.sav` 为 SQLite；猫/house_state blob 含 LZ4。
- 当前读取：READONLY | NOMUTEX，250 ms busy timeout，
  BEGIN DEFERRED TRANSACTION。
- WAL/SHM：当前绑定没有 SQLite backup API；主文件在线复制的一致性
  未证明，发现 sidecar 时拒绝备份。
- 并发保存：SceneContext 可报告 save_in_progress，但不能证明游戏运行时
  不会同时保存，因此真实执行不以该信号单独授权。
- 匿名验证：fake reader 只按 ID 内部关联，journal 仅保存操作索引。
- build 身份：已有可执行文件 SHA-256 证据，但未接入可验证写适配器。
- 联网：未进行；活动 AGENTS.md 禁止 web research。

备份/journal/恢复：
- 仅接受游戏已确认静止、无 WAL/SHM 的离线 `.sav`。
- 复制到临时文件，验证 SHA-256 与大小后原子发布。
- 拒绝路径穿越、重解析点逃逸、覆盖源和覆盖已有备份。
- journal 和 recovery JSON 使用临时文件加原子替换。
- journal 不记录猫名、CatId、存档名或个人路径。
- 自动覆盖运行中存档恢复固定为 false。

事务测试覆盖：
- 备份失败时 0 写调用。
- 恢复包发布失败时 0 写调用。
- 第 1/3/最后一个移动失败。
- 第 1/2/最后一个淘汰失败。
- 移动/淘汰读回失败和保护猫验证失败路径。
- 淘汰前场景、保护或保存前置条件变化。
- 回滚成功、回滚失败、ManualRecoveryRequired。
- 重复 OperationId、同计划并发、move-only、Unsupported。
- NoMove、NoCull、NoCullOrMove、FullyUnmanaged、fail_closed、
  adventure-box、未知/重复 ID、候选顺序变化。

主要修改文件：
- `include/auto_cattery/execution/*.hpp`
- `src/execution/*.cpp`
- `tests/execution_*`
- `include/auto_cattery/config.hpp`
- `src/config.cpp`
- `config/default_config.json`
- `config/config.schema.json`
- `include/auto_cattery/workflow/organize_workflow_facade.hpp`
- `src/workflow/organize_workflow_facade.cpp`
- `tests/config_tests.cpp`
- `tests/organize_workflow_facade_tests.cpp`
- `tests/test_main.cpp`
- `CMakeLists.txt`
- `CODEX_TASK.md`
- `docs/implementation-status.md`
- `.auto-cattery/state.json`
- `.auto-cattery/reports/stage-10.md`

验证：
- Debug `phase10_unit_tests`：通过（最终 5.45 秒）。
- Debug `phase10_dll_load_smoke`：通过（0.07 秒）。
- Release `phase10_unit_tests`：通过（0.47 秒）。
- Release `phase10_dll_load_smoke`：通过（0.06 秒）。
- Debug/Release 均通过 x64 DLL 和必要导出检查。
- 所有文件测试只使用临时目录和合成数据。

真实游戏状态：
- real adapter：Unsupported。
- 未部署 DLL。
- 未读取或写入玩家当前真实存档。
- 未执行真实移动、淘汰或恢复。
- 不需要玩家手动测试。

未实施：
- Stage 11 完整自动整理流水线。
- Stage 12 标记扩展、Stage 13 复杂 UI、Stage 14 管理中心。
- 自动组队、自动休息、自动推进日期、自动出征选择。
- 任何猜测字段、偏移、RoomId、函数签名或真实写入。

剩余 blocker：
- 缺少许可清楚、签名明确、可重复验证的 move/cull/restore API。
- 缺少运行中一致 SQLite backup 或官方保存快照证据。
- 缺少复制测试存档上的独立 move-only 与撤销验证。

本地 commit：将在最终命名提交后由回复报告。
是否 push：否

阶段：08
状态：已完成（不可绕过的保护策略与严格只读 sidecar 边界）

实现：
- 新增独立 Protection 领域模型、ProtectionPolicy、权限合并和精确摘要
  重检。支持 None、NoCull、NoMove、NoCullOrMove、FullyUnmanaged。
- NoCull 禁止淘汰；NoMove 明确禁止移动；FullyUnmanaged 不参与自动
  分类或候选操作；本阶段没有移动实现。
- 游戏原生锁定、收藏及特殊状态使用适配输入。Unknown 一律 fail closed，
  不猜测存档字段、标志位、ID 或偏移。
- 新增严格 sidecar 只读边界，只保存/解释 MOD 记录契约，不修改游戏
  存档或猫名。缺失、空白、损坏、读取失败、旧/未来 schema、重复 CatId、
  非法保护级别、非法身份、非整数/越界期限均阻止破坏性操作。
- 稳定 CatId 或当前身份 token 不足、身份冲突时，不应用可能失效的
  sidecar None/解除保护记录，而是保守保护。
- ProtectionPolicy 已接入 CullSafetyFactsByCat。缺少策略结果、硬保护、
  Unknown、最低池、低置信度及阶段 07 任何安全门都会阻止候选。
- 白名单/保护优先于黑名单。黑名单只重排已经通过全部安全门的只读预览
  候选，不能创造候选或绕过保护。
- 预览摘要使用排序后的完整保护条目进行精确相等比较；变化返回
  CancelAndRepreview，不接执行器。
- 所有 destructive_action_allowed 仍恒为 false。

主要修改文件：
- `include/auto_cattery/protection/domain.hpp`
- `include/auto_cattery/protection/policy.hpp`
- `include/auto_cattery/protection/sidecar.hpp`
- `src/protection/policy.cpp`
- `src/protection/sidecar.cpp`
- `include/auto_cattery/classification/protection_adapter.hpp`
- `src/classification/protection_adapter.cpp`
- `include/auto_cattery/classification/domain.hpp`
- `src/classification/classifier.cpp`
- `tests/protection_policy_tests.cpp`
- `tests/protection_sidecar_tests.cpp`
- `tests/classifier_tests.cpp`
- `tests/snapshot_probe.cpp`
- `config/default_config.json`
- `config/config.schema.json`
- `config/protection.schema.json`
- `include/auto_cattery/config.hpp`
- `src/config.cpp`
- `tests/config_tests.cpp`
- `CMakeLists.txt`
- `tools/build.ps1`
- `THIRD_PARTY_NOTICES.md`
- `CODEX_TASK.md`
- `docs/implementation-status.md`
- `.auto-cattery/state.json`
- `.auto-cattery/reports/stage-08.md`

构建与测试：
- Debug `phase08_unit_tests`：通过（4.96 秒，最终测试集）。
- Debug `phase08_dll_load_smoke`：通过（0.07 秒）。
- Release `phase08_unit_tests`：通过（0.41 秒，最终测试集）。
- Release `phase08_dll_load_smoke`：通过（0.05 秒）。
- 两种配置均完成 x64 DLL 导出检查。
- 覆盖全部保护级别与组合、NoCull、NoMove、FullyUnmanaged、白黑名单
  冲突、Unknown 原生状态、最低池、低置信度、保护摘要变化、稳定身份
  不足/冲突、sidecar 全部错误边界及 1000 条记录确定性/性能。

当前真实存档只读验证（Debug 与 Release 一致）：
- `house_cats=8 rooms=1 assigned=8 adventure=0 day=17`
- `warnings=2 errors=0 stable_ids=1`
- `ranked=8 recommended=0 stable_ranking=1 breeding_ranked=8`
- `protected=8 stable_protection=1`
- `preview_culls=0 executable_culls=0`
- 保护字段和 sidecar 边界无法确认，因此 8 只猫均保守保护；这是预期的
  fail-closed 结果，不代表已识别游戏原生锁定或收藏状态。

范围与限制：
- 未实现 sidecar 写入、原子替换或备份恢复；当前证据不足时保持只读。
- 未实现或猜测游戏原生收藏、锁定、特殊状态及完整身份指纹解析。
- 未实现阶段 09 房间规划、阶段 10 移动/淘汰执行、阶段 11 流水线、
  自动组队、自动休息、自动出征选择或保存写入。
- 本阶段无新增 UI、无 DLL 部署、无玩家操作路径，因此无需玩家手动测试。

本地 commit：`feat: add non-bypassable cat protection policy layer`
（精确提交哈希由提交完成后的最终回复报告）
是否 push：否

阶段：09
状态：已完成（容量感知的只读房间规划与房间不足回退）

实现：
- 新增独立 RoomPlanning 领域模型、输入验证器、保守能力适配器和纯只读
  RoomPlanner。
- RoomCapability 明确区分游戏已确认硬容量、MOD 软布局偏好与 Unknown。
  未知硬容量不会回退到默认软容量，也不会授权目标移动。
- 验证快照 ID、分类、ProtectionPolicy 决定、当前保护摘要、字符串
  RoomId、居民和能力输入；重复/缺失/未知/不一致输入全部 fail closed。
- NoMove、NoCullOrMove、FullyUnmanaged、fail_closed、冒险箱、未知来源
  房能力和核心繁育猫不会产生移动。
- 抽象完整能力夹具支持确定性角色分配、最小移动、已知硬容量约束、部分
  成功、unplaced 和最小容量释放建议。
- 容量释放建议只消费 Stage 07/08 已安全排序的
  capacity_relief_candidates，并再次核对 ProtectionPolicy；所有建议和
  移动均不可执行。
- 预览后保护摘要变化返回 CancelAndRepreview，不接移动/淘汰执行器。
- 真实快照适配器不从房间名、楼层、当前人数或文档示例推测角色、容量、
  特殊/锁定/强制居民或接收/移出权限，全部保持 Unknown。
- 未确认关系、性别、繁育资格、兼容规则和真实繁育房能力时，不生成繁育
  配对或虚构繁育布局。

主要修改文件：
- `include/auto_cattery/room_planning/domain.hpp`
- `include/auto_cattery/room_planning/validator.hpp`
- `include/auto_cattery/room_planning/capability_adapter.hpp`
- `include/auto_cattery/room_planning/planner.hpp`
- `src/room_planning/validator.cpp`
- `src/room_planning/capability_adapter.cpp`
- `src/room_planning/planner.cpp`
- `tests/room_planning_validator_tests.cpp`
- `tests/room_capability_adapter_tests.cpp`
- `tests/room_planner_tests.cpp`
- `tests/snapshot_probe.cpp`
- `include/auto_cattery/config.hpp`
- `src/config.cpp`
- `config/default_config.json`
- `config/config.schema.json`
- `tests/config_tests.cpp`
- `CMakeLists.txt`
- `THIRD_PARTY_NOTICES.md`
- `CODEX_TASK.md`
- `docs/implementation-status.md`
- `.auto-cattery/state.json`
- `.auto-cattery/reports/stage-09.md`

分批构建与测试：
- 批次 1：Debug 构建、当时阶段单测和 DLL load smoke 通过。
- 批次 2：Debug 构建、当时阶段单测和 DLL load smoke 通过。
- 批次 3：首次测试发现 unplaced 计数大于最终最小容量缺口；在当前批次
  内修正后 Debug 构建、阶段单测和 DLL load smoke 通过。
- 批次 4：Debug `phase09_unit_tests` 通过（5.05 秒），
  `phase09_dll_load_smoke` 通过（0.07 秒）。
- Release `phase09_unit_tests` 通过（0.40 秒），
  `phase09_dll_load_smoke` 通过（0.06 秒）。
- 两种配置均由 `tools/build.ps1` 执行 x64 DLL 导出检查。

测试覆盖：
- 0/1/2/多房间、重复 RoomId/居民、未知 CatId、多房间重复猫。
- 快照/分类/保护/能力输入不一致，当前保护摘要不一致。
- 已知硬容量、未知硬容量、未知角色/特殊/锁定/强制居民/收发权限。
- MOD 软容量不替代游戏硬容量。
- NoMove、NoCullOrMove、FullyUnmanaged、fail_closed、白黑名单保护边界、
  冒险箱和缺失当前房间。
- 所有猫保护时稳定空移动计划；无关系/繁育房证据时不生成繁育配对。
- 部分成功、unplaced、安全候选不足、最小容量释放数量。
- 重复输入确定性、已满足结果幂等、保护摘要变化取消。
- 1000 猫/100 房间重复规划一致且有界。
- 所有 destructive_action_allowed、移动执行和淘汰执行权限为 false。

当前真实存档只读验证（Debug 与 Release 一致）：
- `house_cats=8 rooms=1 assigned=8 adventure=0 day=17`
- `warnings=2 errors=0 stable_ids=1`
- `ranked=8 recommended=0 stable_ranking=1 breeding_ranked=8`
- `protected=8 stable_protection=1`
- `preview_culls=0 executable_culls=0`
- `stable_room_plan=1 planned_moves=0 executable_moves=0`
- `room_validation_errors=0`

范围与限制：
- 当前存档/解析器未证明真实硬容量、房间角色、特殊房间、玩家锁房、游戏
  强制居民、幼猫房规则或接收/移出权限；真实计划因此保守为 0 移动。
- 未实现阶段 10 移动/淘汰执行与撤销、阶段 11 完整流水线、保存写入、
  自动组队、自动休息、自动推进日期或自动出征选择。
- 本阶段无新增 UI、无 DLL 部署、无游戏写入，无需玩家手动测试。

本地 commit：`feat: implement capacity-aware room assignment planner`
（提交哈希由提交完成后的最终回复报告；Git 提交无法在自身内容中保存自身哈希）
是否 push：否

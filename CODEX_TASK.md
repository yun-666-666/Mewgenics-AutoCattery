# CODEX CURRENT TASK — STAGE 09

## Scope

只实施阶段 09：容量感知、保护不可绕过、确定性的只读房间规划与房间
不足回退。禁止移动执行、淘汰执行、存档写入、执行器和复杂 UI。

阶段 09 已完成。除非玩家明确要求，不得开始阶段 10。

## Evidence rules

- 阶段文档和用户提供的 MIT Toolkit 只作为流程、接口和算法思想参考。
- Toolkit 的整数 RoomId、房间类型、容量、繁育/幼猫/特殊/锁房标志及
  兼容关系示例均未复制。
- 当前 RoomId 为字符串；现有快照只确认房间 ID、居民和当前房间关系。
- 真实硬容量、房间角色、特殊状态、玩家锁定、强制居民和接收/移出权限
  仍为 Unknown，不从名称、人数或软容量推断。
- `default_soft_capacity` 只用于拥有已确认硬容量时的 MOD 布局偏好，
  绝不授权移动或替代游戏硬容量。

## Implemented boundary

- 独立 RoomCapability、规划结果、输入验证、保守能力适配器和纯只读
  RoomPlanner。
- 快照、分类、ProtectionPolicy、保护摘要、房间与居民输入不一致时
  fail closed。
- Unknown 房间能力、特殊/锁定/强制居民状态、冒险箱、NoMove、
  NoCullOrMove、FullyUnmanaged 和 fail_closed 都不能生成移动。
- 繁育配对证据和真实繁育房能力未确认时，核心繁育猫保持原位，不生成
  配对或虚构繁育布局。
- 抽象完整证据夹具支持已知硬容量、确定性排序、最小移动、部分成功、
  unplaced 和按 Stage 07/08 安全候选顺序的最小容量释放建议。
- 所有移动和淘汰执行权限恒为 false；保护摘要变化仅返回
  CancelAndRepreview。
- 真实存档使用保守能力适配器，预期并验证为 0 planned moves。

## Required validation

- Debug/Release `phase09_unit_tests`。
- Debug/Release `phase09_dll_load_smoke`。
- 真实存档只读探针：0 校验错误、稳定 ID/分类/保护/房间计划、0 planned
  moves、0 executable moves、0 executable culls。
- `git diff --check` 与源码范围检查。

## Stop condition

阶段 09 已完成。阶段 10 只能在玩家明确要求后开始，永不自动 push。

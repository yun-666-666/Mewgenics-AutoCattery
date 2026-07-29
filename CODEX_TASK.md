# CODEX CURRENT TASK — STAGE 08

## Scope

只实施阶段 08：在阶段 07 的只读分类器之前建立不可绕过的保护策略，
支持 MOD sidecar 白名单/保护记录和仅用于安全候选排序的黑名单。
禁止房间规划、移动、淘汰、执行器、存档写入或复杂 UI。

阶段 08 已完成。除非玩家明确要求，不得开始阶段 09。

## Evidence rules

- 阶段文档和用户提供的 Toolkit 只作为 MIT 许可的流程与接口参考。
- Toolkit 的六属性、原生锁定/收藏、mutation、性别、繁育资格、示例
  ID 和其他游戏状态未复制到当前适配器。
- 当前本地存档和解析器仍不能证明游戏原生锁定、收藏或特殊状态字段，
  因而这些状态保持 Unknown，并由保护策略 fail closed。
- Sidecar 只描述 MOD 自己的记录；本阶段只实现严格读取，不实现写入。

## Implemented boundary

- 独立 `ProtectionLevel`、`ProtectionPolicy`、精确摘要重检和权限结果。
- 支持 None、NoCull、NoMove、NoCullOrMove、FullyUnmanaged 及保守合并。
- 原生状态 Unknown、sidecar 缺失/损坏、稳定身份不足或身份冲突都会禁止
  淘汰和移动。
- 严格 sidecar schema 拒绝空文件、损坏 JSON、旧/未来 schema、重复
  CatId、非法枚举、非法身份、非整数或越界期限。
- 白名单/硬保护优先；黑名单只能重排已经通过阶段 07 全部安全门的预览
  候选。
- 分类器要求每只猫都有 ProtectionPolicy 决定；缺失决定不产生候选。
- 所有 `destructive_action_allowed` 恒为 false。

## Required validation

- Debug/Release `phase08_unit_tests`。
- Debug/Release `phase08_dll_load_smoke`。
- 保护级别/合并、白黑名单冲突、Unknown、最低池、低置信度、摘要变化、
  sidecar 错误边界、身份冲突和 1000 条记录。
- 当前真实存档只读探针必须 0 校验错误、保护摘要稳定、0 预览淘汰、
  0 可执行淘汰。
- 源码范围检查确认无阶段 09/10/11 功能。

## Stop condition

阶段 08 已完成。阶段 09 只能在玩家明确要求后开始，永不自动 push。

# CODEX CURRENT TASK — STAGE 07

## Scope

只实施阶段 07：在阶段 05 的不可变快照和阶段 06 的单猫战斗排名之上，
建立单猫繁育评分、稳定排序、角色分类、最低保留池和只读淘汰候选。
禁止 UI 接入、自动组队、房间规划、移动、淘汰、存档写入或执行器。

阶段 07 已完成。除非玩家明确要求，不得开始阶段 08。

## Evidence rules

- `AutoCatteryDocs/16_steps/07_繁育评分与淘汰候选.md` 只作范围参考，
  字段、权重、性别、繁育资格、保护状态和置信度不得照搬。
- 用户明确要求使用
  `Mewgenics_AutoCattery_Codex_Toolkit_v1.0.0`；本阶段复用了其 MIT
  许可的评分、稳定排序、保留池和预览候选流程，并替换错误数据假设。
- 用户已明确允许联网核对；本阶段已有本地真实存档、SDK 格式证据和
  Toolkit 接口足以完成安全边界，未额外联网。
- 未证实字段必须保持 `Unknown`/limitation，不得用文档示例值替代。

## Implemented boundary

- 繁育基础分只使用已确认的 7 项 genetic + heredity bonus；装备加成明确
  排除，不冒充可遗传价值。
- 技能、被动和疾病默认权重均为 0；只有显式配置的真实保存 ID 才改变
  分数。
- 当前适配器未确认繁育资格、性别/兼容、亲缘、稀有特征和特殊保护状态，
  因此保持 Unknown，并阻止当前真实存档产生淘汰候选。
- 分类复用阶段 06 的单猫战斗排名，建立战斗、核心/备用繁育、普通保留、
  保护、无资格和淘汰候选角色。
- 只有最低战斗/繁育/普通保留池均满足，稳定 ID、亲缘能力、繁育资格、
  保护、特殊状态、成年/受伤状态和置信度均明确时，才可生成预览候选。
- `quality_cull_candidates` 与 `capacity_relief_candidates` 分离；阶段 07
  不计算房间容量。
- 所有 `destructive_action_allowed` 恒为 false；不接执行器或写适配器。

## Required validation

- Debug/Release `phase07_unit_tests`。
- Debug/Release `phase07_dll_load_smoke`。
- 七属性、装备排除、显式 ID override、缺字段、NaN/Infinity、同分稳定
  ID、最低池、小猫舍、全保护、未知保护、低置信度和候选稳定顺序。
- 当前真实存档只读探针必须为 0 校验错误、稳定 ID/排名、0 预览淘汰、
  0 可执行淘汰。
- 源码检查确认无自动组队、房间移动、执行器或保存写入逻辑。

## Stop condition

阶段 07 已完成。阶段 08 只能在玩家明确要求后开始，永不自动 push。

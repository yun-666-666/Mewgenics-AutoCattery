# CODEX CURRENT TASK — STAGE 06

## Scope

只实施阶段 06：基于阶段 05 的不可变只读快照，建立单猫独立战斗评分、
确定性排序、推荐前 N 和缓存。禁止自动组队、职业组合、站位分析、UI 标记、
移动、淘汰或存档写入。

阶段 05 已完成，提交为 `252399f`。阶段 06 的代码、配置、自动化测试和
当前真实存档只读排名探针均已完成。除非玩家明确要求，不得开始阶段 07。

## Evidence rules

- `AutoCatteryDocs/16_steps/06_战斗猫评分与排序.md` 只作范围参考，字段、
  槽位和权重不得照搬。
- 用户明确要求使用
  `Mewgenics_AutoCattery_Codex_Toolkit_v1.0.0`；本阶段复用了其 MIT
  许可的单猫评分、稳定排序和前 N 选择器流程，并以真实快照模型替换错误
  假设。
- 用户已明确允许联网核对；本地当前存档、游戏数据和 SDK 资料优先。
- 未证实字段必须保持 `Unknown`/limitation，不得以健康、成年、可出战或
  零惩罚的事实冒充。

## Implemented boundary

- 默认分数只使用已确认的 7 项总属性：
  genetic + heredity bonus + equipment bonus。
- 存档核心能力序列修正为 10 槽：
  move、basic attack、4 active、2 passive、2 disorder。
- 移动和基础攻击没有通用默认价值；技能、被动、疾病默认权重也为 0，
  只有显式配置 override 才改变分数。
- 幼猫、死亡、不可出战和受伤过滤仅在状态已知时执行；当前保存适配器
  尚未可靠解析这些状态，因此写入 limitation，并由默认资格门禁排除，
  不伪造资格事实。
- 排序顺序为 eligible、score、confidence、真实总属性和稳定 CatId。
- 推荐集合在合格且达到阈值后取前 `recommended_count`。
- 非有限配置被拒绝；负权重和零权重被明确支持。
- 缓存键为 snapshot ID、算法版本和确定性配置哈希。
- 不接 UI，不自动选队，不分析职业组合或协同。

## Required validation

- Debug/Release `phase06_unit_tests`。
- Debug/Release `phase06_dll_load_smoke`。
- 同分稳定 ID 决胜、缺字段、负/零权重、NaN/Infinity、阈值、推荐数量、
  资格过滤、缓存失效、1000 只猫和重复 100 次确定性测试。
- 当前真实存档只读探针：0 校验错误、连续快照稳定 ID 和稳定排名。
- 源码检查确认评分目录不存在 team/synergy/party/composition 逻辑。

## Stop condition

阶段 06 已完成。阶段 07 只能在玩家明确要求后开始，永不自动 push。

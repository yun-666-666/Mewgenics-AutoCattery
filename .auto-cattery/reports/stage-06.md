阶段：06
状态：已完成（纯只读单猫评分、排序、推荐选择与缓存）

真实数据与参考边界：
- 使用用户指定的 `Mewgenics_AutoCattery_Codex_Toolkit_v1.0.0` 作为
  MIT 许可的确定性选择器流程基线。
- Toolkit/阶段文档中的 6 属性模型不符合当前游戏存档；正式实现使用
  STR/DEX/CON/INT/SPD/CHA/LCK 共 7 项。
- 属性原始值取当前存档已验证的 genetic、heredity bonus、equipment
  bonus，并以三者总和参与评分。
- 核心能力序列经本地 SDK、公开源代码格式资料和当前真实存档只读探针
  交叉核对，修正为 move、basic attack、4 active、2 passive、
  2 disorder 共 10 槽。
- 外部资料仅用于核对结构；未复制 AGPL 保存编辑器代码。

实现：
- 新增 `scoring` 领域模型、单猫评分器、确定性排序器和单项解释。
- 排序依次使用 eligible、score、confidence、真实总属性和稳定 CatId。
- 推荐集合只从合格且达到阈值的结果中取前 `recommended_count`。
- 默认 7 属性权重均为 1；能力、被动、疾病、受伤默认权重均为 0。
  这些平衡价值没有游戏统一真值，必须通过配置显式覆盖。
- 支持负权重、零权重和能力 ID override；拒绝 NaN/Infinity。
- 已知死亡、幼猫、不可出战和受伤状态可安全过滤。当前保存适配器尚未
  可靠解析这些状态，保持 Unknown、写入 limitation，并由默认
  `require_confirmed_eligibility` 门禁排除推荐。
- 缓存键包含 snapshot ID、算法版本和与 map 插入顺序无关的配置哈希。
- 配置 schema 与默认配置已加入阶段 06 字段。
- 只读探针加入排名数量与连续快照稳定性检查，不输出猫名或猫 ID。

主要文件：
- `include/auto_cattery/scoring/domain.hpp`
- `include/auto_cattery/scoring/combat_scorer.hpp`
- `include/auto_cattery/scoring/combat_ranker.hpp`
- `include/auto_cattery/scoring/combat_ranking_cache.hpp`
- `src/scoring/combat_scorer.cpp`
- `src/scoring/combat_ranker.cpp`
- `src/scoring/combat_ranking_cache.cpp`
- `tests/combat_scorer_tests.cpp`
- `tests/combat_ranker_tests.cpp`
- `tests/combat_ranking_cache_tests.cpp`
- `tests/snapshot_probe.cpp`
- `config/default_config.json`
- `config/config.schema.json`

构建与测试：
- Debug `phase06_unit_tests`：通过。
- Debug `phase06_dll_load_smoke`：通过。
- Release `phase06_unit_tests`：通过。
- Release `phase06_dll_load_smoke`：通过。
- 覆盖同分稳定 ID、缺字段、负/零权重、非法非有限配置、阈值、
  推荐数大于合格数、资格过滤和缓存失效。
- 1000 只猫评分排序在测试上限 2 秒内完成。
- 相同输入重复 100 次，排名与推荐集合完全一致。
- 评分源码目录检查无 team、synergy、party、composition 或自动组队逻辑。

当前真实存档只读验证：
- `house_cats=8 rooms=2 assigned=8 adventure=1 day=17`
- `warnings=2 errors=0 stable_ids=1`
- `ranked=8 recommended=0 stable_ranking=1`
- 两个 warning 是仍未证实的 relationships 和 room capacities。
- 未将存档、猫名、猫 ID、Steam ID 或个人完整路径写入仓库。

游戏验证状态：
- 本阶段不接 UI、不标记猫，按阶段验收无需玩家游戏内点击。
- 当前真实存档端到端只读探针已通过。
- 未部署阶段 06 DLL，避免把尚未到阶段 12 的推荐 UI 行为带入游戏。

风险与未实施内容：
- 当前保存适配器尚未可靠读取 life stage、injury、can-fight 和 level；
  不会猜测这些值。未来适配器提供已验证字段后，现有过滤路径可直接使用。
- 未实现 min-max/fixed-range 归一化；当前算法版本明确为
  `known-save-fields-v1` 原始总属性模式。
- 未实现职业加成、突变评分或通用技能强度表，因为没有可证明的统一真值。
- 阶段 07 的繁育/淘汰分类、阶段 12 的 UI 高亮和所有写入均未实施。

本地 commit：见 Git HEAD（实际 SHA-1 由完成提交后的最终回复报告）
是否 push：否

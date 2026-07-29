阶段：07
状态：已完成（纯只读繁育评分、分类与淘汰候选预览）

真实数据与参考边界：
- 使用用户指定的 `Mewgenics_AutoCattery_Codex_Toolkit_v1.0.0` 作为
  MIT 许可的确定性评分、排序、保留池和候选生成流程基线。
- Toolkit/阶段文档中的 6 属性、mutation、繁育资格、性别、亲缘、保护
  和兼容字段不能直接用于当前项目；未复制这些示例值或虚构字段状态。
- 正式繁育基础分使用当前存档已验证的 7 项 genetic + heredity bonus；
  equipment bonus 明确排除。
- 当前适配器没有可靠解析繁育资格、性别/繁育兼容、亲缘、稀有特征、
  保护和特殊状态，全部保持 Unknown/limitation。

实现：
- 新增小文件形式的繁育领域模型、单猫评分器和确定性排序器。
- 新增分类领域模型和只读分类器，复用阶段 06 单猫战斗排名。
- 支持战斗推荐、核心繁育、备用繁育、普通保留、保护、无资格和淘汰
  候选角色。
- 最低战斗池、繁育池和普通保留池在候选生成前强制检查。
- 未确认稳定 ID、繁育资格、亲缘保护、显式保护、特殊状态、成年/受伤
  状态或置信度时，候选生成安全失败。
- `quality_cull_candidates` 与 `capacity_relief_candidates` 分开输出；
  后者只是供阶段 09 使用的已排序安全候选池，不在本阶段计算容量。
- 所有决定的 `destructive_action_allowed` 恒为 false。
- 配置默认值和 schema 已加入繁育评分及分类字段；主观 ID 权重默认 0。
- 未接 UI、房间规划、移动、淘汰、执行器或保存写入。

主要文件：
- `include/auto_cattery/breeding/domain.hpp`
- `include/auto_cattery/breeding/breeding_scorer.hpp`
- `include/auto_cattery/breeding/breeding_ranker.hpp`
- `include/auto_cattery/classification/domain.hpp`
- `include/auto_cattery/classification/classifier.hpp`
- `src/breeding/breeding_scorer.cpp`
- `src/breeding/breeding_ranker.cpp`
- `src/classification/classifier.cpp`
- `tests/breeding_scorer_tests.cpp`
- `tests/breeding_ranker_tests.cpp`
- `tests/classifier_tests.cpp`
- `tests/snapshot_probe.cpp`
- `config/default_config.json`
- `config/config.schema.json`

构建与测试：
- Debug `phase07_unit_tests`：通过。
- Debug `phase07_dll_load_smoke`：通过。
- Release `phase07_unit_tests`：通过。
- Release `phase07_dll_load_smoke`：通过。
- 覆盖真实 7 属性、装备排除、显式技能/被动/疾病 ID override、缺字段、
  非有限配置、阈值、同分稳定 ID、最低池、小猫舍、全保护、未知保护和
  低置信度。
- `git diff --check`：通过（仅有 Git 的 LF/CRLF 转换提示，无空白错误）。
- 新增领域源码检查无 team、synergy、party、composition、房间移动、
  执行器或保存写入逻辑。

当前真实存档只读验证：
- `house_cats=8 rooms=1 assigned=8 adventure=0 day=17`
- `warnings=2 errors=0 stable_ids=1`
- `ranked=8 recommended=0 stable_ranking=1`
- `breeding_ranked=8 preview_culls=0 executable_culls=0`
- 两个快照 warning 是仍未证实的 relationships 和 room capacities。
- 未输出或提交猫名、猫 ID、Steam ID、个人存档或个人完整路径。

游戏验证状态：
- 本阶段不接 UI、不修改猫，按阶段验收无需玩家游戏内点击。
- 当前真实存档端到端只读探针已通过。
- 未部署阶段 07 DLL，避免把未到后续阶段的行为带入游戏。

风险与未实施内容：
- 未确认字段会阻止当前真实存档生成候选，这是安全门而不是默认值缺失。
- 未实现阶段 08 保护规则、阶段 09 房间容量规划、阶段 10 执行/撤销。
- 未实现性别分池、配对兼容、亲缘风险或稀有突变加权；在真实字段和规则
  得到证实前不会猜测。
- 候选仅供预览，永远不代表已允许执行破坏性操作。

本地 commit：包含本报告的 Git HEAD（实际 SHA-1 由提交后的最终回复报告）
是否 push：否

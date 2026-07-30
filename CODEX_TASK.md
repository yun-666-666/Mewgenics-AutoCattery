# CODEX CURRENT TASK - CURRENT BUILD MOVEONLY CONSOLIDATION

## Current objective

整理并固化当前 build 已通过玩家验证的原生 MoveOnly 自动分房能力，维护当前
状态文档，并为后续猫属性、技能、被动、变异和遗传评分研究准备证据化输入。

## Required reading

1. `AGENTS.md`
2. `README.md`
3. `docs/implementation-status.md`
4. `docs/pre-completion-functional-roadmap.md`
5. `.auto-cattery/state.json`
6. `.auto-cattery/reports/stage-14.md`
7. 当前代码、测试、最新运行日志和 `git status --short`

## Confirmed current capability

- 当前 build 限定的 House 原生 `MoveOnly` 已启用。
- 每次进入 House 和每次预览前刷新运行时猫数与房间数。
- 当前按运行时猫数选择存档候选。
- 2 房 8 猫存档已提交 6 次原生移动。
- 2 房 25 猫存档已提交 10 次原生移动。
- 重复执行对已经到位的猫提交 0 次。
- 真实淘汰仍未启用。

## Current business priorities

1. 全房间人数、公母比例和容量优化。
2. 全属性 7 培育与稳定繁殖判定。
3. 全 7 稳定后技能、被动和变异遗传评分。
4. 保护规则接回实时移动。
5. 真实淘汰、预览、journal 和撤销。
6. 相同猫数存档的 CatId 集合级匹配。

## Evidence rules

- 当前代码、当前 build、最新运行日志和玩家实机结果优先于历史阶段文档。
- 历史报告中的“当时未实现”不能覆盖后续已验证实现。
- 网络资料和 Grok 研究只能提供评分输入假设；任何游戏字段、遗传概率、技能
  ID、变异 ID 和机制必须有可追溯来源或本地验证。
- 未确认信息必须标记为未知，并附验证方法，不能写成事实。
- 不修改游戏原始文件、活动存档或 Steam Cloud 数据。

## Completion requirements for this task

- 当前文档不再把工作流描述为 PreviewOnly。
- 剩余业务功能有一个权威路线图。
- Grok 研究提示词可直接复制，并要求最终输出超级详细的可追溯研究文档。
- Debug/Release 构建和测试通过。
- `git diff --check` 通过。
- 只暂存本任务相关文件，创建一个本地 commit，绝不 push。

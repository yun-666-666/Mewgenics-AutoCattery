# CODEX CURRENT TASK - STAGE 30 MAIN-SAVE BREEDING FEEDBACK

## Current objective

分析玩家已启用的本地猫数据历史，从第 257 天到当前主存档逐日比较出生、消失、
七维缺口、性别、父母组合和 COI，并根据实际繁殖结果优化当前 MoveOnly 的繁育
房组合池。不得提交采集 JSON、存档、猫名、个人路径或账号信息。

## Required reading

1. `AGENTS.md`
2. `README.md`
3. `docs/implementation-status.md`
4. `docs/pre-completion-functional-roadmap.md`
5. `.auto-cattery/state.json`
6. `.auto-cattery/reports/stage-29.md`
7. `.auto-cattery/reports/stage-30.md`
8. 当前代码、测试、最新运行日志和 `git status --short`

## Confirmed current capability

- 当前 build 限定的 House 原生 `MoveOnly` 已启用。
- 每次进入 House 和每次预览前刷新运行时猫数与房间数。
- 当前按运行时猫数选择存档候选。
- 2 房 8 猫存档已提交 6 次原生移动。
- 2 房 25 猫存档已提交 10 次原生移动。
- 重复执行对已经到位的猫提交 0 次。
- 真实淘汰仍未启用。
- 通用保护管理器已部署，默认规则为空；只有玩家主动应用才写入稳定猫指纹。

## Current business priorities

工作顺序：先实现用户明确要求、能在游戏里直接测试的核心 MOD 功能，再进行
额外校验、安全加固、报告或研究；除必要构建/测试外，不得让这些工作拖延功能。

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

## Completion requirements for current stage

- 使用多日历史而不是单一最新快照形成可复核汇总。
- 区分自然死亡、其他离开 House 和新增来源，不把未知原因写成事实。
- 繁育房按全体异性交叉配对质量选择稳健组合池，不只选择若干独立高分配对。
- 增加确定性回归，证明独立第二名配对交叉质量差时会被整体更稳健组合替代。
- Debug/Release 构建和测试通过，Release 部署后由玩家继续积累次日出生结果。

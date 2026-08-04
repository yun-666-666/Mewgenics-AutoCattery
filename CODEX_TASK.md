# CODEX CURRENT TASK - F10 IN-GAME MANAGEMENT PANEL

## Current objective

在不更换 Mewjector/MewUI 技术路线的前提下，把现有设置与猫保护管理迁入
House 游戏界面，并用 F10 打开或关闭；不再构建、部署或发布外部 EXE 编辑器。

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

- House 场景按 F10 打开和关闭游戏内总面板，Esc 也能关闭。
- 面板打开时不会把点击或滚轮传给后方 House 操作。
- 提供设置分页与保护分页；配置和保护继续使用现有持久化模型。
- 保护页必须由玩家明确选择猫后才能应用或移除规则。
- 不生成外部编辑器 EXE，不更换或并装另一套 `version.dll` 加载框架。
- Debug/Release 构建和测试通过，Release 部署后等待玩家实机验收。

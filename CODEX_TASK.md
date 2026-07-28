# CODEX CURRENT TASK — STAGE 04 RESTART

## Scope

只实施修订后的阶段 04：验证玩家所指的“新一天＋游戏原生出发牌”画面、战斗归来后的 House 和内部场景的真实对应关系，再实现“标记推荐战斗猫”按钮。

阶段 04 已于 2026-07-28 经玩家确认真实游戏验收通过，状态为
`completed`。本文件仍只记录阶段 04；阶段 05 尚未实施。

## Required reading

修改任何文件前必须完整读取：

1. `AGENTS.md`
2. `AutoCatteryDocs/16_steps/04_次日推荐标记按钮.md`
3. `docs/stage04-failure-retrospective-2026-07-28.md`
4. `docs/implementation-status.md`
5. `docs/phase02-manual-test.md`
6. `docs/phase03-manual-test.md`
7. `AutoCatteryDocs/contracts/自动化核心接口契约.md`
8. `.auto-cattery/state.json`
9. 当前 Git 状态、阶段 02/03 场景与 UI 代码、最新 AutoCattery 运行日志

## Authoritative user-visible target

玩家要求的第四步显示位置是：**新一天开始后、仍能看到家园且右侧存在游戏原生“出发!”牌子的画面**。

玩家截图和同一时间窗口的运行日志是事实来源。不得把 `House`、`ClassChooser` 或 `EmbarkSelectionReady` 的名称直接当作玩家画面的定义。必须先证明它们的对应关系。

玩家在 2026-07-28 进一步确认状态边界：

- “标记推荐战斗猫”只在每天第一次出征前的新一天 House 显示。
- 当天战斗归来后该按钮不再显示，直到结束当天并进入下一天才再次显示。
- 阶段 03“自动整理猫舍”按钮在新一天和战斗归来后的正常 House 都显示。
- 两个 MOD 按钮缩小后横向并排放在右上角资源栏正下方；不得覆盖游戏
  原生“结束一天”或“出发!”牌子及其点击区域。
- 玩家已明确取消“家具模式隐藏阶段 03 按钮”的修复与验收要求；本阶段不再修改或评判该行为。

## Rejected work

以下失败实现不得复用、恢复或继续修改：

- `70c0e69`
- `dbbffa1`
- 后续未提交的 ClassChooser SWF linkage 猜测实验

它们只能通过失败复盘了解错误原因。禁止从中复制 UI、SWF 时间轴、linkage 改名或挂载逻辑。

## Mandatory phase A — read-only evidence first

在正式 UI 实现前：

1. 复现并记录阶段 03 基线：
   - 新一天目标画面；
   - 当天第一次战斗归来后的 House；
   - 点击游戏原生“出发!”牌子前后。
2. 将玩家截图时间与 `SceneContext`、`SceneProbe`、generation、加载场景摘要和组件类型对应起来。
3. 明确证明：
   - 目标画面的真实内部 context；
   - 区分“当天首次出征前”和“已战斗归来”的可靠只读状态信号；
   - 一个可由当前 MewUI API 安全访问的挂载点；
   - 一个不会自动播放、闪烁或暴露其他帧的隐藏/移除方法；
   - generation 变化时的安全清理顺序。
4. 现有日志不足时，只增加最小只读探针。

缺少任何一项证据时必须停止，报告“部分完成/阻塞”，不得继续制作或部署正式按钮/SWF。

## Phase B — implementation only after the evidence gate

证据门禁全部通过后，才允许：

- 在玩家确认的目标画面添加一个稳定、可点击的推荐按钮。
- 使用唯一角色 `AutoCattery.Recommendation.MarkCombatCatsButton`。
- 第一次点击显示纯 MOD 自有演示标记，第二次点击清除。
- 不改变玩家已明确移出范围的家具模式行为。
- 将自建节点绑定 `scene_generation`，先清标记再卸载。

本阶段不读取真实猫或评分，不选择队伍，不修改房间、存档、日期或出征状态。

## Hard prohibitions

- 禁止把 ClassChooser 名称直接等同为玩家目标画面。
- 禁止使用 House 专属的 `test_button`、`test_text`、`test_text_2` 去挂另一个场景。
- 禁止把推荐按钮永久挪到普通 House 来绕过未知挂载点。
- 禁止机械重命名 `HouseStatusUI` SWF linkage 为 `ClassChooser`。
- 禁止使用 `goto-and-play` / `MewUI_PlayMovieClipFrame` 作为未经现场证明的隐藏方案。
- 禁止假定 disabled 等于 hidden。
- 禁止发布 `CLEAN UP!`、`TEST`、`TEST2`、计数器、导航样例或其他原始 MewUI 演示内容。
- 禁止修改游戏原生“出发!”牌子及其点击行为。
- 禁止自动选猫、组队、休息、推进日期或开始出征。
- 禁止在玩家完成全部游戏内门禁前宣称修复完成。
- 禁止联网研究、猜测 API/节点/偏移/场景/linkage，禁止提前实施阶段 05。

## Required validation

- Debug 和 Release 构建。
- 当前单元测试与 DLL load smoke。
- 新增的日内状态、context、generation 和幂等清理测试。
- 玩家验证推荐按钮无闪烁且可点击。
- 玩家验证两个 MOD 按钮不遮挡原生“结束一天”或“出发!”牌子。
- 家具模式行为不再属于阶段 04 验收范围。
- 五十次推荐开关不改变选猫状态。
- 十次目标画面往返无重复和残留。
- 常见分辨率、超宽屏与 80%–150% UI 缩放检查。
- 最新日志无重复挂载错误，且无猫、房间、存档、日期或选队写操作。

自动化测试不能替代游戏内验证。

## Reporting and commit

- 填写 `.auto-cattery/reports/stage-04.md`，分别记录证据阶段与实现阶段。
- 必须记录截图/日志对应关系、真实挂载点和已验证的生命周期清理方法。
- 证据或游戏内门禁未通过时，阶段状态只能为“部分完成”或“阻塞”。
- 只有阶段 04 全部通过后才创建阶段完成提交并调用完成流程。
- 永不 push。

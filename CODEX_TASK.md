# CODEX CURRENT TASK — STAGE 12

## Scope

只实施阶段 12：玩家主动点击现有推荐按钮后，读取并验证 Stage 11 的
MOD sidecar；历史数据不可安全使用时，只允许复用 Stage 6 单猫战斗评分；
CatId→猫卡证据不足时，只运行匿名 UI mapping probe。

不实施阶段 13～16、自动组队、自动选择、自动确认、队伍槽位写入、自动
休息、日期推进、出征导航、猫名模拟标记或任何游戏/存档写入。

## Evidence result

- 当前能力为 `ProbeRequired`，不是 `VerifiedHighlight`。
- Stage 11 writer 的实际 schema 1 只有 checksum、创建 game day、源
  snapshot ID、战斗算法版本、配置摘要及推荐 CatId/排名/分数/置信度。
  它没有 build identity、save identity 或完整并发信息；PreviewOnly
  生产流程也没有 Committed sidecar。
- 玩家实机确认：挑猫和放入出征盒子发生在 `House`；点击游戏原生
  `出发!` 后的 `ClassChooser` 只能查看已装盒的猫，不能选择或更换。
  因此 ClassChooser 不是 Stage 12 推荐入口。
- 当前 House 证据仍不能证明候选 CatId、CatId→可见猫视图、纯视觉
  marker API 或猫移动/装盒生命周期。生产候选 source 因而保持
  Unsupported；Unknown 资格仍由 Stage 6 fail closed。
- 2026-07-29 实机日志证明探针每次均在 House 完成并输出 `AC12102`：
  组件从 606 变为 608 后保持不变；后续装入/移出操作没有产生可区分的
  匿名汇总，因此仍不能把这两个组件认定为猫卡或 CatId 边界。
- 活动 `AGENTS.md` 禁止 web research；用户虽允许联网，本阶段未联网。
- Toolkit 1.0.0 只用于核对 MIT 许可、单猫评分、不自动组队/选择、稳定
  CatId 决胜和确定性输出。其整数 RoomId、六属性、示例 ID、游戏 API、
  UI 节点/卡片/marker 假设均未进入实现。

## Implemented boundary

- sidecar reader 与 Stage 11 writer 共用同一 checksum 实现；文件缺失为
  正常状态，损坏、旧/未来 schema、字段错误或重复身份 fail closed。
- compatibility validator 覆盖 Unknown day、N+1、N+2、配置/算法变化、
  build/save identity 缺失及当前候选交集。
- 即时 provider 只接收 House 当前 generation 下显式验证的候选只读
  source，并直接调用 Stage 6 `RankCombatCats`；没有点击时 0 source/
  0 评分。
- 生产 source 返回 Unsupported，不读取玩家真实存档，也不把存档中的
  House 记录直接冒充当前可见、可装盒的候选视图。
- 复用 Stage 4 按钮。玩家点击后读取 MOD 自有
  `state/recommendations.json` 并武装匿名 mapping probe；按钮显示
  `Probe Required`，不会显示虚假推荐。
- 玩家点击后留在 House，probe 才采集三次稳定匿名样本：generation、
  组件/类型/Button 数量及匿名 type/role digest。重复点击可对比未装盒、
  装入和移出猫时的结构变化。日志不含猫名、CatId、指针、存档名或路径。
- 实机发现 `Probe Required` 只在 ESC 引发 detach/attach 后恢复；原因是
  probe 完成后控制器没有主动恢复按钮状态。现在 `AC12102` 完成后按
  相同 scene generation 保持 `Probe Required` 两秒，再恢复为
  `Mark Combat Cats`。
- 左侧整理按钮的 Completed/Failed 反馈同样保持两秒后恢复 Ready；反馈
  期间不接受重复点击。
- generation/UnsafeTransition 会重置或清除 probe。没有稳定 CatId、
  view identity、视觉 marker 与 recycle 证据时始终不映射、不标记。

## Stage gate

`ProbeRequired; real highlight blocked by unverified House CatId-to-view and visual marker lifecycle`

Stage 12 未完成真实高亮验收，Stage 13 继续 blocked。用户明确授权后，
只读探针 DLL 已部署到真实 MOD 目录；未读取或写入玩家真实存档。需要
玩家按阶段报告中的最小步骤采集只读证据。

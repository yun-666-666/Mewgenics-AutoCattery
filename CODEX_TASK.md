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
- 当前 `ClassChooser` 证据只证明安全业务场景和匿名组件边界，不能证明
  当前候选 CatId、CatId→猫卡映射、纯视觉 marker API 或卡片复用生命
  周期。
- House 存档快照不能冒充 ClassChooser 当前候选集合。生产候选 source
  因而保持 Unsupported；Unknown 资格仍由 Stage 6 fail closed。
- 活动 `AGENTS.md` 禁止 web research；用户虽允许联网，本阶段未联网。
- Toolkit 1.0.0 只用于核对 MIT 许可、单猫评分、不自动组队/选择、稳定
  CatId 决胜和确定性输出。其整数 RoomId、六属性、示例 ID、游戏 API、
  UI 节点/卡片/marker 假设均未进入实现。

## Implemented boundary

- sidecar reader 与 Stage 11 writer 共用同一 checksum 实现；文件缺失为
  正常状态，损坏、旧/未来 schema、字段错误或重复身份 fail closed。
- compatibility validator 覆盖 Unknown day、N+1、N+2、配置/算法变化、
  build/save identity 缺失及当前候选交集。
- 即时 provider 只接收显式的“当前已确认出征候选”只读 source，并直接
  调用 Stage 6 `RankCombatCats`；没有点击时 0 source/0 评分。
- 生产 source 返回 Unsupported，不读取玩家真实存档，也不把 House 猫
  当作 ClassChooser 候选。
- 复用 Stage 4 按钮。玩家点击后读取 MOD 自有
  `state/recommendations.json` 并武装匿名 mapping probe；按钮显示
  `Probe Required`，不会显示虚假推荐。
- 只有玩家点击后又进入 ClassChooser，probe 才采集三次稳定匿名样本：
  generation、组件/类型/Button 数量和角色稳定性。日志不含猫名、CatId、
  指针、存档名或个人路径。
- generation/UnsafeTransition 会重置或清除 probe。没有稳定 CatId、
  view identity、视觉 marker 与 recycle 证据时始终不映射、不标记。

## Stage gate

`ProbeRequired; real highlight blocked by unverified ClassChooser CatId-to-view and visual marker lifecycle`

Stage 12 未完成真实高亮验收，Stage 13 继续 blocked。未部署 DLL，未读取
或写入玩家真实存档。需要玩家按阶段报告中的最小步骤采集只读证据。

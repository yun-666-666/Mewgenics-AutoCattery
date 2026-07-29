# CODEX CURRENT TASK — STAGE 12

## Scope

只实施阶段 12：玩家主动点击现有推荐按钮后，读取并验证 Stage 11 的
MOD sidecar；历史数据不可安全使用时，只允许复用 Stage 6 单猫战斗评分；
CatId→猫卡证据不足时，只运行匿名 UI mapping probe。

不实施阶段 13～16、自动组队、自动选择、自动确认、队伍槽位写入、自动
休息、日期推进、出征导航、猫名模拟标记或任何游戏/存档写入。

## Evidence result

- 2026-07-29 玩家实机日志 `AC12105` 已证明当前 build 的 8 个
  HouseCat 与 8 个只读快照 CatId 存在完整、一致、稳定的双射：
  `offset=128 width=8 matched=8 roots=8 stable_bijection=1`。
- Stage 11 writer 的实际 schema 1 只有 checksum、创建 game day、源
  snapshot ID、战斗算法版本、配置摘要及推荐 CatId/排名/分数/置信度。
  它没有 build identity、save identity 或完整并发信息；PreviewOnly
  生产流程也没有 Committed sidecar。
- 玩家实机确认：挑猫和放入出征盒子发生在 `House`；点击游戏原生
  `出发!` 后的 `ClassChooser` 只能查看已装盒的猫，不能选择或更换。
  因此 ClassChooser 不是 Stage 12 推荐入口。
- 玩家实机验收确认纯文字列表在猫多时难以定位，不满足“标记足够明显”。
  第一版 8 个复制吊牌又暴露了真实 UI 缺陷：每个按钮复制了整条绳子，
  后绘制的绳子/命中区域覆盖前面的按钮；禁用组件也没有隐藏空吊牌，
  且 8 行遮挡原生“出发”按钮。该版验收失败。
- 2026-07-29 实机日志证明探针每次均在 House 完成并输出 `AC12102`：
  组件从 606 变为 608 后保持不变；后续装入/移出操作没有产生可区分的
  匿名汇总，因此仍不能把这两个组件认定为猫卡或 CatId 边界。
- 用户明确要求本任务可联网。已检索公开 Mewgenics/Mewtator/MewUI 资料；
  未找到可直接复用的滚动推荐列表实现，实际修复仍基于本地 MIT MewUI
  源码、FLA/SWF 结构、当前 EXE 和玩家截图。
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
- 匿名总数组无法区分 604→606 的具体来源。当前只读探针追加 `AC12103`
  技术组件类型/数量/根节点数量，以及 `AC12104` Button role/数量；不
  输出猫名、CatId、指针或存档身份。需要用装盒前后差异确定真实类型。
- 2026-07-29 类型日志证明 House 中有 8 个 `HouseCat` 且全部有根节点，
  与只读快照的 8 只猫数量一致。`Ragdoll` 根节点随装盒从 0→1→2，但
  移出后仍为 2，属于缓存状态，不能作为身份边界。
- 新增 `AC12105` HouseCat 身份 probe：在只读快照 CatId 集合与 HouseCat
  组件之间寻找完整、唯一、一致的内存布局双射；日志只输出计数、宽度、
  相对偏移和稳定布尔值，不输出 CatId、指针或存档身份。
- 稳定双射成立后即时复用 Stage 6 单猫评分，按 CatId 生成最多 8 条
  `#排名 猫名 分数 ?`，但界面只复用 4 个无绳子按钮。鼠标停在列表上
  滚轮可逐项向下/向上浏览；点击物理行时会换算为当前可见的真实排名。
  `?` 明示当前 reader 尚不能确认年龄、受伤和出战资格；不使用猫名
  做映射。
- 推荐行使用私有 SWF 按钮角色：删除绳子层，disabled 帧无任何吊牌
  画面并停住。未标记和清除后 4 行真正隐藏，不保留空牌。
- 当前 EXE 本地反汇编验证了 HouseCatClickManager 的原生点击路径：
  House 组件、HouseCat 组件和 `show_drawer=1` 进入详情函数。该调用被
  隔离在 build-specific adapter，调用前验证两段当前 EXE 指令签名、
  House 唯一性、组件类型/归属、House scene 和 generation。
- 详情点击是玩家主动操作，只改变 House 当前查看/绿色焦点猫；不调用
  冒险盒、出征队伍、确认或存档接口。
- `Probe Required` 保持两秒后变为 `Clear Recommendations`；再次点击
  清除列表。generation/UnsafeTransition/离开 House 会清除结果。
- 任何快照、generation、双射、root、评分或文本节点异常均 fail closed。

## Stage gate

`ScrollableClickableDetailsValidationRequired; four rope-free rows deployed`

Release DLL 与 UI 数据 MOD 已部署到真实目录。Stage 12 等待玩家确认
四行显示/隐藏、滚轮浏览、点击打开正确猫详情、场景退出无残留且冒险盒/
出征队伍没有变化；确认前 Stage 13 继续 blocked。

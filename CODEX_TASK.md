# CODEX CURRENT TASK - STAGE 42 EXACT NATIVE FURNITURE TILE SEMANTICS

## Current objective

修复 v0.5.28 实机“点击自动放置但画面不动”：日志已证明按钮和回滚链路正常，
实际是第二步 `object_cinderblock1` 被原生校验拒绝，随后第一步被完整恢复。布局器
必须使用当前游戏原生校验的真实 tile 规则生成临时落点和最终阁楼布局，不能继续
用推测的 Surface/Support 关系发布假合法方案。

## Accepted runtime evidence

- 2026-08-10 20:27 的 v0.5.28 日志：`AC3900` 已收到点击；第一步
  `small_trash_can2` 跨房间提交成功；第二步 `object_cinderblock1` 被拒绝；
  `AC3907 restored=1/1` 成功，所以玩家看到家具保持原位。
- 失败 cinderblock 的两个 Support 计划落在 `set_bone_sink` 的两个 Surface 上。
- 7 猫当前 10 件紧凑实机布局的资源统计为 `Surface=0`，不能证明 Support 可连接
  Surface；此前 Stage 41 的这一 accepted foundation 是错误的。
- 当前 build 原生函数已直接只读反汇编确认：
  - validate RVA `0x2EDE60`：Support tile 3 只接受目标 grid 值 2；
  - Hitbox/Solid tile 1/2 会拒绝目标 grid 值 1、2、5 及房间阻挡值 6、7、8；
  - Surface tile 4 不写入 grid，也不提供值 2 支撑；
  - commit RVA `0x2EE160` 只把 tile 1、2、5 写入 grid，Support/Surface 不占位。
- 最多猫主存档在任何进一步采集前只创建了一份校验一致的离线备份；不得再为本
  阶段创建第二份该存档备份。

## Stage 42 completion boundary

- 规划占用与当前原生 grid 语义一致：
  - Hitbox 与 Solid 都占位，并与既有 Hitbox、Solid、PoopLogic 冲突；
  - Surface 不占位、不阻挡、也不提供支撑；
  - Support 不占位，可共享同一承重点，但每个 Support 必须连接房间或家具 Solid；
  - PoopLogic 按当前原生矩阵隔离建模，不能被静默当作 Empty。
- 当前状态检查、最终 packing、临时撤离、直接安装和依赖保护全部使用同一套规则，
  不能只修最终候选而保留假合法 staging。
- 新增真实骨头水槽 Surface + cinderblock 回归：不得生成 Support-on-Surface 目标；
  同时保留 7 猫 Solid 多层摆放与 couch/dresser 不嵌入回归。
- 只读探针输出每个真实存档中 Support 的提供者分类，至少覆盖 7 猫、24 猫当前、
  24 猫失败前和最多猫主存档；任何 executable plan 的 Support-on-Surface 计数必须
  为 0。
- 原生拒绝日志必须包含 item、来源/目标房间、规划坐标、placement/signature/SEH
  结果，后续失败不再只显示一个家具名。
- 版本升级为 v0.5.29；Debug/Release 构建、单元测试、DLL smoke、真实存档只读
  探针通过后 DLL-only 部署。优先由 Codex 直接完成一次受控游戏内复现；若环境
  无法可靠操作游戏，明确保留玩家最终验收门禁。
- 更新 Stage 42 状态与报告，精确暂存并创建一个本地 commit，不 push。

## Completion evidence

- 24 猫当前存档与失败前存档的 Debug/Release 只读探针都选择 37x11 `Attic`，
  20/20 件已放置家具全部纳入方案，`deferred=0`、`unsupported=0`，Support-on-Surface
  为 0；吊笼通过家具 Solid 承重链进入阁楼，两件 Hitbox-only 墙挂小物也会进入阁楼。
- 按第一批 move 重建最新 live grid 后再次分析，当前与失败前存档均为
  `moves=0`、`kept=20`、`deferred=0`、`target=Attic`。
- 玩家显式点击“开始分析”会清除旧 completed-room lock，并重新捕获当前家具坐标、
  房间 base/live grid；相同 scene generation 下手动改变 live grid 会产生不同 binding
  digest 的确定性回归已通过。
- Debug/Release 完整构建和两套 4/4 CTest 通过；v0.5.29 Release DLL 已部署，build、
  dist、安装 SHA-256 均为
  `F35D2E8380233180992F9F0F40BD9A0FE7C1FE829BE1282A7341849C84DB1293`。
- 自动验证不替代原生实机验收；24 猫测试存档仍需玩家点击自动放置、保存重进，
  并在手动移动一件家具后再次点击开始分析确认界面结论刷新。

## Safety boundary

- 不直接写 `.sav`，不修改 Steam Cloud，不删除或复制第二份最多猫存档备份。
- 游戏内受控测试只能使用原生 House 家具路径；失败必须恢复本批次，游戏进程和
  scene 不安全时不得继续移动。
- 不从仓库取家具，不改变 scale/旋转，不自动移动猫、休息、结束一天、出征、组队
  或淘汰。
- 不把只读求解器通过声明为实机完成；最终完成仍要求原生执行链路成功。

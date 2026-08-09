# CODEX CURRENT TASK - STAGE 33 HOUSE/FURNITURE BUTTON MODE SEPARATION

## Current objective

修复 Stage 32 的界面范围错误：普通 House 必须恢复“自动整理猫舍”和
“标记推荐战斗猫”及原有功能；只有当前 House 加载已验证的
`FurnitureBuildingUI` 组件时，同一对 MOD 按钮才切换为禁用的“自动放置”和
可点击的“开始分析”。

## Required reading

1. `AGENTS.md`
2. `docs/furniture-auto-placement-design.md`
3. `docs/implementation-status.md`
4. `.auto-cattery/state.json`
5. `.auto-cattery/reports/stage-32.md`
6. `docs/stage04-failure-retrospective-2026-07-28.md`
7. 玩家六张实测截图、v0.5.16 日志、当前代码与 `git status --short`

## Confirmed current-build evidence

- 玩家已确认 2/3/4 房家具分析数值正确。
- 玩家截图确认普通 House 被错误显示为“自动放置/开始分析”。
- 历史只读组件探针已确认家具编辑模式存在组件类型
  `FurnitureBuildingUI`；不得猜新的节点名、偏移或场景名。
- 家具模式与普通 House 共用 `test_button` 和 `recommend_button`，必须在运行时
  切换文案、可用性和点击路由。

## Stage 33 completion boundary

- 普通 House：恢复猫舍整理预览/执行与只读战斗猫推荐/详情。
- 家具模式：显示禁用的“自动放置”和“开始分析”；仅运行 Stage 32 只读分析。
- 离开家具模式后清除家具摘要并恢复普通 House 文案；过期异步分析不得显示。
- 模式检测失败时默认普通 House，不调用家具移动、旋转或保存写入。
- 组件类型扫描仅在 House manager 或组件数量变化时执行，不回到每帧全组件扫描。
- Debug/Release 完整构建与 4/4 CTest 通过，部署 v0.5.17 并验证安装哈希。
- 玩家实测普通 House、进入家具、退出家具三段切换后才能确认游戏内验收。

## Out of scope

- 家具用途规划、合法布局、方向/旋转、Anchor、Solid 和原生家具移动。
- 自动组队、出征、结束一天、淘汰或直接修改活动存档。

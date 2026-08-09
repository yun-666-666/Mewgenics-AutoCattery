# CODEX CURRENT TASK - STAGE 32 FURNITURE ANALYSIS BUTTON AND SNAPSHOT

## Current objective

实现家具自动放置设计 F02：家具界面左侧按钮显示为“自动放置”并保持禁用；右侧
按钮显示为“开始分析”。玩家点击“开始分析”后，只读选择当前运行时对应存档，
捕获当前 generation、动态房间集合、当前猫身份和全部家具，生成不可变绑定摘要。
不得移动家具，不得调用旧猫自动整理或战斗推荐点击路径。

## Required reading

1. `AGENTS.md`
2. `docs/furniture-auto-placement-design.md`
3. `docs/implementation-status.md`
4. `.auto-cattery/state.json`
5. `.auto-cattery/reports/stage-31.md`
6. `.auto-cattery/reports/stage-32.md`
7. 当前代码、测试、三个活动存档只读结果和 `git status --short`

## Confirmed current-build evidence

- 第 265 天主档：4 房、257 件家具、仓库 115 件。
- 第 32 天存档：3 房、20 件家具；空的 `Floor1_Small` 可被 House 快照发现。
- 第 17 天存档：2 房、10 件家具；空阁楼可被 House 快照发现。
- 当前三个存档的家具实例对 `furniture_info.data` 和
  `furniture_effects.gon` 覆盖率均为 100%。
- 真实 1/5 房玩家存档仍不可用；动态 1/2/3/5/7 房由同一分析服务合成回归。

## Evidence rules

- 当前代码、当前 build 资源、真实存档只读结果和玩家实测优先于设计文档。
- 当前存档选择优先匹配运行时完整 CatId 集合；只有运行时身份不可用时才退回
  当前 House 猫数量。
- 未识别的空房间保留匿名运行时房间槽，不伪造游戏房间 ID。
- 不输出存档路径、账号、猫名或其他个人信息。
- 不修改活动存档、Steam Cloud 或游戏原始文件。

## Stage 32 completion boundary

- 未点击“开始分析”时，家具分析源读取次数为零。
- 分析绑定当前存档匿名身份、game day、scene generation、猫、动态房间、全部
  家具实例、房间几何、家具信息目录和家具效果目录。
- UI 显示房间数、家具数、仓库数与“自动放置尚未启用”。
- 左侧“自动放置”不可交互；本阶段不存在家具移动、旋转或布局求解。
- 合成测试覆盖 1、2、3、5、7 房，相同输入摘要确定一致；generation 0 和过期
  UI 结果安全拒绝。
- Debug/Release 构建与完整 CTest 通过，三个活动存档只读回归通过。
- 部署玩家可见 Release，更新阶段报告并创建一个本地提交；不 push。

## Remaining F01/F03 work

- 方向/翻转、稀有状态、Solid、Anchor、Background 与 580 字节 payload 仍未知。
- 真实 1 房和 5 房需玩家存档或运行时证据。
- F03 才实现动态房间用途、家具分配和合法布局预览；在布局合法性和原生移动
  得到验证前，“自动放置”继续禁用。

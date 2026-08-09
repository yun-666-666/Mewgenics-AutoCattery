# CODEX CURRENT TASK - STAGE 37 FURNITURE MANUAL MOVE RUNTIME DELTA PROBE

## Current objective

优先打通家具原生移动。复用现有 UI 线程与探针报告框架，在当前已验证的家具
摆放界面中提供 F7 双快照：第一次采集 `FurnitureBuildingUI` 与 House scene 的
两层对象图，玩家手动移动并放下一个家具后第二次采集，输出变化对象、vtable
RVA、字节区间和 `int32/double/pointer` 候选。报告用于下一阶段直接锁定当前 build
的选中家具对象、transform 字段与原生拿起/放置调用接口。

## Required reading

1. `AGENTS.md`
2. `docs/furniture-auto-placement-design.md`
3. `docs/implementation-status.md`
4. `.auto-cattery/state.json`
5. `.auto-cattery/reports/stage-36.md`
6. 玩家完成的 7 猫 `object_cattree1` 移动前后存档差分
7. `docs/stage04-failure-retrospective-2026-07-28.md`
8. 当前 `Mewgenics.exe` 静态证据、三个现有存档、当前资源、代码与
   `git status --short`

## Confirmed current-build evidence

- 当前 `Mewgenics.exe` SHA-256 仍为
  `C3A41E436A93FA58CD386EC46DAD5C2A6F21A583D33C3A57A15A2604C726439E`。
- 7 猫存档中同一 `object_cattree1` 实例手动移动后仅 `x: -6 -> 3`、
  `y: -7 -> -9`；key、item、room、z、flags、scale 与其他 9 件家具完全不变，
  前后 SQLite integrity 均为 `ok`。
- 玩家确认当前家具本身没有旋转入口；MVP 不再等待旋转/翻转证据，只处理现有
  朝向与原生移动。
- Stage 36 已证明存档坐标、完整 24x24 家具网格、房间基础碰撞坐标和提交路径；
  当前缺口集中为运行时选中对象与原生移动接口。

## Stage 37 completion boundary

- F7 第一次按下采集移动前对象图，第二次按下采集移动后对象图并发布 JSON。
- 同时采集 `FurnitureBuildingUI` 与 House scene manager，每个根最多两层、96 个
  可读对象；只输出变化摘要，不输出原始内存字节。
- 报告包含对象地址、父指针偏移、vtable RVA、变化字节区间和有界数值候选。
- 家具界面右侧直接显示“已记录移动前”与“采集完成”，无需查看控制台。
- Debug/Release 聚焦编译和同一单元测试目标通过；Release DLL 直接部署并校验。
- 本阶段不自动移动家具、不直接写 `.sav`，只创建一个本地提交且不 push。

## Out of scope

- 本阶段不猜测对象偏移或原生函数签名；等玩家 F7 报告后直接沿 vtable/RVA 追踪。
- 家具用途规划和合法布局求解不夹带在探针阶段。
- 不启用批量“自动放置”，不直接修改活动存档。
- 自动组队、出征、结束一天、淘汰或直接修改活动存档。

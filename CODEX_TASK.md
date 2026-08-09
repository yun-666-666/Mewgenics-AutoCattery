# CODEX CURRENT TASK - STAGE 34 FURNITURE PLACEMENT GRID READ-ONLY DECODE

## Current objective

继续 F01 的最小只读增量：从当前 `resources.gpak/data/furniture_info.data`
确认并解码每件家具的 12x13 放置网格，使后续合法布局不再缺少家具自身的
Hitbox、Solid、Support、Surface 与 PoopLogic 形状。保持完整 opaque payload，
未知或未来格式必须按单件家具安全降级。

## Required reading

1. `AGENTS.md`
2. `docs/furniture-auto-placement-design.md`
3. `docs/implementation-status.md`
4. `.auto-cattery/state.json`
5. `.auto-cattery/reports/stage-31.md`
6. `docs/stage04-failure-retrospective-2026-07-28.md`
7. Stage 33 玩家验收、当前代码、当前 Debug 构建结果与 `git status --short`

## Confirmed current-build evidence

- 634 条 `furniture_info.data` 记录的 580 字节 payload 中，仅偏移 `224..379`
  在当前资源内变化，正好是 12x13 共 156 字节。
- 当前 634 条网格全部只含 `0..5`；已用本地家具形状与公开家具编辑器说明交叉
  确认：0 Empty、1 Hitbox、2 Solid、3 Support、4 Surface、5 PoopLogic。
- 第 17、32、265 天三个存档的全部活动家具均能映射到可解码网格，覆盖分别为
  10/10、20/20、257/257；资源目录支持 634/634，网格外非零记录为 0。
- Stage 33 已由玩家确认普通 House、进入家具、退出家具三段模式切换正常。

## Stage 34 completion boundary

- 对当前 580 字节记录保留完整 opaque payload，同时暴露固定 12x13 网格。
- 非 `0..5` 的单件记录只标记为不支持，不使整个目录或其他家具解析失败。
- 记录网格外非零字节，供未来 build 识别格式漂移；不为未知字节赋予语义。
- 匿名探针输出目录与活动家具的网格覆盖率及五类非空 tile 总数，不输出路径、
  账号、猫名或家具实例身份。
- Debug/Release 完整构建与 4/4 CTest 通过；三个现有 2/3/4 房存档只读复核通过。
- 本阶段不部署、不改版本、不修改活动存档，只创建一个本地提交且不 push。

## Out of scope

- 家具用途规划、合法布局求解、方向/旋转、Anchor、房间禁放区与原生家具移动。
- 启用“自动放置”、部署新 DLL、修改游戏文件或活动存档。
- 自动组队、出征、结束一天、淘汰或直接修改活动存档。

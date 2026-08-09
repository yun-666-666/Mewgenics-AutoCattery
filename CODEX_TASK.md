# CODEX CURRENT TASK - STAGE 35 FURNITURE RARE FLAG READ-ONLY DECODE

## Current objective

继续 F01 的最小只读增量：根据当前 `Mewgenics.exe` 的创建和序列化路径，确认
存档家具记录原 `u64 unknown_before_room` 的已知位 `0x2` 是 Rare 标志。保留
完整原始位；任何其他位只使该家具实例的 flags 不受支持，不使整个存档解析失败。

## Required reading

1. `AGENTS.md`
2. `docs/furniture-auto-placement-design.md`
3. `docs/implementation-status.md`
4. `.auto-cattery/state.json`
5. `.auto-cattery/reports/stage-34.md`
6. `docs/stage04-failure-retrospective-2026-07-28.md`
7. 当前 `Mewgenics.exe` 静态证据、三个现有存档、当前代码与 `git status --short`

## Confirmed current-build evidence

- 当前 `Mewgenics.exe` SHA-256 为
  `C3A41E436A93FA58CD386EC46DAD5C2A6F21A583D33C3A57A15A2604C726439E`。
- `FurniturePieceEntry` 当前大小为 `0x68`；创建路径 RVA `0x205090` 先把
  `entry+0x28` 清零，读取资源键 `can_be_rare`，允许且请求稀有时执行
  `or qword ptr [entry+0x28], 2`。
- 当前 load/save 路径 RVA `0x22F7E0` / `0x230510` 均把存档中的该 `u64`
  对应到 `entry+0x28`，证明它是持久化 flags，而不是分析工具自行推断的字段。
- 第 17、32 天存档分别为 10/10、20/20 条 flags 支持且 Rare=0；第 265 天为
  257/257 条支持，其中 3 条为 `0x2`，其余 254 条为 0。

## Stage 35 completion boundary

- 将原字段改为保留原值的 `placement_flags`，只把 `0x2` 暴露为 Rare。
- 提供已知位检查；出现 `0x2` 之外的位时该实例安全降级，解析其他家具不失败。
- 匿名探针只输出支持数量、Rare 数量和原始 flags 计数，不输出路径、账号、猫名
  或家具实例身份。
- Debug/Release 完整构建与 4/4 CTest 通过；三个现有存档只读复核通过。
- 本阶段不部署、不改版本、不修改活动存档，只创建一个本地提交且不 push。

## Out of scope

- 尾部两个 `u32` 的语义、方向/旋转、Anchor、房间禁放区和原生家具移动。
- 家具用途规划与合法布局求解。
- 启用“自动放置”、部署新 DLL、修改游戏文件或活动存档。
- 自动组队、出征、结束一天、淘汰或直接修改活动存档。

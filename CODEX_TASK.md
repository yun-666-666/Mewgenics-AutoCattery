# CODEX CURRENT TASK - STAGE 36 FURNITURE GRID COORDINATE READ-ONLY CORRECTION

## Current objective

继续 F01 的只读基础，并纠正 Stage 34 的网格尺寸误判：以当前
`Mewgenics.exe` 的家具初始化、房间网格初始化和坐标转换路径为准，将
`furniture_info.data` 的 580 字节 payload 解码为 4 字节 opaque 头加完整
24x24 byte 网格；将存档尾部两个字段解码为有符号 `scale_x/scale_y`，并只对
`-1/+1` 提供安全网格坐标变换；同时解码房间一格边界扩展和
`built_in_collision` 的运行时行方向。

## Required reading

1. `AGENTS.md`
2. `docs/furniture-auto-placement-design.md`
3. `docs/implementation-status.md`
4. `.auto-cattery/state.json`
5. `.auto-cattery/reports/stage-34.md`
6. `.auto-cattery/reports/stage-35.md`
7. `docs/stage04-failure-retrospective-2026-07-28.md`
8. 当前 `Mewgenics.exe` 静态证据、三个现有存档、当前资源、代码与
   `git status --short`

## Confirmed current-build evidence

- 当前 `Mewgenics.exe` SHA-256 为
  `C3A41E436A93FA58CD386EC46DAD5C2A6F21A583D33C3A57A15A2604C726439E`。
- 家具初始化 RVA `0x2EBC80` 在 `0x2EBD9C` 获得家具信息 payload 后于
  `0x2EBDA1` 跳过 4 字节，再在 `0x2EBE69` 初始化 24x24 网格，并于
  `0x2EBEA0..0x2EBEC6` 复制完整 576 字节。
- 当前 634 条资源记录的 4 字节头均为 0，完整 24x24 网格值均为 `0..5`。
  Stage 34 观察到的 payload `224..379` 只是当前非零单元集中出现的扁平区间，
  不是 12x13 行列布局；旧计数仍正确，但坐标形状解释错误。
- 家具恢复 RVA `0x2EC600..0x2EC627` 将 `entry+0x58/+0x5C` 作为两个有符号
  `i32` 转成 `double` 写入组件 `+0xB8/+0xC0`；提交 RVA
  `0x2EE369..0x2EE3A2` 将这两个组件值取整后写回同一字段。
- 交互 RVA `0x2ECE4B..0x2ECEB5` 只翻转组件 `+0xB8` 的符号；家具网格转换
  RVA `0x2EE880` 与恢复路径共同证明，在当前支持的 `scale_x/scale_y = ±1`
  下，单元映射为 `room = saved_position + scale * local_cell`。
- 水平翻转若保持组件中心不动，保存原点应满足
  `position_x + ceil(11.5 * scale_x)` 不变；因此 `scale_x: 1 -> -1` 时
  `position_x` 应增加 23，反向则减少 23。该静态预测仍需玩家存档差分确认。
- `FurnitureGrid::init` RVA `0x2E8230` 读取房间 width/height 后由
  `0x2E80FC..0x2E8104` 各扩展一格边界；无自定义碰撞时边界值为 2。存在
  `built_in_collision` 时按 `height+2` x `width+2` 全矩阵读取，并反转资源行序
  写入运行时 y 轴。
- 第 17、32、265 天存档当前分别 10/10、20/20、257/257 件家具的 scale 都是
  `(1,1)`；因此仍缺少玩家制造的水平翻转前后差分。

## Stage 36 completion boundary

- 将家具基础网格纠正为 payload offset 4 的完整 24x24，并继续保留全部 580
  字节 opaque payload；非法 tile 只让单件家具网格安全降级。
- 将尾部字段改为有符号 `scale_x/scale_y`；只支持 `±1`，其他值只让该实例的
  网格变换不受支持。
- 提供无写入纯函数，将 24x24 本地单元映射到保存坐标；不生成布局或移动。
- 提供房间基础碰撞网格解码：默认一格值 2 边界，自定义矩阵尺寸必须严格匹配，
  值域必须可保存为 byte，资源行序转成运行时 y 轴；异常房间单独降级。
- 匿名探针输出网格、scale 与房间碰撞支持摘要，不输出路径、账号、猫名或家具
  实例身份。
- Debug/Release 构建与 4/4 CTest 通过；三个现有存档只读复核通过。
- 本阶段不部署、不改版本、不修改活动存档，只创建一个本地提交且不 push。

## Out of scope

- 把静态水平翻转结论当作玩家存档差分；垂直翻转入口、任意缩放和旋转仍未知。
- Anchor、Background、门口、墙面、天花板、斜顶等碰撞值的完整语义。
- 家具用途规划、合法布局求解和任何原生家具拿起/移动/放下。
- 启用“自动放置”、部署新 DLL、修改游戏文件或活动存档。
- 自动组队、出征、结束一天、淘汰或直接修改活动存档。

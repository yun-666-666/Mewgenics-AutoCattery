# Stage 36：家具网格坐标与 scale 只读校正

更新日期：2026-08-09

状态：完成；Debug/Release 完整构建、两套 4/4 CTest、当前资源与三个现有存档
只读复核通过。本阶段不部署、不修改活动存档、不启用自动放置。

## 本阶段边界

- 纠正 Stage 34 将 payload 非零扁平区间解释为 12x13 网格的错误。
- 只按当前 build 静态路径解码完整家具网格、带符号 scale 和房间基础碰撞坐标。
- 只提供纯坐标变换与匿名只读探针，不实现布局、移动、Anchor 或碰撞值完整语义。

## 当前 build 静态证据

- 当前 `Mewgenics.exe` SHA-256：
  `C3A41E436A93FA58CD386EC46DAD5C2A6F21A583D33C3A57A15A2604C726439E`。
- 家具初始化函数 RVA `0x2EBC80`：
  - `0x2EBD9C` 取得家具信息 payload，`0x2EBDA1` 明确跳过 4 byte；
  - `0x2EBE69` 以 24x24 初始化运行时 byte 网格；
  - `0x2EBEA0..0x2EBEC6` 按 24 列、24 行复制全部 576 byte。
- 当前 634 条资源记录的 4 byte 头均为 0，完整 24x24 网格值均为 `0..5`；
  非零 tile 的全局坐标边界为 x=`7..15`、y=`9..15`。Stage 34 的 payload
  `224..379` 只是恰好覆盖当前非零值的扁平带状区间，tile 总数未错，但按 12
  列换行后的形状错误。
- 家具恢复函数 RVA `0x2EBC80`：`0x2EC609..0x2EC627` 将 `entry+0x58/+0x5C`
  的两个有符号 `i32` 转成 double，写入组件 `+0xB8/+0xC0`。
- 家具提交函数 RVA `0x2EE230`：`0x2EE369..0x2EE3A2` 读取上述两个 double，
  取整后写回 `entry+0x58/+0x5C`。存档 load/save 路径按 x/y、z、两个 scale 的
  顺序持久化，当前解析顺序不变，只修正字段类型与名称。
- 家具交互函数 RVA `0x2ECDC0`：`0x2ECE4B..0x2ECEB5` 通过 `-0.0` sign mask
  只翻转组件 `+0xB8` 及关联家具的同字段；未找到 `+0xC0` 的交互翻转路径。
- 网格转换 RVA `0x2EE880` 将 24x24 本地单元中心按 `(x+0.5-12,
  y+0.5-12)` 变换。结合恢复时的 `ceil(11.5*scale)` 原点补偿，当前支持的
  `scale=±1` 精确化简为：
  `room_x = position_x + scale_x * local_x`、
  `room_y = position_y + scale_y * local_y`。
- `FurnitureGrid::init` RVA `0x2E8230`：
  - `0x2E84F5/0x2E8541` 读取 height/width；
  - `0x2E80FC..0x2E8104` 各加 2，形成一格外边界；
  - 无自定义矩阵时 `0x2E8195..0x2E820C` 将四边写为值 2；
  - `0x2E8776..0x2E8876` 读取完整 `built_in_collision`，x 顺序不变，资源行序
    反转后写入运行时 y 轴。

## 实现

- `FurniturePlacementGrid` 改为 payload offset 4 的完整 24x24 byte 网格；580
  byte 原始 payload 继续全部保留，4 byte 头仍为 opaque。
- `unknown_flag_1/2` 改为有符号 `scale_x/scale_y`；只把两个轴均为 `±1` 的实例
  标记为可安全变换，其他值不影响其他家具解析。
- 新增 `MapGridCellToRoom` 纯函数，使用 64 位中间值并在 `i32` 溢出时失败。
- 新增房间基础碰撞解码：
  - 尺寸为 `width+2` x `height+2`；
  - 无矩阵时生成值 2 边界与值 0 内部；
  - 有矩阵时严格检查行列和 byte 值域，并反转资源行序；
  - 任一异常只让当前房间 `supported=false`。
- 匿名探针改为输出 24x24 支持率、signed scale pair、水平/垂直翻转计数，以及
  每个公开房间定义的运行时碰撞尺寸与支持状态。
- Stage 34 报告保留历史结果并增加后续纠错提示；当前设计和实现状态改用 Stage
  36 结论。

## 真实只读验证

- 当前资源：634/634 个完整 24x24 网格支持；4 byte 头非零记录 0；11/11 个
  房间基础碰撞网格支持。
- 第 17 天：10/10 家具网格与 scale 支持，scale `(1,1)` 共 10；Rare 0。
- 第 32 天：20/20 家具网格与 scale 支持，scale `(1,1)` 共 20；Rare 0。
- 第 265 天：257/257 家具网格与 scale 支持，scale `(1,1)` 共 257；Rare 3。
- 三个存档 tile 总数与 Stage 34 相同，因为旧扁平区间覆盖了全部当前非零 byte；
  本阶段修正的是每个 byte 的 24x24 坐标，不是计数。
- 全部探针通过只读 SQLite 与资源读取完成，未写入任何存档。

## 构建与检查

- 聚焦 Debug `auto_cattery_tests` 与 `furniture_geometry_probe` 编译：通过。
- 聚焦 `auto_cattery_tests.exe`：通过。
- `git diff --check`：通过。
- `tools\build.ps1 -Configuration Debug`：自然完成，退出码 0；4/4 CTest 通过，
  DLL 导出和 x64 检查通过。构建完成后对应 15 分钟检查任务已立即删除。
- Debug `furniture_geometry_probe`：第 17、32、265 天结果与“真实只读验证”一致。
- `tools\build.ps1 -Configuration Release`：自然完成，退出码 0；4/4 CTest 通过，
  DLL 导出和 x64 检查通过。构建完成后对应 15 分钟检查任务已立即删除。
- Release `furniture_geometry_probe`：第 17、32、265 天结果与 Debug 一致。
- Release DLL SHA-256：
  `02DE62631370B84BBBF735E612D46AA75BE60691A7110E2EC6908D675456AD8D`。

## 玩家验证门槛

- 当前三个存档全部只有 `(1,1)`，无法用现有真实数据证明玩家水平翻转后确实只
  改变 `scale_x`。
- Stage 36 自动化和静态结论完成后，下一步需要玩家选择一件容易识别的非对称
  家具，保存未翻转状态，再在家具模式水平翻转一次并保存退出。
- 通过标准：同一实例的 `scale_x` 只在 `1/-1` 间切换，`scale_y` 保持 1；家具
  ID、房间、y/z、Rare 与其他实例不出现非预期变化。保存的 x 不要求原样不变，
  而应保持 `position_x + ceil(11.5 * scale_x)` 不变：`1 -> -1` 时 x 增加 23，
  `-1 -> 1` 时 x 减少 23。若测试期间移动、旋转或重新放置家具，本次差分无效。

## 风险与省略的后续工作

- 尚无玩家制造的水平翻转存档差分；静态证据不能替代这个真实保存门槛。
- `scale_y` 已证明是运行时 y scale，但当前未发现玩家垂直翻转入口；不声称游戏
  支持玩家垂直翻转。
- 房间碰撞值 2、6、8 等的完整语义、门口、墙面、天花板、斜顶、Anchor、
  Background 和 Surface 支撑规则仍未知。
- 原生家具拿起、移动、放下、回仓库和结果读回仍未确认。
- “自动放置”继续禁用；版本仍为 v0.5.18，本阶段不部署。

## 交付记录

- 核心：`include/auto_cattery/snapshot/detail/furniture_attributes.hpp`、
  `include/auto_cattery/snapshot/detail/furniture_geometry.hpp`、
  `src/snapshot/furniture_room_attributes.cpp`、
  `src/snapshot/furniture_geometry.cpp`、`src/furniture_analysis/service.cpp`。
- 测试与探针：`tests/furniture_attributes_tests.cpp`、
  `tests/furniture_geometry_tests.cpp`、`tests/furniture_geometry_probe.cpp`、
  `tests/furniture_analysis_service_tests.cpp`。
- 状态与文档：`CODEX_TASK.md`、`docs/furniture-auto-placement-design.md`、
  `docs/implementation-status.md`、`.auto-cattery/state.json`、Stage 34 纠错提示与
  本报告。
- 本地 commit：Release 验证后创建；最终哈希由最终回复记录。
- 是否 push：否

# CODEX CURRENT TASK - STAGE 31 FURNITURE GEOMETRY READ-ONLY PROBE

## Current objective

为家具自动放置功能建立第一个可复核的只读基础：严格读取家具实例身份、仓库/
房间归属、坐标和仍未知的原始字段；从当前 build 的 `resources.gpak` 读取房间
尺寸、阁楼内建碰撞、House 布局以及家具信息目录。不得移动家具、修改活动存档、
改按钮、部署或预先实现布局求解。

## Required reading

1. `AGENTS.md`
2. `docs/furniture-auto-placement-design.md`
3. `docs/implementation-status.md`
4. `.auto-cattery/state.json`
5. `.auto-cattery/reports/stage-30.md`
6. `.auto-cattery/reports/stage-31.md`
7. 当前代码、测试、最新只读探针结果和 `git status --short`

## Confirmed current-build evidence

- `furniture` 表为 `key INTEGER PRIMARY KEY, data BLOB`；SQLite key 是家具实例
  稳定身份的当前只读来源。
- 当前家具 blob 为版本 1、两个 `u32 length + u32 unknown` 字符串头、一个
  `u64 unknown_before_room`、坐标 `i32 x/y + u32 z` 和两个尾部 `u32 unknown`。
- 第 265 天最新存档有 257 件家具：142 已放置、115 在仓库，实际有家具的
  房间为 4 个。所有家具名称和房间长度后的 unknown 均为 0；
  `unknown_before_room` 为 0 的 254 件、为 2 的 3 件；尾部 unknown 均为 1,1。
- `house.gon` 当前有 11 个房间定义、3 个 House 布局；普通矩形房、大小阁楼、
  5 个 Basement 均动态读取。阁楼碰撞矩阵为真实资源数据。
- `furniture_info.data` 当前为版本 1、634 个唯一家具 ID；每行名称头后有
  580 字节仍未解释的 payload。当前全部名称头 unknown 为 0。
- 当前 257 个家具实例对 `furniture_info.data` 和 `furniture_effects.gon` 的
  覆盖率均为 257/257。

## Evidence rules

- 当前代码、当前 build 资源、真实存档只读结果和玩家实测优先于设计文档。
- 网络资料只用于提出或交叉验证格式假设；未由当前资源/存档验证的字段继续命名
  为 unknown，不把它们写成旋转、翻转、稀有度、锚点或碰撞尺寸。
- `furniture_info.data` 的 580 字节 payload 在可靠解码前保持 opaque。
- 不输出存档路径、账号、猫名或其他个人信息。
- 不修改游戏原始文件、活动存档或 Steam Cloud 数据。

## Stage 31 completion boundary

- 严格拒绝错误版本、截断、重复实例 key 和尾随字节。
- 合成测试覆盖仓库家具，以及 1、2、3、5、N>5 房布局，不存在四房固定数组。
- Debug/Release 构建和完整 CTest 通过。
- 对最新存档和当前 `resources.gpak` 运行匿名只读探针。
- 更新阶段报告并创建一个本地提交；不部署、不 push。

## Remaining F01 work

- 用游戏家具编辑器或受控只读运行时探针确认方向/翻转、稀有状态、Solid、Anchor、
  Background 与 580 字节 payload 的具体字段。
- 确认空房间也能被运行时动态发现，不能只依赖当前有家具的房间集合。
- 在这些几何语义得到验证前，不开始 F02 按钮改名，也不启用自动放置。

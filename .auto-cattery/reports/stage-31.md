# Stage 31：家具实例与房间几何只读探针第一增量

更新日期：2026-08-09

状态：实现完成；Debug/Release 自动化与当前存档/资源只读探针通过。未部署、
未修改活动存档、未改按钮、未移动家具。

## 本阶段边界

- 只实现设计 F01 的可确认部分：家具实例、仓库/房间归属、坐标、资源房间尺寸、
  阁楼碰撞、House 布局和家具信息目录。
- 不解释证据不足的字段，不实现方向/翻转、稀有度、占地、Anchor、布局求解、
  按钮改名或原生家具移动。
- 真实活动存档和 `resources.gpak` 全程只读；探针不输出路径、账号或猫名。

## 格式校正

当前 build 与真实数据确认家具 blob 为：

1. `u32 format_version`。
2. `u32 item_length + u32 unknown + item_id`。
3. `u64 unknown_before_room`。
4. `u32 room_length + u32 unknown + room_id`。
5. `i32 x + i32 y + u32 z`。
6. `u32 unknown_flag_1 + u32 unknown_flag_2`。

此前把字符串头整体写成 `u64 length` 只是对当前零高位样本“碰巧可读”，本阶段
已按两个 32 位字段保守建模。公开实现用于交叉验证，未复制其未经本机证明的
语义：

- <https://github.com/michael-trinity/mewgenics-savegame-editor/blob/main/app/utils/parse/furniture.ts>
- <https://github.com/Pseudonym-Tim/mewgenics-furniture-framework>

第二个公开实现也显示 `furniture_info.data` 行头为两个 `u32`；本机资源进一步
验证版本 1、634 行、每行 580 字节 payload，并精确消费到 EOF。第二个头字段
当前全为 0，继续命名为 unknown。

## 当前 build 与第 265 天只读结果

- 家具共 257 件：已放置 142，仓库 115。
- 有家具的房间共 4 个：`Attic` 48、`Floor1_Large` 36、
  `Floor1_Small` 29、`Floor2_Large` 29。
- 坐标范围：x `-10..22`、y `-11..0`、z `0..149`。
- 物品/房间字符串长度后的 unknown：257 条均为 0。
- `unknown_before_room`：254 条为 0，3 条为 2。
- 两个尾部 unknown：257 条均为 `1,1`。
- 当前 257 条实例对 `furniture_info.data` 和 `furniture_effects.gon` 的覆盖率
  均为 257/257。
- `house.gon`：11 个房间定义、3 个 House 布局；普通房为 16x7，SmallAttic
  为 18x5、碰撞 20x7，LargeAttic 为 35x9、碰撞 37x11，另有 5 个 33x5
  Basement 定义。
- House1/2/3 的资源位置数为 2/8/10，其中 House2/3 包含 5 个 Basement，
  不能把这个数量直接当作玩家普通房间数。

## 代码与测试

- `FurnitureStorageRecord` 保留 SQLite key；`ReadFurniture()` 改为读取
  `SELECT key, data`。
- `FurniturePlacement` 保存稳定实例 ID、版本、完整坐标和仍未知的原始字段。
- 新增 `furniture_geometry` 只读资源模块：严格提取 GPAK 目标条目、解析
  `house.gon` 动态房间/布局，并保留 `furniture_info.data` opaque payload。
- 新增匿名 `furniture_geometry_probe`。
- 回归覆盖完整记录、仓库空房间、截断、尾随字节、错误版本、重复 key，及
  1、2、3、5、7 房合成 House 布局和家具目录截断/尾随字节。

## 验证记录

- `git diff --check`：通过。
- `.\tools\build.ps1 -Configuration Debug`：通过，4/4 CTest 通过。
- Debug `furniture_geometry_probe ..`：通过，得到上述第 265 天与当前资源结果。
- `.\tools\build.ps1 -Configuration Release`：通过，4/4 CTest 通过。
- Release `furniture_geometry_probe ..`：通过，与 Debug 匿名摘要一致。
- 未运行 deploy/package；本阶段没有玩家可见按钮或家具移动行为。

## 未完成与风险

- 3 条 `unknown_before_room=2` 的家具种类并不足以证明该字段语义；禁止命名为
  旋转、稀有或锚点。
- `furniture_info.data` 的 580 字节 payload 尚未解码，家具占地、Solid、
  Anchor、Background 和合法旋转仍未知。
- 当前存档只证明 4 个有家具的房间；空房间与真实 1/2/3/5 房动态发现还需
  运行时只读探针或对应玩家存档。
- 在这些未知项解决前，`自动放置` 必须保持不可用，F02 不得绕过几何门槛。

本轮最终本地 commit：由最终回复记录；提交对象不能在自身内容中包含最终哈希。

是否 push：否

# Stage 35：家具 Rare 持久化标志只读解码

更新日期：2026-08-09

状态：完成；Debug/Release 自动化和三个现有存档只读复核通过。本阶段未部署、
未修改活动存档、未启用自动放置。

## 本阶段边界

- 只继续 F01：确认并解码家具实例记录中已由当前可执行文件证明的 Rare 位。
- 保留完整原始 flags；未知其他位按单件实例安全降级。
- 不解释尾部字段，不实现方向、布局、用途规划或原生家具移动。

## 当前 build 静态证据

- 当前 `Mewgenics.exe` SHA-256：
  `C3A41E436A93FA58CD386EC46DAD5C2A6F21A583D33C3A57A15A2604C726439E`。
- `FurniturePieceEntry` 创建路径 RVA `0x205090` 分配 `0x68` 字节记录，将
  `entry+0x28` 初始化为 0，并读取资源键 `can_be_rare`。
- 当调用方请求稀有且资源允许稀有时，RVA `0x205238` 执行
  `or qword ptr [rsi+0x28], 2`。
- load/save 路径 RVA `0x22F7E0` / `0x230510` 均将存档中的该 `u64` 映射到
  `entry+0x28`。因此 `0x2` 是持久化 Rare 标志，不是旋转或方向。

## 实现

- 将 `unknown_before_room` 改为保留原值的 `placement_flags`。
- 新增 `FurniturePlacementFlag::Rare = 0x2`、已知位掩码、Rare 查询和已知位检查。
- 未知位不会使整个家具表解析失败；调用方可通过单件记录的已知位检查安全降级。
- 家具分析绑定摘要继续包含完整 raw flags，确保 flags 变化会使旧分析失效。
- 匿名探针输出 flags 支持数、Rare 数和 raw flags 计数，不输出存档路径、账号、
  猫名或家具实例身份。
- 合成回归覆盖 `0x2` Rare 和 `0x6`（Rare 加未知位）的单件降级。

## 真实只读验证

- 第 17 天：10/10 条只含已知位，Rare 0，raw 0 共 10 条。
- 第 32 天：20/20 条只含已知位，Rare 0，raw 0 共 20 条。
- 第 265 天：257/257 条只含已知位，Rare 3；raw 0 共 254 条、raw 2 共 3 条。
- 三个存档继续保持家具 info/grid/effect 全覆盖；只读 SQLite 与资源探针未写入
  任一存档。

## 构建与检查

- `git diff --check`：通过。
- `tools\build.ps1 -Configuration Debug`：自然完成，退出码 0；原进程未停止、
  取消或重启，也未启动重复构建。
- Debug CTest：4/4 通过；DLL 导出和 x64 检查通过。
- Debug `furniture_geometry_probe`：第 17、32、265 天结果如上。
- `tools\build.ps1 -Configuration Release`：自然完成，退出码 0；原进程未停止、
  取消或重启，也未启动重复构建。
- Release CTest：4/4 通过；DLL 导出和 x64 检查通过。
- Release `furniture_geometry_probe`：第 17、32、265 天结果与 Debug 一致。
- Release DLL SHA-256：
  `8C98B8C9EDE34847215E784B8815648B14B0945A69102FECB30C8F8AFE83EB7B`。

## 风险与省略的后续工作

- `0x2` 之外的所有 placement flags 位仍未知，必须继续按单件实例不支持处理。
- 尾部两个 `u32` 虽然当前三个存档均为 `1,1`，尚无静态或差分证据证明语义。
- 当前证据没有方向/旋转位；实例网格变换、Anchor、门口、斜顶与完整房间禁放区
  仍未知。
- 原生家具拿起、旋转、移动、放下、回仓库和结果读回仍未确认。
- “自动放置”继续禁用；版本仍为 v0.5.18，本阶段不部署。

## 交付记录

- 核心：`include/auto_cattery/snapshot/detail/furniture_attributes.hpp`、
  `src/snapshot/furniture_room_attributes.cpp`、`src/furniture_analysis/service.cpp`。
- 测试与探针：`tests/furniture_attributes_tests.cpp`、
  `tests/furniture_geometry_probe.cpp`。
- 状态与文档：`CODEX_TASK.md`、`docs/furniture-auto-placement-design.md`、
  `docs/implementation-status.md`、`.auto-cattery/state.json` 与本报告。
- 本地 commit：Release 验证后创建；最终哈希由最终回复记录。
- 是否 push：否

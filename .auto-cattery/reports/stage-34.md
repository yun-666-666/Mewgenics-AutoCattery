# Stage 34：家具 12x13 放置网格只读解码

更新日期：2026-08-09

状态：完成；Debug/Release 自动化与三个活动存档只读复核通过。本阶段未部署、
未修改活动存档、未启用自动放置。

## 本阶段边界

- 只继续 F01：从当前 `furniture_info.data` 的 580 字节 payload 解码家具自身
  放置网格。
- 保留完整 opaque payload；格式漂移按单件家具安全降级。
- 不实现用途规划、合法布局、方向/旋转、Anchor、原生家具移动或存档写入。

## 当前 build 格式证据

- 当前 634 条记录仅 payload 偏移 `224..379` 变化，连续 156 字节等于
  `12x13` 网格；其余 payload 字节当前均为 0。
- 所有网格值均为 `0..5`。结合本地已知家具形状与公开家具编辑器的绘制说明，
  确认：0 Empty、1 Hitbox、2 Solid、3 Support、4 Surface、5 PoopLogic。
- 公开实现仅用于交叉确认 tile 语义，最终偏移、尺寸、值域和覆盖率均以当前
  本机 `resources.gpak` 的只读统计为准：
  <https://github.com/Pseudonym-Tim/mewgenics-furniture-framework>。

## 实现

- `FurniturePlacementGrid` 固定保存 12x13 tile，并提供只读访问与计数。
- 解析器复制并保留原 580 字节 payload，再解码网格；遇到大于 5 的值时只把
  当前记录标记为 `supported=false`，其他目录记录仍可使用。
- 额外统计每条记录网格外非零字节，供未来游戏 build 识别格式漂移。
- 匿名探针输出活动家具网格覆盖率、资源目录支持率和五类非空 tile 数量；不输出
  存档路径、账号、猫名或家具实例身份。
- 合成回归覆盖六种 tile、非法 tile 的单记录降级，以及既有截断、尾随字节和
  动态 1/2/3/5/7 房路径；解析实现继续保留完整 opaque payload。

## 真实只读验证

- 第 17 天：10 件家具，网格覆盖 10/10；Hitbox 15、Solid 11、Support 15、
  Surface 0、PoopLogic 0。
- 第 32 天：20 件家具，网格覆盖 20/20；Hitbox 32、Solid 33、Support 37、
  Surface 18、PoopLogic 0。
- 第 265 天：257 件家具，网格覆盖 257/257；Hitbox 606、Solid 395、
  Support 383、Surface 167、PoopLogic 0。
- 三个存档均为 `info/grid/effect` 全覆盖；资源目录 634/634 网格支持，网格外
  非零记录 0。全部通过只读 SQLite 与资源探针完成，零保存写入。

## 构建与检查

- Debug 完整构建自然完成，约 458 秒；未停止、取消、重启或启动重复构建。
- Debug CTest：4/4 通过；DLL 导出和 x64 检查通过。
- Debug 输出：`dist/Debug/AutoCattery.dll`。
- Release 完整构建自然完成，约 7 分 12 秒；原进程未被停止、取消、重启，且
  没有启动重复构建。
- Release CTest：4/4 通过；DLL 导出和 x64 检查通过。
- Release 输出：`dist/Release/AutoCattery.dll`，SHA-256：
  `BB7262314D2C1A223FEEAEDC610F33B82B1E7386C4CB62A80FDEF6957EA2ADB8`。

## 风险与省略的后续工作

- 12x13 网格是家具自身基础形状；实例方向/旋转如何变换网格仍未知。
- 尚未确认 Anchor、Background、墙面/天花板、门口、斜顶禁放区和房间完整
  可放置网格，因此还不能证明任何候选布局合法。
- 当前活动家具未出现 PoopLogic tile，不代表目录中不存在或该语义可忽略。
- 原生拿起、旋转、移动、放下、回仓库和结果读回仍未知。
- “自动放置”继续禁用；未部署新 DLL，版本仍为 v0.5.18。

## 交付记录

- 核心：`include/auto_cattery/snapshot/detail/furniture_geometry.hpp`、
  `src/snapshot/furniture_geometry.cpp`。
- 测试与探针：`tests/furniture_geometry_tests.cpp`、
  `tests/furniture_geometry_probe.cpp`。
- 状态与文档：`CODEX_TASK.md`、`docs/furniture-auto-placement-design.md`、
  `docs/implementation-status.md`、`.auto-cattery/state.json` 与本报告。
- 本地 commit：Release 验证完成后创建；最终哈希由最终回复记录。
- 是否 push：否

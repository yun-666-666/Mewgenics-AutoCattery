# Stage 37：家具手动移动运行时对象图差分探针

更新日期：2026-08-09

状态：实现、Debug/Release 聚焦构建、单元测试、Release 部署和安装校验完成；
等待玩家执行一次 F7 移动前后采集。

## 玩家存档差分输入

- 7 猫存档 `object_cattree1` 稳定实例从 `(-6,-7)` 移到 `(3,-9)`。
- 仅 `x/y` 改变；key、item、room、z、flags、scale 与其他 9 件家具完全不变。
- 前后 SQLite `integrity_check=ok`，家具 key 集合与表行数不变。
- 玩家确认家具没有旋转入口；MVP 固定当前朝向，不再等待旋转/翻转证据。

## 实现

- 新增 F7 双快照控制器：第一次 F7 采集移动前，第二次 F7 采集移动后并写报告。
- 同时以 `FurnitureBuildingUI` 和 House scene manager 为根，最多遍历两层、每根
  96 个可读对象。
- 每个变化对象输出地址、父对象/偏移、vtable RVA、变化字节区间，以及有界的
  `int32`、`double`、pointer 候选。
- JSON 不包含原始内存字节；文件写到已存在的本地 `diagnostics` 目录。
- 家具界面右侧直接显示移动前已记录、第二次 F7 和报告完成状态。
- 探针不调用原生移动函数，不直接写活动存档。

## 构建、测试与部署

- Debug 聚焦 `auto_cattery_tests` 与 `AutoCattery`：通过。
- Debug `auto_cattery_tests.exe`：通过。
- Release 聚焦 `auto_cattery_tests` 与 `AutoCattery`：通过。
- Release `auto_cattery_tests.exe`：通过。
- `git diff --check`：通过，仅有既有 LF/CRLF 提示。
- Release DLL 已直接复制到 `dist/Release` 与游戏 `mods`，未运行会重写数据文件的
  完整部署脚本。
- build、dist、安装 DLL SHA-256 一致：
  `DFEDFADEA642879477DF6E0EEF5E27F6A0DF204B2CFF893264F4315AD707751C`。
- `tools/verify_install.ps1`：通过；14 个职业重投次数保持用户当前值 20。

## 玩家采集步骤

1. 进入 7 猫存档的家具摆放界面，保持家具都已放下。
2. 按一次 F7；右侧应显示“已记录移动前”。
3. 手动移动并放下同一个“猫爬架 A”。
4. 再按一次 F7；右侧应显示“采集完成”和报告文件名。
5. 保存并完全退出游戏，回复 Codex“F7 采集完成”。

通过标准：生成 `furniture-move-probe-*.json`，两个根均完成采集，至少出现一个
变化对象或指针边；报告可用于把 vtable RVA 映射回当前可执行文件。

## 下一步

读取玩家报告，定位选中家具组件、transform/entry 指针和提交调用路径；下一阶段
直接实现只移动一件家具的 `FurniturePlacementGateway`，再进入布局与批量执行。

## 风险与省略的后续工作

- 当前尚未取得玩家 F7 生成的真实报告，因此还没有确认选中家具组件、运行时
  transform/entry 指针和原生拿起/放置调用签名。
- 本阶段只采集对象图变化，不声称已经能够自动移动家具。
- Anchor、Background、门口、墙面、天花板、斜顶与碰撞 byte 完整语义仍未完成；
  合法布局求解、家具到房间分配、批量执行和撤销均留到后续阶段。
- 当前功能版本仍保持现有朝向；玩家已确认家具本身没有旋转入口。

## 交付记录

- 探针核心：`src/ui/mew_ui_furniture_move_probe.h`、
  `src/ui/mew_ui_furniture_move_probe.c`、
  `src/ui/furniture_move_probe_controller.hpp`、
  `src/ui/furniture_move_probe_controller.cpp`、
  `src/ui/furniture_move_probe_report.hpp`、
  `src/ui/furniture_move_probe_report.cpp`。
- 集成与测试：`include/auto_cattery/ui/mew_ui_bridge.hpp`、
  `src/ui/mew_ui_bridge.cpp`、`CMakeLists.txt`、`tests/test_main.cpp`、
  `tests/mew_ui_furniture_move_probe_tests.cpp`。
- 状态与文档：`CODEX_TASK.md`、`docs/furniture-auto-placement-design.md`、
  `docs/implementation-status.md`、`.auto-cattery/state.json` 和本报告。
- 本地 commit：最终差异审查后创建；最终哈希由最终回复记录。

是否 push：否

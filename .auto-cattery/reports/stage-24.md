# Stage 24 当前报告

更新日期：2026-08-05

状态：全房间性别覆盖问题已修复，v0.5.9 已完成 Release 构建、加载检查、部署
和安装哈希校验，等待玩家实机复测。

## 实机证据与根因

- 玩家 85 猫存档为 8 母、77 公，四个目标房约 21 猫；预览把母猫分成
  `1/3/3/1`，并把阁楼从 `3:21` 调成 `1:21`。
- 当前 F10 配置没有目标公母比例参数，因此不是玩家设置导致。
- 旧规划器为每个至少 2 猫的目标房创建一公一母硬性槽，剩余同性猫进入普通
  槽，导致比例既不均匀，也会为了战斗房和普通房的性别覆盖产生无意义移动。

## 本轮修复

- 删除全房间一公一母硬约束。
- 性别槽只按已确认推荐繁育对的实际性别建立；没有可靠繁育对时性别不触发
  任何移动。
- 多房间时先确定最佳战斗培养房，再从其他房间选择繁育对目标房，避免战斗与
  繁育用途重叠。
- 保留房间人数均衡、具体猫用途、保护规则、原生 MoveOnly 和重复预览幂等。

## 文件与验证

- 规划器：`src/room_planning/balanced_move_only_targets.cpp`、
  `balanced_move_only_breeding.cpp`、`balanced_move_only_internal.hpp`。
- 算法版本：`include/auto_cattery/room_planning/domain.hpp`。
- 回归测试：`tests/balanced_move_only_planner_tests.cpp`、
  `tests/deterministic_room_assignment_tests.cpp`。
- Debug 单元测试通过；85 猫极端性别场景确认不再生成 `sex-balance` 移动；
  已确认繁育对进入与战斗培养房不同的房间。
- Release 只构建 `AutoCattery` 和 `dll_smoke_tests` 必要目标，未重复构建 CLI、
  探针或整套 Release 单测。DLL 加载 smoke 退出码为 0。
- 已部署 v0.5.9；构建、`dist/Release` 和游戏安装目录 DLL 的 SHA-256 均为
  `B13B6B4E10FA0A6014EB51A652FE31A1AC4EE39BE58200DD97BFFD4111970DC4`。
- `tools/verify_install.ps1` 通过；玩家现有 `level_up.reroll_count=10` 保持不变，
  14 个职业数据均通过校验。

## 实际命令

- Release 必要目标：`cmake --build build --config Release --target AutoCattery dll_smoke_tests --parallel`。
- DLL 加载：`build/Release/dll_smoke_tests.exe build/out/Release/AutoCattery.dll`。
- 部署：`tools/deploy.ps1 -GameRoot D:/steam/steam/steamapps/common/Mewgenics -Configuration Release`。
- 安装校验：`tools/verify_install.ps1 -GameRoot D:/steam/steam/steamapps/common/Mewgenics`。

## 风险与未包含工作

- 自动化不能证明真实 House 预览中的房间用途和逐猫移动完全符合玩家预期，仍需
  玩家用同一 85 猫存档复测。
- 本阶段不新增房间用途手动配置、不扩展多繁育房管理、不实现淘汰、自动战斗队伍、
  自动休息或自动出征。

## 待验收

- 玩家重新生成同一存档预览，确认战斗房和普通房不再因公母比例调整。
- 有可靠繁育对时，确认两只繁育猫进入独立繁育目标房。

本轮本地 commit：以最终回复中的提交哈希为准。

是否 push：否

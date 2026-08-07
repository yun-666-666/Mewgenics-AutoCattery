# Stage 29：大批量原生移动闪退修复与繁育组合池优化

更新日期：2026-08-07

状态：v0.5.14 最终 Release DLL 已编译、加载 smoke 并部署；按玩家要求停止重复
全量测试，等待主存档实机复测。

## 崩溃证据与根因边界

- 主存档 `steamcampaign01.sav` 第 263 天只读解析正常：88 只 House 猫，
  `errors=0`，稳定 CatId、评分、保护和房间规划均可重复。
- 21:05:11 的整理成功提交 32 次原生移动；21:05:26 生成完整崩溃转储
  `Mewgenics.exe(1).14660.dmp`。异常为 `0xC0000374 STATUS_HEAP_CORRUPTION`，
  崩溃线程与 AutoCattery MewUI 线程一致。
- 用与已部署 DLL 同尺寸的 Release MAP 符号化后，堆检测发生在
  `operator new`，调用链包含 `MewUiBridge::OnTick`、`SceneContextService`、
  `InGamePanelController::Poll/Attach` 和 MewUI 节点查找。该链是检测到已损坏
  堆的位置，不足以证明节点查找本身是最早写坏位置。
- 最新日志中，未整理时 House 往返正常；一次 UI 回调同步提交 32/43 次原生
  移动后才在场景/UI 后续分配窗口崩溃。此前 6/10 次批次已由玩家验证，故本轮
  关闭高风险的单 tick 大批量调用路径，不虚构未定位到的游戏内部写坏指令。

## 修复

- `RuntimeHouseMoveGateway` 先完整验证待执行猫与目标房间，再把单批原生调用限制
  为最多 8 次，并准确返回已完成与剩余数量。
- 首批由玩家第二次点击确认；有剩余时按钮保持 Running，下一 UI tick 才刷新
  实时 House 快照，异步生成新预览，再到后续 UI tick 执行下一批。
- 离开 House、scene generation 改变、视图失效或新预览失败时清空 continuation，
  不再调用剩余批次。
- 工作流和日志明确记录 partial batch 的 `completed_moves`、`remaining_moves`；
  不把 8 次部分提交伪报为整个 43 次计划已一次完成。

## 主存档变化与自动化优化

- 第 262→263 天：House 猫 80→88，母/公 15/65→20/68，成年/幼猫
  73/7→77/11；平均七维缺口 4.66→4.57，缺口不高于 3 的猫 35→38，
  七维最低值至少 6 的猫 38→41。
- 8 只新增幼猫中 3 只缺口 2、4 只缺口 4、1 只缺口 7；整体基础属性改善，
  但没有一只是当天首选 729/761 所生。
- 729/761 为已验证的一公一母成年配对，七维取最大值覆盖 7/7，后代 COI 为 0；
  当前算法过去只保证这对同房，其余 20 个繁育房位置由与繁育质量无关的稳定
  顺序填充，容易稀释目标组合。
- 新规划保留首选对，并从现有合格配对排序中贪心选择不重复配对填充其余繁育
  槽位。排序继续只使用已验证七维、性向和后代 COI；不猜出生概率、不删猫、
  不宣称同房即可强制游戏选择某一对。
- 主存档 Debug 探针仍稳定读取 88 猫、4 房、3916 个 COI；新算法预览目标房为
  `Attic`，需要玩家确认组合池与后续出生结果。

## 文件

- 批次执行与工作流：`runtime_house_move_gateway.*`、
  `house_button_controller.*`、execution/workflow domain、router/facade。
- 繁育组合池：classification domain/classifier、plan digest、
  `balanced_move_only_*`。
- 回归：`runtime_house_state_tests.cpp`、`workflow_execution_router_tests.cpp`、
  `house_button_controller_tests.cpp`、`balanced_move_only_planner_tests.cpp`。
- 版本与文档：CMake、description、README、用户手册、状态、路线图、CHANGELOG、
  v0.5.14 发布说明和本报告。

## 自动化验证

- 实现阶段定向 Debug 回归通过，覆盖 43 次计划每批上限、跨 tick 自动续批、
  scene 取消和繁育组合池；最终 settle-tick 小调整后按玩家要求未重复全量测试。
- Debug `snapshot_probe`：88 猫、4 房、`errors=0`、稳定 ID/评分/保护/房间规划。
- Debug `breeding_data_probe`：FOUNDATION，推荐 729/761，coverage=7，COI=0，
  目标房 `Attic`。
- 最终 Release 必要目标 `AutoCattery` 与 `dll_smoke_tests`：通过；按玩家要求未跑
  重复全量 CTest。
- Release 已部署到游戏目录；build、dist、安装 DLL 均为 1,376,768 字节，
  SHA-256 均为 `97C08E1E4EEB9813134462D656E8E8150EE46104E35D504CBDC475C69D872940`。
- 已安装数据 MOD 为 v0.5.14；玩家当前 `level_up.reroll_count=20` 保持并同步。

## 游戏验证与风险

- 自动化不能证明游戏内部堆损坏已完全消失；必须由玩家在 88 猫主存档执行一次
  大计划，观察日志是否按最多 8 次分批，并在完成后往返 House/结束一天复测。
- 新繁育组合池会对已经按 v7 整理过的主存档产生一次较大的重新分房计划；分批
  执行降低单 tick 风险，但实际繁殖选择仍由游戏决定。
- 未修改活动存档、Steam Cloud、游戏原始文件、淘汰、自动组队、自动休息、
  自动推进日期或自动出征。

本轮最终本地 commit：由最终回复记录；提交对象不能在自身内容中包含最终哈希。

是否 push：否

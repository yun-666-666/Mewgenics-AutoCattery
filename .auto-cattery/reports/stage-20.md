# Stage 20 Report

## 范围

- 修复进入大猫舍时的原生 UI 挂载过渡风险。
- 降低 79 猫及更大 House 存档进入和反复开关 F10 面板时的同步工作量。

## 崩溃证据与判断

- `crash_2026-08-02-13-31-39.txt` 的栈顶为 `FAPOBase_Initialize`/
  `FAPOBase_Reset`，随后进入 `AutoCattery_Shutdown`；WER 记录为
  `0xc0000005` access violation。
- 同一时刻 MOD 日志最后顺序为 `AC18000` 面板节点挂载、`AC14315 cats=79`、
  `AC4100` 推荐按钮挂载，之后没有 `AC18002 F10 opened`。
- 因此无法把崩溃归因到某一个原生函数，但可以确认崩溃发生在玩家未按 F10
  时的 House 自动 UI 挂载窗口；本阶段针对该窗口做了收敛和延迟。

## 实际修改

- `src/ui/in_game_panel_controller.cpp`：House 就绪时不再自动解析面板节点，
  只有 F10 按下或面板已打开时才 Attach。
- `src/ui/mew_ui_management_panel_view.cpp`：隐藏面板只切回隐藏帧，不再同步
  清空约 72 个文本/帧节点；再次显示仍由 `Show()` 按当前页覆盖全部内容。
- `src/ui/mew_ui_bridge.cpp`、`include/auto_cattery/ui/mew_ui_bridge.hpp`：按
  House 场景代次记录就绪时间，推荐按钮至少等待 750ms 后再注册原生按钮和
  回调，离开/切换场景立即重置。

## 构建与检查

- `tools\build.ps1 -Configuration Debug`：成功，CTest `5/5` 通过。
- `tools\build.ps1 -Configuration Release`：成功，CTest `5/5` 通过。
- `tools\deploy.ps1 -GameRoot D:\steam\steam\steamapps\common\Mewgenics
  -Configuration Release`：成功。
- `tools\verify_install.ps1 -GameRoot D:\steam\steam\steamapps\common\Mewgenics`：成功。
- Release DLL 与已部署 DLL SHA-256 均为
  `E7DD12C0B9145ED512880449C1F6F95BBEB213C714E4C5357DBDBA14835B363F`；
  SWF 与已部署 SWF SHA-256 均为
  `B5745CD022EB47DCCCB25661044C0575D73E5C917EFD478A66AF0759118ACAC3`。

## 游戏验证状态

- 自动化构建、测试、部署完成。
- 79 猫存档重新进入 House、F10 打开/关闭、推荐按钮延迟挂载和连续多猫
  存档体感仍需玩家实机确认；本地无法替代游戏进程验收。
- 未修改原始游戏文件、存档或 Steam Cloud。

## 风险与省略的后续工作

- 750ms 是 UI 原生对象稳定等待，不是对游戏内部时序的事实断言；若实机仍有
  崩溃，应继续用最新 WER/MOD 日志缩小到具体原生调用。
- 本阶段没有改变推荐探针的点击后行为，也没有实现自动组队、真实淘汰、撤销
  或后续遗传评分功能。

## Git

- 代码提交：`12744a0 fix: defer house UI attachment for large saves`。
- 报告提交：随本报告单独提交。
- 是否 push：否。

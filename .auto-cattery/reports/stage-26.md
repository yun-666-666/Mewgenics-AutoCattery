# Stage 26 Report

## 范围

- 修复玩家选择主存档后进入 House 闪退、25 猫存档正常的问题。

## 崩溃证据与根因

- `Mods\\AutoCattery\\logs\\auto_cattery.log`：2026-08-02 14:36:54
  进入 `House` 后，在 14:36:56 自动记录 `AC18000` 面板节点挂载，79 猫运行时
  上下文随后触发游戏退出。
- Windows Application Error/WER：14:36:58，`Mewgenics.exe` 在自身偏移
  `0x4c351` 发生 `0xc0000005`；同一轮日志没有玩家按 F10 的记录。
- 对比正常流程：25 猫存档在 14:38:14 进入 House 后可完成 `AC18000`、F10
  打开/关闭和预览，说明问题集中在进入大存档时自动挂载面板的时序窗口。
- 根因是 Stage 24 为处理进入场景闪烁而恢复了 House 就绪时的自动
  `MewUiManagementPanelView::Attach()`；这会在玩家尚未请求 F10 时进行整套
  面板节点查找和隐藏初始化。

## 实际修改

- `src/ui/in_game_panel_controller.cpp`：恢复按需挂载保护。面板未挂载、玩家
  未按 F10 且当前未打开时，House tick 立即返回；按 F10 时仍会正常 Attach、
  打开并渲染面板，已挂载后的关闭/再次打开路径不变。
- `.auto-cattery/reports/stage-26.md`：记录崩溃证据、验证和部署状态。

## 构建与检查

- `tools\\build.ps1 -Configuration Debug`：成功，CTest `5/5`。
- `tools\\build.ps1 -Configuration Release`：成功，CTest `5/5`。
- `tools\\deploy.ps1 -GameRoot D:\\steam\\steam\\steamapps\\common\\Mewgenics -Configuration Release`：成功。
- `tools\\verify_install.ps1 -GameRoot D:\\steam\\steam\\steamapps\\common\\Mewgenics`：通过。
- `git diff --check`：通过。

## 游戏验证状态

- 修复版 Release DLL 和已启用的 Mewtator UI 数据 MOD 已部署。
- 未修改原始游戏文件、活动存档或 Steam Cloud。
- 请完全重启游戏，先进入主存档；确认不再闪退后，再按 F10 验证面板仍能按需
  挂载和打开。25 猫存档仍应保持原有行为。

## 风险与省略的后续工作

- 本地自动化无法替代主存档真实渲染线程验收；若仍崩溃，需要保留新的 WER 和
  崩溃时间附近的 `auto_cattery.log`，继续定位其他 House 原生挂载路径。
- 未改变 79 猫预览、面板缓存和性能优化逻辑，也未实现路线图中的真实淘汰、
  撤销或同猫数存档身份匹配。

## Git

- 本地提交：以最终回复中的提交哈希为准。
- 是否 push：否

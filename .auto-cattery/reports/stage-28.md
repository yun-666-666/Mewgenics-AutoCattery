# Stage 28 Report

## 范围

- 修复 79 猫存档进入 House 后因面板隐藏初始化导致的游戏闪退。
- 保持进入 House 时面板完全隐藏，不重新引入此前的整面板闪烁。

## 根因与修复

- 2026-08-02 15:02:08 的崩溃报告为 `0xC0000374` heap corruption，发生在
  `HouseReady` 后、玩家按 F10 之前；调用栈包含 `AutoCattery.dll` 的 UI 时间轴
  调用路径。
- `PrimeHidden()` 在首个 House tick 对约 70 个自定义 MovieClip 逐个执行原生
  `goto-and-play`，在 79 猫大存档的 UI 初始化窗口触发堆损坏。
- 已移除进入场景时的 `PrimeHidden()` 和对应状态字段。面板 MovieClip 的 SWF
  时间轴新增一个隐藏帧，使 MewUI 默认实例帧索引 2 仍为空；正常帧改为 3，按下
  帧改为 4。进入 House 不再执行任何面板节点查找或时间轴调用，F10 仍按需挂载
  并显示背景、按钮和文字。

## 修改文件

- `src/ui/in_game_panel_controller.cpp`
- `src/ui/mew_ui_management_panel_view.cpp`
- `src/ui/mew_ui_management_panel_view.hpp`
- `tools/swf_panel_shapes.py`
- `assets/swfs/auto_cattery_house.swf`

## 构建与检查

- 重新生成 SWF：`python tools/build_house_ui_asset.py third_party/mew_ui_api/swfs/house_ui_test.swf assets/swfs/auto_cattery_house.swf`。
- `tools\\build.ps1 -Configuration Debug`：成功，CTest `5/5`。
- `tools\\build.ps1 -Configuration Release`：成功，CTest `5/5`。
- SWF 结构探针确认 `panel_background` 定义为 5 帧，默认索引 2 无绘制内容。
- Release 部署：成功。
- `tools\\verify_install.ps1 -GameRoot D:\\steam\\steam\\steamapps\\common\\Mewgenics`：通过。
- `git diff --check`：通过。

## 游戏验证状态

- Release DLL 和重新生成的 SWF 已部署。
- 尚需玩家完全重启游戏后进入 79 猫主存档确认：不闪退、进入 House 不显示面板，按 F10 后背景板和文字正常显示，关闭后保持隐藏。
- 未修改原始游戏文件、活动存档或 Steam Cloud。

## Git

- 本地提交：以最终回复中的提交哈希为准。
- 是否 push：否

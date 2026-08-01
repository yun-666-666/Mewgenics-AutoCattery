# Stage 18 - F10 游戏内设置与保护面板

## 状态

实现、自动化测试和 Release 部署完成；等待玩家在 House 场景实机验收。

## 实现内容

- F10 在 House 打开或关闭原生 MewUI/SWF 面板，Esc 关闭。
- 面板提供 8 个设置分页，覆盖外部设置编辑器的全部现有可编辑项。
- 数值用左右区域减少/增加，布尔值点击切换；每次修改通过现有
  `SettingsFileEditor` 原子写入 `user_config.json`，运行时继续热更新。
- 保护页异步读取本机存档，提供存档切换、5 猫分页、保护等级、固定房间、
  应用和移除；未明确选择猫时拒绝写入。
- 面板打开时停用背后的两个 MOD 按钮，并将鼠标点击和滚轮消息改为
  `WM_NULL`，避免传给 House。
- 保留 `AutoCatterySettings.exe`，未更换 Mewjector `version.dll`。

## 主要文件

- `src/ui/in_game_panel_controller.*`
- `src/ui/in_game_panel_protection.cpp`
- `src/ui/in_game_panel_render.cpp`
- `src/ui/in_game_settings_model.*`
- `src/ui/in_game_settings_pages.cpp`
- `src/ui/mew_ui_management_panel_view.*`
- `src/ui/mew_ui_management_panel_input.cpp`
- `src/ui/mew_ui_bridge.cpp`
- `tools/build_house_ui_asset.py`
- `assets/swfs/auto_cattery_house.swf`
- `tests/in_game_settings_model_tests.cpp`

## 构建与检查

- Debug：`auto_cattery_tests`、`AutoCattery` 编译通过。
- Debug：直接运行 `build/Debug/auto_cattery_tests.exe` 通过。
- Release：`tools/build.ps1 -Configuration Release` 通过。
- CTest：5/5 通过，包括 DLL load、设置编辑器、恢复 CLI、SaveLab smoke。
- SWF 构建器重新生成资产并通过 Python 语法检查。
- Release 已通过 `tools/deploy.ps1` 部署；`tools/verify_install.ps1` 通过。

## 游戏验收待办

- 两房或三房测试存档进入 House 后，验证 F10/Esc 开关。
- 验证设置翻页、左右调整、关闭重开后仍保留。
- 验证保护页必须先选猫，并可应用/移除保护及固定房间。
- 验证面板打开时背景点击和滚轮不触发 House 操作。

## 风险与后续

- 中文字体、非 16:9 缩放、鼠标消息拦截仍需真实游戏画面确认。
- 本阶段没有实现真实淘汰、journal、撤销或自动队伍组成。
- 实现提交：本报告随当前实现提交，玩家验收后在验证提交中记录哈希。
- 是否 push：否

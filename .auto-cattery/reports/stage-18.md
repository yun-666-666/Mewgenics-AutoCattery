# Stage 18 - F10 游戏内设置与保护面板

## 状态

实现、自动化测试和 Release 部署完成；等待玩家在 House 场景实机验收。

## 实现内容

- F10 在 House 打开或关闭原生 MewUI/SWF 面板，Esc 关闭。
- 设置页按外部编辑器布局改为战斗、繁育/分类、房间/安全三栏，一屏覆盖全部
  45 个现有可编辑项；不再用 8 个全宽分页。
- 数值用左右区域减少/增加，布尔值点击切换；每次修改通过现有
  `SettingsFileEditor` 原子写入 `user_config.json`，运行时继续热更新。
- 保护页异步读取本机存档，提供存档切换、每页 9 猫、保护等级、固定房间、
  应用和移除；上一页/下一页及鼠标滚轮均可翻页，未选猫时拒绝写入。
- 面板打开时将鼠标点击和滚轮消息改为 `WM_NULL`，并在 MewUI 的
  `ButtonCanActivate` 链统一拒绝所有原生游戏按钮；关闭或脱离场景立即恢复。
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
- `tools/swf_panel_layout.py`
- `assets/swfs/auto_cattery_house.swf`
- `tests/in_game_settings_model_tests.cpp`

## 构建与检查

- Debug：`tools/build.ps1 -Configuration Debug` 通过。
- Release：`tools/build.ps1 -Configuration Release` 通过。
- Debug/Release 的 CTest 均为 5/5 通过，包括 DLL load、设置编辑器、恢复 CLI、
  SaveLab smoke；新增检查确认三栏扁平映射为 45 项。
- SWF 构建器重新生成资产并通过 Python 语法检查。
- Release 已通过 `tools/deploy.ps1` 部署；`tools/verify_install.ps1` 通过。
- 新版 SWF 重复生成 SHA-256 一致：
  `5D9BDFEE8CC96A4E83A0CFCC7442DAEC52F7A7B62D9D4DE08CB88BD3CFBCDBDB`。

## 游戏验收待办

- 两房或三房测试存档进入 House 后，验证 F10/Esc 开关。
- 验证设置三栏位置、文字大小、左右调整、关闭重开后仍保留。
- 验证保护页必须先选猫，并可应用/移除保护及固定房间。
- 验证保护猫列表可用鼠标滚轮翻页。
- 验证面板打开时“结束一天”和其他背景游戏按钮均不可点击，关闭后恢复。

## 首次实机问题与修复

- 玩家截图确认旧皮肤的小纸片在未按 F10 时覆盖 House 并持续闪烁。
- 最新日志只有 `HouseReady` 和既有按钮附着，没有 F10 面板附着或异常；结合
  代码确认根因是面板节点仅在第一次按 F10 时才被锁到隐藏帧。
- 现改为 House 就绪即附着并锁住隐藏帧；F10/Esc 关闭只隐藏、不解除锁帧。
- 已将横向拉伸纸片换成原创 SWF 矩形皮肤：整块浅色面板、规整列表、金棕
  选中态和黑色描边，不依赖另一套 UI 加载器或游戏封闭资源。
- 修复版 Release 已重新构建和部署，等待玩家再次进入测试存档确认。

## 第二轮布局与输入修复

- 玩家确认整块面板不再闪烁，但指出原设置布局没有复用外部编辑器的三栏分组，
  顶部页签错位、隐藏操作仍留下空框，猫列表也不能用滚轮翻页。
- SWF 现提供独立的三组标题、45 个紧凑设置控件和 12 个保护页宽控件；未使用的
  翻页、应用、移除控件切到隐藏帧，不再保留空白矩形。
- 玩家实测还发现面板打开后仍能点击后方“结束一天”。原 Windows 消息钩子不足以
  阻止游戏自身的 Button 激活路径，现增加真正的模态输入状态，在面板显示期间从
  统一 `ButtonCanActivate` 钩子拒绝所有游戏按钮。
- Release 已重新部署并通过安装检查，等待玩家在测试存档确认三栏布局、滚轮和
  背景按钮拦截。

## 风险与后续

- 三栏新布局、滚轮翻页、原生按钮模态拦截和非 16:9 缩放仍需真实游戏确认。
- 本阶段没有实现真实淘汰、journal、撤销或自动队伍组成。
- 实现提交：本报告随当前实现提交，玩家验收后在验证提交中记录哈希。
- MewUI 依赖提交：`3cf26d8 feat: add modal native button block`。
- 是否 push：否

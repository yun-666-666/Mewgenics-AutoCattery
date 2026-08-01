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
- `src/ui/in_game_panel_settings.cpp`
- `src/ui/in_game_settings_model.*`
- `src/ui/in_game_settings_input.cpp`
- `src/ui/in_game_settings_pages.cpp`
- `src/ui/mew_ui_management_panel_view.*`
- `src/ui/mew_ui_management_panel_input.cpp`
- `src/ui/mew_ui_management_panel_edit.cpp`
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
  `BB8B33CBBC669EF11AD36F94A865859A85F9DF4FACEF34A2426C27733E3AB380`。

## 游戏验收待办

- 两房或三房测试存档进入 House 后，验证 F10/Esc 开关。
- 验证设置三栏位置、文字大小、左右调整、关闭重开后仍保留。
- 验证点击数值中间后可直接输入，首个字符替换旧值，Enter 保存、Esc 取消。
- 验证保护页必须先选猫，并可应用/移除保护及固定房间。
- 验证切换存档后只显示该存档的猫，猫列表为三列布局。
- 验证保护等级和固定房间点击后显示选项页，选择后返回猫列表。
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

## 第三轮可用性修复

- 玩家截图确认三栏布局已经出现，但紧凑文字过小；现按页签、分组、设置行、
  猫卡和选择框分别放大字体，并保持文字宽度适配各自按钮框。
- 数值设置现在保留左右区域微调，点击中间进入面板内数字输入；首个输入字符
  替换旧值，输入过程直接显示在选中行，Enter 原子保存，Esc 取消。
- 修复隐藏帧仍写入独立文字层导致的跨页残留；隐藏控件现在同时清空文字，且
  不可见的翻页、应用和移除区域不再响应点击。
- 保护模型原本就只为当前选中存档重建猫列表，本轮补充回归断言，并把显示改为
  每页三列九猫；切换存档会清空旧选择并从新存档第一页开始。
- 保护等级和固定房间不再左右循环，点击后进入独立选项页；点击选项即返回当前
  存档猫列表，房间超过九项时仍可使用翻页或滚轮。
- Debug/Release 均重新构建且各 5/5 测试通过；Release 已部署并通过安装检查。

## 风险与后续

- 放大字体、直接输入、三列猫卡、选项页、跨页隐藏、原生按钮模态拦截和
  非 16:9 缩放仍需真实游戏确认。
- 本阶段没有实现真实淘汰、journal、撤销或自动队伍组成。
- 实现提交：本报告随当前实现提交，玩家验收后在验证提交中记录哈希。
- MewUI 依赖提交：`3cf26d8 feat: add modal native button block`。
- 是否 push：否

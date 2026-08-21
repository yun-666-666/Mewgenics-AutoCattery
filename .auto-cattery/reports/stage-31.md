# Stage 31：家具摆放模式隐藏 House 原有控制

更新日期：2026-08-21

状态：基于 `v0.5.15`（`f617e917bec3c2dbe91cf099f192cd828ba89b51`）继续修复家具模式退出后的按钮交互恢复；2026-08-21 最新 Debug/Release 构建及各 4/4 CTest 均通过，Release 已部署；等待玩家实机确认两个按钮恢复点击且不再闪退。

## 范围与行为

- 普通 House 保留原有 `自动整理猫舍` 与 `标记推荐战斗猫`，未引入 `自动放置`、`开始分析` 或家具分析/摆放功能。
- 使用玩家在当前游戏 build 已确认的 `FurnitureBuildingUI + 0x78` 字节识别真实家具摆放状态：普通 House 为 `0`，进入摆放为 `1`，退出恢复为 `0`。
- 进入家具摆放后，两项控制均禁用点击，并切换到共享按钮 MovieClip 新增的空白停止帧，完整移除按钮图像和命中区域。
- 退出家具摆放后，两项原有控制恢复到可见帧，并恢复进入前的整理状态及战斗猫标记可用状态。
- `FurnitureBuildingUI` 组件按 House scene manager/组件数变化重新定位；每个 UI tick 只读取已缓存组件的 `+0x78`。组件缺失或读取失败时按普通 House 处理。

## 文件

- 模式检测与桥接：`include/auto_cattery/ui/mew_ui_bridge.hpp`、`src/ui/mew_ui_bridge.cpp`、`src/ui/mew_ui_scene_probe.c`、`src/ui/mew_ui_scene_probe.h`。
- 控制器：`include/auto_cattery/ui/house_button_controller.hpp`、`src/ui/house_button_controller.cpp`、`include/auto_cattery/ui/recommendation_marker_controller.hpp`、`src/ui/recommendation_marker_controller.cpp`。
- 视图与资源：`src/ui/mew_ui_house_button_view.cpp/.hpp`、`src/ui/mew_ui_recommendation_marker_view.cpp/.hpp`、`tools/build_house_ui_asset.py`、`assets/swfs/auto_cattery_house.swf`。
- 回归：`tests/house_button_controller_tests.cpp`、`tests/recommendation_marker_controller_tests.cpp`。

## 验证与部署

- `python -m py_compile tools/build_house_ui_asset.py`：通过。
- SWF 结构检查：共享按钮 81 帧、81 个 `ShowFrame`；新增隐藏帧移除深度 `1/5/8`。
- `git diff --check`：通过（仅 Git 的 LF/CRLF 提示）。
- `tools/build.ps1 -Configuration Debug`：通过，4/4 CTest 通过。
- `tools/build.ps1 -Configuration Release`：通过，4/4 CTest 通过。
- `tools/deploy.ps1 -GameRoot '..' -Configuration Release`：成功；Release DLL 与 UI data mod 已部署，保留玩家当前升级重骰值 20。
- 未启动或控制游戏；未运行 `verify_install.ps1`；未执行哈希/校验和检查。

## 游戏验证与风险

- 玩家需在普通 House 确认两个原有控制可见且可用；通过左上角交叉工具进入家具摆放后，确认两者无闪烁、残留图像或可点击热区；退出后确认两者只恢复一次且无重复实例。
- 日志应在状态切换时出现 `AC3210`，分别记录隐藏与恢复。
- `+0x78` 是当前游戏 build 的实机证据，未来游戏更新后需要重新验证；自动化测试不能替代本轮视觉实机验收。

本轮最终本地 commit：由最终回复记录；提交对象不能在自身内容中包含最终哈希。

是否 push：否

## 2026-08-20 空预览提示文本修复

- 缩短 F10 完整预览页的空预览中文提示，移除按钮名称两侧在当前字体下显示拥挤的中文引号。
- 新提示为：`暂无预览。关闭 F10 后点击自动整理猫舍即可生成。`
- 修改文件：`src/ui/in_game_panel_controller.cpp`。
- 空预览提示修复本身未改动 House 按钮隐藏逻辑；后续合并仍保留了该逻辑。
- 按玩家要求仅执行 Release 构建与部署，不运行 CTest、安装校验、哈希或其他额外检查。

是否 push：否

## 2026-08-21 最近两次家具界面闪退修复

- 玩家操作与本地证据对齐：第一次在家具摆放后退出、两个 House 按钮无法点击，结束一天时于 11:20:28 闪退；第二次重新启动后进入家具摆放，于 11:22:04 闪退。
- 第一次 WER 为 UI 线程 `0xC000001D` 非法指令，转储栈包含 `AutoCattery.dll`；第二次为 `0xC0000374` 堆损坏。两次主 MOD 日志最后的共同新增路径均为 `AC3210` 家具模式按钮隐藏/恢复。
- 第一轮依据时间线行为判断持续播放是崩溃与失去点击的主要触发，并将恢复路径改为停止帧；后续玩家实机证明按钮仍会在家具模式退出后失去点击，因此该判断只覆盖了持续播放风险，不是完整根因。
- `MewUiHouseButtonView` 与 `MewUiRecommendationMarkerView` 的可见帧恢复均改为 `HoldMewUiMovieClipFrame(..., 0)`，与隐藏帧和管理面板既有 goto-and-stop 语义一致；家具模式仍隐藏两个普通 House 按钮，退出后恢复并停在可见帧。
- 聚焦源码检查确认两处可见帧路径均不再调用持续播放。
- `tools/build.ps1 -Configuration Debug`：通过，4/4 CTest 通过。
- `tools/build.ps1 -Configuration Release`：通过，4/4 CTest 通过。
- 确认 `Mewgenics.exe` 未运行后执行 `tools/deploy.ps1 -GameRoot '..' -Configuration Release`：成功；保留玩家重骰值 20，未运行哈希或安装完整性校验。
- 自动化验证不能代替实机生命周期验证；部署后由玩家执行“进入家具界面 -> 摆放 -> 退出 -> 点击两个按钮 -> 结束一天 -> 再次进入家具界面”的完整路径。

是否 push：否

## 2026-08-21 家具模式退出后按钮仍不可点击的第二轮修复

- 玩家复测确认：直接进入 House 时两个按钮均可点击并正常执行；只有经过“进入家具界面 -> 退出回 House”后，按钮画面恢复但点击事件完全消失。失败会话关闭家具模式后没有 `AC3102`、`AC3104` 或推荐按钮点击日志；重新启动、不进入家具模式时点击日志立即恢复。
- 进一步检查发现家具模式隐藏路径同时做了两类操作：`MewUI_SetButtonInteractable(button, 0)` 强制阻止点击，以及 `MewUI_SetButtonEnabled(button, 0)` 直接关闭原生 Button 组件的 update/activate 字节。空白帧已经足以隐藏图像，交互 override 已足以阻止点击；额外关闭组件与“图像恢复但输入状态未恢复”的实机现象一致，是本轮最小移除项。
- 控制器还会在家具模式中向视图传入临时 `Hidden` / `false`，覆盖视图保存的真实整理状态和推荐可用状态；退出时因此出现一次不必要的“恢复画面 -> 仍按临时禁用状态处理 -> 再启用”切换。
- 修复后，家具模式只保留 `FORCE_DISABLED` 交互 override 和空白停止帧，Button 组件继续留在 House 的 update/input buckets；控制器继续保留真实状态/可用性，由原生视图负责家具模式的临时隐藏。退出时直接按真实状态恢复，不再关闭并重新开启组件。
- 更新回归断言：家具模式中控制器仍拒绝点击且 `ShouldShow()` 为 false，但传给原生视图的底层整理状态保持 `Ready`、推荐可用性保持 true，避免临时隐藏污染恢复状态。
- `tools/build.ps1 -Configuration Debug`：通过，4/4 CTest 通过。
- `tools/build.ps1 -Configuration Release`：通过，4/4 CTest 通过。
- 确认 `Mewgenics.exe` 未运行后执行 `tools/deploy.ps1 -GameRoot '..' -Configuration Release`：成功；保留玩家重骰值 20。
- 未运行哈希、`verify_install.ps1` 或安装完整性检查；未启动或操纵游戏。玩家实机点击和闪退路径仍是最终验收门槛。

是否 push：否

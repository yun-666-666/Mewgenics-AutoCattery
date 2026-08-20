# Stage 31：家具摆放模式隐藏 House 原有控制

更新日期：2026-08-20

状态：基于 `v0.5.15`（`f617e917bec3c2dbe91cf099f192cd828ba89b51`）完成最小回退修复；Debug/Release 构建及 4/4 CTest 均通过，Release 已部署；等待玩家实机确认视觉效果。

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

## 2026-08-20 管理面板遗留按钮残影修复

- F10 管理面板打开时，两个 House 控制现在与家具摆放模式共用 SWF 空白停止帧；不再只是禁用点击，因此半透明面板下不再透出按钮文字或图像。
- 关闭 F10 后按当前真实家具模式恢复：普通 House 恢复原按钮，仍处于家具摆放时继续保持隐藏。
- 修改文件：`src/ui/house_button_controller.cpp`、`src/ui/recommendation_marker_controller.cpp`。
- 仅执行 Release 直接构建：`cmake --build build --config Release --parallel`，成功生成 `build/out/Release/AutoCattery.dll`。
- 按玩家要求未运行 CTest、`verify_install.ps1`、哈希或其他额外校验。
- 新 Release DLL 已复制到 `dist/Release` 并通过 `tools/deploy.ps1 -GameRoot '..' -Configuration Release` 部署；等待玩家在游戏内打开 F10 确认残影消失。

是否 push：否

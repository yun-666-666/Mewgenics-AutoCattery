# Stage 26 当前报告

更新日期：2026-08-05

状态：繁育房基本性别均衡与战斗房性别无关选择已实现；v0.5.11 完成必要构建、
单测、DLL 加载检查、部署和安装校验，等待玩家用实际存档复测预览。

## 根因与修复

- v0.5.9 只给推荐繁育对建立一公一母两个槽，繁育房其余位置仍为任意性别，
  因此 8 母、77 公的 21 猫繁育房仍可能显示 `1:20`。
- 战斗推荐还残留一个“混合性别”检查，会为了性别替换更高潜力猫。
- 现在只有确认一公一母推荐繁育对后，独立繁育目标房才按全屋可用公母建立
  最佳可达配额。推荐配对、固定房和不可移动居民先计入；无法 1:1 时安全降级。
- 战斗培养房、训练和普通功能房完全忽略性别，不因比例替换或移动猫。
- 同性或性别未知组合不再作为可产小猫的推荐繁育对。

## 文件

- 规划与配对：`src/room_planning/balanced_move_only_*.cpp`、
  `src/workflow/preview_builder.cpp`、`src/breeding/pair_ranker.cpp`。
- UI 与版本：`src/ui/in_game_preview_model.cpp`、相关 domain 版本、CMake 和资源版本。
- 回归：`tests/balanced_move_only_planner_tests.cpp`、
  `tests/workflow_preview_builder_tests.cpp`、`tests/pair_ranker_tests.cpp`、
  `tests/in_game_preview_model_tests.cpp`。
- 文档：README、用户指南、实现状态、路线图、证据、CHANGELOG 和 v0.5.11 说明。

## 验证

- Debug 必要目标：
  `cmake --build build --config Debug --target auto_cattery_tests --parallel`。
- Debug 单测：`build/Debug/auto_cattery_tests.exe`，退出码 0。
- 覆盖 85 猫、8 母/77 公：21 猫繁育房目标为 `8:13`，所有性别原因移动只进入
  该繁育房；战斗房无性别配额；重复预览 0 移动。
- 覆盖无可靠繁育对：不生成 `sex-balance` 移动。
- 覆盖固定房与 4 只 `NoMove` 公猫占用繁育房：安全降级为 `1:5`，推荐配对仍
  同房，预览不失败并记录限制。
- Release 必要目标：
  `cmake --build build --config Release --target AutoCattery dll_smoke_tests --parallel`。
- DLL smoke、部署和 `verify_install.ps1` 均通过；14 个职业保持 F10 配置的 10 次
  重骰，AutoCattery 仍为 Mewtator 最后加载项。
- 构建、dist 和安装 DLL SHA-256：
  `1BA1336F1E2092C5FA9657FE7171A677D234CA668EED77CEBB23176F9EF0F627`。

## 风险与未包含工作

- 自动化不能代替实际 House 预览，需要玩家确认当前房间属性识别出的繁育房符合
  预期，并确认战斗房不再因性别调整。
- 本阶段不扩展多繁育房手动配置，不实现自动组队、自动休息/推进日期、自动出征
  或真实淘汰。

本轮本地 commit：以最终回复中的提交哈希为准。

是否 push：否

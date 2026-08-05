# Stage 28：8 猫推荐按钮与动态升级重骰修复

更新日期：2026-08-05

状态：两项实机问题已修复；Debug/Release 全量构建与 4/4 CTest 均通过，
v0.5.13 Release 已部署，等待玩家实机验证。

## 玩家问题与根因

1. 8 猫存档无法点击“推荐战斗猫”
   - 最新日志显示进入 `SaveSelectionScreen` 时，运行时仍枚举到残留的
     `Battle`/`Map` 场景。
   - 推荐控制器因此在新存档 House 打开前错误记录 `AC4103`，把新选择的
     8 猫存档继承为“当天已出征”。
   - 修复后，存档选择界面会以 `AC4105` 开始新的推荐生命周期；同一选择界面
     的重复帧不会反复重置或写日志，残留远征场景也不会再次关闭按钮。

2. F10 设置为 11，但升级时仍只有 `SkillsPassivesFirstData` 的 3 次
   - AutoCattery 自己的文件已经包含 `AddLevelUpRerolls 11`，但两套数据 MOD
     同时修改相同职业资源时，玩家当前启动方式仍可能采用旧补丁。
   - 参考并复用了玩家提供的已验证项目
     `D:\steam\steam\steamapps\common\Mewgenics\mewmod\SkillsPassivesFirstData`
     的原生 `AddLevelUpRerolls N` 路线。
   - 数值没有固定为 11。F10 每次保存任意 `0`–`99` 当前值时，AutoCattery 会
     原子写入自己的普通/进阶职业补丁，并在检测到同级
     `SkillsPassivesFirstData` 时同步写入其两份补丁。部署脚本执行相同同步。

## 修改文件

- 推荐生命周期：
  `include/auto_cattery/ui/recommendation_marker_controller.hpp`、
  `src/ui/recommendation_marker_controller.cpp`、
  `src/ui/mew_ui_bridge.cpp`。
- 动态重骰同步：
  `src/level_up_reroll_data.cpp`、`tools/deploy.ps1`、
  `tools/verify_install.ps1`。
- 回归测试：
  `tests/recommendation_marker_controller_tests.cpp`、
  `tests/in_game_settings_model_tests.cpp`。
- 版本与文档：
  `CMakeLists.txt`、`assets/description.json`、`CHANGELOG.md`、`README.md`、
  `README_EN.md`、`docs/USER_GUIDE.md`、`docs/implementation-status.md`、
  `docs/RELEASE_NOTES_v0.5.13.md`、`.auto-cattery/state.json`。

## 自动化验证

- `git diff --check`：通过。
- PowerShell parser 检查：`tools/deploy.ps1`、`tools/verify_install.ps1` 通过。
- 定向 Debug 单元测试：通过。
- `.\tools\build.ps1 -Configuration Debug`：通过，4/4 CTest 通过。
- `.\tools\build.ps1 -Configuration Release`：通过，4/4 CTest 通过。
- `.\tools\deploy.ps1 -GameRoot 'D:\steam\steam\steamapps\common\Mewgenics' -Configuration Release`：
  成功；读取当前 F10 配置为 11，并同步已安装的 `SkillsPassivesFirstData`。
- 按用户要求未运行安装哈希验证。

## 游戏验证、风险与后续范围

- 待玩家确认：切换到 8 猫存档后推荐按钮可点击；使用包含两个数据 MOD 的
  启动参数进入战斗升级时显示 11 次；F10 改成其他数值并重启后同步变化。
- 本轮没有修改存档、Steam Cloud、Steam 启动项、原始游戏文件或用户未跟踪资料。
- `SkillsPassivesFirst.dll` 的技能/被动优先逻辑未修改，只同步其独立数据 MOD
  的升级重骰职业补丁。
- 真实淘汰、自动组队、自动休息/推进日期和自动出征仍未实现。

本轮最终本地 commit：由最终回复记录；Git 提交对象不能在自身内容中包含自己的最终哈希。

是否 push：否


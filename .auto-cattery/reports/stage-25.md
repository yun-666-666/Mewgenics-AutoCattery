# Stage 25 当前报告

更新日期：2026-08-05

状态：F10 重骰覆盖和关闭面板后的 House UI 重挂载风险已修复；v0.5.10 已完成
Debug 单测、Release DLL 构建与加载检查、部署和安装校验，等待玩家实机复测。

## 实机证据与根因

- 玩家配置与 AutoCattery 数据文件均为 `AddLevelUpRerolls 10`，但 Mewtator
  `modlist.txt` 原顺序为 `AutoCattery`、`SkillsPassivesFirstData`。
- 后加载的 `SkillsPassivesFirstData` 对全部职业再次写入 3 次重骰，覆盖了
  AutoCattery；实机看到的 5 次由该 3 次和猫自身两个变异提供的 2 次组成。
- 最新崩溃报告 `mod_logs/crashes/9080-20260805-011141.txt` 为
  `0xC0000374` 堆损坏。崩溃前日志记录 House 场景内 F10 关闭后整理与推荐按钮
  重新挂载；随后大量异常位于游戏 `MewUI_FindChildByName` 内部的过期根访问。
- 原控制流把 F10 临时打开等同于功能关闭，每次打开都卸载两个按钮，关闭后
  重新扫描 UI 根并注册。这增加了在 House 生命周期边界访问失效根的机会。

## 本轮修复

- F10 保存重骰时，原子写入 14 个职业数据后，去重并把当前 AutoCattery 数据
  MOD 移到 `modlist.txt` 末尾；负载顺序写入失败时回滚职业数据文件。
- 部署脚本始终去重并把 AutoCattery 放在最后；安装校验要求它只出现一次且为
  最后一项，避免未来再次静默覆盖。
- F10 打开期间，整理按钮和推荐按钮保留原挂载与回调，仅切换为隐藏/不可交互；
  F10 关闭后直接恢复内部状态，不再 Detach、重新查找和 Attach。
- 真正离开 House、场景 generation 改变或配置明确关闭功能时，原有清理边界
  保持不变。
- v0.5.9 的房间用途规则保持不变：只有已确认的繁育配对房考虑性别；战斗房和
  普通房不因公母比例移动猫。

## 文件与验证

- 重骰与加载顺序：`src/level_up_reroll_data.cpp`、`tools/deploy.ps1`、
  `tools/verify_install.ps1`、`tests/in_game_settings_model_tests.cpp`。
- UI 生命周期：`include/auto_cattery/ui/house_button_controller.hpp`、
  `src/ui/house_button_controller.cpp`、
  `include/auto_cattery/ui/recommendation_marker_controller.hpp`、
  `src/ui/recommendation_marker_controller.cpp`、`src/ui/mew_ui_bridge.cpp` 及对应测试。
- 版本与文档：`CMakeLists.txt`、`assets/description.json`、README、用户手册、
  当前状态、路线图、CHANGELOG 和 v0.5.10 发布说明。

## 实际命令与结果

- Debug 必要目标：`cmake --build build --config Debug --target auto_cattery_tests --parallel`。
- Debug 单测：`build/Debug/auto_cattery_tests.exe`，退出码 0。
- Release 必要目标：
  `cmake --build build --config Release --target AutoCattery dll_smoke_tests --parallel`。
- DLL 加载：`build/Release/dll_smoke_tests.exe build/out/Release/AutoCattery.dll`，
  退出码 0。
- 为避免再次编译，仅把已生成的 Release DLL 与资源同步到 `dist/Release`，随后
  执行 `tools/deploy.ps1 -GameRoot D:/steam/steam/steamapps/common/Mewgenics -Configuration Release`。
- 安装校验：
  `tools/verify_install.ps1 -GameRoot D:/steam/steam/steamapps/common/Mewgenics`，通过。
- 构建、`dist/Release` 和游戏安装 DLL 的 SHA-256 均为
  `E0839A20EABC08E2DDCD225A8060BB50ECCD4815179AC182FBF38528909298AD`。
- 已安装数据 MOD 版本为 v0.5.10；最终加载顺序为
  `SkillsPassivesFirstData`、`AutoCattery`；普通与进阶职业文件各确认 7 条
  `AddLevelUpRerolls 10`。

## 风险与待验收

- 自动化不能证明真实游戏中的最终升级界面次数。玩家需通过 Mewtator 重启游戏，
  用当前带两个额外重骰变异的猫确认显示 12 次，而不是旧的 5 次。
- 玩家需在 House 多次打开/关闭 F10，再往返一次战斗并结束一天，确认按钮不闪、
  House 不出现异常风暴、卡顿或闪退。
- 本阶段不修改繁育房选择、战斗房分配、真实淘汰、自动组队、自动休息或自动出征。

本轮本地 commit：以最终回复中的提交哈希为准。

是否 push：否

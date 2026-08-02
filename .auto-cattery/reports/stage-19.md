# Stage 19 Report

## 范围

- F10 顶级“完整预览”页：逐猫来源/目标/原因/性别/潜力，房间整理前后人数与
  公母比例。
- 缓存 MewUI 节点并跳过未变化更新，移除面板刷新中的重复 House 全场景扫描。
- 中文/英文切换，默认中文；当前 build 无已验证语言读取接口，因此使用手动
  选项。
- 默认关闭、只写本机、隐私最小化的猫技术数据收集。
- 双语使用文档、游戏值参考、致谢与 v0.5.0 发布打包。

## 实际文件

- 配置/收集：`include/auto_cattery/config.hpp`、
  `include/auto_cattery/diagnostics/cat_data_collector.hpp`、
  `src/diagnostics/cat_data_collector.cpp`、`src/config/*`、
  `src/settings_file_editor.cpp`、`config/*`。
- 预览/面板：`src/ui/in_game_preview_model.*`、`src/ui/in_game_panel_*`、
  `src/ui/mew_ui_management_panel_*`、House 按钮与推荐标记视图。
- 规划原因：`src/room_planning/balanced_move_only_assignment.cpp`。
- 资源：`assets/data/text/combined.csv.append`、
  `assets/localization/strings.json`、`assets/swfs/auto_cattery_house.swf`。
- 测试：`tests/cat_data_collector_tests.cpp`、
  `tests/in_game_preview_model_tests.cpp` 及相关现有测试。
- 文档/发布：`README.md`、`README_EN.md`、`ACKNOWLEDGEMENTS.md`、
  `CHANGELOG.md`、`docs/GAME_VALUE_REFERENCE.md`、`tools/package_release.ps1`。

## 命令与结果

- `python tools/build_house_ui_asset.py ...`：成功生成新面板 SWF。
- Debug `auto_cattery_tests` clean build：成功。
- Debug 测试程序：通过，退出码 0。
- `.\tools\build.ps1 -Configuration Debug`：成功，CTest `5/5` 通过，DLL
  导出和 x64 校验通过。
- `.\tools\build.ps1 -Configuration Release`：成功，CTest `5/5` 通过，DLL
  导出和 x64 校验通过。
- `.\tools\deploy.ps1 ... -Configuration Release` 与
  `.\tools\verify_install.ps1 ...`：成功，Release DLL 与 Mewtator 数据 MOD
  已安装并启用。
- 发布资产 SHA-256、GitHub 远端和 Release 校验：见最终发布记录。

## 游戏验证状态

- 自动化完成。
- 25/79 猫存档的 F10 实际帧流畅度、完整预览视觉排版、双语切换和数据开关仍
  需玩家在 House 场景实机确认。
- 真实淘汰不在本阶段范围，仍保持禁用。

## 风险与省略的后续工作

- 当前 build 没有已验证的游戏语言读取入口，故不自动猜测语言。
- 性别未知时比例明确显示未知数，不把未知强行归类为公或母。
- 同猫数不同存档的完整 CatId 集合匹配、真实淘汰、撤销与持久化实机验证仍是
  后续工作。

## Git

- 本地提交：本报告所在的 Stage 19 提交（最终 hash 见 `git log -1`）。
- 是否 push：是（用户明确要求发布 v0.5.0；发布后以 GitHub 远端和 Release
  校验为准）。

## 2026-08-02 后续修复

- 图片中的 `HOUSE.RECOMMEND_ROW` 占位文本来自面板把动态字符串误当作
  localization key；`src/ui/mew_ui_management_panel_view.cpp` 现在使用缓存的
  文本节点和 direct-text API，空字符串也能正确清理旧内容。
- `third_party/mew_ui_api` 新增 `MewUI_SetTextElementText`，本地提交
  `9766e8e`；主仓库只更新对应子模块指针和面板调用。
- `tools/build.ps1 -Configuration Release`：成功；CTest 5/5、DLL load smoke、
  x64 校验通过。
- `tools/deploy.ps1 -GameRoot D:\\steam\\steam\\steamapps\\common\\Mewgenics
  -Configuration Release`：成功，已部署到 Mewjector 与 Mewtator AutoCattery。
- 实机视觉和 79 猫打开面板的体感仍待玩家复测；若仍有延迟，下一步只采集
  `AC18002` 到首帧显示的耗时，不扩大功能范围。

## 2026-08-02 发布与兼容性补充

- `src/save_safety/game_build_gate.cpp` 不再用固定文件大小或 SHA-256 阻止
  MOD 启用；运行时只确认目标是正规的 `Mewgenics.exe`。
- 原生适配器仍逐次检查指针、组件和调用结果；未知游戏布局会安全失败，
  保留面板、只读预览和外部编辑器，不把未来版本误报为已验证兼容。
- README、README_EN、ACKNOWLEDGEMENTS、THIRD_PARTY_NOTICES 和
  `docs/USER_GUIDE.md` 已同步说明 F10 各按钮、数据收集 ZIP→GitHub Issues
  流程、参考 MOD 内容以及 OpenAI GPT-5.6 制作声明。
- `tools/build.ps1 -Configuration Release`：成功，CTest `5/5`、DLL load
  smoke、x64 校验通过。
- `tools/build.ps1 -Configuration Debug`：成功，CTest `5/5`、DLL load smoke、
  x64 校验通过。
- `tools/deploy.ps1 -GameRoot D:\\steam\\steam\\steamapps\\common\\Mewgenics
  -Configuration Release` 与 `tools/verify_install.ps1`：成功。
- 当前回合未完成 F10/House 视觉实机复测；因此不能把新版本兼容性或 25/79 猫
  流畅度写成已由实机证明。

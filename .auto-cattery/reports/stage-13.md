阶段：13
状态：完成；跨存档推荐修复已通过玩家实机复验，可以开始 Stage 14。

本阶段实际能力：
- 配置 schema v2、编译期安全默认值、发布默认配置、用户覆盖和会话覆盖保持
  不变。缺失、损坏、截断或非法配置不会关闭硬安全约束。
- 外部 `AutoCatterySettings.exe` 是唯一的权重/规则编辑界面；它验证候选配置
  后原子写入 MOD 自己的 `config/user_config.json`，并保留未知字段。
- 游戏内设置快捷入口、四个设置按钮及其键盘/鼠标控制已经删除。
- `Auto-Organize Cattery`、`Mark Combat Cats`、推荐列表和推荐详情均保留。
- 配置热加载仍只在工作流 Idle 时应用；应用后使旧预览和旧推荐失效，不会
  自动整理、评分、选猫、休息、推进日期或出征。
- `never_auto_select=true`、破坏性操作前预览、未知 build 中止、离场清理和
  stale 重算不可关闭；当前真实写适配仍不支持。

跨存档推荐故障证据：
- 玩家在同一次实机运行中报告：8 猫存档的 `Mark Combat Cats` 正常，切到
  另一个存档后推荐失效。
- 2026-07-30 13:19:56 的真实运行日志记录 8 猫存档：
  `house_cats=8 requested_ids=8 layouts=2 offset=128 width=8 matched=8`
  且 `selected_exact=1 stable_bijection=1`。
- 2026-07-30 13:21:32 的真实运行日志记录切换后的 25 猫存档：
  `house_cats=25 requested_ids=25 layouts=0 matched=0`
  且 `selected_exact=0 stable_bijection=0`。
- 同一当前游戏 build 的历史真实日志在 2026-07-30 02:46:50 已记录同一个
  25 猫规模存档能以 `offset=128 width=8` 完成 25/25 唯一双射。因此没有把
  猫数量、身份偏移或字段宽度当作新值修改。

根因与修复：
- `src/ui/mew_ui_house_cat_probe.c` 原先要求每个 `HouseCat` 对象起始地址后的
  完整 `0x800` 字节位于同一可读内存区。切换存档后对象重新分配；对象靠近
  内存区末端时，即使真实使用的身份字段仍可读，也会在布局扫描前被拒绝，
  产生 `layouts=0`。
- 探针现在只在每次读取候选 4/8 字节前检查该实际范围是否可读，并保留 SEH
  安全失败路径。
- 没有降低身份门槛：仍要求全量猫 ID 唯一双射、所有有效布局映射一致、
  推荐目标数量精确且 root node 完整；不允许部分或猜测匹配。
- 新增页边界回归测试：79 只猫中的最后一个 `HouseCat` 距可读页末只有
  `0x100` 字节，下一页为 `PAGE_NOACCESS`。测试要求继续完成 79/79 双射、
  命中既有 `0x80`/8 字节身份布局，并返回正确 root node。旧实现会拒绝该
  场景。

本次修改文件：
- 构建和桥接：`CMakeLists.txt`、
  `include/auto_cattery/ui/mew_ui_bridge.hpp`、`src/ui/mew_ui_bridge.cpp`。
- 身份探针：`src/ui/mew_ui_house_cat_probe.c`。
- 回归测试：`tests/mew_ui_house_cat_probe_tests.cpp`、`tests/test_main.cpp`。
- 删除的游戏内设置模块：
  `include/auto_cattery/settings_service.hpp`、
  `include/auto_cattery/ui/settings_panel_controller.hpp`、
  `src/settings_editor.cpp`、`src/settings_service.cpp`、
  `src/ui/settings_panel_controller.cpp`、
  `src/ui/mew_ui_settings_panel_view.hpp`、
  `src/ui/mew_ui_settings_panel_view.cpp`、
  `tests/settings_panel_controller_tests.cpp`、
  `tests/settings_service_tests.cpp`。
- 阶段记录：`.auto-cattery/reports/stage-13.md`、
  `.auto-cattery/state.json`。

保留且未修改：
- 外部设置程序及其配置模型、解析、验证、迁移、热加载和文件编辑服务。
- 推荐按钮、推荐列表、推荐详情和现有全量唯一身份校验。
- 已安装的
  `D:/steam/steam/steamapps/common/Mewgenics/Mods/AutoCattery/config/user_config.json`。
- 用户未跟踪的 Toolkit、文档压缩包和 `PushToMeow/`。

执行的验证：
- `.\tools\build.ps1 -Configuration Debug`：通过；
  `phase13_unit_tests` 6.39 秒、`phase13_dll_load_smoke` 0.04 秒、
  `phase13_settings_editor_validate` 0.02 秒，3/3 通过。
- `.\tools\build.ps1 -Configuration Release`：通过；
  `phase13_unit_tests` 1.06 秒、`phase13_dll_load_smoke` 0.06 秒、
  `phase13_settings_editor_validate` 0.05 秒，3/3 通过。
- `.\tools\deploy.ps1 -GameRoot
  '<GAME_ROOT>' -Configuration Release`：通过。
- `.\tools\verify_install.ps1 -GameRoot
  '<GAME_ROOT>'`：通过。
- Release DLL 的源文件和安装文件均为 809984 bytes，SHA-256 均为
  `B60CE2B41738819F2A5DEBC02DF2BD118F64C76FF9D16C1E92315D7EEFF1BD6A`。
- Release 外部设置程序的源文件和安装文件均为 316928 bytes，SHA-256 均为
  `F4A2ABBE1164A29231A00FF987031DFBECBAC75E0FBF6A16AA7C914C8FC425DF`。
- 部署前后 `user_config.json` 的 SHA-256 均为
  `3345FCA91F5CF268E6DD911A901AC8D24AD01C269BF9CEB2746580C511401C7D`；
  用户配置没有被覆盖。
- `git diff --check`：通过，仅有 Git 的 LF/CRLF 工作区提示。

游戏内验证状态：
- 玩家按要求完成 8 猫存档与 25 猫存档之间的切换复验，确认两边的
  `Mark Combat Cats` 和推荐列表均正常，并确认已删除的游戏内设置入口不再
  响应。
- 2026-07-30 14:34:52 的修复后真实日志记录 25 猫存档：
  `house_cats=25 requested_ids=25 layouts=2 offset=128 width=8 matched=25`
  且 `roots=25 coverage_ready=1 selected_exact=1 stable_bijection=1`。
- 同次日志记录 `marked=10`、`mapped_house_cats=25/25`、
  `expedition_selection_changed=0`，证明推荐显示成功且没有自动选择出征猫。
- 玩家证据与运行日志一致，Stage 13 验收完成，Stage 14 不再阻塞。

已知风险：
- 当前日志中的游戏 build identity 仍显示 `unknown`。本次只沿用已经由同一
  build 多次实机证明的身份布局，不把它升级为通用 build 假设。
- 若实机仍失败，必须依据新日志继续定位，不允许放宽全量双射或 root-node
  安全条件。

明确未实施：
- Stage 14 的真实存档写入、备份管理或恢复工具。
- 自动组队、自动选猫、自动确认、自动休息、日期推进或自动出征。

本地 Stage 13 主实现 commit：
`3ca1fcb4f4e93cbe0af72c451d1542e04e8f44fc`

外部编辑器收尾 commit：
`9cef537e54cc2f121616640bfb8269e1de3616db`

本次兼容性修复 commit：
`69b04e0a2d240a934ee07347764581fc3841cfb8`

是否 push：否

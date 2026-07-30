阶段：13
状态：完成；权重/规则编辑已移到游戏外，Stage 14 不再阻塞

实际能力：
- 配置升级为 schema v2，总 schema 与 general、UI、战斗评分、繁育评分、
  分类、保护、房间规划、执行安全、推荐标记、诊断模块各自有明确版本边界。
- 加载顺序为编译期安全默认值、发布默认配置、用户覆盖、会话覆盖；缺失文件
  使用安全默认值，损坏/截断/非法文件拒绝并保留上一份运行时配置。
- 读取边界包括 1 MiB 文件上限、32 层嵌套、每对象 1024 项、拒绝数组；
  解析和字段验证错误包含 JSON 解析位置或配置路径。
- v1 配置只在内存中迁移至 v2，不改写用户文件，因此没有需要备份的迁移写入；
  未来 schema 强制只读，未知模块版本拒绝。
- 验证覆盖推荐数量 1～100、完整七项已确认 stat 权重、有限数及范围、
  保留池/房间容量边界、未知日志级别、保护与房间 fail-closed 规则。
- `never_auto_select=true`、破坏性操作前预览、未知 build 中止、离场清理和
  stale 重算不可关闭；当前真实写适配仍不支持。关闭备份会强制
  `execution.cull_enabled=false`，不能产生无备份淘汰。
- 文件热加载使用 500ms 去抖，只在工作流 Idle 时应用；忙碌时延迟，失败时
  保持旧不可变配置。配置生效统一更新整理工作流、使旧预览失效并清除旧推荐，
  不自动开始整理、评分、选猫、休息、日期推进或出征。
- 新增应用层 `SettingsService` 和三页设置模型。根据玩家反馈，游戏内只暴露
  简单页的四项快捷设置：推荐数、受伤排除、最低保留数和软溢出；F9 打开/
  关闭，方向键、Enter 和鼠标操作保持不变，Tab/PageDown/PageUp 不再进入
  高级权重或安全规则页。
- 新增独立 Windows x64 程序 `AutoCatterySettings.exe`。战斗/繁育七项权重、
  评分阈值、分类保留池、房间规则、推荐显示和执行安全选项在游戏外编辑，保存
  到 MOD 自己的 `config/user_config.json`。程序先验证候选配置，再以同目录临时
  文件原子替换；非法输入不覆盖旧文件，并保留用户配置中的无关字段。
- 开启单击执行模式前有独立确认对话框；保存配置不会启动整理、评分、选猫、
  休息、日期推进或出征。游戏未启动时下次启动读取，运行中仅在工作流 Idle 时
  由现有热加载服务应用。
- 设置视图仅复用 Stage 12 已实机验证的四个 MOD 自有推荐行、文本节点、命中
  区域和停帧行为；未新增或猜测游戏原生节点、函数、偏移、场景、ID、存档字段
  或 API，未修改 SWF 与游戏原始文件。
- 推荐结果现在遵守已加载的推荐数量、是否显示排名和是否显示分数；仍只对
  单只猫独立评分，不实现组队算法或自动选择。

证据与取舍：
- 检查了仓库已有评分、分类、房间规划、保护、执行和 Stage 12 推荐契约，
  并检查 Toolkit 与已导入 AutoCatteryReference 的选择器边界；没有复制
  Toolkit 示例值或重新实现选择器。
- 当前仓库、既有实机日志和已验收 Stage 12 UI 证据足够，本阶段没有采用
  在线资料，也没有复制外部代码或资源。
- 玩家截图确认当前 build 的 House 中，F9 四项快捷设置与
  `Mark Combat Cats` 按钮同时存在。此次运行时变更只删除分页热键分支，没有
  删除或改名四项设置行、推荐按钮、推荐列表或推荐详情行为。
- 没有提供“保守/平衡/激进”预设：文档中的平衡值没有当前 build 或玩家
  证据，照搬会违反真实值规则。配置文件仍支持全部现有评分覆盖表。
- 没有实现导入/导出按钮：本阶段没有安全、已验证的文本输入/文件选择 UI
  契约；schema 和分层配置文件已可人工复制，未为此发明游戏 API。
- 没有实施 Stage 14 的存档写入、备份管理中心或恢复工具；Stage 10 真实写
  adapter 仍为 unsupported。

修改文件：
- 构建/配置：`CMakeLists.txt`、`config/config.schema.json`、
  `config/default_config.json`、`include/auto_cattery/version.hpp`、
  `tools/build.ps1`、`tools/deploy.ps1`、`tools/verify_install.ps1`。
- 配置模型/运行时：`include/auto_cattery/config.hpp`、
  `include/auto_cattery/config_runtime.hpp`、`src/config.cpp`、
  `src/config/config_json.hpp`、`src/config/defaults.cpp`、
  `src/config/reader.cpp`、`src/config/migration.cpp`、
  `src/config/validation.cpp`、`src/config/decoder.cpp`、
  `src/config_runtime.cpp`。
- 设置应用/UI：`include/auto_cattery/settings_service.hpp`、
  `include/auto_cattery/settings_file_editor.hpp`、
  `include/auto_cattery/ui/settings_panel_controller.hpp`、
  `include/auto_cattery/ui/mew_ui_bridge.hpp`、`src/settings_service.cpp`、
  `src/settings_editor.cpp`、`src/settings_file_editor.cpp`、
  `src/settings_app/settings_app.cpp`、
  `src/settings_app/settings_form.hpp`、
  `src/settings_app/settings_form.cpp`、
  `src/settings_app/settings_form_data.cpp`、
  `src/settings_app/settings_form_schema.hpp`、
  `src/settings_app/settings_form_schema.cpp`、
  `src/ui/settings_panel_controller.cpp`、
  `src/ui/mew_ui_settings_panel_view.hpp`、
  `src/ui/mew_ui_settings_panel_view.cpp`、`src/ui/mew_ui_movie_clip.hpp`、
  `src/ui/mew_ui_movie_clip.cpp`、`src/ui/mew_ui_config_binding.cpp`、
  `src/ui/mew_ui_bridge.cpp`、
  `src/ui/mew_ui_recommendation_marker_view.cpp`。
- 工作流：`include/auto_cattery/workflow/organize_workflow_facade.hpp`、
  `include/auto_cattery/workflow/preview_store.hpp`、
  `src/workflow/organize_workflow_facade.cpp`、
  `src/workflow/preview_store.cpp`、`src/workflow/digests.cpp`。
- 测试：`tests/config_tests.cpp`、`tests/config_boundary_tests.cpp`、
  `tests/config_migration_tests.cpp`、`tests/config_runtime_tests.cpp`、
  `tests/settings_service_tests.cpp`、
  `tests/settings_file_editor_tests.cpp`、
  `tests/settings_panel_controller_tests.cpp`、
  `tests/organize_workflow_facade_tests.cpp`、
  `tests/workflow_digest_tests.cpp`、
  `tests/workflow_preview_store_tests.cpp`、`tests/test_main.cpp`。
- 阶段记录：`.auto-cattery/reports/stage-13.md`、
  `.auto-cattery/state.json`。
- 分发/启动：`tools/open_settings.ps1`；`tools/build.ps1`、
  `tools/deploy.ps1` 和 `tools/verify_install.ps1` 增加独立编辑器处理。

验证：
- `tools/build.ps1 -Configuration Debug`：通过；最终
  `phase13_unit_tests` 4.29 秒、`phase13_dll_load_smoke` 0.03 秒、
  `phase13_settings_editor_validate` 0.02 秒，100% 通过。
- `tools/build.ps1 -Configuration Release`：通过；
  `phase13_unit_tests` 0.54 秒、`phase13_dll_load_smoke` 0.07 秒、
  `phase13_settings_editor_validate` 0.05 秒，100% 通过。
- `git diff --check`：通过；仅 Git 的 LF/CRLF 工作区提示，无空白错误。
- PowerShell `ConvertFrom-Json`：源、Debug 和 Release 的
  `default_config.json`、`config.schema.json` 全部可解析。
- 分发一致性：源/Debug/Release 默认配置 SHA-256 均为
  `23DCB56079A93FFA49462B2C440CCA59FE9100A75CE2926AAFA1542579E02D1B`；
  schema 均为
  `088B36C5533FC9E248576956254BA4DCEF3C962B93FBB0A671DDEB16B8754321`。
- Release 已通过现有部署脚本安装；`tools/verify_install.ps1` 通过。
  安装 DLL 为 836096 bytes，SHA-256
  `94BE3F57177C668435BB49A0C25C629299C7C55FA10278B8AC61F8B93B6116C2`；
  独立编辑器为 316928 bytes，SHA-256
  `F1B0AED7CE75D3FBBF24269EA044C904BDBB4EB75799E071068875FFBB70615A`。
  二者及安装默认配置均与 `dist/Release` 一致；安装路径执行
  `AutoCatterySettings.exe --validate` 返回 0。部署前后的
  `user_config.json` 都不存在，部署没有制造或覆盖用户设置。
- 使用 Windows UI 自动化真实启动 Debug 编辑器并检查完整可访问性树与窗口
  截图：三栏、45 个字段控件、状态区和两个命令按钮均可见，中文无重叠或截断；
  检查后未保存并正常关闭窗口。

自动化覆盖：
- 有效、缺失、非法、截断 JSON，根数组、未知数组、1 MiB 上限、边界值、
  未知枚举/统计项、NaN 字符串、非有限数、v1 迁移、未来 schema、未知模块版本。
- 500ms 去抖、评分/规划/应用期间延迟、失败保留旧配置、成功清错、连续会话
  覆盖合并、Idle 后应用、配置 generation/digest 和失效回调。
- 设置三页、权重精确键匹配、边界调整、只读控件、单击模式双确认、忙碌
  状态提示、会话配置应用。
- 外部编辑器覆盖首次创建、持久保存后重新加载、保留未知字段、候选文件清理、
  非法推荐数量拒绝且原文件字节不变。
- 配置 digest 覆盖执行与保护字段；配置变化重建 PreviewBuilder、清空旧预览，
  非 Idle 拒绝重配。

游戏内验证：
- 玩家截图已经确认 F9 四项快捷设置和 `Mark Combat Cats` 按钮在当前 build
  同时存在。此次只收窄键盘分页路由，不改动这些已验证的 MewUI 节点与控制器。
- 最终权重/规则界面现在是可独立自动验证的 Windows 程序，不再需要玩家进入
  游戏检查高级设置页。Stage 13 验收完成，可以继续 Stage 14。

已知风险：
- F9 四项仍是会话覆盖；外部编辑器写入的文件配置才跨游戏重启保留。这是配置
  分层设计，外部保存不会自动触发工作流。
- Stage 10 真实写 adapter 仍不支持，因此关闭只读或开启单击模式也不会让
  真实移动/淘汰变得可执行。

本地实现 commit：3ca1fcb4f4e93cbe0af72c451d1542e04e8f44fc
提交信息：`feat: add validated hot-reloadable configuration and settings UI`
验证门禁 commit：aa5429a8f1c60d33e14a174a3bcc202ad724e11c
本次玩家反馈收尾：独立编辑器、F9 分页收窄、报告与状态在同一后续提交中；
其最终哈希由提交创建后记录在任务结果中（提交内容不能自引用自身哈希）。
是否 push：否

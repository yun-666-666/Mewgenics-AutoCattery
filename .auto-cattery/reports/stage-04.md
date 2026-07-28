阶段：04
状态：已完成（正式实现已部署，玩家真实游戏验收通过）

最终需求边界：
- 推荐按钮从每天新一天 House 刚出现时就显示，不等待玩家把猫放进箱子。
- 当天进入 Map/Battle 后关闭；战斗归来后的 House 不显示。
- 结束当天经过 Interstitial 并进入下一天 House 后重新显示。
- 阶段 03 按钮在新一天和战斗归来后的正常 House 都显示。
- 玩家已取消家具模式问题的修复与验收要求。

真实证据：
- 20:58:43 新一天 House：原生牌 role 为 EndDay_Sign。
- 20:59:46 猫已放入箱子：同一 House 的原生牌 role 变为 Depart_Sign。
- 21:03:35 战斗归来 House：原生牌再次为 EndDay_Sign。
- 因此不能只用原生牌 role 区分新一天与战斗归来。
- 本阶段使用已观测场景历史：初始 House 可用；Map/Battle 关闭；
  关闭后的 Interstitial→House 重新开启。

性能问题证据与修复：
- 玩家报告第一层后期卡顿，回家丢弃猫时严重卡顿。
- 丢弃猫会让 House 短暂卸载/恢复并重复触发阶段 03 挂载。
- chainloader.log 在 21:03:50–21:03:59 的 House 丢弃猫/恢复窗口记录
  1,500 条 VEH，其中 1,404 条来自同一地址；旧挂载路径会遍历全部
  House 组件并逐个调用类型探测。
- 阶段 03 与阶段 04 现在直接使用各自专属、已知 SWF 节点创建 Button，
  不再执行全场景 Button role 类型扫描。
- 21:27–21:30 最新复测日志确认：第四步按钮挂载成功且点击不会产生
  VEH；但第三步每次点击仍因两段说明文字的场景扫描集中产生约 300 条
  VEH。现已移除 test_text/test_text_2 及对应运行时扫描，第三步完成状态
  仍通过按钮自身的 Preview Complete 标签反馈。
- 同一 House manager 短暂失活后复用已有 Button component，避免重复创建。
- 详细 F8 组件探针已从正式实现撤回，运行时 debug override 已删除。

本轮布局问题与修复：
- 玩家截图确认旧版推荐按钮覆盖了原生“结束一天”牌子的点击区域，
  因而无法结束当天并继续生命周期测试。
- 最新日志 AC4100/AC4102 证明第四步按钮实际已挂载且可响应；此次阻塞
  是布局错误，不是按钮缺失。
- 阶段 03 按钮移动到逻辑坐标 (1010,85)，阶段 04 按钮移动到
  (1175,85)，二者统一缩放为 0.65，横向并排并靠近右上资源栏下方。
- 阶段 04 复制按钮从最高深度 255 降到紧邻阶段 03 的未占用深度 19，
  让原生资源栏像遮挡左侧绳子一样遮挡右侧向上延伸的绳子。
- 第四步英文标签缩短为 Mark Combat Cats，开启状态缩短为
  Demo ON - Click to Clear，以适配紧凑按钮。

主要实现：
- 唯一 role：
  AutoCattery.Recommendation.MarkCombatCatsButton
- 专属 SWF 节点：recommend_button。
- 第一次点击把按钮文字切换为纯 MOD 演示标记开启状态；第二次清除。
- 点击不读取或写入猫、队伍、房间、日期或存档。
- 退出 House/generation 变化时先清演示状态再停用控件。
- ClassChooser 取消返回时仍属于首次出征前；只有 Map/Battle 关闭当天按钮。

修改文件：
- CMakeLists.txt
- CODEX_TASK.md
- AutoCatteryDocs/16_steps/04_次日推荐标记按钮.md
- docs/stage04-evidence-test.md
- tools/build_house_ui_asset.py
- assets/swfs/auto_cattery_house.swf
- assets/data/text/combined.csv.append
- assets/localization/strings.json
- assets/description.json
- include/auto_cattery/ui/mew_ui_bridge.hpp
- include/auto_cattery/ui/recommendation_marker_controller.hpp
- src/ui/mew_ui_bridge.cpp
- src/ui/mew_ui_house_button_view.cpp
- src/ui/mew_ui_house_button_view.hpp
- src/ui/recommendation_marker_controller.cpp
- src/ui/mew_ui_recommendation_marker_view.cpp
- src/ui/mew_ui_recommendation_marker_view.hpp
- tests/recommendation_marker_controller_tests.cpp
- tests/test_main.cpp

构建与测试：
- Debug build：通过。
- Release build：通过。
- Debug phase04_unit_tests：通过。
- Debug phase04_dll_load_smoke：通过。
- Release phase04_unit_tests：通过。
- Release phase04_dll_load_smoke：通过。
- 新增测试覆盖：错误 context 拒绝、幂等挂载、50 次切换、
  离开 House 先清标记、ClassChooser 取消语义、Map/Battle 日内关闭、
  战斗归来保持关闭、Interstitial→下一天重新开启。
- SWF 重复构建 SHA-256 一致。
- SWF placement 检查：test_button 恰好一个，recommend_button 恰好一个；
  test_nav_*、test_toggle、test_text、test_text_2、test_text_3 均未放置。
- 布局 SWF 可重复构建 SHA-256：
  1301FBE7B9E1ED36C0DCF6086F161AF00759CA510A9C1626F13FC755E844BF5C

部署：
- Release DLL 已部署并通过 verify_install.ps1。
- 已安装 DLL 与 dist SHA-256：
  B85EAAA8484682D60220024133D80C265AAC0FCF9DF2D033A828A77449CC5EEA
- 已安装 SWF 与源资产 SHA-256：
  1301FBE7B9E1ED36C0DCF6086F161AF00759CA510A9C1626F13FC755E844BF5C
- 诊断 user_config.json 已删除，正式运行不输出 SceneProbe DEBUG 流。
- 上一轮 AutoCattery 日志没有 WARN/ERROR，也没有猫、房间、日期、
  队伍或存档写操作记录。

玩家真实游戏验收：
- 2026-07-28 玩家明确确认“第四步已经完成了，可以通过了”。
- 原生“结束一天”牌子无遮挡并可点击。
- 两个缩小后的 MOD 按钮在右上资源栏下方横向并排，间距与高度通过。
- 右侧按钮绳子与左侧一致，由原生资源栏遮挡，不再覆盖栏目。
- 新一天/战斗归来生命周期、按钮交互和卡顿修复作为阶段 04 整体通过。
- 家具模式问题已由玩家明确移出范围，不作为验收项。

游戏内当前结论：
- 上一版：推荐按钮未实现，验收失败。
- 21:27 版：按钮已显示并可点击，但错误覆盖原生“结束一天”牌子，
  阻塞后续测试。
- 最终上移及绳子层级修正版：玩家确认通过，阶段 04 完成。
- 家具模式：玩家明确移出范围，不作为通过或失败项。

未实现且留给后续阶段：
- 阶段 05 及真实猫快照/评分；本阶段未提前实施。

本地 commit：本阶段完成提交（实际 SHA-1 见 Git HEAD 与最终回复）
是否 push：否

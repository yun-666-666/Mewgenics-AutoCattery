阶段：12
状态：VisualValidationRequired；安全 House 推荐列表已部署，等待玩家验收

实际能力：
- 玩家点击 House 的 `Mark Combat Cats` 后才读取只读快照、验证当前
  scene generation 并运行映射；未点击时不评分、不映射、不显示。
- 2026-07-29 实机 `AC12105` 证明 8 个只读 CatId 与 8 个 rooted
  HouseCat 完整、一致、稳定双射：offset=128、width=8、matched=8、
  roots=8、stable_bijection=1。
- 复用 Stage 6 `RankCombatCats` 独立评分，最多显示 8 只，不做职业搭配、
  站位或队伍协同。
- 本地 MewUI 没有安全、已验证的 HouseCat 描边/星标 API，因此按阶段
  停止条件使用 MOD 自有 House 文本列表作为视觉回退：
  `* #排名 猫名 分数 ?`。
- 猫名只用于显示，身份匹配只使用稳定 CatId。`?` 表示年龄、受伤和
  出战资格当前仍无法由 reader 确认。
- 点击后 `Probe Required` 保持两秒，再变为 `Clear Recommendations`；
  第二次点击清除列表。退出 House/generation 变化也清除。
- 任一快照、generation、双射、root、评分或 UI 文本节点验证失败均
  fail closed，按钮两秒后恢复，不制造虚假推荐。

安全边界：
- 自动选择、自动装盒、确认出征、队伍槽位写入、自动休息、日期推进、
  出征导航：0。
- 推荐路径没有选择/装盒/write API；日志 `AC12106/AC12107` 明确记录
  `selection_changed=0`，且不记录猫名、CatId、指针或存档路径。
- 未修改游戏原始文件、猫名、猫数据或玩家存档。
- 历史 Stage 11 schema 1 缺 build/save identity，仍不得直接信任；
  当前实现使用即时只读快照重新评分。
- 活动 `AGENTS.md` 禁止 web research；本阶段未联网。

本次修改文件：
- `assets/data/text/combined.csv.append`
- `assets/localization/strings.json`
- `assets/swfs/auto_cattery_house.swf`
- `include/auto_cattery/ui/mew_ui_bridge.hpp`
- `include/auto_cattery/ui/recommendation_marker_controller.hpp`
- `src/ui/mew_ui_bridge.cpp`
- `src/ui/mew_ui_recommendation_marker_view.cpp`
- `src/ui/mew_ui_recommendation_marker_view.hpp`
- `src/ui/recommendation_marker_controller.cpp`
- `tests/recommendation_marker_controller_tests.cpp`
- `tools/build_house_ui_asset.py`
- `CODEX_TASK.md`
- `docs/implementation-status.md`
- `.auto-cattery/state.json`
- `.auto-cattery/reports/stage-12.md`

验证：
- House SWF 从固定 MewUI MIT 示例重新生成：通过；恰好保留并重命名一个
  `recommend_summary` 文本节点。
- Debug build：通过。
- Debug `phase12_unit_tests`：通过（5.60 秒）。
- Debug `phase12_dll_load_smoke`：通过（0.04 秒）。
- Release build：通过。
- Release `phase12_unit_tests`：通过（0.59 秒）。
- Release `phase12_dll_load_smoke`：通过（0.07 秒）。
- 新测试覆盖 stale generation 拒绝、两秒状态保持、列表显示、第二次
  点击清除及不重复发起评分请求。
- Release DLL 已部署：
  `D:\steam\steam\steamapps\common\Mewgenics\Mods\AutoCattery.dll`
- Mewtator UI 数据 MOD 已部署并启用：
  `D:\steam\steam\steamapps\common\Mewgenics\Mewtator\mods\AutoCattery`
- 构建与安装 DLL 大小均为 715776 bytes，SHA-256 均为
  `F2BB8B3588045F3D70AAD1218D2E595CA590B47FBD9227514AA28B7306A4FB6D`。

玩家最终验收：
1. 通过 Mewtator 启动游戏，进入可挑猫/装盒的 House。
2. 记住当前盒中猫，点击 `Mark Combat Cats`。
3. 确认 `Probe Required` 后出现排名/猫名/分数/`?` 列表，按钮变为
   `Clear Recommendations`。
4. 确认没有猫被自动放入盒子，也没有原生选择状态变化。
5. 点击 `Clear Recommendations`，确认列表消失且按钮恢复。
6. 再显示一次列表后离开并重新进入 House，确认没有残留。

剩余事项：
- 玩家可见验收尚未返回；在通过前 Stage 12 不标记 completed，
  Stage 13 继续 blocked。
- 如果文本列表位置、字号、遮挡或清除行为异常，需根据实机截图/日志
  做最小调整。

本地 commit：本次实现提交见最终回复（报告与代码同一提交）
是否 push：否

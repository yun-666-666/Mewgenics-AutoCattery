阶段：12
状态：ProbeRequired；真实 CatId→猫卡高亮未完成

实际能力：
- 已复用 Stage 4 的 `AutoCattery.Recommendation.MarkCombatCatsButton`。
- 玩家点击前：0 sidecar 读取、0 候选枚举、0 评分、0 mapping、0 marker。
- 点击后只读取 MOD 自有 `state/recommendations.json`；缺失是正常状态。
- 按钮明确显示 `Probe Required`，不会把 demo 或任意猫伪装成推荐。
- 玩家点击后留在 House，已武装 probe 才采三次匿名稳定样本。
- 当前能力不是 VerifiedHighlight，也不是可产出真实推荐 CatId 的
  ReadOnlyRecommendation。

推荐快照：
- reader 按 Stage 11 实际 schema 1 读取 envelope/payload。
- writer/reader 共用同一 FNV-1a checksum 实现。
- 校验 schema、checksum、day、source snapshot、配置摘要、算法版本、
  CatId 字符串、rank、finite score、0..1 confidence、重复 CatId/rank。
- 文件缺失、损坏、旧/未来 schema 或缺字段均 fail closed。
- schema 1 不含 build identity、save identity 或完整并发信息，因此即使
  Day N+1、配置和算法一致，也不得假装兼容；默认要求即时重算。
- Stage 11 PreviewOnly 没有 Committed，因此生产环境默认没有 sidecar。

即时重算：
- `InstantRankingProvider` 只接受显式的、当前 House generation 的已
  确认候选 source，并直接调用 Stage 6 `RankCombatCats`。
- Unknown 资格仍由默认 `require_confirmed_eligibility=true` 排除。
- 存档中的 House 记录没有被当作当前可见、可装盒候选视图。
- 生产 `UnsupportedCombatCandidateSource` fail closed，因此本次推荐来源
  为“不可用”；没有读取玩家真实存档。

CatId→猫卡证据与 probe：
- 玩家实机确认挑猫/装盒发生在 House；ClassChooser 只展示已装盒子集，
  不能选择或更换猫。
- 已证实的只有 House 场景、scene generation、MewUI 组件列表、组件
  类型调用和 Button role 读取边界。
- 未证实当前候选 CatId、CatId→view、纯视觉 marker API、选择状态独立、
  卡片复用/滚动/分页/筛选生命周期。
- 新 probe 只输出 request、generation、匿名 component/type/Button
  数量、stable role 布尔结果及匿名 type/role digest；不输出角色文本、
  猫名、CatId、指针、Steam ID、存档名或个人路径。
- generation 变化重新取稳定样本；UnsafeTransition/退出清除。
- `stable_cat_id_boundary=0`、`visual_marker_boundary=0` 时永远不标记。
- 2026-07-29 实机日志包含六组完整 `AC12101`/`AC12102`。前两次为
  606 components、digest `68be98812c154e9b`；后四次为 608 components、
  digest `3f012bf02853f222`。Button 数量始终为 76。
- 后四次在多次装入/移出操作之间完全相同，而且测试中的 ESC 会触发
  UnsafeTransition 和新 generation；因此该变化不足以证明猫卡数量、
  CatId 或 view identity，真实高亮仍 fail closed。
- 实机同时证实按钮缺少完成回调：probe 已输出 `AC12102`，文字仍停在
  `Probe Required`，只有 ESC 导致重新 attach 后才恢复。本次已补充
  generation 匹配的完成回调，探针完成后主动恢复按钮文字。

Toolkit：
- 读取 `LICENSE_TOOLKIT.txt`、`README_FIRST_先读我.md`、核心接口契约、
  Codex 集成清单及 C++ 单猫评分/排序实现与测试。
- 实际采用：MIT 许可边界、不可变快照、单猫评分、不自动组队/选择、
  稳定 CatId 决胜、确定性和置信度输出契约。
- 拒绝：整数 RoomId、六属性、示例 ID、能力/资格示例、游戏 API、
  ClassChooser 卡片结构、节点名、marker、选择接口及任何虚构生命周期。

修改文件：
- `CMakeLists.txt`
- `CODEX_TASK.md`
- `docs/implementation-status.md`
- `.auto-cattery/state.json`
- `.auto-cattery/reports/stage-12.md`
- `include/auto_cattery/recommendation/*.hpp`
- `src/recommendation/*.cpp`
- `src/ui/mew_ui_mapping_probe.c/.h`
- `include/auto_cattery/ui/mew_ui_bridge.hpp`
- `include/auto_cattery/ui/recommendation_marker_controller.hpp`
- `src/ui/mew_ui_bridge.cpp`
- `src/ui/mew_ui_recommendation_marker_view.cpp/.hpp`
- `src/ui/recommendation_marker_controller.cpp`
- `include/auto_cattery/workflow/recommendation_snapshot_writer.hpp`
- `src/workflow/recommendation_snapshot_writer.cpp`
- `tests/recommendation_*_tests.cpp`
- `tests/test_main.cpp`

文件大小：
- 所有新增产品文件和新增测试文件均低于 250 行。
- 最大新增产品/测试文件为
  `tests/recommendation_marker_controller_tests.cpp`，167 行。
- 未修改用户已有的 `mew_ui_scene_probe.c/.h` 内容；两者 blob hash 与
  当前 index 完全相同。

验证：
- Debug build：通过。
- Debug `phase12_unit_tests`：通过（6.97 秒）。
- Debug `phase12_dll_load_smoke`：通过（0.12 秒）。
- Release build：通过。
- Release `phase12_unit_tests`：通过（0.58 秒）。
- Release `phase12_dll_load_smoke`：通过（0.07 秒）。
- `git diff --check`：通过（仅 Git 的预期 LF→CRLF 提示）。
- 禁止行为、Stage 13～16、隐私/密钥、存档/WAL/SHM、日志、二进制和
  个人路径扫描：通过。
- 覆盖 sidecar 缺失/checksum/schema/字段、Unknown/N+1/N+2、配置/算法/
  identity、候选交集、空/Unknown/少量/100/1000、稳定 CatId、无点击
  0 调用、重复点击 debounce、generation/UnsafeTransition、probe 完成
  恢复按钮和匿名边界。
- 真实 marker 完整/部分 mapping、卡片复用和视觉清理测试未伪造；这些
  依赖当前缺失的真实适配器证据，是本阶段 blocker。

安全与隐私：
- 自动选择、确认、队伍槽位写入、自动休息、日期推进、自动导航：0。
- 未读取或写入玩家真实存档。
- 未修改猫名、猫数据、游戏存档或原始游戏文件。
- 用户明确授权后，本次 Release DLL 已部署到真实 MOD 目录；Mewtator
  数据 MOD 已部署并启用，安装校验通过。
- 构建 DLL 与已安装 DLL 的 SHA-256 均为
  `ED8BA206684C2B070C1F09711A78C8865C5F6BF7529859937D884A553D7617EA`，
  大小均为 683520 bytes。
- 用户允许联网，但活动 `AGENTS.md` 禁止 web research；本次未联网。

玩家最小只读测试：
1. 通过 Mewtator 启动游戏，使已启用的 AutoCattery 数据 MOD 路径传入
   Mewgenics。
2. 新一天 House 点击 `Mark Combat Cats`；确认文字短暂显示
   `Probe Required`，随后无需 ESC 自动恢复为 `Mark Combat Cats`。
3. 玩家手动放一只猫入盒，再点同一按钮并等待三秒。
4. 再手动放一只猫入盒，重复点击和等待。
5. 手动移出一只猫，再重复点击和等待；不要点击游戏原生 `出发!`。
6. 正常退出游戏，提供日志中四组 `AC12101`、`AC12102` 行。
7. 回滚只需恢复原已安装 DLL；本阶段没有存档或游戏数据需要恢复。

剩余 blocker：
- 缺少当前 House 可见/可装盒候选 CatId 的可重复验证只读边界。
- 缺少稳定 CatId→view 绑定、纯视觉 marker add/remove、选择状态独立及
  view recycle/滚动/分页/筛选生命周期证据。
- schema 1 缺少 build/save identity，Stage 11 writer 也未接入真实
  Committed 生产流程。
- Stage 12 真实高亮验收未通过；不得开始 Stage 13。

本地 commit：由本阶段最终单一提交创建，哈希见最终回复。
是否 push：否

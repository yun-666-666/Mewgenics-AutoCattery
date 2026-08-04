# AutoCattery Stage 27 交接文档

更新时间：2026-08-05

工作目录：`D:\steam\steam\steamapps\common\Mewgenics\mew_auto`

当前分支：`main`

## 用户当前要求

- 立即暂停开发。
- 不再运行任何构建、测试、部署或安装校验。
- 保留当前未完成修改，并记录可供下一次继续的准确状态。

恢复工作前必须先获得用户明确同意；不得因为本文列出了后续命令就自行构建。

## 已完成并已推送的版本

GitHub `origin/main` 当前包含：

- `3ee5252 fix: honor reroll settings and stabilize F10 controls`
  - F10 设置的升级重骰次数会生成到全部 14 个玩家职业。
  - AutoCattery 在 Mewtator `modlist.txt` 中去重并保持最后加载，避免其他 MOD
    覆盖玩家设置。
  - F10 打开/关闭不再卸载并重新扫描 House 控件，降低重启或场景切换闪退风险。
- `e3c20dd fix: balance sexes only in breeding rooms`
  - 只有确认的一公一母繁育对所在繁育房才考虑公母比例。
  - 战斗、训练、普通房完全忽略性别。
  - 固定猫、不可移动猫和推荐繁育对先计入，无法达到 1:1 时安全降级。

已推送版本号为 v0.5.11。该版本此前已通过 Debug、Release、DLL smoke、部署和
安装校验；本交接中的未提交 v0.5.12 修改没有这些验证结果。

## 当前未提交的 Stage 27 修改

目标是纠正房间用途自动判断，并接通原有幼猫分离设置。

### 已写入代码的行为

1. 繁育房选择
   - 不再先把最高刺激房预留成所谓“战斗培养房”。
   - 先选择繁育房，再选择独立战斗驻留房。
   - 有其他可用房间时，舒适 `<= -10` 的房间不会仅凭高刺激成为繁育房。
   - 在没有采用未经验证权重的前提下，先比较舒适与刺激的较低项，避免任何一项
     出现明显短板；稳定全 7 后继续把 Mutation 作为额外同分依据。

2. 战斗驻留房
   - `PreferDevelopmentRoom` 已从“刺激、舒适、健康”改为“健康、舒适、刺激”。
   - 战斗推荐成年猫优先进入安全的高健康、高舒适房间。
   - 没有自动启用低舒适 Fight Club，因为公开机制表明它存在受伤和死亡风险。
   - 推荐繁育对不再计入战斗驻留目标，即使它们也拥有较高战斗潜力。

3. 幼猫房
   - `keep_kittens_separate_when_possible` 已从配置接入当前 MoveOnly 规划器。
   - 繁育房和战斗驻留房之外仍有可用房间时，选择高健康、高舒适房作为育幼目标。
   - 幼猫不再被“高潜力猫”身份拉入战斗驻留房。
   - 固定房、NoMove 和不可移动居民仍优先；容量不足时安全分散，不绕过保护。
   - 新移动原因：`kitten-nursery-room`。

4. 分配实现
   - `BalancedSlot` 新增 `kitten_preferred`。
   - 最小成本匹配同时考虑育幼目标和成年战斗潜力目标。
   - 新增角色容量计算，避免固定普通居民占用后仍生成不可实现的角色槽。
   - 固定猫选槽时优先匹配其幼猫/成年潜力身份，之后才安全降级。

5. 版本与文档
   - 源码版本已暂时改为 `0.5.12`。
   - 算法版本已改为 `current-build-purpose-aware-room-planning-v7`。
   - 已新增 `docs/RELEASE_NOTES_v0.5.12.md`，并更新中英文说明与路线文档。

## 已增加但尚未运行完成的回归场景

`tests/balanced_move_only_planner_tests.cpp` 已调整或新增：

- 85 猫、8 母/77 公场景：繁育性别均衡目标由旧的 `Floor1_Small` 改为属性更
  合适的 `Attic`，所有性别原因移动仍只允许进入繁育房。
- 12 猫、4 房场景：
  - `Floor1_Large` 设置为舒适 -15、刺激 100，验证单项高刺激不能让低舒适房
    成为繁育房。
  - `Attic` 为舒适 30、刺激 40，推荐繁育对预期进入此房。
  - `Floor1_Small` 健康 50，作为战斗驻留目标。
  - `Floor2_Large` 健康 35、舒适 10，三只幼猫预期全部进入此育幼目标。
  - 三只幼猫预期产生三个 `kitten-nursery-room` 移动原因。

这些断言只代表预期，尚不能声称通过。

## 构建与测试状态

- 第一次命令直接调用 `cmake`，但当前 PowerShell `PATH` 中没有 CMake，因此
  命令立即失败，实际没有开始构建。
- 随后改用 Visual Studio 内置 CMake，启动 Debug 的
  `auto_cattery_tests` 目标。
- 该进程运行约一分钟后，用户要求暂停；已立即终止执行单元。
- 没有取得编译成功、测试成功或测试失败结果。
- 没有运行 Release 构建、DLL smoke、部署、安装校验或游戏实测。

因此当前 v0.5.12 必须视为“代码已写、完全未验证”的中间状态。

## 当前修改文件

代码和测试：

- `include/auto_cattery/room_planning/domain.hpp`
- `src/room_planning/balanced_move_only_internal.hpp`
- `src/room_planning/balanced_move_only_context.cpp`
- `src/room_planning/balanced_move_only_preferences.cpp`
- `src/room_planning/balanced_move_only_targets.cpp`
- `src/room_planning/balanced_move_only_assignment.cpp`
- `src/room_planning/balanced_move_only_fixed.cpp`
- `src/room_planning/balanced_move_only_breeding.cpp`
- `src/room_planning/balanced_move_only_planner.cpp`
- `tests/balanced_move_only_planner_tests.cpp`

版本和文档：

- `CMakeLists.txt`
- `assets/description.json`
- `CHANGELOG.md`
- `README.md`
- `README_EN.md`
- `docs/USER_GUIDE.md`
- `docs/current-room-allocation-evidence.md`
- `docs/implementation-status.md`
- `docs/pre-completion-functional-roadmap.md`
- `docs/RELEASE_NOTES_v0.5.12.md`

本文自身：

- `docs/HANDOFF_STAGE_27.md`

## 下一次恢复时的最小处理顺序

只有用户明确允许恢复后才执行：

1. 先运行 `git status --short` 和 `git diff --check`，确认没有新的用户修改。
2. 人工复核 `balanced_move_only_targets.cpp` 中角色容量、固定猫和推荐繁育对的
   计数，重点防止无符号减法与角色槽数量不一致。
3. 只运行一次 Debug 目标构建和单测；若失败，只修直接错误，不扩大检查范围。
4. Debug 通过后只运行一次 Release 的 `AutoCattery` 与 `dll_smoke_tests` 目标。
5. 更新 `.auto-cattery/reports/stage-27.md`，记录真实命令和结果。
6. 用户允许部署时才部署并运行安装校验。
7. 使用明确文件列表暂存，仅提交本阶段文件；提交后仍不得 push，除非用户明确要求。

## 完整自动化目标仍未完成的部分

- 当前只建立单一繁育房、战斗驻留房和可选育幼房，没有多繁育房或玩家手动房间
  用途锁定。
- `ApplyPotentialOnlyRoles` 仍可能把繁育核心/储备的展示主角色重置为战斗推荐或
  普通储备；推荐配对 ID 会保留，但多房间长期保留策略尚未完成。
- 稳定全 7 后的技能、被动、疾病和变异评分主要影响推荐配对，尚未完整转换为
  多房间保留与分配策略。
- `StablePairTraitScore` 仍使用全屋最高刺激，而不是最终选定繁育房的实际刺激。
- 真实淘汰仍禁用，不能声称已经自动保留/淘汰全部优质遗传猫。
- 不得实现自动组队、自动休息/推进日期或自动选择出征。

## 工作区保护

下列未跟踪目录和 ZIP 是用户原有资料，不属于本阶段，不得修改、删除或提交：

- `All7Cat_ManualRoomAllocator/`
- `Mewgenics_AutoCattery_Codex_Toolkit_v1.0.0/` 及对应 ZIP
- 中文 16 步实施文档目录及 ZIP
- `PushToMeow/`
- 全 7 猫房间分配方案 ZIP
- `tools/__pycache__/`

当前没有 Stage 27 功能代码提交，也没有 Stage 27 push；交接文档可作为独立的
本地文档提交，不能把未验证功能代码一并暂存。

# Stage 27 房间用途自适应与幼猫分离

更新日期：2026-08-05

状态：公开教程与当前 build 机制资料已整理；房间用途自适应、繁育/战斗/育幼
角色互斥和幼猫分离已实现；Debug/Release 全量构建与 4/4 CTest 均通过。未部署，
等待玩家用测试存档验证实际 House 预览和原生移动。

## 研究与证据

- 新增 `docs/mewgenics-breeding-automation-research.md`，逐项记录来源、置信度、
  自动化映射和未确认边界。
- 当前繁育概率使用 SciresM 对 `glaiel::CatData::breed` 的当前版本逆向笔记：
  <https://gist.github.com/SciresM/95a9dbba22937420e75d4da617af1397>。
- 房间用途方向参考 Steam 社区《Breeding Basics》和《Meta Breeding Guide》；
  视频教程只用于交叉验证繁育房、育幼房、Mutation 房和 Fight Club 的用途拆分。
- Appeal 已明确为全屋属性，不参与单房用途排序。
- 社区推荐的具体技能、被动、疾病和变异强弱没有硬编码；默认等权并保留 F10
  逐 ID 配置覆盖。

## 已实现行为

- 先选择繁育房，再选择独立战斗驻留房；不再先占用最高刺激房。
- 繁育房排除舒适 `<= -10` 的自动失败环境，并比较 Comfort/Stimulation 的
  短板；稳定全 7 后才把 Mutation 作为额外同分依据。
- 战斗驻留房按 Health、Comfort、Stimulation 排序；繁育对和幼猫不作为普通
  战斗驻留猫。
- `keep_kittens_separate_when_possible` 已接入 MoveOnly。存在第三个可用房时，
  幼猫优先进入高 Health、高 Comfort 育幼目标。
- 配置关闭幼猫分离时不再生成育幼角色槽或 `kitten-nursery-room` 原因。
- 固定战斗潜力猫占用育幼候选房时，角色容量会预留其位置；容量不足的幼猫安全
  分散，只有真正进入育幼目标的移动才记录 `kitten-nursery-room`。
- 角色计数增加无符号减法前置检查；固定猫、NoMove 和不可移动居民继续优先。
- Fight Club 因受伤和死亡风险不自动启用；真实淘汰继续禁用。

## 文件

- 研究与当前证据：
  `docs/mewgenics-breeding-automation-research.md`、
  `docs/current-room-allocation-evidence.md`、
  `docs/GAME_VALUE_REFERENCE.md`。
- 房间规划：`include/auto_cattery/room_planning/domain.hpp`、
  `src/room_planning/balanced_move_only_*.cpp`。
- 回归：`tests/balanced_move_only_planner_tests.cpp`。
- 版本与用户文档：`CMakeLists.txt`、`assets/description.json`、`CHANGELOG.md`、
  `README.md`、`README_EN.md`、`docs/USER_GUIDE.md`、
  `docs/implementation-status.md`、`docs/pre-completion-functional-roadmap.md`、
  `docs/RELEASE_NOTES_v0.5.12.md`。

## 自动化验证

- `git diff --check`：通过。
- `tools/build.ps1 -Configuration Debug`：通过。
  - `phase14_unit_tests`：通过。
  - `phase14_dll_load_smoke`：通过。
  - `phase14_restore_cli_smoke`：通过。
  - `phase14_save_lab_cli_smoke`：通过。
  - Debug DLL SHA-256：
    `A3FD69EFB1188C54A71780743F5A93F71B0B83E3EFD40013A23F7C014EFC25AC`。
- `tools/build.ps1 -Configuration Release`：通过。
  - `phase14_unit_tests`：通过。
  - `phase14_dll_load_smoke`：通过。
  - `phase14_restore_cli_smoke`：通过。
  - `phase14_save_lab_cli_smoke`：通过。
  - Release DLL SHA-256：
    `139347B623381D3135724C235BA64ECCB5F2E260C2F9A62764906C228C58DCF1`。
- 新回归覆盖：
  - 低舒适/高刺激房不能抢占繁育用途。
  - 推荐繁育对进入综合 Comfort/Stimulation 更合适的房间。
  - 战斗推荐成年猫优先进入高 Health、高 Comfort 房。
  - 三只幼猫进入独立育幼房并生成可审计原因。
  - 关闭幼猫分离后不生成育幼原因。
  - 固定居民占用育幼房时只保留可实现的育幼槽，溢出安全分散。
  - 85 猫、8 母/77 公在 22 猫繁育房中达到当前最佳可达 `8:14`。

## 游戏验证、风险与后续阶段

- 本轮未部署，未修改活动存档、游戏原文件或 Steam Cloud。
- 需要玩家用测试存档确认：实际属性识别出的繁育/战斗/育幼房符合预期；首次
  执行移动数量合理；重复执行为 0；保护规则和手动移动失效门仍正常。
- 当前只建立单一繁育房、战斗驻留房和可选育幼房。多繁育房、玩家手动用途锁、
  Mutation 筛选房和 Fight Club 明确启用属于后续独立阶段。
- 稳定全 7 特征评分仍使用全屋最高刺激，而不是最终选定繁育房实际刺激；后续
  需要消除配对排序与房间规划之间的两阶段循环，不能直接猜测。
- 真实淘汰、淘汰确认、journal、撤销、自动组队、自动休息/推进日期和自动出征
  均未实现。

本轮本地 commit：以最终回复中的提交哈希为准。

是否 push：否

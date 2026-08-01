# Stage 15 分阶段全 7 繁育配对

更新日期：2026-08-01

状态：完成，原配对、基础属性默认读取、稳定房间身份和房外猫原生搬入均已
通过玩家实机确认。

## 已实现

- 七项基础属性在未解锁显示功能的存档中也默认读取；性取向和 pedigree
  仍按本机已验证的 Tink 解锁标记启用，未解锁维度保持未知、不参与配对。
- 解析 `pedigree` 的亲缘、游戏缓存后代 COI 和当前可访问猫表。
- 只用七项基础属性做全 7 判断；后天修正和装备加成不再冒充可遗传属性。
- 排除幼猫、死亡猫、硬性性取向不兼容和缺少 COI 的配对。
- FOUNDATION 阶段按 7 点覆盖、双方稳定 7 项、COI 和性取向排序；配对进入
  高刺激房。双方全 7 且 COI 为 0 后进入 STABLE_ALL7，改为高舒适房优先。
- 配对作为两个具体猫目标接入最小成本房间分配，不会因各房人数已经相同而
  停止。

## 本机真实证据

- 主存档：79 只当前猫，3160 个当前猫无序配对 COI（含自身），性取向为
  异性恋 60、双性恋 9、同性恋 10。
- 主存档阶段为 FOUNDATION；最佳成年配对覆盖 6/7 个 7 点属性，其中 4 项
  双方均为 7，目标房按当前属性为 `Attic`。
- 未解锁属性显示的 25 猫档仍读到 175 个基础属性值，范围 3–7；未解锁
  性取向和 pedigree 保持关闭。
- 未解锁属性显示的 8 猫档仍读到 56 个基础属性值，范围 3–7；未解锁
  性取向和 pedigree 保持关闭。
- 日志暴露三房间批量移动后会用旧存档住户重新推断房间身份，造成连续交换
  具体猫；现改为同一 House generation 固定房间身份映射。
- 最新日志确认规划器已经把房外猫列入移动，但执行包装器把 null 当前房间
  错当成无效猫，连续返回 `AC14303 invoked=0`；现改为独立校验 HouseCat
  组件和目标房，允许当前房间为 null 的猫调用原生搬入路径。

## 技术依据与边界

- 当前游戏存档、`npc_progress` 和 `pedigree` 是本阶段的本机依据。
- 基础属性继承、刺激度影响的外部技术参照：
  <https://gist.github.com/SciresM/95a9dbba22937420e75d4da617af1397>。
- 成年门槛和繁育概览的辅助参照：
  <https://mewgenics.wiki.gg/wiki/Breeding>。外部资料只作假设输入，最终以
  当前 build 与玩家实机结果为准。
- 尚未实现全 7 后的技能、被动、变异评分，也未自动推进天数或选择出征。

## 自动化与命令

- `auto_cattery_tests.exe`：通过。
- `breeding_data_probe.exe`：三个本机存档只读探针通过。
- Release 完整构建成功；CTest 5/5 通过。
- `git diff --check`：通过。
- Release DLL 已部署并通过安装核对；build/dist/安装 SHA-256 均为
  `437DAB122882DB99CCBB2E296D87126FF31629A31E955453A4632AD2DEAE6D81`。

## 玩家验收

原配对、25/8 猫基础属性默认读取、同一布局第二次 0 移动和拖到房外的猫参与
自动整理，均已由玩家报告通过。最新日志确认房外状态下原生移动直接提交，未再
出现旧版 `AC14303 invoked=0`。

## 变更文件

- 快照/进度门：`snapshot/domain.hpp`、`progress_unlocks.*`、
  `pedigree_parser.*`、`cat_personality.*`、`unlocked_breeding_data.*`。
- 配对：`breeding/domain.hpp`、`pair_ranker.*`、`breeding_ranker.cpp`、
  `breeding_scorer.cpp`。
- 分房：`classification/*`、`balanced_move_only_*`、`preview_builder.cpp`。
- 验证：配对、pedigree、猫 blob、确定性分房测试和只读探针。

本地 commit：见本阶段最终回复。

是否 push：否

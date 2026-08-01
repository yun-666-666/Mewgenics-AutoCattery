# Stage 16 稳定全 7 遗传特征评分

更新日期：2026-08-01

状态：代码、当前资源、真实存档只读探针和自动化验证完成；现有三个存档均未
达到稳定全 7，因此没有伪造玩家实机结果。

## 已实现

- FOUNDATION / BASE_ALL7 继续只看七维、性取向和游戏缓存 COI。
- 只有存在“双方七维全 7 且后代 COI 为 0”的可繁育配对，才进入
  STABLE_ALL7 并启用遗传特征分。
- 技能第一/第二槽、被动按当前最高刺激房的继承概率计分；`SkillShare` 不作
  普通可继承被动计分。
- 疾病按父母各自 15% 的继承风险扣分。
- 从猫 blob 外观块读取 15 个主部位 ID；只接受当前 `resources.gpak` 中
  `data/mutations/*.gon` 的确切目录项。`birth_defect` 和缺失部位为负面，
  未入目录的低 ID 基础部位忽略。
- 普通变异、出生缺陷、技能、被动和疾病均有默认等权及逐 ID 配置覆盖。
- 稳定阶段繁育房改为刺激优先、变异属性次优、舒适再次；不再沿用缺乏机制
  依据的“稳定后固定舒适优先”。

## 当前 build 证据

- 本机当前 GPAK 只读扫描得到 771 个分类目录项，其中 53 个带出生缺陷语义。
- 79 猫主档：`visual_traits=1`，普通变异部位 398，出生缺陷部位 13；仍为
  FOUNDATION，原推荐配对、COI 和 `Attic` 目标不变。
- 25 猫档：普通变异部位 19，出生缺陷部位 6；性取向/亲缘仍未解锁。
- 8 猫档：普通变异与出生缺陷均为 0；性取向/亲缘仍未解锁。

## 机制依据与边界

- 当前 build 的 GPAK 是变异 ID 和 `birth_defect` 分类依据。
- SciresM 对当前繁育函数的逆向笔记用于刺激、技能、被动、疾病和部位继承
  公式：<https://gist.github.com/SciresM/95a9dbba22937420e75d4da617af1397>。
- 普通技能和变异具体强弱没有可靠统一答案，因此默认等权；玩家可在
  `breeding_scoring` 中按保存 ID 覆盖。
- 尚无自然达到 STABLE_ALL7 的本机存档，不能进行真实后代结果验收；这不
  影响现有 FOUNDATION 三档回归，后续获得合适测试档再补玩家验证。

## 自动化与命令

- Debug 构建成功，CTest 5/5 通过。
- `breeding_data_probe.exe` 对 79/25/8 猫三份实际存档只读通过。
- Release 构建成功，CTest 5/5 通过；build/dist/安装 DLL SHA-256 均为
  `90184B22B63E16F465816C13F170273F36DB2457A11307ABA8002A5C249C6D85`。
- `git diff --check`：通过。

## 变更文件

- 快照与目录：`snapshot/domain.hpp`、`visual_traits.hpp`、
  `visual_part_slots.cpp`、`mutation_catalog.cpp`、快照适配器与 seal。
- 分阶段评分：`pair_trait_scorer.*`、`pair_ranker.cpp`、
  `breeding_ranker.cpp`、`breeding_scorer.cpp`。
- 配置与规划：默认配置、schema、解析/校验/digest、繁育房偏好。
- 验证：blob、GPAK 目录、阶段门、配对排序、房间选择和配置 digest 测试。

本地 commit：见本阶段最终回复。

是否 push：否

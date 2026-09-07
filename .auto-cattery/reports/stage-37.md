# Stage 37 — 通用配对评分遵守繁育属性权重

日期：2026-09-08。候选版本：v0.5.31。状态：本地构建、测试和部署完成，玩家实机及长期出生收益待验收。

## 范围和证据

玩家游戏数据只用作发现问题与验证的参考，不作为固定猫群、人口或房间模板。本机配置使用了不等权繁育偏好；代码检查确认单猫评分使用 stat_weights，而配对的父母较高属性加总忽略该配置。这是与猫身份、数量和房间数无关的通用配置问题。

修正配对基础属性项为逐项 max(父母基础属性) × 现有配置权重，再沿用原有配对分组成。默认权重各1时分数保持；无玩家私有数值写入默认配置。现有房间规划没有改动，不新增固定两猫、固定阁楼或固定人口分布策略。

## 文件

- src/breeding/pair_ranker.cpp：传入并应用现有繁育属性权重。
- include/auto_cattery/breeding/domain.hpp：繁育算法标识 v5。
- tests/pair_ranker_tests.cpp：合成猫测试默认等权、提高智力、提高力量时的推荐变化。
- CMakeLists.txt、assets/description.json：候选版本0.5.31。
- docs/USER_GUIDE.md：中英文权重说明与实际行为一致。
- CODEX_TASK.md、.auto-cattery/state.json：记录修正后的范围及玩家待验收状态。

## 验证

- tools/build.ps1 -Configuration Release：会话88994，退出码0。
- CTest 5/5通过，含单元测试和DLL load smoke，总测试时间2.41秒。
- git diff --check通过。
- git diff --exit-code确认房间规划实现、房间算法版本和原分房测试均已恢复。
- code-simplifier完成一次范围内检查，无需额外简化，没有重复构建。
- 确认Mewgenics未运行后，复制候选DLL及description.json至已安装MOD；安装描述版本0.5.31，文件写入检查完成。未写玩家配置、存档或其他MOD。

## 玩家验证与风险

重启游戏，F10生成完整预览，确认自动整理与原房间用途策略正常。配对在原有其他评分项共同作用下响应当前繁育权重；改变权重不保证每个存档都会更换首选配对，不能将合成用例的结果套在玩家猫群上。

本次只证明配置进入配对分以及现有回归通过，不证明繁殖概率、全七出生率或多日收益提高。大幅调整用户权重会改变其与既有COI等分项的相对影响；本次未调整用户权重或默认值。

未实现额外房间策略、自动淘汰、休息、出征、五房兼容或遗传公式调整。原有build-ninja/、tests/__pycache__/、tools/__pycache__/保持未跟踪。

## Git

代码验证提交：4285740fb7591ee36497d2ac075e4a304d9e83df（随后仅补入本报告，最终本地提交ID见任务交付）。
提交消息：fix: honor configured breeding stat weights in pair ranking。
是否 push：否。

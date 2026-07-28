# C++20 自动化领域核心

这是不依赖游戏 API 的纯 C++20 核心，供 Codex 移植或直接加入现有 CMake。它包含：

- `ScoreCombat`：单猫战斗评分；
- `ScoreBreeding`：单猫繁育评分；
- `ClassifyCats`：核心/备用/保留/淘汰候选；
- `PlanRooms`：容量感知、保护感知的确定性换房差异计划。

它**不包含**：游戏内存读取、场景识别、UI、真实移动、淘汰、存档写入或自动组队。

构建：

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

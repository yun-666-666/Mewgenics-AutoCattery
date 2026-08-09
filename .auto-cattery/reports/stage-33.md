# Stage 33：普通 House 与家具界面按钮模式分离

更新日期：2026-08-09

状态：v0.5.17 的组件存在性判断已被玩家实测否定；v0.5.18 改用玩家确认的家具
摆放状态字节。Debug/Release 完整构建与 4/4 CTest、部署和安装哈希验证已通过；
玩家三段按钮切换复测待执行。

## 本阶段边界

- 普通 House 恢复“自动整理猫舍”和“标记推荐战斗猫”以及原有整理预览、执行、
  战斗推荐和详情路径。
- 仅当玩家点击左上角交叉工具、真正进入家具摆放状态时，同一对按钮切换为禁用的
  “自动放置”和可点击的“开始分析”。
- 离开家具模式后清除家具摘要、恢复普通 House 文案；过期异步家具分析不得显示。
- 模式检测失败时默认普通 House，不调用家具移动、旋转、出售、删除或保存写入。

## 实现

- v0.5.17 错误地把 `FurnitureBuildingUI` 的存在当成家具模式；玩家进入普通 House
  后仍看到家具按钮，证明该组件是 House 常驻组件。
- 2026-08-09 只读实机探针确认 `FurnitureBuildingUI + 0x78`：普通 House 为 `0`，
  点击左上角进入家具摆放时 `0 -> 1`，退出时 `1 -> 0`。
- `mew_ui_bridge` 缓存 House manager、组件数量和组件指针，仅在 manager 或数量
  变化时重新查找组件；每个 UI tick 只读取已实测的 `+0x78` 单字节。
- 组件查找或单字节读取失败时安全退回普通 House，不显示家具按钮。
- 两个 House 按钮 view 根据模式切换文案、启用状态和点击路由；普通 House
  继续走整理/战斗推荐，家具模式只走 Stage 32 只读分析。
- 进入家具模式会取消旧战斗推荐映射请求并清除详情目标；家具分析结果绑定请求
  generation 和模式，退出后到达的旧结果会被拒绝。
- 修复版本更新为 v0.5.18，并同步 Changelog、description、任务状态和本报告。

## 自动化验证

- Debug 完整构建：通过；`build-stage33\out\Debug\AutoCattery.dll` 已生成。
- Debug CTest：4/4 通过，包括单元测试、DLL load、restore CLI 和 save lab CLI。
- Release 完整构建：通过，退出码 0。
- Release CTest：4/4 通过，包括单元测试、DLL load、restore CLI 和 save lab CLI。
- 现有回归继续覆盖普通 House 按钮生命周期、家具模式路由、模式来回切换、旧异步
  结果拒绝和推荐详情恢复。
- v0.5.18 Release DLL SHA-256：
  `BD0C7B634A531CAF8B0B0777B064D91832851B66489C4DE602FBCAECB84E142E`。

## 部署与安装验证

- 按现有 `build.ps1` 文件清单打包到 `dist\Release`，没有启动竞争构建。
- `tools\deploy.ps1` 成功部署到 Mewjector 与 Mewtator；保留用户配置，当前
  14 个职业升级重投次数仍为 20。
- `tools\verify_install.ps1` 通过；安装 DLL 为 x64，Mewtator 数据 MOD、顺序和
  14 个职业数据均有效。
- 测试产物、`dist\Release` 和安装 DLL 的 SHA-256 完全一致：
  `BD0C7B634A531CAF8B0B0777B064D91832851B66489C4DE602FBCAECB84E142E`。

## 玩家实测门

1. 通过 Mewtator 启动游戏，进入普通 House：应显示“自动整理猫舍”和
   “标记推荐战斗猫”，并能使用原功能。
2. 进入家具编辑界面：按钮应切换为“自动放置”和“开始分析”；“自动放置”保持
   禁用，点击“开始分析”显示当前房间/家具只读摘要。
3. 退出家具编辑界面返回普通 House：旧按钮和原功能应恢复，家具摘要应消失，
   不应出现重复按钮、残留文字或闪烁。

未收到玩家三段验证前，不把游戏内验收标记为完成。

## 风险与省略的后续工作

- `FurnitureBuildingUI + 0x78` 是当前 build 的玩家实测状态标志；未来游戏更新若
  改变布局或语义，探测会安全退回普通 House，需要重新实测适配。
- 家具用途规划、方向/旋转、占地、Anchor、Solid、合法布局与原生移动仍未确认，
  “自动放置”继续禁用。
- 未实现自动组队、自动出征、结束一天、淘汰或活动存档直接写入。

## 交付记录

- 核心 UI/适配器：`src/ui/mew_ui_bridge.cpp`、两个 House 按钮 view、推荐标记
  controller/view、`mew_ui_scene_probe.c` 及对应头文件。
- 测试：`tests/house_button_controller_tests.cpp`、
  `tests/recommendation_marker_controller_tests.cpp`、`CMakeLists.txt`。
- 本次必要元数据与记录：v0.5.18 description、Changelog、`CODEX_TASK.md`、state
  与本报告。
- 实际命令：独立 Debug/Release CMake build、两套 CTest、打包哈希比较、
  `deploy.ps1`、`verify_install.ps1`。
- 本地 commit：包含本报告的 Stage 33 commit；最终哈希由最终回复记录。
- 是否 push：否

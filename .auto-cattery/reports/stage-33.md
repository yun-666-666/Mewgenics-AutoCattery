# Stage 33：普通 House 与家具界面按钮模式分离

更新日期：2026-08-09

状态：实现完成；Debug/Release 完整构建与 4/4 CTest、v0.5.17 Release 部署和
安装哈希验证通过。玩家三段界面切换验证待执行。

## 本阶段边界

- 普通 House 恢复“自动整理猫舍”和“标记推荐战斗猫”以及原有整理预览、执行、
  战斗推荐和详情路径。
- 仅当当前 House 加载已验证组件类型 `FurnitureBuildingUI` 时，同一对按钮切换为
  禁用的“自动放置”和可点击的“开始分析”。
- 离开家具模式后清除家具摘要、恢复普通 House 文案；过期异步家具分析不得显示。
- 模式检测失败时默认普通 House，不调用家具移动、旋转、出售、删除或保存写入。

## 实现

- `mew_ui_scene_probe` 新增只读组件类型探测，复用历史实测确认的
  `FurnitureBuildingUI`，未猜测新场景名、节点名、偏移或 API。
- `mew_ui_bridge` 缓存 House manager 与组件数量，仅在二者变化时重新探测模式，
  不在每帧扫描全部组件。
- 两个 House 按钮 view 根据模式切换文案、启用状态和点击路由；普通 House
  继续走整理/战斗推荐，家具模式只走 Stage 32 只读分析。
- 进入家具模式会取消旧战斗推荐映射请求并清除详情目标；家具分析结果绑定请求
  generation 和模式，退出后到达的旧结果会被拒绝。
- 恢复普通 House 中英文文本，版本更新为 v0.5.17，并同步 README、用户指南、
  家具设计文档、实现状态、Changelog、description 和发行说明。

## 自动化验证

- Debug 完整构建：通过；`build-stage33\out\Debug\AutoCattery.dll` 已生成。
- Debug CTest：4/4 通过，包括单元测试、DLL load、restore CLI 和 save lab CLI。
- Release 完整构建：通过，退出码 0。
- Release CTest：4/4 通过，包括单元测试、DLL load、restore CLI 和 save lab CLI。
- 新增/扩展回归覆盖普通 House 完整按钮生命周期、家具模式路由、模式来回切换、
  旧异步结果拒绝和推荐详情恢复。
- Release DLL SHA-256：
  `0DFE81EEF15500CF98C72F8B9A17159CA0C060B5AFB49E2E4EBA652A11E131E0`。

## 部署与安装验证

- 将已通过 Release CTest 的 `build-stage33` DLL 按现有 `build.ps1` 文件清单打包
  到 `dist\Release`，没有再次启动竞争构建。
- `dist\Release` DLL 与测试产物 SHA-256 一致。
- `tools\deploy.ps1` 成功部署到 Mewjector 与 Mewtator；保留用户配置，当前
  14 个职业升级重投次数仍为 20。
- `tools\verify_install.ps1` 通过；安装 DLL 为 x64，Mewtator 数据 MOD、顺序和
  14 个职业数据均有效。
- 安装 DLL SHA-256 与 `dist\Release` 一致：
  `0DFE81EEF15500CF98C72F8B9A17159CA0C060B5AFB49E2E4EBA652A11E131E0`。

## 玩家实测门

1. 通过 Mewtator 启动游戏，进入普通 House：应显示“自动整理猫舍”和
   “标记推荐战斗猫”，并能使用原功能。
2. 进入家具编辑界面：按钮应切换为“自动放置”和“开始分析”；“自动放置”保持
   禁用，点击“开始分析”显示当前房间/家具只读摘要。
3. 退出家具编辑界面返回普通 House：旧按钮和原功能应恢复，家具摘要应消失，
   不应出现重复按钮、残留文字或闪烁。

未收到玩家三段验证前，不把游戏内验收标记为完成。

## 风险与省略的后续工作

- `FurnitureBuildingUI` 组件存在性是当前 build 已验证模式标志；未来游戏更新若
  改名或改变组件生命周期，探测会安全退回普通 House，需要重新实测适配。
- 家具用途规划、方向/旋转、占地、Anchor、Solid、合法布局与原生移动仍未确认，
  “自动放置”继续禁用。
- 未实现自动组队、自动出征、结束一天、淘汰或活动存档直接写入。

## 交付记录

- 核心 UI/适配器：`src/ui/mew_ui_bridge.cpp`、两个 House 按钮 view、推荐标记
  controller/view、`mew_ui_scene_probe.c` 及对应头文件。
- 测试：`tests/house_button_controller_tests.cpp`、
  `tests/recommendation_marker_controller_tests.cpp`、`CMakeLists.txt`。
- 玩家资产与文档：本地化文本、v0.5.17 description/release notes、README、
  中英文用户指南、设计/状态文档、Changelog、`CODEX_TASK.md`、state 与本报告。
- 实际命令：独立 Debug/Release CMake build、两套 CTest、打包哈希比较、
  `deploy.ps1`、`verify_install.ps1`。
- 本地 commit：包含本报告的 Stage 33 commit；最终哈希由最终回复记录。
- 是否 push：否

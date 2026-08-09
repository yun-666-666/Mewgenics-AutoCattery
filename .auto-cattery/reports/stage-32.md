# Stage 32：开始分析按钮与动态家具分析快照

更新日期：2026-08-09

状态：实现完成；Debug/Release 自动化、三个活动存档只读回归、Release 部署与
安装哈希验证通过。玩家界面验证待用户执行。

## 本阶段边界

- 左侧 `test_button` 显示为“自动放置”，从创建到后续状态同步均不可交互。
- 右侧 `recommend_button` 显示为“开始分析”，旧战斗猫推荐请求、映射探针和
  猫详情点击路径不再由该按钮触发。
- 玩家点击后异步生成只读分析摘要；不移动、旋转、出售或删除家具，不修改活动
  存档、Steam Cloud 或游戏原始文件。

## 实现

- 新增 `FurnitureAnalysisService` 与不可变输入/输出模型。
- 分析源优先用当前运行时完整 CatId 集合选择对应存档，无法取得完整身份时才
  回退 House 猫数量。
- 每次点击重新捕获当前 day、猫与房间、全部家具实例、`house.gon`、
  `furniture_info.data` 和 `furniture_effects.gon`。
- 房间集合取存档房间、家具所属房间和运行时已识别房间的并集；若运行时房间数
  更多，则增加明确标记的匿名槽，不伪造房间 ID。
- 绑定摘要包含 generation、匿名存档身份、猫房间、每件家具原始字段、房间
  几何、家具信息 580 字节 payload 与家具效果。
- 右侧四行显示房间数、家具总数/已放置数、仓库数和“自动放置尚未启用”。

## 自动化与真实存档验证

- Debug 4/4 CTest：通过。
- Release 4/4 CTest：通过。
- 合成分析回归覆盖 1、2、3、5、7 房；未调用服务前读取次数为 0；相同输入
  binding digest 一致；generation 0 安全拒绝；匿名空房槽可扩展到 5 房。
- 第 265 天主档：4 房，257 件家具，142 已放置、115 仓库，资源覆盖 257/257。
- 第 32 天存档：3 房，20 件家具，资源覆盖 20/20，空 `Floor1_Small` 可见。
- 第 17 天存档：2 房，10 件家具，资源覆盖 10/10，空阁楼可见。
- 三次测试均使用只读 SQLite 和资源探针，退出码 0，没有保存写入。
- v0.5.16 Release 已部署；安装检查通过，构建 DLL 与安装 DLL SHA-256 一致：
  `88BD0923656B00BB18D2A9A3DE90388DFE2377642106B9E7733820613797F8BC`。
- 玩家仍需确认两个按钮文字、左侧禁用状态、右侧四行数据和跨三个存档结果。

## 风险与后续

- 真实 1 房和 5 房仍无现成玩家存档；本阶段只由动态合成回归证明没有四房数组。
- 家具方向、翻转、占地、Solid、Anchor、Background 与 opaque payload 语义
  未确认，因此“自动放置”必须继续禁用。
- F03 才实现房间用途、家具分配和合法布局预览；F04/F05 才允许受控原生移动与
  自动执行。

## 交付记录

- 核心与适配器：`include/auto_cattery/furniture_analysis/domain.hpp`、
  `include/auto_cattery/furniture_analysis/service.hpp`、
  `src/furniture_analysis/service.cpp`、`save_snapshot_adapter.*`、
  `runtime_matched_save_snapshot_adapter.*`、`runtime_house_state_capture.cpp`。
- UI：`mew_ui_bridge.*`、两个 House 按钮 view、旧推荐 controller 的只读分析
  状态机接管、`in_game_panel_controller.cpp`。
- 测试：`furniture_analysis_service_tests.cpp`、推荐 controller 回归、
  `test_main.cpp`、`CMakeLists.txt`。
- 玩家资产与文档：按钮文本、v0.5.16 description/release notes、README、用户
  指南、设计/状态文档、`CODEX_TASK.md`、state 与本报告。
- 命令：Debug/Release build、CTest、三个活动存档的
  `furniture_geometry_probe` 与 `room_attribute_probe`、deploy、verify_install。
- 本地 commit：包含本报告的 Stage 32 commit；最终哈希由最终回复记录。
- 是否 push：否

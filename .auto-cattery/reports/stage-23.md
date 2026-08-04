# Stage 23 当前报告

更新日期：2026-08-04

状态：战后返回 House 的 UI 查询异常风暴已定位、修复、构建并部署；等待玩家
再次完成一场战斗后确认实际流畅度。

## 问题证据

- 玩家连续两次实测均在战斗后回到 House 明显卡顿。
- 最新 `chainloader.log` 达到 695,797,561 字节；卡顿窗口内同一游戏 UI 线程
  每毫秒记录约数十至数百个 `0xC0000005`，异常地址集中在
  `Mewgenics.exe+0x5A0E0/0x5A0E3`。
- 当前游戏基址为 `0x7FF704330000`，绝对异常地址
  `0x7FF70438A0E0/0x7FF70438A0E3` 与上述 RVA 精确对应。
- MewUI 备用 root-owned 子节点查询入口为 `0x5A0B0`；AutoCattery 的面板根
  定位和后续子节点解析正调用该入口。实测 House 有 4,336 个组件，导致一次
  挂载触发大量被 SEH 捕获、又被全局 VEH 逐条写盘的异常。
- AutoCattery 自身记录的面板渲染仅 91 微秒、挂载仅约 10–19 毫秒，因此不是
  规划算法或文字渲染本身耗时；异常处理和 695 MB 日志写入才是主要卡顿路径。

## 修复边界

- 新增 AutoCattery 自有的安全节点定位器：校验场景组件数组、组件与根指针，
  去重根节点，并只调用已确认的直接子节点查询 `0xE86F0`。
- 面板先定位 `panel_background` 所属 SWF 根，全部固定控件、设置行、列表行和
  文本都从同一个根解析并缓存。
- 整理按钮和推荐按钮使用同一安全定位器；找不到节点时直接安全失败，不再把
  空节点交给 MewUI 触发内部全场景回退。
- 移除面板的旧场景扫描兼容路径。旧 SWF 节点不完整时 UI 安全禁用，不制造
  异常风暴。
- 未修改游戏原始文件、存档、玩家配置、旧日志或未跟踪工具目录。

## 文件与命令

- 节点定位：`src/ui/mew_ui_safe_node_lookup.*`、
  `src/ui/mew_ui_scene_components.*`。
- UI 调用方：`src/ui/mew_ui_management_panel_view.*`、
  `src/ui/mew_ui_house_button_view.cpp`、
  `src/ui/mew_ui_recommendation_marker_view.*`。
- 测试：`tests/mew_ui_safe_node_lookup_tests.cpp`、`tests/test_main.cpp`。
- 构建接入与文档：`CMakeLists.txt`、`CHANGELOG.md`、当前状态和路线图。
- Debug：`.\tools\build.ps1 -Configuration Debug`。
- Release：`.\tools\build.ps1 -Configuration Release`。
- 部署：`.\tools\deploy.ps1 -GameRoot <GAME_ROOT> -Configuration Release`。
- 安装校验：`.\tools\verify_install.ps1 -GameRoot <GAME_ROOT>`。

## 验证结果

- Debug 编译通过，CTest `4/4` 通过。
- Release 编译通过，CTest `4/4` 通过。
- 新单元测试覆盖空组件、多个组件共享同一根、根去重、找到即停和找不到节点。
- 静态检查确认三个 House UI 视图中危险的 root-owned/全场景节点查询调用为 0。
- Release DLL 已部署；构建与安装 DLL 的 SHA-256 均为
  `1E9A95348E8D015DF323E7ADBC85645FF45421672488FDB5477838970A91B483`。
- 安装验证通过；既有 `level_up.reroll_count=10` 保持不变，14 个职业数据一致。
- 玩家实机验证待完成：完成一场战斗返回 House，观察卡顿和新日志是否仍出现
  `Mewgenics.exe+0x5A0E0/0x5A0E3` 异常风暴。

## 风险与未做事项

- 自动化不能证明真实游戏帧率；最终验收必须以玩家战后回家复测为准。
- 本阶段只修复 House UI 节点定位性能，不扩展淘汰、分房或繁育功能。
- 未删除 695 MB 的历史 `chainloader.log`，避免擅自清理用户诊断证据。

本轮本地 commit：以最终回复中的提交哈希为准。

是否 push：否

# Stage 21 - House UI 生命周期、输入与 DLL-only 修复

## 实际问题与日志证据

- `2026-08-04 15:38`：F10 面板关闭后同一秒进入设置覆盖层，House generation
  连续从 2 变为 3/4/5；旧视图允许在同一 scene manager 下复用节点。
- `2026-08-04 18:14:28`：次日 House 被观察后推荐控件挂载延迟，同一毫秒发生
  `0xC0000374` heap corruption。崩溃栈不能单独证明 AutoCattery 是唯一来源，
  但时间窗口与跨 generation 原生 UI 指针/按钮记录复用一致。
- 设置页的隐藏底部导航命中区先于设置行判断，直接遮挡了战斗“幸运权重”所在
  区域；设置行本身还有 4 像素垂直空隙。
- 用户截图中的 Microsoft Visual C++ Debug Assertion 来自仓库开发测试程序
  `build\Debug\auto_cattery_tests.exe`，不是游戏进程。新增命中测试最初在
  `AC_CHECK(optional.has_value())` 失败后仍使用 `optional->`，触发了 CRT 的
  `operator->() called on empty optional` 调试断言。

## 实际修改

- SceneContext 离开稳定 House 时使用 `AbandonScene`，只释放 MOD 引用和输入钩子，
  不再对可能正在销毁的原生节点写文字、帧或按钮状态。
- House 按钮、推荐按钮、推荐四行和 F10 面板不再跨 generation 复用原生指针。
- 当本日推荐不可用时仍在稳定 House 挂载控件并禁用，同时把四行停止在隐藏帧，
  避免冒险返回后播放 SWF 时间轴直至次日。
- F10 面板消息钩子消费 Esc；隐藏导航不再抢占设置行，设置命中高度覆盖完整 25px。
- 回归测试对所有可选命中结果先做显式条件保护；未来命中断言失败时只报告测试失败，
  不再解引用空 `optional` 或弹出 CRT 调试断言窗口。
- 删除外部设置 EXE 源码和构建目标；以后不再生成、复制或发布辅助 EXE，
  但构建和部署不会主动清理用户保留的历史 EXE。

## 文件

- `src/ui/*panel*`、`src/ui/*button*`、`src/ui/*recommendation*`、`src/ui/mew_ui_bridge.cpp`
- `include/auto_cattery/ui/*button_controller.hpp`、`management_panel_input.hpp`
- `tests/house_button_controller_tests.cpp`、`recommendation_marker_controller_tests.cpp`、
  `virtual_viewport_tests.cpp`
- `CMakeLists.txt`、`tools/build.ps1`、`tools/deploy.ps1`、
  `tools/verify_install.ps1`、`tools/package_release.ps1`
- 当前 README、用户手册、状态、路线图和 CHANGELOG。

## 验证

- Debug 增量构建及 `ctest --test-dir build -C Debug --output-on-failure`：4/4 通过。
- `tools/build.ps1 -Configuration Release`：Release 构建、DLL 导出/架构检查及
  4/4 CTest 全部通过。
- 新 Release 玩家分发为 DLL-only。构建目录保留 CTest/开发探针 EXE，历史分发
  EXE 也允许由用户自行保留；这些文件不会再由当前构建安装或打包。
- Release 已部署并通过 `tools/verify_install.ps1`；安装 DLL 与分发 DLL 的 SHA-256
  均为 `C9AA06C2664AF0DD3BA0764894FBBFA3FEA89DC4F9FE0C70EAF30D66173FDAAB`，
  本次部署前已按原始要求删除旧 `mods\AutoCattery\AutoCatterySettings.exe`；根据
  用户后续要求，当前脚本不再主动删除其他历史 EXE。
- `tools/package_release.ps1 -Version 0.5.5 -Configuration Release` 成功；逐项读取
  `AutoCattery-v0.5.5-Windows-x64.zip` 后确认玩家包 EXE 数量为 0。
- 玩家实机：待复测 F10 -> Esc -> 返回游戏、两次 F10、冒险返回和结束一天。

## 风险与未做事项

- 崩溃报告没有符号化到具体写坏堆的位置，因此此修复关闭了日志和代码共同指向的
  生命周期风险，但仍需玩家实机长时间复测才能确认闪退完全消失。
- 没有启用真实淘汰、自动组队、自动结束一天或后续阶段功能。

## Git

- 本地提交：由本阶段最终提交承载，哈希见最终交付结果。
- 是否 push：否

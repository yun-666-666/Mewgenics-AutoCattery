# Stage 31：MewUI 多版本运行时地址定位

更新日期：2026-09-04

状态：实现、自动验证和本机部署完成；玩家实机验证待完成。

## 问题与结论

- 本机 F10 无响应时，同次启动的 chainloader 日志只加载了
  `CombineDuplicateFurniture.dll`，游戏目录当时没有 `mods\AutoCattery.dll`，
  因而 AutoCattery 没有运行。
- 另一名玩家的 beta 崩溃来自版本地址漂移：公开版 `1.1.b21039` 的 scene-ready
  Hook RVA 为 `0x962820`，beta `1.1.b21220` 的对应 RVA 为 `0x96ABD0`；v0.5.24
  固定使用旧地址。
- Windows 事件中的 `0xC0000409` 和 `Mewgenics.exe+0xD5CB6D` 落在
  `mov ecx,7; int 29` 的进程快速失败路径，不能单独解释成普通栈溢出。

## 实现

- 将版本更新到 v0.5.25。
- MewUI 在初始化时扫描实际加载的 `Mewgenics.exe` `.text`，要求每个所需函数
  只有一个匹配，并把原 canonical RVA 映射到本次运行解析出的 RVA。
- scene-ready、Button activate 与 Button can-activate Hook 使用解析结果。
- 不校验 EXE 哈希或时间戳，也没有单版本地址白名单。必要目标无法唯一定位时，
  不向未知地址安装 Hook，UI 初始化返回失败。
- 启动日志输出三个关键 Hook RVA，供玩家日志确认实际加载布局。
- 构建脚本在 Visual Studio 实例未注册时使用现有 `vcvars64.bat` 和 Visual Studio
  自带 Ninja Multi-Config，保留正常注册环境下的 Visual Studio 生成器路径。
- 离线定位测试不限制 RVA 必须属于旧版本列表，只检查传入 EXE 的目标是否唯一
  可定位。

## 修改文件

- `CMakeLists.txt`
- `assets/description.json`
- `CHANGELOG.md`
- `docs/RELEASE_NOTES_v0.5.25.md`
- `third_party/mew_ui_api/src/native/mew_ui_api.c`
- `tests/mew_ui_runtime_locator_tests.py`
- `tools/build.ps1`
- `CODEX_TASK.md`
- `docs/implementation-status.md`
- `.auto-cattery/state.json`
- `.auto-cattery/reports/stage-31.md`

## 构建与检查

- `./tools/build.ps1 -Configuration Debug`
  - 构建成功。
  - CTest 5/5 通过。
  - DLL load smoke 通过。
- `./tools/build.ps1 -Configuration Release`
  - 构建成功。
  - CTest 5/5 通过。
  - DLL load smoke 通过。
- `python tests/mew_ui_runtime_locator_tests.py third_party/mew_ui_api/src/native/mew_ui_api.c D:/steam/steam/steamapps/common/Mewgenics/Mewgenics.exe`
  - PASS：30 个运行时 UI 地址均唯一解析。
  - scene-ready：`0x962820`。
  - Button activate：`0x9764B0`。
  - Button can-activate：`0x9766C0`。
- `code-simplifier` 最终复核仅移除了离线测试中的固定 RVA 允许列表；没有改变
  运行时 API、输出或 Hook 行为，因此复用此前完成的 Debug/Release 构建结果，并
  复跑了受影响的定位检查。

## 部署

- 命令：`./tools/deploy.ps1 -GameRoot D:/steam/steam/steamapps/common/Mewgenics -Configuration Release`。
- `mods\AutoCattery.dll` 已部署。
- `Mewtator\mods\AutoCattery\description.json` 为 v0.5.25。
- `modlist.txt` 中 `AutoCattery` 恰好一次并位于最后。
- 现有 `user_config.json`、`protection.json`、`AutoCatteryData`、日志、诊断数据和
  其他 MOD 均保留。
- 部署时游戏进程未运行；没有启动或控制 Mewgenics。

## 玩家验证状态

待玩家通过 Mewtator 验证：

1. 游戏正常启动，chainloader 加载 v0.5.25，并输出三个 runtime RVA。
2. 进入 House 后 F10 可打开/关闭面板，Esc 可关闭。
3. 两个普通 House 按钮均可点击。
4. 切换存档或离开并重新进入 House 后 UI 仍正常。
5. beta 玩家使用同一个 v0.5.25 包完成相同步骤。

当前自动检查只直接读取了本机公开版 EXE。beta 地址漂移已通过二进制分析确认，
但 beta 进程中的完整 Hook 链、F10 和 House UI 行为仍必须由玩家实机确认。

## 风险与后续边界

- 新版本若改变某个目标函数的指令形态，使目标为零匹配或多匹配，MOD 会拒绝
  安装未知 Hook；这会使 AutoCattery UI 不可用，但避免向错误地址写 Hook。
- 本阶段不修改存档、游戏原文件、保护规则、规划算法或其他 MOD。
- 本阶段不发布、不 push；是否制作公开 Release 需在玩家验证后另行决定。

## Git

- Stage 31 本地提交：本报告所在的顶层提交；精确哈希在任务最终交付中记录。
- MewUI 子模块修改提交：`c70774e`，由顶层提交引用。
- 是否 push：否。

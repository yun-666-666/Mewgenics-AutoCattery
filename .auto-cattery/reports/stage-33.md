# Stage 33：BETA 跨 MOD scene-ready Hook 兼容修复

更新日期：2026-09-05

状态：实现、自动验证和本机部署完成；玩家三 MOD 共存实机验证待完成。

## 问题与结论

- 玩家使用 v0.5.26 进入当前 BETA House 后不再闪退，但 AutoCattery 的 F10 面板和
  两个普通 House 按钮全部不可用。
- 同次启动 PID `28312` 中，AutoCattery 于 `23:38:55` 完成 `AC1000`、`AC1001`
  和 `AC1200`；随后 CombineDuplicateFurniture v0.6.19 通过 Mewjector 在
  scene-ready RVA `0x96AC50` 安装优先级 40 的 Hook。
- AutoCattery 的 MewUI 初始化由定时器延迟执行，从 `23:38:55.969` 开始持续报告
  `Runtime UI locator could not uniquely resolve scene-ready update`。同次启动没有
  `AC1202`、`HouseReady`、`AC18000`、`AC3100` 或 `AC4100`，因此整个 UI 路径从未
  启动。
- 同一磁盘 BETA EXE 的离线定位仍可唯一解析全部 30 个 UI 地址，包括 scene-ready
  `0x96AC50`、Button activate `0x97E8E0`、Button can-activate `0x97EAF0`。
- 根因是家具合并 MOD 先行 Hook 改写了进程内 scene-ready 函数开头，导致旧的
  live-image 特征扫描安全失败；不是 AutoCattery 根据 BETA 版本号禁用自身。
- SkillsPassivesFirst 只 Hook `0x384450` 和 `0x3803B0`，现有证据不支持它导致本次
  UI 初始化失败，因此没有向“喵喵-技能全升级”项目创建修复任务。
- 已将上述证据发送到“喵喵-家具合并”项目的新任务“修复 AutoCattery Hook 兼容”，
  供对应项目处理共享 Hook 的兼容性。

## 实现

- MewUI 使用 `GetModuleFileNameW` 获取当前进程实际加载的游戏 EXE 路径，通过
  `CreateFileMappingW` 的 `PAGE_READONLY | SEC_IMAGE` 和 `MapViewOfFile` 创建未被
  其他 MOD Hook 改写的干净 PE 映像。
- 干净映像与当前加载映像比较 PE `TimeDateStamp` 和 `SizeOfImage`，只在确认是同一
  映像后用于地址解析；这不是版本白名单或固定地址门禁。
- 全部 30 个运行时 UI 地址优先从干净映像定位。干净映像无法打开时才回退到进程内
  映像，并明确记录 locator source。
- 定位结果仍转换为当前进程 RVA，并通过 Mewjector `InstallHook` 安装，因此
  AutoCattery 可以加入 CombineDuplicateFurniture 已建立的 scene-ready Hook 链。
- 成功启动将记录 `Runtime UI locator source: clean executable image` 和三项关键
  runtime RVA；保留完整 F10、Esc、两个普通 House 按钮、MoveOnly、配置、保护规则
  与本地数据。
- 最终 code-simplifier 仅整理本阶段新增的映像视图资源清理路径，统一由
  `MewUI_CloseImageView` 释放，没有改变定位或 Hook 行为。

## 修改文件

- `third_party/mew_ui_api/src/native/mew_ui_api.c`
- `third_party/mew_ui_api` 子模块指针
- `tests/mew_ui_runtime_locator_tests.py`
- `CMakeLists.txt`
- `assets/description.json`
- `CHANGELOG.md`
- `docs/RELEASE_NOTES_v0.5.27.md`
- `CODEX_TASK.md`
- `docs/implementation-status.md`
- `.auto-cattery/state.json`
- `.auto-cattery/reports/stage-33.md`

## 构建与检查

- 当前 BETA locator 测试：PASS，30 个 UI 地址全部唯一；scene-ready `0x96AC50`、
  Button activate `0x97E8E0`、Button can-activate `0x97EAF0`。
- 新增回归用例复制 `.text` 并改写 scene-ready 特征的首个必要字节，确认旧的进程内
  特征无法再唯一匹配，而原始干净映像仍可解析。
- 首轮 Debug 完整构建：成功；CTest 5/5 通过；DLL load smoke 通过。
- 首轮 Release 完整构建：成功；CTest 5/5 通过；DLL load smoke 通过。
- code-simplifier 后执行 `./tools/build.ps1 -Configuration Debug`：10 个增量任务成功；
  CTest 5/5 通过；DLL load smoke 通过。
- code-simplifier 后执行 `./tools/build.ps1 -Configuration Release`：10 个增量任务
  成功；CTest 5/5 通过；DLL load smoke 通过。
- 曾直接调用 `cmake --build build-ninja`，但当前终端未配置 `cmake` PATH，命令没有
  执行；随后使用项目 `build.ps1` 成功完成同等且更完整的增量构建与测试，因此这不
  是代码或构建失败。
- `git diff --check`、JSON 解析和最终工作树检查在顶层提交前完成。

## 部署

- 部署前确认没有 `Mewgenics` 进程；没有启动或控制游戏。
- `./tools/deploy.ps1 -GameRoot D:/steam/steam/steamapps/common/Mewgenics -Configuration Release`
  - Release DLL 和 Mewtator 数据 MOD 部署成功，版本为 v0.5.27。
- `./tools/verify_install.ps1 -GameRoot D:/steam/steam/steamapps/common/Mewgenics`
  - 退出码 0。
- `Mods\AutoCattery.dll`、`user_config.json`、`protection.json` 和
  `AutoCatteryData` 均存在；`modlist.txt` 中 AutoCattery 恰好一次且位于最后。
- 其他 MOD 没有删除、禁用或改名；原有日志和诊断文件保留。
- 当前日志最后修改时间早于 v0.5.27 部署时间，尚无部署后的玩家启动记录。

## 玩家验证状态

待玩家保持 AutoCattery、CombineDuplicateFurniture 和 SkillsPassivesFirst 三个 DLL
同时启用，通过 Mewtator 启动当前 BETA：

1. 日志应出现 `Runtime UI locator source: clean executable image`。
2. 日志应出现 `Runtime UI functions located: scene=0x96AC50 can_activate=0x97EAF0 activate=0x97E8E0` 和 `AC1202`。
3. 进入 House 停留至少 10 秒，确认出现 `HouseReady`、`AC18000`、`AC3100`、
   `AC4100`，并确认不闪退。
4. 测试 F10 打开/关闭、Esc，以及两个普通 House 按钮。
5. 离开并重新进入 House，或切换存档后再次执行上述检查。

自动构建、离线地址定位和安装检查不能替代实际游戏 UI 与 Hook 链验证；玩家实机
结果仍是本阶段接受边界。若失败，需提供同一次启动的完整 chainloader、
AutoCattery 日志和最新 WER/崩溃文件。

## 风险与后续边界

- v0.5.27 已消除已确认的“先行 Hook 擦除 live-image 特征”问题，但尚无部署后运行
  日志证明当前 BETA 中三个 MOD 的实际 Hook 链和 UI 生命周期完整通过。
- 本阶段不修改存档、游戏原文件、家具合并或技能全升级 MOD，不改变保护规则、规划
  算法或真实淘汰状态。
- 本阶段不发布、不 push；是否制作公开 Release 需在玩家验证后另行决定。

## Git

- MewUI 子模块本地提交：`b97fe05 fix: locate hooks from clean game image`。
- Stage 33 顶层本地提交：本报告所在的顶层提交；精确哈希在任务最终交付中记录。
- 是否 push：否。

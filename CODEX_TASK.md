# CODEX CURRENT STATUS

更新日期：2026-09-04

Stage 31 的 v0.5.25 兼容性实现、Debug/Release 构建、自动测试和本机部署已经
完成，当前等待玩家实机验证。公开 Latest Release 仍为玩家已验收的 v0.5.24；
本机游戏目录已部署 v0.5.25 测试候选。

## Required reading

1. `AGENTS.md`
2. `README.md`
3. `docs/implementation-status.md`
4. `docs/pre-completion-functional-roadmap.md`
5. `.auto-cattery/state.json`
6. 当前代码、测试、最新运行日志和 `git status --short`

## Stage 31 scope

- 原因：v0.5.24 使用固定 MewUI RVA；游戏 beta 的 scene-ready 地址发生变化后，
  Hook 会落到错误位置并导致启动崩溃。
- 修复：从实际运行的 `Mewgenics.exe` 映像 `.text` 中唯一定位所需 UI 函数，
  再把原 canonical RVA 映射到本次运行解析出的 RVA。
- 不使用 EXE 哈希、时间戳或单版本地址白名单；定位不唯一或不可用时不安装错误
  Hook，AutoCattery UI 初始化安全失败。
- 启动日志会输出 scene-ready、Button activate 和 Button can-activate 的运行时
  RVA，便于区分实际加载布局。

## Completed verification

- Debug：CTest 5/5 通过，DLL load smoke 通过。
- Release：CTest 5/5 通过，DLL load smoke 通过。
- 当前本机 EXE：离线定位 30 个 UI 地址均为唯一匹配；scene-ready 为
  `0x962820`，Button activate 为 `0x9764B0`，Button can-activate 为
  `0x9766C0`。
- Release 已部署到游戏目录；`AutoCattery.dll` 存在，Mewtator 数据 MOD 为
  v0.5.25，`modlist.txt` 中 AutoCattery 恰好一次且位于最后。
- 现有 `user_config.json`、`protection.json` 和 `AutoCatteryData` 均保留。

## Next work

由玩家通过 Mewtator 启动并验证：启动、进入 House、F10 开关与 Esc、两个 House
按钮、切换存档或离开/重进 House。beta 玩家也需用同一个 v0.5.25 包验证。若有
崩溃或 UI 失效，收集同一次启动的完整 `chainloader.log`、
`mods\AutoCattery\logs\auto_cattery.log` 和最新 `mod_logs\crashes\*.txt`。

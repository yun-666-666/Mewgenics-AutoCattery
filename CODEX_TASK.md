# CODEX CURRENT STATUS

更新日期：2026-09-04

Stage 32 的 v0.5.26 BETA House 闪退修复、Debug/Release 构建、自动测试和
本机部署已经完成，当前等待玩家实机验证。公开 Latest Release 仍为玩家已验收的
v0.5.24；本机游戏目录已部署 v0.5.26 测试候选。

## Required reading

1. `AGENTS.md`
2. `README.md`
3. `docs/implementation-status.md`
4. `docs/pre-completion-functional-roadmap.md`
5. `.auto-cattery/state.json`
6. 当前代码、测试、最新运行日志和 `git status --short`

## Stage 32 scope

- 当前 BETA 两次进入 House 后都在管理面板完成隐藏挂载（`AC18000`）后约两秒
  终止，异常均为 `0xC0000005`、`Mewgenics.exe+0x963041`。
- 管理面板原来会在首次挂载时查找大量超过 15 字节的 MOD 自有节点名，使原生
  `MewNarrowString` 进入 heap-backed 所有权转移路径；该路径已有 House 初次挂载
  堆损坏历史。
- 修复把全部 145 个管理面板 artwork/text 实例改为唯一、ASCII 且不超过 15 字节
  的名称，使原生 child lookup 保持 inline-string 路径。
- 保留完整 F10 设置、猫保护、完整预览、Esc、两个普通 House 按钮和 MoveOnly；
  没有在 BETA 下禁用功能。
- 当前 BETA 的运行时地址定位仍唯一通过：scene-ready `0x96AC50`、Button activate
  `0x97E8E0`、Button can-activate `0x97EAF0`。

## Completed verification

- 聚焦 SWF 资产测试：145 个面板实例名称唯一、完整落入资产、均不超过 15 字节，
  且 C++ 查找名与资产一致。
- Debug：CTest 5/5 通过，DLL load smoke 通过。
- Release：CTest 5/5 通过，DLL load smoke 通过。
- Release 已部署到游戏目录；`AutoCattery.dll` 存在，Mewtator 数据 MOD 为
  v0.5.26，`modlist.txt` 中 AutoCattery 恰好一次且位于最后。
- 现有 `user_config.json`、`protection.json` 和 `AutoCatteryData` 均保留。
- 部署时 Mewgenics 未运行；没有启动或控制游戏。

## Next work

由玩家通过 Mewtator 启动当前 BETA，并使用此前闪退的同一存档验证：进入 House
不再闪退、F10 开关与 Esc、两个普通 House 按钮、离开并重进 House或切换存档。
若仍崩溃，收集同一次启动的完整 `chainloader.log`、
`Mods\AutoCattery\logs\auto_cattery.log` 和最新 Windows WER/崩溃文件。

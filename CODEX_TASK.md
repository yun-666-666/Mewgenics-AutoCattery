# CODEX CURRENT STATUS

更新日期：2026-09-05

Stage 33 的 v0.5.27 BETA 跨 MOD Hook 兼容修复、Debug/Release 构建、自动测试和
本机部署已经完成，当前等待玩家实机验证。公开 Latest Release 仍为玩家已验收的
v0.5.24；本机游戏目录已部署 v0.5.27 测试候选。

## Required reading

1. `AGENTS.md`
2. `README.md`
3. `docs/implementation-status.md`
4. `docs/pre-completion-functional-roadmap.md`
5. `.auto-cattery/state.json`
6. 当前代码、测试、最新运行日志和 `git status --short`

## Stage 33 scope

- 玩家使用 v0.5.26 进入当前 BETA House 后不再闪退，但 AutoCattery 的 F10 和两个
  普通 House 按钮全部缺失。
- 同次启动 PID `28312` 中，CombineDuplicateFurniture v0.6.19 先通过 Mewjector
  安装 scene-ready Hook `0x96AC50`；随后 AutoCattery 的延迟 MewUI 初始化持续报告
  `Runtime UI locator could not uniquely resolve scene-ready update`。
- 磁盘上的同一 BETA EXE 仍可唯一解析 30 个 UI 地址：scene-ready `0x96AC50`、
  Button activate `0x97E8E0`、Button can-activate `0x97EAF0`。故障来自先行 Hook
  改写了进程内特征，不是 BETA 版本号禁用。
- 修复从当前进程实际 EXE 路径创建干净 `SEC_IMAGE` 映射并解析 RVA，再通过
  Mewjector 加入既有 Hook 链；只有干净映像不可用时才回退进程内映像。
- 保留 v0.5.26 的短面板节点名、完整 F10、两个普通 House 按钮和 MoveOnly。

## Completed verification

- 当前 BETA 离线定位 30/30 唯一通过；新增测试确认模拟 scene-ready Hook 改写会使
  旧的进程内签名失效。
- Debug：CTest 5/5 通过，DLL load smoke 通过。
- Release：CTest 5/5 通过，DLL load smoke 通过。
- MewUI 子模块本地提交：`b97fe05`。
- Release 已部署到游戏目录；`AutoCattery.dll` 存在，Mewtator 数据 MOD 为
  v0.5.27，`modlist.txt` 中 AutoCattery 恰好一次且位于最后。
- 现有 `user_config.json`、`protection.json` 和 `AutoCatteryData` 均保留。
- 部署时 Mewgenics 未运行；没有启动或控制游戏。

## Next work

由玩家通过 Mewtator 启动当前 BETA，并保持 AutoCattery、
CombineDuplicateFurniture、SkillsPassivesFirst 同时启用。日志应出现
`Runtime UI locator source: clean executable image`、三项 runtime RVA 和 `AC1202`；
进入 House 后应出现 `HouseReady`、`AC18000`、`AC3100`、`AC4100`，并验证 F10、
Esc、两个普通 House 按钮以及离开/重进 House或切换存档。

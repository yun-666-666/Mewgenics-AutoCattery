# CODEX CURRENT STATUS

更新日期：2026-09-05

Stage 34 的 v0.5.28 BETA 原生房间枚举入口修复、自动验证和本机部署已经完成，
等待玩家实机复测。玩家使用 v0.5.27 连续两次进入 House 后均闪退；公开 Latest
Release 仍为玩家已验收的 v0.5.24。

## Required reading

1. `AGENTS.md`
2. `README.md`
3. `docs/implementation-status.md`
4. `docs/pre-completion-functional-roadmap.md`
5. `.auto-cattery/state.json`
6. 当前代码、测试、最新运行日志和 `git status --short`

## Stage 34 scope

- 两次独立启动 PID `29952`、`22356` 均成功记录 clean-image locator、三项 runtime
  UI RVA、`AC1202`、`HouseReady` 和 `AC18000`，证明 Stage 33 的跨 MOD Hook 修复生效。
- 两份完整转储均为 `0xC0000005`、`Mewgenics.exe+0x963041`，崩溃线程栈含连续
  AutoCattery 帧；调用点把目标设为 `game_base + 0x963040`，参数 component id 为
  `0x1D2`，对应 `AcMewEnumerateNativeHouseRooms()` 的固定房间 bucket 准备入口。
- 当前 BETA 的真实函数入口为 `0x963030`；`0x963040` 位于入口内部一条 `call`
  指令的最后一个字节，执行会跳到 `0x963041` 并写入随机地址。
- CombineDuplicateFurniture v0.6.20 当时只安装 `0x1AC360` Hook，并明确延迟
  scene-ready Hook 到家具模式；本次崩溃不归因于家具合并 MOD。
- 修复必须同时保留已验证稳定版 `0x963040` 和当前 BETA `0x963030`，调用前验证
  不受相对调用位移影响的函数签名；未知或歧义布局安全返回 0，不调用原生入口。

## Required verification

- 单元测试覆盖稳定版/BETA 入口选择和签名不匹配时的 fail-closed。
- 当前 BETA 离线特征必须唯一解析到 component bucket prepare `0x963030`。
- Debug/Release CTest 与 DLL load smoke 必须通过。
- 游戏退出后部署 v0.5.28，保留配置、保护规则、本地数据和其他 MOD。

## Verification completed

- 当前 BETA 离线特征唯一解析 30 个 MewUI 地址，并将 component bucket prepare
  解析为 `0x963030`。
- Debug 与 Release 构建均成功；CTest 5/5 与 DLL load smoke 均通过。
- 游戏退出后已部署 v0.5.28；AutoCattery 在 Mewtator `modlist.txt` 中恰好一次且
  位于最后，用户配置、保护规则、本地数据和另外两个 MOD 均保留。

## Next work

由玩家保持 AutoCattery、CombineDuplicateFurniture、SkillsPassivesFirst 同时启用，
通过 Mewtator 启动当前 BETA。进入 House 后应越过此前 `AC18000` 后的崩溃点，出现
`AC3100`、`AC4100`，再验证 F10、Esc、两个普通 House 按钮、MoveOnly 预览以及
离开/重进 House。

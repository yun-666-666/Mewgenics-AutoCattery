# Stage 34：BETA 原生房间枚举入口修复

更新日期：2026-09-05

状态：实现、自动验证和本机部署完成；玩家三 MOD 共存实机验证待完成。

## 问题与结论

- 玩家使用 v0.5.27 连续两次启动当前 BETA，进入 House 后均在 `AC18000` 之后
  闪退，对应 PID 为 `29952` 和 `22356`。
- 两次日志均成功记录 `Runtime UI locator source: clean executable image`、
  scene-ready `0x96AC50`、Button activate `0x97E8E0`、Button can-activate
  `0x97EAF0`、`AC1202`、`HouseReady` 和 `AC18000`，证明 Stage 33 的跨 MOD Hook
  定位修复已经生效。
- 两份完整转储均为 `0xC0000005`，终止于 `Mewgenics.exe+0x963041`，崩溃线程栈
  含连续 AutoCattery 帧。调用点计算 `game_base + 0x963040`，参数 component id 为
  `0x1D2`，对应 `AcMewEnumerateNativeHouseRooms()` 的 component-bucket prepare。
- 当前 BETA 的真实函数入口为 `0x963030`。旧地址 `0x963040` 位于从 `0x96303D`
  开始的一条相对 `call` 指令的最后一个字节，从该处执行会落到 `0x963041` 并向
  非法地址写入。
- CombineDuplicateFurniture v0.6.20 在这两次崩溃前只安装了 `0x1AC360` Hook，
  且日志明确将 scene-ready Hook 延迟到家具模式；本次崩溃归因于 AutoCattery，
  不是家具合并或 SkillsPassivesFirst。

## 实现

- 保留已验证稳定版候选 `0x963040`，新增当前 BETA 候选 `0x963030`。
- 对两个候选入口分别校验三段不包含相对调用位移的机器码签名。
- 仅在恰好一个候选完整匹配且目标内存具有可执行保护时构造并调用函数指针。
- 未匹配或出现歧义时安全返回 0，停用原生房间枚举，不再调用未经验证的地址。
- 新增纯选择函数和单元测试，覆盖稳定版、当前 BETA 与签名不匹配三种路径。
- 当前 BETA 离线定位测试增加 component-bucket prepare，并确认唯一解析到
  `0x963030`。
- 最终 code-simplifier 复核只将 `prepare_rva == 0` 检查提前到函数指针计算之前，
  未改变接口、输出或行为。

## 修改文件

- `src/ui/mew_ui_house_move_adapter.c`
- `src/ui/mew_ui_house_move_adapter.h`
- `tests/mew_ui_house_move_probe_tests.cpp`
- `tests/mew_ui_runtime_locator_tests.py`
- `CMakeLists.txt`
- `assets/description.json`
- `CHANGELOG.md`
- `docs/RELEASE_NOTES_v0.5.28.md`
- `CODEX_TASK.md`
- `docs/implementation-status.md`
- `.auto-cattery/state.json`
- `.auto-cattery/reports/stage-34.md`

## 构建与检查

- 当前 BETA 离线定位：PASS；30 个 MewUI 地址唯一解析，scene-ready `0x96AC50`、
  Button activate `0x97E8E0`、Button can-activate `0x97EAF0`、component bucket
  prepare `0x963030`。
- 首轮 Debug 完整构建成功；CTest 5/5 通过；DLL load smoke 通过。
- 首轮 Release 完整构建成功；CTest 5/5 通过；DLL load smoke 通过。
- code-simplifier 后 Debug 增量构建成功；CTest 5/5 通过；DLL load smoke 通过。
- code-simplifier 后 Release 增量构建成功；CTest 5/5 通过；DLL load smoke 通过。
- 测试代码最初使用 `std::fill(..., 0U)` 产生 C4244 警告，已改为
  `std::uint8_t{0}`；后续 Debug/Release 构建没有该警告。

## 部署

- 部署前确认没有 `Mewgenics` 进程；没有启动或控制游戏。
- 执行 `./tools/deploy.ps1 -GameRoot D:/steam/steam/steamapps/common/Mewgenics
  -Configuration Release`，Release DLL 与 Mewtator 数据 MOD 部署成功，版本为
  v0.5.28。
- 执行 `./tools/verify_install.ps1 -GameRoot
  D:/steam/steam/steamapps/common/Mewgenics`，退出码 0；安装版本、DLL 架构、已启用
  数据 MOD 和 14 个职业的升级重骰数据均通过检查。
- `mods/AutoCattery.dll`、`mods/AutoCattery/config/user_config.json`、
  `mods/AutoCattery/config/protection.json` 和 `mods/AutoCattery` 运行数据目录均存在。
- Mewtator 数据 MOD 的 `description.json` 为 v0.5.28；`modlist.txt` 中 AutoCattery
  恰好一次且位于最后。
- `CombineDuplicateFurniture.dll`、`SkillsPassivesFirst.dll` 和
  `SkillsPassivesFirstData` 均保留；没有删除、禁用或改名其他 MOD。

## 玩家验证状态

待玩家保持 AutoCattery、CombineDuplicateFurniture 和 SkillsPassivesFirst 同时启用，
通过 Mewtator 启动当前 BETA：

1. 进入此前闪退的存档和 House，停留至少 10 秒，确认越过 `AC18000` 后不闪退。
2. 确认两个普通 House 按钮出现，并测试 F10 打开/关闭与 Esc。
3. 第一次点击自动整理，确认能够生成 MoveOnly 预览；无需执行实际移动即可覆盖原生
   房间枚举路径。
4. 离开并重新进入 House，再重复一次上述检查。
5. 日志应继续出现 clean-image locator、三项 runtime UI RVA、`AC1202`、
   `HouseReady`、`AC18000`，并进一步出现 `AC3100`、`AC4100`。

自动构建、离线地址定位和部署检查不能替代实际游戏行为；玩家实机结果仍是本阶段
接受边界。若仍崩溃，需提供同一次启动的完整 chainloader、AutoCattery 日志和最新
完整转储。

## 风险与后续边界

- v0.5.28 已消除当前两个完整转储证明的错误原生入口调用，但尚无部署后玩家运行
  记录证明当前 BETA 的完整 House、F10、按钮和 MoveOnly 流程通过。
- 本阶段不修改存档、游戏原文件、家具合并或技能 MOD，不改变保护规则、规划算法或
  真实淘汰状态。
- 本阶段不发布、不 push；是否制作公开 Release 需在玩家验证后另行决定。

## Git

- Stage 34 顶层本地提交：本报告所在的提交；精确哈希在任务最终交付中记录。
- 是否 push：否。

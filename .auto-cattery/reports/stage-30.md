# Stage 30：主存档多日反馈分析与全房交叉稳健繁育池

更新日期：2026-08-08

状态：v0.5.15 Debug/Release 全量构建与 4/4 CTest 通过，主存档只读探针通过，
Release 已部署并完成安装校验；等待玩家继续积累出生结果。

## 多日数据边界

- 分析来源为玩家已明确开启的本地 `AutoCatteryData`，未读取或修改活动存档，
  未把采集 JSON、猫名、存档名、个人路径或账号信息复制进仓库。
- 可用主存档历史覆盖游戏第 257–264 天，不是单一最新快照；同一天的重复采样
  用于确认日内规划和最终房间状态，每日趋势使用当天最后一份有效快照。
- 现有收集器在成功生成整理预览时按游戏日和内容摘要保留不同 JSON，因此本轮
  已具备跨日分析条件；没有新增自动休息、自动推进日期或存档写入。

## 第 257–264 天变化

- House 猫总数 `87 -> 94`；期间观察到 40 只新进入当前 House 快照，其中
  33 只是具有父母和第 1 天年龄证据的新生猫，另 7 只为第 2 天且无父母记录的
  新出现猫，来源保持未知。
- 33 只猫离开后续 House 快照；其中 15 只在最后出现时已标记死亡，另外 18 只
  的离开原因无法由当前数据证明，不写成淘汰、出售或远征事实。
- 母/公从 `8/79 -> 21/73`，极端公猫偏斜有所缓解但仍明显存在。
- 全屋平均七维缺口 `4.678 -> 4.511`；缺口不高于 3 的猫 `35 -> 42`；全 7 猫
  仍为 0。七维最低值至少 6 的绝对数量 `43 -> 43`，因总猫数增加，占比下降。
- 33 只已确认新生猫平均七维缺口 2.879、平均后代 COI 0.2125，其中 23 只
  缺口不高于 3。

## v0.5.14 后的实际繁殖反馈

- 第 264 天观察到的 5 只新生猫来自繁育房内 4 组不同交叉组合，而不是首选
  配对；这证明同房后游戏会从房内候选中交叉选配，不能把“多组独立好配对”
  等同于“整个房间任意组合都好”。
- 这 5 只新生猫平均七维缺口 2.60，优于前六个观察日新生猫的 2.93；平均后代
  COI 0.168，优于此前的 0.220；4/5 缺口不高于 3。样本仍小，只作为方向证据，
  不宣称已经证明长期因果。

## 算法优化

- `current-build-purpose-aware-room-planning-v9` 保留首选配对作为繁育房种子。
- 其余繁育槽位改为逐猫构建全房交叉稳健池：候选猫必须与已经选入的全部异性
  猫都有合格配对证据。
- 候选优先级依次为：稳定全 7 阶段的全交叉稳定性、最大化最弱交叉配对分数、
  最小化最坏后代 COI、最大化平均交叉配对分数、稳定 CatId。
- 若缺少某个交叉组合的性向/亲缘/COI 证据，该猫不会被标记为繁育池优选；
  不猜测游戏繁殖概率，也不启用自动休息、自动淘汰或自动出征。

## 文件

- 算法：`src/room_planning/balanced_move_only_breeding.cpp`、
  `include/auto_cattery/room_planning/domain.hpp`。
- 回归：`tests/balanced_move_only_planner_tests.cpp`。
- 版本与文档：`CMakeLists.txt`、`assets/description.json`、`CHANGELOG.md`、
  `README.md`、`README_EN.md`、`CODEX_TASK.md`、相关 docs、v0.5.15 发布说明、
  状态文件和本报告。

## 验证与部署

- 确定性回归包含一个陷阱：独立第二名配对自身很强，但与首选配对交叉时 COI
  很差；v9 必须选择独立排名稍低、但与整个已选群体交叉更稳健的组合。
- `git diff --check`：通过。
- `\.\tools\build.ps1 -Configuration Debug`：通过，4/4 CTest 通过。
- `\.\tools\build.ps1 -Configuration Release`：通过，4/4 CTest 通过。
- Debug `snapshot_probe`：第 264 天、94 猫、4 房、`warnings=1`、`errors=0`、
  稳定 ID/评分/保护/房间计划，零真实淘汰。
- Debug `breeding_data_probe`：FOUNDATION，94 猫、4465 个配对 COI，首选配对
  七维覆盖 7/7、后代 COI 0，目标房 `Attic`；v9 对旧 v8 房间成员生成 48 次
  重新分房预览，实际执行仍受二次确认和每 tick 8 次批次限制。
- `\.\tools\deploy.ps1 -GameRoot '<GAME_ROOT>' -Configuration Release`：
  成功，保留并同步玩家当前升级重骰 20。
- `\.\tools\verify_install.ps1 -GameRoot '<GAME_ROOT>'`：
  DLL、x64、Mewtator 启用状态和 14 职业重骰验证通过。
- build、dist、安装 DLL 均为 1,379,328 字节，SHA-256 均为
  `101CA66A6EAE41A09FF5D8A1987FBB3760556F068E81E19D350B47CEDE3E5750`；
  已安装数据 MOD 显示 v0.5.15。

## 游戏验证与风险

- 需要玩家继续至少结束数个游戏日并正常生成预览，观察 v0.5.15 后新生猫的
  七维缺口、COI 和实际父母组合；单日 5 只样本不足以证明长期收益。
- 全房交叉稳健池会改变当前 94 猫主存档的具体繁育房成员，可能产生一次新的
  MoveOnly 计划；仍需第二次点击确认，并按每 UI tick 最多 8 次原生移动执行。

本轮最终本地 commit：由最终回复记录；提交对象不能在自身内容中包含最终哈希。

是否 push：是（用户明确要求；待最终推送完成后确认）

## 2026-08-26 v0.5.17 战斗卡顿热修复

- 玩家本次 11:45:31–12:47:13 战斗会话中，AutoCattery 没有执行整理业务，
  但 v0.5.15 `MewUiBridge::OnTick()` 仍按 UI 帧检查配置文件、枚举场景并分配
  临时场景容器。同期 Mewjector 记录 204 次被游戏捕获的 C++ 异常；系统日志
  没有 GPU 复位、WHEA、存储重试或战斗时 CPU 固件限频。
- `Battle` 或 `Map` 就绪时，AutoCattery 现在立即废弃 House/F10/推荐 UI 和
  MoveOnly 运行时入口，并停止业务、UI、控制器及配置轮询；仅保留每秒一次的
  最小场景探针，以便检测远征结束。
- `Battle`/`Map` 消失，或进入 `SaveSelectionScreen`/`ClassChooser` 后，MOD
  在同一次探针中退出休眠，重新按正常生命周期观察 House 并挂载 UI。
- 配置文件时间检查从每个 UI tick 改为最多每 500 ms 一次，保留现有 500 ms
  热重载防抖语义。
- 修改文件：`src/ui/mew_ui_bridge.cpp`、
  `include/auto_cattery/ui/mew_ui_bridge.hpp`、`CMakeLists.txt`、
  `assets/description.json`、`CHANGELOG.md`、
  `docs/RELEASE_NOTES_v0.5.17.md` 和本报告。
- 构建、测试、部署结果：
  - 使用现有 Visual Studio 2022 Community 的 `VsDevCmd.bat`、MSVC
    14.44.35207 和 Ninja 构建；默认 `tools/build.ps1` 因 VS Installer 已不再
    注册该现存实例而无法使用其 Visual Studio generator 路径。
  - `build-v0517-debug` 增量构建成功，CTest 4/4 通过。
  - `build-v0517-release` 完整 176 步构建成功。首次 CTest 的三个冒烟测试
    通过，`phase14_unit_tests` 在 Release 优化速度下因异步预览未及时完成而
    出现一次时序失败；同一二进制直接复现通过，随后定向 CTest 1/1 通过。
    本轮未改动该无关测试。
  - Release DLL 已部署到 `Mewgenics/mods/AutoCattery.dll`，安装文件大小
    1,380,352 字节；Mewtator `AutoCattery/description.json` 显示 v0.5.17。
    部署保留现有保护配置，并同步玩家当前升级重骰 20。
- 游戏验证状态：未启动或操纵游戏；需要玩家重新进行一段战斗，比较持续帧时间
  和返回 House 时的日志。
- 风险：游戏若短暂保留就绪的 `Battle`/`Map` 场景，返回 House 后最多可能
  延迟约 1 秒恢复；存档选择和职业选择场景会覆盖残留远征场景，避免永久休眠。
  本轮不会改变 MoveOnly、评分、保护或存档写入边界。
- 省略的后续工作：未修改 Mewjector 的全局异常记录策略，避免把本次
  AutoCattery 热修复扩大为前置加载器变更。
- 本次本地 commit：由最终回复记录。
- 是否 push：否。

## 2026-08-26 v0.5.18 粘性远征休眠修复

- 最新 13:59:25–15:09:10 会话已不再加载 `AutoCatteryFurniture.dll`，但主
  AutoCattery 在同一次远征中记录了大量交替的 `AC1203`/`AC1204`。原因是
  战斗动画、房间切换和过场会让 `Battle`/`Map` 暂时不就绪，v0.5.17 因而
  错误恢复全部 UI、业务和配置轮询，随后又重新休眠。
- 回家场景同样存在短暂 ready 窗口；过早发布 `HouseReady` 会立即挂载面板和
  按钮，随后又发布 `UnsafeTransition` 并废弃 UI，形成玩家看到的闪烁。最新
  日志中的过早 House 挂载还伴随 AutoCattery 调用链内的空指针访问。
- v0.5.18 在首次识别 `Battle`/`Map` 后保持粘性休眠，不再因中间场景缺失而
  唤醒。只有明确进入 `SaveSelectionScreen`/`ClassChooser`，或 `House` 连续
  ready 3 秒，才退出休眠并恢复正常 House 生命周期。
- 修改文件：`src/ui/mew_ui_bridge.cpp`、
  `include/auto_cattery/ui/mew_ui_bridge.hpp`、`CMakeLists.txt`、
  `assets/description.json`、`CHANGELOG.md`、
  `docs/RELEASE_NOTES_v0.5.18.md` 和本报告。
- 构建、测试、部署结果：
  - `build-v0517-debug` 完整构建成功，CTest 4/4 通过。
  - `build-v0517-release` 完整构建成功，CTest 4/4 通过。
  - Release DLL 已部署到 `Mewgenics/mods/AutoCattery.dll`，安装文件大小
    1,380,864 字节；Mewtator `AutoCattery/description.json` 显示 v0.5.18。
  - 已再次确认 `Mewgenics/mods/AutoCatteryFurniture.dll` 不存在，部署主 MOD
    未将家具组件恢复；保护配置保留，升级重骰继续同步为 20。
- 游戏验证状态：未启动或操纵游戏；部署后由玩家验证长时间远征和稳定回家。
- 本次本地 commit：由最终回复记录。
- 是否 push：否。

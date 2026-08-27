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

## 2026-08-26 v0.5.19 底层战斗休眠修复

- 最新 18:56:17–19:35:53 玩家会话中，v0.5.18 只记录一次 `AC1203`，没有
  `AC1204`；这证明上层 UI、工作流和配置轮询已在整段远征保持休眠。玩家仍在
  不到两关后严重卡顿，因此继续调整 `MewUiBridge::OnTick()` 的场景粘性不能
  解决剩余问题。
- 根因位于上层回调之前：共享 MewUI 场景钩子仍会在每个 ready update 执行
  `MewUI_Tick()`，遍历最多 256 个按钮记录；两个全局原生按钮钩子也仍会查找
  AutoCattery 按钮记录。
- v0.5.19 新增 MewUI 底层 work-suspended 状态。远征休眠锁定后，场景回调仅
  保留每秒一次的唤醒探针，跳过按钮维护循环；按钮可激活和激活钩子直接透传
  原游戏/下游钩子，不再扫描 AutoCattery 记录。稳定 House 或选择场景恢复前
  明确解除底层休眠，初始化和关闭时也重置该状态。
- 同期加载的独立 `CombineDuplicateFurniture.dll` 共享场景 ready 钩子。其
  v0.6.2 空闲时只检查热键；仅在 House 内按下热键或批处理仍在运行时才探测
  家具组件，战斗期间不再逐 update 扫描家具 UI。
- 主 MOD 修改文件：`src/ui/mew_ui_bridge.cpp`、`CMakeLists.txt`、
  `assets/description.json`、`CHANGELOG.md`、
  `docs/RELEASE_NOTES_v0.5.19.md`、MewUI 子模块和本报告。
- 构建与测试：
  - `build-v0517-debug` 完整 221 步构建成功，CTest 4/4 通过。
  - `build-v0517-release` 完整 221 步构建成功，CTest 4/4 通过。
  - `CombineDuplicateFurniture` v0.6.2 Release 构建完成，
    `cdf_focused_tests` 1/1 通过，634 项家具目录探针通过。
- 部署结果：
  - `Mewgenics/mods/AutoCattery.dll` 已部署，大小 1,380,864 字节；Mewtator
    描述显示 v0.5.19。
  - `Mewgenics/Mods/CombineDuplicateFurniture.dll` 已部署，大小 206,336
    字节；Mewtator 描述显示 v0.6.2。
  - `Mewgenics/mods/AutoCatteryFurniture.dll` 仍不存在；玩家保护配置保留，
    升级重骰仍为 20。
- 游戏验证状态：未启动或操纵游戏。需要玩家进行至少 10–15 分钟远征并返回
  House；预期远征期间只有一次 `AC1203`、没有 `AC1204`，返回 House 稳定
  3 秒后面板恢复且不闪烁。
- MewUI 子模块本地 commit：`e2d1f4c perf: suspend MewUI button work during expeditions`。
- CombineDuplicateFurniture 本地 commit：`fe76afd Scan furniture only after the combine hotkey`。
- 主仓库本次 commit：由最终回复记录。
- 是否 push：否。

## 2026-08-27 v0.5.20 House 空窗保持修复

- 玩家确认 v0.5.19 的战斗卡顿已经消失，证明底层 MewUI 战斗休眠修复有效。
- 最新会话在 `23:54:09` 记录稳定 House 恢复、F10 隐藏面板节点和两个 House
  控件成功挂载。玩家随后预览并执行 32 次原生移动，四个 8 次批次在
  `23:55:02` 完成；`23:55:04` 没有出现 Battle、Map、保存或选择场景，只有
  无匹配 ready 场景被通用场景服务累计成 `UnsafeTransition`，导致 UI 被废弃。
- v0.5.20 在当前上下文已经是 House 时忽略纯 `Unknown` 场景空窗，不再把
  House 原生房间更新造成的短暂无 ready 状态当成真正离家。明确的保存场景、
  Battle/Map 远征、职业选择、歧义场景和替换后的 House 实例仍按原路径切换
  上下文并清理旧 UI。
- 修改文件：`src/ui/mew_ui_bridge.cpp`、`CMakeLists.txt`、
  `assets/description.json`、`CHANGELOG.md`、
  `docs/RELEASE_NOTES_v0.5.20.md` 和本报告。
- 验证结果：
  - 新增编译期行为断言：House+Unknown 保持，House+UnsafeTransition 切换，
    Unknown+Unknown 仍进入通用观察路径。
  - `build-v0517-debug` 完整 219 步构建成功，CTest 4/4 通过。
  - `build-v0517-release` 完整 219 步构建成功，CTest 4/4 通过。
- 部署结果：
  - Release DLL 已部署到 `Mewgenics/mods/AutoCattery.dll`，大小
    1,380,864 字节；Mewtator 描述显示 v0.5.20。
  - `CombineDuplicateFurniture` 继续为 v0.6.2；
    `AutoCatteryFurniture.dll` 仍不存在。
  - 玩家保护配置保留，升级重骰仍为 20。
- 游戏验证状态：未启动或操纵游戏。需要玩家回家后执行一次包含多个批次的
  自动整理，并观察整理结束至少 10 秒；预期不会在结束约 2 秒后发布空场景
  `UnsafeTransition`，House 控件应保持稳定。F10 仍可用于额外验证，但不是
  复现或确认本修复的必要条件。
- 本次本地 commit：由最终回复记录。
- 是否 push：否。

## 2026-08-27 v0.5.21 回家后空白控件闪烁修复

- 玩家实机确认 v0.5.20 回家后仍短暂出现管理面板控件但没有文字。最新会话加载
  的确为 v0.5.20；`17:34:06.972` 记录远征休眠解除，`17:34:07.007`
  发布稳定 `HouseReady`，`17:34:07.010` 才挂接隐藏面板。期间没有
  `AC18002`（F10 打开），也没有 v0.5.19 所修的二次 `UnsafeTransition`。
  因此这次不是场景上下文再次撤掉 UI，而是 SWF 在 C++ 挂接前自行显示控件。
- 根因是生成器给私有三帧面板 MovieClip 写入了 AVM1 `DoAction(stop)`，但当前
  House SWF 的 `FileAttributes=0x18`，使用 AVM2/ActionScript 3。当前 EXE 的
  DefineSprite 编译分派对 tag 12 (`DoAction`) 直接走跳过路径，而 tag 82
  (`DoABC`) 与 tag 76 (`SymbolClass`) 才生成并绑定 AS3 类。旧安装资源的面板
  背景、控件和推荐行没有任何 `SymbolClass` 绑定，所以它们的空 frame 0 不会
  停住，在回家后等待 House 连续稳定 3 秒期间可自行前进到有图形的 frame 1/2；
  文字是独立节点且尚未由 F10 渲染，因此玩家只看到无文字控件。
- v0.5.21 从固定的 MIT House 示例 SWF 复用已存在且有效的 AS3 首帧脚本模式：
  三个私有 MovieClip 类各自绑定到 `SymbolClass`，构造器在 zero-based frame 0
  注册唯一 `frame1` 回调，回调只调用 `stop()`。原来不会执行的 AVM1
  `DoAction` 已移除；frame 0/1/2 仍分别为隐藏、正常和按压状态，现有原生
  `goto-and-stop` 显隐路径不变。
- House SWF 现在由 CMake 在构建时从固定源生成，`tools/build.ps1` 只打包该次
  构建输出，避免源码生成器已修复但 `assets/swfs` 中旧二进制仍被部署。
- 修改文件：`tools/build_house_ui_asset.py`、`tools/swf_panel_shapes.py`、
  `tools/swf_frame_scripts.py`、`tests/house_ui_asset_tests.py`、
  `CMakeLists.txt`、`tools/build.ps1`、`assets/description.json`、
  `CHANGELOG.md` 和本报告。没有修改战斗休眠、House 3 秒稳定等待、F10 控制器、
  MoveOnly、评分、保护或存档写入逻辑。
- 构建与检查：
  - 旧安装 SWF 运行新回归检查会确定性失败：
    `sprite 153 has no AS3 stop binding (AVM1 is ignored)`。
  - 修复资源通过 `tests/house_ui_asset_tests.py`：确认 72 个面板图形节点、3 个
    唯一私有 AS3 首帧停止类、每个类的独立注册、frame 0 隐藏及 frame 1/2
    显示状态仍完整。
  - `build-v0517-release` 重新配置并完成 134/134 步 Release 构建；定向 CTest
    `house_ui_asset_tests` 与 `phase14_dll_load_smoke` 2/2 通过。
- 部署结果：游戏未运行时使用 `tools/deploy.ps1` 部署 Release DLL 和本次生成的
  SWF。安装描述与 DLL 内嵌版本均为 v0.5.21；安装 SWF 再次通过同一资源检查；
  `protection.json`、`user_config.json` 均保留，升级重骰继续为 20，
  `AutoCatteryFurniture.dll` 仍不存在。
- 游戏验证状态：未启动或操纵游戏。请玩家完全重启游戏，通过 Mewtator 进入
  House，进行一段战斗后回家，整个返回过程不要按 F10并观察至少 10 秒。预期
  House 稳定等待的 3 秒内不会再出现空白面板控件；F10 主动打开后文字和控件仍
  正常，关闭后不残留。自动化检查证明资源结构与安装内容，不代替实机渲染验收。
- 风险：该修复依赖当前固定 House 示例 SWF 中已有的 AS3 `stop()` 类模板；
  生成器会在模板结构变化时直接失败，不会静默生成未停止的面板。没有新增游戏
  地址、偏移或运行时原生调用。
- 省略的后续工作：未缩短 House 3 秒稳定等待，也未恢复进入 House 时对约 70 个
  子节点逐个调用原生时间轴函数；历史实机证据表明这两种方向分别会恢复过早
  UI 挂接和大存档 heap corruption 风险。
- 本次本地 commit：由最终回复记录。
- 是否 push：否。

## 2026-08-27 v0.5.22 跨日 House 控件重挂接修复

- 玩家实机确认 v0.5.21 已消除战斗后回家时管理面板的空白闪烁；同一轮测试中，
  两个普通 House 按钮首次进入时正常，但结束一天、House 实例刷新后恢复为素材
  默认文字 `Clean Up!`。最新日志显示 House 从 generation 2 切换到 generation 3
  后只有管理面板重新挂接，整理按钮和推荐按钮仍保留上一代节点，直到战斗回家
  进入 generation 8 才重新挂接。
- v0.5.22 让整理按钮和推荐按钮显式记录并比较 `scene_generation`。挂接目标代数
  变化时，控制器先废弃旧场景节点记录，再挂接当前 House 覆盖层；
  `MewUiBridge` 不再把“仍附着于旧 House”误判为当前代已挂接。
- 同时收紧管理面板挂接路径：先用短名称 `test_button` 定位 AutoCattery 自己的
  覆盖层根，再仅在该根内查找 `panel_background`，不再对多个 House 根执行长名称
  原生子节点查找。该调整针对本轮 `0xC0000374` 报告中确认的
  `MewUiManagementPanelView::Attach -> FindSceneUiNode -> FindChildByName` 调用链，
  但是否消除首次进入时的疑似闪退仍需玩家实机确认。
- 修改文件：`include/auto_cattery/ui/house_button_controller.hpp`、
  `include/auto_cattery/ui/recommendation_marker_controller.hpp`、
  `src/ui/house_button_controller.cpp`、`src/ui/recommendation_marker_controller.cpp`、
  `src/ui/mew_ui_bridge.cpp`、`src/ui/mew_ui_management_panel_view.cpp`、
  `tests/house_button_controller_tests.cpp`、
  `tests/recommendation_marker_controller_tests.cpp`、
  `tests/house_ui_asset_tests.py`、`CMakeLists.txt`、`assets/description.json`、
  `CHANGELOG.md`、`docs/RELEASE_NOTES_v0.5.22.md` 和本报告。
- 构建与测试：
  - 在 Visual Studio 2022 Community `VsDevCmd.bat` x64 环境中构建。
  - `build-v0517-debug` 构建成功，最终 CTest 5/5 通过。
  - `build-v0517-release` 构建成功。首次 CTest 中
    `phase14_unit_tests` 因测试的异步忙等在 Release 优化后过早结束而失败；将
    该测试改为最长 1 秒、每次 1 ms 的有界等待后，Release CTest 5/5 通过，
    Debug CTest 也重新验证为 5/5 通过。
  - 新增确定性回归覆盖 generation 1 到 generation 2 的按钮替换：旧节点执行
    一次 `AbandonScene()`，新节点完成第二次 `Attach()`；SWF 资产检查确认两个
    按钮和 `panel_background` 位于同一个 AutoCattery 覆盖层 Sprite。
- 部署结果：部署前确认 `Mewgenics.exe` 未运行；执行
  `tools/deploy.ps1 -GameRoot 'D:\steam\steam\steamapps\common\Mewgenics' -Configuration Release`
  和 `tools/verify_install.ps1` 均退出 0。Release DLL、Mewtator UI 数据和 v0.5.22
  描述已部署；现有保护配置未被改动，升级重骰继续同步为 20。
- 游戏验证状态：未启动或操纵游戏。玩家需完全重启后依次验证首次进入 House、
  结束一天后的两个按钮文字、战斗回家后的管理面板和按钮、F10 开关，并等待至少
  30 秒观察稳定性。自动化构建、测试与安装验证不能替代这些玩家可见结果。
- 风险：跨日按钮重挂接由确定性控制器测试覆盖，但 House 原生节点生命周期和首次
  进入时的疑似崩溃只能由当前游戏 build 的实机运行确认；本轮未修改 MoveOnly、
  评分、保护、存档写入或远征休眠逻辑。
- 省略的后续工作：未增加额外轮询、重试、看门狗或新的原生查找路径。
- 本次本地 commit：由最终回复记录。
- 是否 push：否。

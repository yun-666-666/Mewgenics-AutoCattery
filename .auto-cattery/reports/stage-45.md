# Stage 45 - 自动仓库属性替换与自动放置

日期：2026-08-14
版本：v0.5.56

## v0.5.56 固定逐房顺序与一次最终布局

- 检查了玩家要求的今天两次 v0.5.55 实机会话。凌晨 `02:23` 会话共点击自动放置
  `7` 次、提交 `147` 笔移动，但只涉及 `22` 个 stable key，并触发 `6` 次
  `AC3926`；刚才 `12:49` 会话点击 `8` 次、提交 `172` 笔移动，却只涉及 `12` 个
  stable key，并触发 `7` 次 `AC3926`。后者的规划目标只在 `Floor1_Large` 和
  `Attic` 间出现，最后连续多批仍只重排 Attic。两次会话都没有到达 `AC3922` 或
  `AC3929`，与玩家看到房间不推进、按钮停在“正在放置…”一致。
- 日志中的具体闭环并非偶发原生拒绝：例如刚才同一批 Attic 家具 key 58、153、166、
  226、238、370、375、413、433、446、453、462 在多套密封方案中反复交换坐标；
  `AC3904` 每批都成功，随后重新分析又生成另一套重排。根因是求解器仍优先返回单件
  局部改善/仓库填充，宽规划又允许临时 staging；UI 则用 32 笔检查点中断同一次点击。
- 房间选择改为固定可见顺序：`Attic -> Floor2_Large -> Floor1_Large ->
  Floor1_Small -> Floor2_Small`，对应阁楼、阁楼左下、左下角、右下角、阁楼右下。
  四房或缺房存档只跳过不存在的 ID，不读取或编码本存档家具数量、key 或属性值。
- 当前目标房的候选范围是仓库加所有尚未锁定房间；完成房间继续使用实时家具签名锁，
  因此阁楼完成后其家具不会再被第二房拿走，后续房间按相同规则逐层缩小候选池。
  属性替换也只允许发生在当前固定目标房，保留 v0.5.49 每次家具模式最多一次替换的
  原生对象生命周期门。
- 求解器不再把单件仓库填充或单件局部重排当作本房最终答案，而是直接比较宽布局候选。
  候选上限覆盖当前 188 件规模，先保留按房间用途排好的属性顺序，再以装入数量、占格
  和包围盒紧凑度决胜。用途已达动态目标时，严格提升用途综合排名的家具仍可进入候选；
  明显空房继续接受不降低用途排名的家具。
- 执行计划删除临时 staging 分支。任何已摆家具若不能从当前位置直接移动到最终坐标，
  该最终候选会在分析阶段被拒绝并尝试下一个候选；已接受计划内同一 stable key 最多
  出现一次，不再先堆到一处、再拆开、最终回到近似原布局。
- 一次自动放置在完整布局成功后立即把目标房写入完成锁并停止，等待玩家对下一个房间
  再次“开始分析 + 自动放置”。删除 32 笔 `AC3926` 检查点，但保留每笔 250 ms、
  UI tick 串行执行、原生拒绝时逆序回滚以及 state-edge 容量上限。清理预览后再显式把
  按钮设为“放置已完成”，避免退出路径遗留“正在放置…”。
- 自动化回归新增/更新：固定五房排序；阁楼可从普通房取家具；阁楼锁定后其 key 不再
  出现在下一房计划；宽布局含仓库家具；同一计划 stable key 唯一；属性替换绑定当前
  目标房；用途已满足时仍选择严格提升综合用途属性的仓库家具。
- Release 增量编译已生成 `build\out\Release\AutoCattery.dll`，大小 `1924608`
  字节，生成时间 `2026-08-14 13:35:52`。原全构建外层进程在编译后异常退出，遗留的
  10 个 `MSBuild /nodemode:1 /nodeReuse:true` 进程均已确认父进程消失且不再执行；
  没有重复启动整套构建，而是从未完成边界执行
  `cmake --build build --config Release --target RUN_TESTS`。4 项 CTest 全部通过，
  `0` 项失败，退出码 `0`，`LastTest.log` 更新时间为 `2026-08-14 13:52:51`。
- 已补齐 `dist\Release` 数据文件，验证 DLL 包含 `AutoCattery_Initialize`、
  `AutoCattery_Shutdown` 导出且为 x64；随后执行
  `tools\deploy.ps1 -GameRoot D:\steam\steam\steamapps\common\Mewgenics
  -Configuration Release`，非递归 Mewjector DLL 与 Mewtator AutoCattery data MOD
  部署成功，已安装 `description.json` 显示 `0.5.56`。按玩家要求未运行哈希或
  `verify_install.ps1`，未启动、进入或控制游戏。
- 本轮修改文件：`CMakeLists.txt`、`CODEX_TASK.md`、`assets/description.json`、
  `include/auto_cattery/furniture_planning/layout_solver.hpp`、
  `src/furniture_analysis/service.cpp`、`src/furniture_planning/layout_solver.cpp`、
  `src/ui/mew_ui_bridge.cpp`、`tests/furniture_analysis_service_tests.cpp`、
  `tests/furniture_layout_solver_tests.cpp`、本报告及 `.auto-cattery/state.json`。
  本地提交：见包含本报告的 Stage 45 任务提交；是否 push：否。
- 玩家复测仍是完成门：五房应按固定顺序五轮完成，四房应四轮完成；每轮只锁定一个
  房间，后续房间不得取走已锁房家具，按钮必须回到“放置已完成”，并确认同一批
  stable key 不再在房间内反复搬动。保存、完全退出并重进后的最终布局仍待玩家验证；
  构建与 CTest 不替代这些游戏内结果。

## v0.5.55 完整密封计划、状态域 tabu 与空间陈设底线

- 最新 v0.5.54 实机会话共执行 `142` 笔家具移动，但只涉及 `17` 件家具：
  `small_food_broccoli key=34` 移动 `41` 次、经过 `14` 个提交坐标；
  `small_waterbottle key=57` 移动 `34` 次、经过 `12` 个坐标；
  `small_deadrat key=58` 移动 `20` 次、经过 `11` 个坐标后最终回到 Attic 原位
  `(-8,-11)`。仓库计数只从 `166` 降到 `165`，最终仍有 `165/190` 件留在仓库，
  与玩家截图中五房大面积空置、少量家具挤在边角的结果一致。这证明旧版不是正常逐房
  推进，而是少数家具反复占用执行机会。
- 日志直接显示规划器多次生成 `11`、`14`、`15`、`16`、`17`、`18` 步的安全方案，
  但 UI 在没有属性替换时把 `layout_plan.moves` 强行缩成第一步；第一步可能只是为
  Support 链腾位置的临时 staging。旧流程执行这一临时步后立即从零分析，于是下一轮
  又把小物件搬回去，玩家也被迫重复点击“分析+放置”。v0.5.55 删除该截断，一次自动
  放置会按 250 ms 间隔连续执行完整密封方案；后续任一步被原生拒绝时，已提交的本套
  布局移动按逆序全部回滚，不留下半套 staging 布局。
- 一次点击的 `32` 笔事务检查点现在只发生在密封方案边界。如果下一套完整方案会跨越
  检查点，则在开始它之前记录 `AC3926` 并暂停；不会执行一半再要求玩家点击。当前计数
  为零时允许一套本身超过 32 步的完整方案原子执行。方案结束后也会等待 250 ms，再
  刷新运行时快照并自动分析，而不是在 `AC3903` 同一毫秒立即重算。因此正常情况下
  一次点击可完成多笔连续移动并自动进入后续房间，玩家操作次数应显著减少。
- v0.5.54 的循环 tabu 虽由整屋 binding 触发，却只保存裸 `FurnitureLayoutMove`，
  造成同一 move 在以后完全不同的整屋状态也被永久过滤。最终日志出现
  `tabu_filtered_moves=1282`、`tabu_moves=55`，搜索空间被旧状态污染。v0.5.55 将 tabu
  保存为 `(binding_digest, first_move)`；分析器只向当前完全匹配的 binding 提供禁用
  move，不同布局不继承旧禁令。回归测试分别证明不同 binding 的相同 move 不受影响，
  匹配 binding 的重复 move 才会被过滤并选择替代动作或安全返回无动作。
- 用途属性达到动态目标后，旧版完全停止仓库填充，因此本次最终只摆放约 25 件家具。
  v0.5.55 增加通用空间陈设底线：按每个房间实时可用格面积计算，当已放家具的
  Hitbox/Solid/PoopLogic 唯一占格低于 `15%` 时，房间可继续接收用途排名相同、且绝不
  降低用途排名的仓库家具；达到底线后仍沿用既有用途停止规则。该比例不编码当前五房、
  190 件仓库或任何具体家具 key，也不会退回到塞满每个格子。新增 `10x5` 定义房间
  （运行时 `12x7` 网格）的回归证明：繁育属性已满足但空间过空时，中性家具仍会产生
  warehouse move；既有较小且已达到覆盖的用途房仍不继续添加。
- 最终 `AC3901 rooms=0 moves=0` 同时报告 `evacuation_blocked=1`、
  `installation_blocked=1`、`deferred=184`，旧版却记录 `AC3922 safe fixpoint`。
  v0.5.55 将这种状态明确归类为失败/暂停，记录 `AC3929` 及 current、evacuation、
  installation、deferred 和 state-tabu 计数；只有确实没有 blocker 时才允许 `AC3922`。
- Release 验证先构建并运行 `build\Release\auto_cattery_tests.exe`。首次运行只因新增
  稀疏房测试把定义尺寸 `10x5` 误写为运行时网格 `10x5` 而失败；按现有坐标契约修为
  `12x7` 后，增量 Release 构建与统一测试程序退出码均为 `0`。随后
  `cmake --build build --config Release --target AutoCattery --parallel` 成功生成
  `build\out\Release\AutoCattery.dll`，大小 `1955328` 字节，最终生成时间
  `2026-08-14 02:20:48`；构建和测试只证明离线行为，实机画面仍以玩家复测为门。
- 已同步 DLL 与 `description.json` 至 `dist\Release`，并执行
  `tools\deploy.ps1 -GameRoot D:\steam\steam\steamapps\common\Mewgenics
  -Configuration Release`。非递归 Mewjector DLL 与 Mewtator data MOD 部署成功，
  Mewtator data MOD 版本为 `0.5.55`，升级重投值继续为 `20`。未运行哈希或
  `verify_install.ps1`，未启动、进入或控制游戏。
- 本轮修改文件：`CMakeLists.txt`、`CODEX_TASK.md`、`assets/description.json`、
  `include/auto_cattery/furniture_analysis/service.hpp`、
  `include/auto_cattery/ui/mew_ui_bridge.hpp`、`src/furniture_analysis/service.cpp`、
  `src/furniture_planning/layout_solver.cpp`、`src/ui/mew_ui_bridge.cpp`、
  `tests/furniture_analysis_service_tests.cpp`、`tests/furniture_layout_solver_tests.cpp`、
  本报告及 `.auto-cattery/state.json`。本地提交：见包含本报告的 Stage 45 任务提交；
  是否 push：否。
- 玩家验证待完成：一次点击应连续完成日志中同一套多步方案，不再每搬一件重新点击；
  key 34、57、58 等小物件不应在十几个位置间往返；用途已达标但明显空旷的房间应继续
  接收不降低用途的家具；达到检查点只能在完整方案边界暂停；真实阻塞必须出现
  `AC3929`，不得再误报 `AC3922`。首次选择存档偶发闪退仍缺少可信符号化栈，本轮未对
  House attach 生命周期作猜测性修改，需玩家同时报告 v0.5.55 首次进入结果。

## v0.5.54 通用整屋布局循环防护

- 玩家最新截图与 MOD 日志确认，`small_deadrat key=58` 在 Attic 内形成
  `(-8,-11) -> (-7,-11) -> (-1,-10) -> (-8,-11)` 三步闭环，连续占用每次
  自动执行的第一笔机会，使其余 `deferred=185` 家具没有继续处理。根因是 v0.5.52
  只比较相邻两次动作是否为严格 `A -> B -> A`，无法识别三步或更长循环；本修复不
  编码 key 58、三个坐标、当前房间或本存档家具数量。
- 连续自动放置现在为每次准备执行的第一条布局移动记录
  `(当前整屋 binding_digest, first FurnitureLayoutMove)`。任何单件或多件家具经过
  任意 2/3/4/5/N 步后，只要整屋布局回到已见状态且规划器准备再次走同一条出边，
  `AC3924` 就会拒绝该重复状态边，仅把这一条 exact move 加入现有 tabu，并保持当前
  focus room 重新分析。求解器因此可选择其他家具或坐标，不会把整房误标为完成。
- 状态边历史按 House scene generation 保存，初始化、关闭 MOD 或进入新 House scene
  时清空；达到每次点击 32 笔事务的 checkpoint 时不会清空，因此下一次点击也不会
  重入已经发现的闭环。历史上限为 4096 条；极端情况下达到上限会以 `AC3928` 安全
  暂停，而不是静默清空后再次循环。
- 聚焦单元测试覆盖首次登记、相同 binding 与相同 move 重复、相同 binding 的不同
  move、不同 binding 的相同 move、五状态循环回到 A、不同 stable key 家具参与，及
  有界容量。既有 solver tabu 回归继续证明禁用重复 move 后可选择替代动作，且不会把
  focus room 写入 exhausted。
- 原始 Release 构建链完成 CMake configure、`auto_cattery_tests` 编译与测试程序执行，
  随后生成 `AutoCattery.dll`，最终 `__AUTOCATTERY_CHAIN_EXIT=0`。产物为
  `build\\Release\\auto_cattery_tests.exe` 与
  `build\\out\\Release\\AutoCattery.dll`；DLL 大小 `1953280` 字节，生成时间
  `2026-08-14 01:22:48`。未将编译或测试视为游戏内完成证明。
- 已同步 Release DLL 与 `description.json` 至 `dist\\Release`，并执行
  `tools\\deploy.ps1 -GameRoot D:\\steam\\steam\\steamapps\\common\\Mewgenics
  -Configuration Release` 完成 DLL/data-only 部署。安装端版本为 `0.5.54`，保留玩家
  现有升级重投 `20`；未启动、进入或控制游戏，也未运行 hash 或
  `verify_install.ps1`。
- 第一次选择存档后的 `ntdll.dll` 访问冲突仍没有可符号化调用栈，且第二次同流程能够
  正常进入；现有证据只能把边界定位在 `HouseReady` 后、首个面板/运行时刷新日志前，
  不能可靠归因于本次已证实的家具规划循环。本版不对 House attach 生命周期作猜测性
  修改，首次进入是否仍闪退需玩家用 v0.5.54 复测并返回新日志/转储边界。

## v0.5.53 实时布局绑定完成锁与用途满足停止

- 玩家截图与最新 MOD 日志共同确认两个独立问题。v0.5.50 在
  `2026-08-13 15:43:54` 的一次点击中以 `AC3922` 累计执行
  `upgrades=1, layout_moves=151`，与五个房间被大量低收益小家具、墙饰和杂物塞满的
  画面一致。玩家随后手动搬空五房后，v0.5.52 在 `20:59` 明确记录
  `AC14315 furniture=0, scene pieces=0`、`AC3201 furniture=190`，但 `AC3901` 仍为
  `rooms=0, moves=0, persistent_locked_rooms=5, no_space=190`。因此“分析后无法放置”的
  直接根因不是 190 件仓库家具没有合法位置，而是上一轮五个完成房间锁在玩家手动
  清空后仍然存活，求解器跳过全部房间并把仓库家具错误计入 `no_space`。
- 完成房间锁现在绑定该房间的实时家具签名。签名确定性记录每件家具的 stable key、
  item id、坐标和横纵方向；每次成功刷新实时家具上下文后重新比对。玩家手动添加、
  移除、搬动、翻转或清空家具时，只解除发生变化的房间锁并清理该房 focus，其他未变
  房间仍可在同一 House scene 内跨家具界面关闭/重开保持完成状态。新 House scene、
  初始化和关闭 MOD 时同时清空锁 ID 与签名。锁因实时布局变化失效时记录独立日志
  `AC3927`；既有 `AC3926` 仍只表示一次点击达到事务上限，避免诊断编号混淆。
- 用途房的仓库填充新增按预计入住人数缩放的停止目标，不编码本存档的 13 只猫、
  190 件家具或五房数据：Breeding 的有效 Comfort 与 Stimulation 各达到
  `2 × 预计入住数`；Kitten/Recovery 的 Health 与有效 Comfort 各达到
  `2 × 预计入住数`；MutationLab 保持 Health 非负、有效 Comfort 高于 `-10`，并将
  Mutation 提升到 `2 × 预计入住数`；CombatStaging 保持 Health 非负，并将有效
  Comfort 降到 `-2 × 预计入住数`。General/Unknown 保留既有通用策略。
- 用途目标达到后，不再向该房加入仓库或跨房 incoming 家具，避免仅因 `Comfort +1`、
  `Appeal +1` 等边际字典序改善继续填满房间；目标房现有家具的合法压紧/重排仍可
  执行，独立的低质量家具属性替换也未被关闭。空房和未达标房间仍会从仓库产生布局
  移动，因此本修复不会把“防过度填充”退化成“空房不放家具”。
- 自动化回归覆盖：实时布局未变化时两房锁保持；只移动 RoomA 时只解除 RoomA；清空
  RoomB 后解除 RoomB；一个房间变化不会清掉其他房锁；已满足动态目标的 Breeding
  房不再生成 warehouse move；空 Breeding 房仍生成一个 warehouse move。为保留旧
  “未满足目标时选择用途更优家具”的测试语义，其独立场景预计入住数由 2 调整为 4，
  使当前 `Comfort=5` 确实低于该场景的动态目标 8。
- 最终聚焦验证命令
  `cmake --build build --config Release --target auto_cattery_tests AutoCattery --parallel`
  成功生成 `build\Release\auto_cattery_tests.exe` 与
  `build\out\Release\AutoCattery.dll`；测试程序随后运行退出码 `0`。最终 DLL 大小
  `1950208` 字节，生成时间 `2026-08-13 22:16:06`。本轮只运行与当前修改相称的
  Release 目标和统一测试程序，没有将编译/单元测试当成游戏内效果证明。
- 已将 v0.5.53 DLL 与 `assets\description.json` 同步到 `dist\Release`，并执行
  `tools\deploy.ps1 -GameRoot D:\steam\steam\steamapps\common\Mewgenics
  -Configuration Release` 完成 DLL/data-only 部署。安装端 `description.json` 为
  `0.5.53`，玩家现有升级重投 `20` 保持不变，AutoCattery 仍位于 Mewtator
  `modlist.txt` 末尾。按当前边界未运行哈希比对或 `verify_install.ps1`，未启动、进入
  或控制游戏。
- 本轮实际修改：`CMakeLists.txt`、`CODEX_TASK.md`、`assets/description.json`、
  `include/auto_cattery/ui/mew_ui_bridge.hpp`、
  `src/furniture_planning/layout_solver.cpp`、`src/ui/mew_ui_bridge.cpp`、
  `src/ui/runtime_house_state.hpp`、`src/ui/runtime_snapshot_overlay.cpp`、
  `tests/furniture_layout_solver_tests.cpp`、`tests/runtime_house_state_tests.cpp`，以及本报告
  和 `.auto-cattery/state.json`。
- 玩家实机验证仍待完成：完全重启游戏并保持当前五房搬空状态后，第一次分析不应再
  出现 `persistent_locked_rooms=5, rooms=0, moves=0, no_space=190`，而应显示非零布局
  移动并允许自动放置；用途房达到目标后应停止继续塞入低收益家具。完成一个房间后
  手动移动或清空其中家具，再分析应出现 `AC3927`，且只有该房重新参与规划。仍需玩家
  确认无悬空、消失、黑件、重复 stable key、卡死、闪退或原生非零 SEH。

## v0.5.52 用途焦点与精确反向移动禁忌

- 评估玩家提供的 `AutoCattery_v0551_家具放置深度研究与优化报告_2026-08-13
  (1).md` 后，源码复核确认三项 P0 根因成立：有用途的 Attic 仍被旧平衡阁楼策略
  覆盖；房间排序只看面积；`AC3924` 把一项立即反向移动升级为整房完成锁。
- `layout_solver.cpp` 现在只在用途不可用时启用 legacy balanced-Attic；有用途的
  Attic 与其他房间统一按 `PurposeRank` 选择仓库件、跨房候选和最终 pack。房间顺序
  改为当前焦点优先，其后为 Breeding、Kitten/Recovery、CombatStaging、
  MutationLab、General，再以面积/Attic/room id 稳定排序。
- 连续执行提交布局移动后保留该目标房为 focus。若刷新规划产生严格反向，UI 不再把
  房间写入 `furniture_locked_room_ids_`；只把该 exact move 加入当前 House scene 的
  tabu 集。求解器在仓库 best-fit、局部压紧和 broad pack 三条路径过滤该 move，并
  继续尝试同房的其他家具或坐标；只有真正 exhausted 才进入完成锁。
- `AC3901` 新增 `tabu_filtered_moves`、`focus_room`、
  `persistent_locked_rooms`；`AC3924` 记录 exact tabu 数；`AC3922` 同时报告本次点击
  blocked 数、持久锁、tabu 与 focus，避免再次把 `blocked_rooms=0` 误读为没有跳房。
- 本轮不采纳报告中的二步 look-ahead、neutral fill、搜索 disposition 全面重构和
  繁育公式阈值校准；这些属于 P1/P2，缺少本轮玩家实机安全门，不与 P0 状态机修复
  混在同一版。原生拒绝后的整房暂缓仍保留，因为该分支受 native rollback/SEH 证据
  约束，不能等同于纯规划振荡。
- 自动化回归新增：有 purpose 的 Attic 必须走 Breeding comparator；Breeding 房优先
  于面积更大的 General Attic；focus 可保持较低默认优先级房；exact tabu 会过滤原
  move 并在同房选择替代坐标，且不会把房间写为 exhausted。
- `build-stage45-release\Release\auto_cattery_tests.exe` 于 2026-08-13 20:47
  聚焦运行退出码 `0`；随后 `AutoCattery` Release 目标成功生成 v0.5.52 DLL。
  原聚焦构建超过 120 秒时保留原始 MSBuild 继续运行，并由当前任务的 8 分钟监控确认
  进程结束后复用既有产物，没有启动重复构建；监控随后已停用。
- 已将 v0.5.52 DLL 与 `description.json` 同步到 `dist\Release`，并运行
  `tools\deploy.ps1 -GameRoot D:\steam\steam\steamapps\common\Mewgenics
  -Configuration Release` 完成 DLL/data 部署。玩家现有升级重投 `20` 保持不变，
  AutoCattery 仍位于 Mewtator modlist 末尾。按玩家既有要求未运行哈希比对和
  `verify_install.ps1`，未启动或控制游戏。
- 玩家实机验证待完成：搬空/近空阁楼不应再只放一件后跳房；若日志出现
  `AC3924 ... tabu_moves=`，下一次 `AC3901` 应保持相同 `focus_room` 并提出不同家具
  或不同坐标，而不是增加 `persistent_locked_rooms`。仍需确认连续放置无悬空、黑件、
  stable-key 重复、退出家具模式闪退或原生 SEH。

## v0.5.51 用途改善筛选与原生事务限流

- 修复用途房仍把“只要能放下”的家具持续塞入房间的问题。仓库填空和未锁定来源房
  候选现在都必须严格提高目标房当前用途排名；无属性、属性不变或用途排名不升的家具
  保留在仓库/原房间，不再为了清空仓库牺牲房间效果和画面。
- 用途感知整房候选先比较最终用途属性，再比较几何填充。战斗房因此只会接收能在健康
  非负硬门下继续降低有效舒适度的候选；新增回归确认 `Comfort -5` 家具优先于无属性
  家具，并确认只有无属性仓库家具时不会生成仓库安装。
- 连续自动放置在每笔已提交事务后等待 250 ms，一次点击累计 32 笔后记录 `AC3926`
  并暂停，玩家重新分析/点击后继续。该上限降低一次点击内大量创建、删除、全屋刷新
  对原生对象生命周期和内存的瞬时压力，不改变单笔失败即停规则。
- 从仓库新建并提交家具后，本次家具模式会话不再允许属性替换，防止新对象立即进入
  create/delete 替换链；普通房内已有家具的压紧移动不触发该门，仍可继续执行。
- `build-stage45-release\\Release\\auto_cattery_tests.exe` 于 2026-08-13 聚焦运行退出码
  `0`；`AutoCattery` Release 目标随后成功生成 v0.5.51 DLL。
- `tools\\deploy.ps1 -GameRoot D:\\steam\\steam\\steamapps\\common\\Mewgenics
  -Configuration Release` 已部署 v0.5.51 DLL 与 UI data mod，玩家现有 20 次升级重投
  设置保持不变，`AutoCattery` 仍位于 Mewtator modlist 末尾。按玩家明确要求未做任何
  SHA-256/文件哈希比对，也未运行 `verify_install.ps1`；未启动或控制游戏。
- 玩家实测待完成：搬空或近空房间后重新分析/自动放置，战斗房舒适度应继续向可达到
  的低值下降，其他用途房不再被无收益杂物塞满；一次点击最多执行 32 笔并暂停，家具
  不应变黑或在退出家具模式时闪退，内存峰值应显著低于此前单击 151 笔事务的运行。

## v0.5.50 按用途分化房间属性与减少布局空隙

- 新增 Breeding、CombatStaging、Kitten、MutationLab、Recovery 五房用途输出，并在
  当前 House generation 内把只读用途结果传给家具属性分析与布局器。用途不可用时
  保守退回原 General 逻辑。
- 战斗房以健康度非负为硬门，随后优先降低有效舒适度；变异房优先变异度，同时要求
  健康度非负、有效舒适度高于 -10；繁育房平衡有效舒适度与刺激度，稳定后再比较
  变异度；育幼与恢复房优先健康度、舒适度。Appeal 不作为普通房用途目标。
- 仓库直接填空从遇到首个合法落点即返回改为完整仓库 best-fit；阻挡面积按 Hitbox、
  Solid、PoopLogic 计算，不再把 Support/Surface 元数据误算为占格，并加入自由空间
  连通块、孤立格、接触边与稳定 tie-break 评分以减少碎片化空隙。
- `tools/build.ps1 -Configuration Release` 完整通过并生成 v0.5.50
  `dist\Release`；Release CTest 4/4 通过。新增用途回归覆盖战斗房负舒适度方向、变异
  房安全高变异方向和第五普通房 capability；原有统一测试全部通过。
- `tools/deploy.ps1 -GameRoot D:\steam\steam\steamapps\common\Mewgenics
  -Configuration Release` 已部署 DLL 与 UI data MOD，并保留玩家 20 次升级重投配置。
  按玩家要求未做 DLL 哈希一致性校验、未运行 `verify_install`，也未启动或控制游戏。
- 玩家实测待完成：五房应分别呈现繁育、战斗、育幼、变异、恢复方向；战斗房舒适度
  应下降且健康度不为负，变异房变异度应上升且健康/舒适底线成立；自动放置后退出
  家具界面仍不得闪退。

## v0.5.49 家具模式属性替换生命周期门

- 玩家 v0.5.48 实机运行从 13:25:32 连续执行到 13:25:43，最终日志为
  `AC3922 upgrades=17, layout_moves=26, quarantined_keys=17`；13:26:44 玩家退出
  家具界面时主 UI 线程抛出 `0xE06D7363`，Windows 随后记录致命 APPCRASH。
- PID 18484 的完整 dump 解析确认该 C++ 异常 ThrowInfo 为 `std::bad_alloc`；同一
  进程另有 `ntdll+0x33ACA`、`0xC0000005` 小 dump。日志最后形成明确的原生对象
  生命周期链：仓库 key 453 放入房间后立即被 key 348 替换，key 348 又立即被
  key 463 替换。每次成功替换都会把旧 `FurniturePiece` 放入游戏延迟删除队列。
- v0.5.49 每次打开家具界面最多允许提交一次属性替换。成功后，本次家具模式会话的
  后续分析跳过属性升级候选，但仍继续普通布局移动和仓库填空；关闭并重新打开家具
  界面后才重新允许一次替换。该门不依赖家具名称或 stable key，也不改变评分、几何、
  Support、碰撞和房间锁规则。
- Release 聚焦构建于 13:54:59 生成统一测试程序、13:55:03 生成 DLL；统一测试串行
  退出码 0，Release CTest 4/4 通过。曾误把统一测试和包含同一测试的 CTest 并行运行，
  两进程竞争相同配置夹具导致一次无效的断言失败/崩溃；串行复测已排除代码回归。
- 聚焦构建只刷新 `build-stage45-release`，第一次标准部署仍从旧 `dist\Release` 取出
  v0.5.48 DLL。确认哈希不匹配后，按标准 build 脚本的复制清单同步新产物和 0.5.49
  资产到 `dist\Release`，重新部署并验证。build、dist、安装 DLL SHA-256 均为
  `E5BE462FC3320148BCDF8C9AB9E74A35442E07E19BE2FCA307CE8B39D5BF7E39`；data MOD
  版本为 0.5.49，DLL x64 与 14 个职业 20 次重投安装校验通过。
- 玩家实机验证待完成：一次属性替换后应继续布局/填空，退出家具界面不得闪退；再次
  打开家具界面才允许下一次属性替换。多轮后保存、完全退出并重进确认持久化。

## v0.5.48 连续布局振荡阻断

- 玩家截图与 2026-08-13 02:33:36–02:33:52 日志一致：
  `wallmounted_cloud key=90` 在 `Floor1_Small` 的 `(-10,-8)` 与 `(-10,-9)`
  之间反复移动。每次 `AC3903` 都是成功提交，排除原生拒绝或视觉动画误判。
- 相邻分析结果在 `moves=29` 与 `moves=1` 间交替。v0.5.47 每轮只执行刷新后计划的
  第一项，导致整房方案的第一步与下一轮局部压紧形成严格二态反转。
- 连续状态机现在保留上一笔真实提交的普通布局移动；如果新计划第一项对同一 item、
  stable key、房间和坐标执行完全反向移动，则记录 `AC3924`，将该目标房加入当前
  House scene 的暂缓房间并继续重新规划其他房间。修复不硬编码云朵名称。
- 属性替换、仓库新放置、玩家新一轮手动启动、AlreadyPlaced 或任何非反向移动都会
  清空该记录，避免跨事务或跨手动运行误判。
- Release DLL 与统一测试已通过；v0.5.48 DLL/UI data MOD 已部署。安装校验确认 DLL
  为 x64、data MOD 版本为 0.5.48、14 个职业继续使用玩家设置的 20 次升级重投；构建
  与安装 DLL 的 SHA-256 均为
  `AF352327036146AD9B6F52F7A77F5F3E51162A288491ACFBD13FA6B1766FB61C`。
- 玩家实机复测待完成：同一存档再次自动放置时，云朵不得继续上下往返；若规划再次
  产生直接反向步骤，日志应出现一次 `AC3924`，随后继续其他房间或结束安全 fixpoint。

## v0.5.46 当前场景 stable-key quarantine 与连续执行

- 新增当前 House scene generation 级别的 stable-key quarantine。属性替换成功后，
  被退回仓库的旧 key 立即加入；仓库创建失败后安全回收的新 key 也加入。分析器会从
  属性升级候选和仓库布局来源中排除这些 key，`FurniturePlacementGateway` 在调用
  原生 create 前再次拒绝，形成双重门。
- 当前阶段不把 delete-queued、普通枚举消失或固定等待时间误当 final deletion ACK；
  quarantine 只在进入新的 House scene generation 时清空。
- “自动放置”现在是一次点击持续运行：每个密封原生事务完成后刷新运行时快照、自动
  重新分析并继续其他未隔离的安全候选；零移动但可锁定的房间会自动进入下一房间，
  直到当前场景没有安全动作。
- 删除跨已提交属性替换的全局逆向重建。后续失败只回滚当前布局移动；先前成功的属性
  事务保持提交，避免用正在延迟删除的旧 key 做危险回滚。原生 SEH 会封锁当前场景的
  后续写入，要求退出并重新进入 House。
- 聚焦 Release `auto_cattery_tests.exe` 通过；Release `AutoCattery.dll` 编译成功并
  部署到当前游戏安装。按玩家要求未运行哈希、安装验证或额外打包。玩家实机验证待
  完成。

## v0.5.47 连续执行提速与可恢复原生拒绝

- 玩家 v0.5.46 日志显示 2026-08-13 02:04:08–02:04:30 连续完成 23 项属性替换和
  多项布局移动，未再复用已 quarantine key、未出现 SEH；生命周期修复生效。
- 同一日志也证明每项执行前重复两次 `AC14319/AC3201`。自动 re-analyze 的 preview
  现在标记为 fresh，续跑直接执行，不再同步重复分析；首次玩家启动仍重新绑定校验。
- 最后停止为 `set_wooden_toilet key=160` 从 `Floor1_Small` 到
  `Floor1_Large` 的跨房移动被原生合法性检查拒绝，`seh=0` 且 rollback `1/1`，不是
  崩溃或 quarantine 失败。此类安全恢复拒绝会暂缓该目标房并继续后续房间。
- 连续布局每次只执行刷新后计划的第一项，避免 26 步旧计划在场景变化后继续使用。
- Release 增量 DLL 编译与统一聚焦测试通过；已部署 v0.5.47 DLL/UI data MOD。按玩家
  要求未运行哈希、安装验证、打包或额外全量测试。玩家实机复测待完成。

## 结果

- “开始分析”产生属性升级候选后，“自动放置”不再只执行布局移动；只存在属性升级
  时按钮也可用。
- 分析器先建立虚拟替换后的家具集合，再交给布局求解器，保证布局步骤绑定仓库家具
  的新 stable key，而不是已经回仓库的旧家具。
- 执行器每个 UI tick 先处理一项仓库属性替换，全部完成后再处理现有布局移动。
- 当前 build 原生适配器按仓库 stable key 创建 `FurniturePiece`，核对 item/key/空
  grid，设置目标 transform，经原生校验和提交读回后排队删除旧 scene piece。
- 单项失败会删除新对象并恢复旧家具；批次后续失败会先逆序恢复布局移动，再逆序
  交换已完成的属性升级，并通过 `AC3907` 报告回滚结果。
- v0.5.34 玩家实测已通过分析门，但第一项
  `special_fightidol key=207 -> set_bone_tv key=97` 因沿用旧家具坐标而被原生放置拒绝，
  安全回滚后显示完成 0、剩余 49。v0.5.35 保留全部跨几何属性候选；分析阶段使用
  替换件的 24x24 放置网格、房间基础格、当前家具占用和既有 Support 依赖，逐项求出
  距离旧位置最近的目标坐标。新建 scene piece 继承旧家具朝向，后续封存布局允许从
  实际创建坐标继续执行；批次回滚仍单独保留并恢复旧家具原始坐标。
- v0.5.35 玩家复测 `special_foodbox key=13 -> set_90s_stove key=99` 时，目标
  `(-10,-11)` 仍被原生合法性校验拒绝。当前存档网格确认 key 13 上方直接或间接
  支撑 key 68、197、236、221、321；问题不是缺少房间临时空位，而是旧底座的依赖链
  仍留在 scene 中。v0.5.36 对每次属性替换先计算完整 Support 依赖链，按最上层到
  最下层逐件调用游戏原生家具栏收回路径，再收回旧底座；随后从仓库 stable key 创建
  新底座，并按最下层到最上层将依赖件原位创建。执行过程不寻找房间地面或跨房间
  临时落点；失败时先清理已重建链和新底座，再恢复旧底座及已收回依赖链。
- v0.5.36 玩家实测两次“开始分析”均以 `AC3205 attribute upgrade support chain
  could not be resolved` 结束，因此没有 `AC3901`，自动放置按界面状态保持禁用。当前
  存档直接复现定位到：key 13 的初始依赖链可以正确解析，但最高收益配对
  `key=13 -> set_90s_stove key=99` 没有完整合法几何落点；旧实现仍把旧坐标写入虚拟
  集合，下一项 key 68 才报依赖解析失败。v0.5.37 在候选贪心选择时同时验证当前虚拟
  Support 链和完整合法目标；不可执行配对不占用 placed/warehouse，继续选择次优的
  可执行配对。
- v0.5.37 玩家实测分析成功并生成 26 项属性替换；第一项无支撑替换
  `key=207 -> 97` 成功。第二项 `key=13 -> 160` 已收回 5/5 支撑件并提交新底座，但
  第一件支撑家具同 tick 重建时在 `Mewgenics.exe+0x5959A` 触发
  `0xC0000005`，日志为 `support=5/5->0`，回滚随后失败并使 House UI 卡死。异常 RVA
  是游戏字符串比较读取空对象；结合完整收回路径会排队延迟删除 component，根因是旧
  支撑 component 尚未清理时又以相同 stable key 创建新 component。
- v0.5.38 支撑链和旧底座不再提前走家具栏删除/重建：最上层到最下层只调用原生
  grid remove，保留原 component、entry、stable key 和坐标；新底座提交后，最下层到
  最上层对原支撑 component 恢复 transform、执行原生合法性校验并提交，全部成功后
  才把仍处于 detached 状态的旧底座正式收入家具栏。失败回滚先解除已重新提交的支撑
  件、移除新底座，再用原 component 恢复旧底座和支撑链，从根源上避免同 tick 同
  stable key 的延迟删除冲突。
- v0.5.38 玩家随后确认 26 项属性替换全部完成，但当前存档仍有 131 件家具在仓库，
  五个房间存在明显空位；两次分析均输出 `rooms=0, moves=0,
  evacuation_blocked=1`，界面误报“无需安全移动”。v0.5.39 修复两层问题：布局器不再
  跳过空 room_id 的仓库记录；目标房现有合法布局作为 required seed 保持原位，只把
  有界确定性仓库候选填入剩余空间。执行器识别空来源房间，按 stable key 创建家具、
  原生校验并提交，成功记录 `AC3913`；单项失败立即收回，批次失败逆序回收已放置
  仓库件。布局受阻的 UI 也不再显示“无需移动”。

## 当前 build 证据

- `0x1ABFF0`：五个真实调用点确认参数为 House scene manager、scene context、
  stable key 指针；内部创建并初始化当前 `FurniturePiece`。
- `0x2EE3D0`：移除家具 grid 占用。
- `0x2EF0D0`：当前 build 的完整 scene 家具收回路径；内部调用 `0x2EE3D0`，将
  `FurniturePieceEntry` 的 room 置空，从 scene 注册集合解绑，清空 piece 的 entry
  指针并排队删除 component。该状态就是家具栏状态，不是模拟摆放。
- `0x2EDE60`：原生放置合法性校验。
- `0x2EE230`：提交家具到目标 grid。
- `0x94A910`：游戏广泛使用的 component 删除路径。完整反汇编确认它读取
  `component + 0x18` 的 context，并在 `context + 0x18` 写入删除状态；调用者不读取
  返回值。实现因此移除了错误的 `component + 0x0F` 成功判断，并让所有家具枚举、
  定位和 stable key 检查忽略已排队删除的对象。
- Stage 44 玩家两次真实仓库取放分别新增 stable key 474、467，证明创建的新 scene
  piece 保留仓库记录 stable key。
- v0.5.32 玩家首次分析日志显示 `furniture=0, scene pieces=0`，随后以
  `current placed furniture coverage is incomplete` 拒绝。根因是删除状态过滤错误读取
  `context+0x18` 的 16-bit 值；当前 build 删除函数只比较 `+0x18` 单字节。v0.5.33
  已改为单字节判断，并新增 `+0x18=0、+0x19!=0` 仍为存活对象的回归测试。
- v0.5.33 玩家复测已恢复 `furniture=112, scene pieces=112`，但随后仍以
  `runtime room identity is incomplete` 拒绝分析。只读快照核对为 13 猫、12 已分配、
  `rooms=1`、`AdventureBox=0`；这不是玩家专用家具数据问题，而是家具分析路径没有
  像普通预览路径一样按已解锁数量补齐普通房间，却把探针检测到的无人使用
  `AdventureBox` 辅助组件加入了待映射快照。同时运行时探针遗漏第五普通房间
  `Floor2_Small`，导致快照 ID 与候选指针集合无法一一对应。
- v0.5.34 在保留既有房间索引的前提下补入 `Floor2_Small`；家具分析按通用可用房间
  顺序补齐 5 个普通房间，只把运行时检测 ID 保留作分析证据，不再把无人使用的辅助
  组件提升为普通房间。新增通用合成回归覆盖 5 个快照房间、7 个原生组件、2 个无关
  组件且第五房没有猫投票；未知猫房间或映射歧义时仍安全拒绝。
- v0.5.34 首次最终后台部署通过 Windows PowerShell 5 执行；旧部署脚本使用
  `Set-Content -Encoding utf8`，因此给四份职业 `.gon.merge` 和 `modlist.txt` 写入
  `EF-BB-BF` UTF-8 BOM，玩家启动时出现 `GON ERROR: More symbols exists after file
  completed parsing`。部署现已改用 `.NET UTF8Encoding(false)` 明确写无 BOM UTF-8，
  安装校验也会主动拒绝 BOM，避免再次把语法正文正确但编码不兼容的 GON 交付。
- v0.5.35 的几何目标求解不会把几何不同的升级候选筛掉；只有家具信息本身无法解析
  时才保留旧坐标交给当前原生路径。确定性回归覆盖“旧位置被另一件家具阻挡，宽替换
  件应移动到最近上一行”的场景，并验证目标从 `(-10,-12)` 变为 `(-10,-11)`。
- 当前 day 339 存档只读 probe：269 件家具、112 件已摆放、157 件在家具栏、5 个
  房间；家具 info/grid/effect 覆盖 269/269，146 个 Support 全部合法。失败底座链为
  `13 -> 68 -> 197 -> 236 -> 221 -> 321`，与 v0.5.36 生成的 top-down 收回顺序一致。

## 文件

- 原生适配器：`src/ui/mew_ui_furniture_move_adapter.c`、
  `src/ui/mew_ui_furniture_move_adapter.h`。
- Gateway：`src/ui/furniture_placement_gateway.cpp`、
  `src/ui/furniture_placement_gateway.hpp`。
- 分析与执行：`src/furniture_analysis/service.cpp`、`src/ui/mew_ui_bridge.cpp`、
  `src/furniture_planning/layout_solver.cpp`、
  `include/auto_cattery/furniture_analysis/domain.hpp`、
  `include/auto_cattery/furniture_planning/layout_solver.hpp`。
- 房间身份修复：`src/ui/mew_ui_house_move_probe.h`、
  `src/ui/mew_ui_house_move_room_scan.c`、`src/ui/runtime_room_resolution.cpp`、
  `src/ui/runtime_matched_save_snapshot_adapter.cpp`。
- 回归：`tests/furniture_analysis_service_tests.cpp`、
  `tests/mew_ui_furniture_placement_tests.cpp`、
  `tests/mew_ui_house_move_probe_tests.cpp`、`tests/runtime_house_state_tests.cpp`。
- 阶段与版本：`CMakeLists.txt`、`assets/description.json`、`CODEX_TASK.md`、
  `.auto-cattery/state.json`、`docs/implementation-status.md`、
  `docs/furniture-auto-placement-design.md`、本报告。
- 部署与安装校验：`tools/deploy.ps1`、`tools/verify_install.ps1`。

## 构建与测试

- v0.5.48 `build-stage45-release` 增量 Release 编译成功；统一
  `auto_cattery_tests.exe` 通过，CTest 4/4 通过。
- 版本升级后执行 `tools\build.ps1 -Configuration Release`。工具观察窗口在全量编译
  期间超时，但保留的原进程继续完成：主 `cmake`/`cl` 退出，新 DLL 于 02:52:36
  生成，新统一测试程序于 02:56:23 生成；残留 MSBuild 均为 CPU 增量 0 的
  `/nodeReuse:true` 空闲节点。未启动重复构建。
- 直接运行上述既有 Release 测试产物成功；`ctest --test-dir build -C Release
  --output-on-failure` 为 4/4 通过，包含 DLL 加载、恢复 CLI 与 save-lab CLI smoke。
- `tools\deploy.ps1 -GameRoot D:\steam\steam\steamapps\common\Mewgenics
  -Configuration Release` 成功；`tools\verify_install.ps1` 成功。部署保留玩家 20 次
  重投设置，data MOD 为 v0.5.48，构建/安装 DLL 哈希一致。

- `tools\build.ps1 -Configuration Debug`：成功；Debug CTest 4/4 通过；Debug DLL
  exports 与 x64 检查通过。
- 独立 `build-stage45-release` Release 全量构建：成功；Release CTest 4/4 通过。
- v0.5.32 最终增量 Release：成功；Release CTest 4/4 通过，但玩家实测分析入口因
  删除状态过滤回归失败，已被 v0.5.33 取代。
- v0.5.33 Debug 与 Release 修复构建：成功；两种配置 CTest 均为 4/4 通过；新增的
  单字节删除状态回归测试通过。
- v0.5.34 Debug 与 Release 联合持久构建：成功；两种配置 CTest 均为 4/4 通过；
  DLL exports 与 x64 检查通过；五房通用房间身份回归通过。
- v0.5.35 Debug 编译成功，Debug CTest 4/4 通过；独立
  `build-stage45-release` Release 编译成功，Release CTest 4/4 通过。新增几何替换目标、
  邻近候选顺序、替换原坐标保存和后续布局从实际创建位置继续的回归均通过。
- v0.5.36 `tools\build.ps1 -Configuration Debug`：成功；Debug CTest 4/4 通过；
  Support 三层链回归确认收回顺序为最上层到最下层。
- v0.5.36 `tools\build.ps1 -Configuration Release`：成功；Release CTest 4/4
  通过；Release DLL exports 与 x64 检查通过。
- v0.5.37 当前 day 339 存档完整分析复现：从 `AC3205` 等价失败变为成功，生成 26 项
  有完整合法目标的属性替换；自动放置可用条件成立。新增回归确认最高收益家具宽于
  房间、无法放置时会选用次优可执行家具。
- v0.5.37 `tools\build.ps1 -Configuration Debug`：成功；Debug CTest 4/4 通过。
- v0.5.37 `tools\build.ps1 -Configuration Release`：成功；Release CTest 4/4
  通过；Release DLL exports 与 x64 检查通过。
- v0.5.38 聚焦 detached-component 回归通过：解除占用后必须保留 stable key、item、
  原坐标和 entry，entry room 必须为空且 component 不得进入删除队列；仍绑定房间或
  已排队删除都会被拒绝。
- v0.5.38 Debug 完整构建成功，CTest 4/4 通过。最终事务收紧后重新编译并运行 Debug
  单元测试成功。
- v0.5.38 Release `AutoCattery` 与单元测试目标编译成功，Release 单元测试成功；仅将
  新 Release DLL 复制到 `Mewgenics\mods\AutoCattery.dll` 供玩家实测。按用户要求，
  功能实机确认前不执行安装校验、SHA-256 比较或包验证。
- v0.5.39 聚焦 Debug 编译与单元测试通过。当前玩家存档只读几何 probe 从
  `rooms=0, moves=0, evacuation_blocked=1` 变为
  `rooms=1, moves=2, target=Floor1_Large, evacuation_blocked=0`；规划前 Support
  144/144 合法，规划后新增两件仓库家具后 Support 146/146 合法。
- v0.5.39 `tools\build.ps1 -Configuration Debug`：全量构建成功；Debug CTest
  4/4 通过；Debug DLL exports 与 x64 检查通过。
- v0.5.39 `tools\build.ps1 -Configuration Release`：全量构建成功；Release CTest
  4/4 通过；Release DLL exports 与 x64 检查通过。
- `tools\deploy.ps1 -GameRoot D:\steam\steam\steamapps\common\Mewgenics
  -Configuration Release`：DLL-only 部署成功。
- v0.5.35 build、`dist\Release\AutoCattery.dll` 与实际安装的
  `Mewgenics\mods\AutoCattery.dll` SHA-256 一致：
  `31C8BF38CBCC2A4ED8FDBDE3EA8744BFC8AEDE197FBFF140E1A4E1E07ADB0546`。
- v0.5.36 build、`dist\Release\AutoCattery.dll` 与实际安装的
  `Mewgenics\mods\AutoCattery.dll` SHA-256 一致：
  `6E7DF91D05C41ACBF2100EF9BA4E4A93C22525C7535C065AB49422BB06229E61`。
- v0.5.37 DLL-only 部署与 `tools\verify_install.ps1` 通过；14 个职业重投保持 20。
  build、`dist\Release\AutoCattery.dll` 与实际安装的
  `Mewgenics\mods\AutoCattery.dll` SHA-256 一致：
  `4D449C17DCA34C36C65B92D1319C189309163CF754206BF2B7E3AF7F22F709F4`。
- 玩家首次启动暴露 BOM GON 错误后，已在同一 Windows PowerShell 5 后台环境重新
  执行修正后的部署与 `verify_install`：成功；AutoCattery 和
  SkillsPassivesFirstData 的四份职业 GON、`modlist.txt` 均确认 `BOM=False`，四份 GON
  各自花括号计数相等，14 个职业重投保持 20；DLL hash 未改变。

## 玩家验证状态

## v0.5.42 玩家反馈修正与当前验证

- 最新 v0.5.41 日志确认分析会依次返回 `Attic moves=0`、
  `Floor1_Large moves=0`，重新进入家具模式后会话锁清空并重复该循环。根因是完整但
  零移动的候选被当作成功目标提前返回。
- 原求解器在仓库非空时跳过其他房间已摆家具，导致 `Floor1_Small` 的四尊无负面
  `+5` 雕像完全没有进入阁楼候选。当前 day 339 存档确认 stable key 145、265、
  283、321 分别为 `special_appealidol`、`special_stimulationidol`、
  `special_evolutionidol`、`special_comfortidol`。
- v0.5.42 允许目标房现有家具参与重排，允许未锁定房间家具进入候选，已锁定房间
  仍不可拆；阁楼按核心最低值、核心总和、Appeal、填充数排序，核心负值家具排除。
- 玩家手动优化并在 2026-08-12 20:04 保存后，最新 day 339 快照为 229 件家具：
  已摆放 144、仓库 85、阁楼 52；当前 Support 为 177/177 全数合法。旧截图和旧
  185/185 只属于上一快照，不再作为固定坐标或固定总数。
- 局部求解覆盖完整 37x11 阁楼，而不是旧截图中的某个红框方向。第一批只读计划为
  2 步：key 144 `small_trash_can2` 在阁楼从 `(22,-11)` 移到 `(12,-9)`，再把
  `Floor1_Small` 的 key 265 `special_stimulationidol` 调入阁楼 `(13,-9)`。
- 真正根因是目标候选按 24 件截断后，未入选家具错误地从来源房执行占用快照消失，
  导致四尊雕像被假报 `evacuation_blocked`。现所有未入选家具继续作为原房间静态
  占用参与逐步执行与 Support 判断，并新增候选截断来源房回归。
- 最终只读验收：`target=Attic`、`moves=2`、`evacuation_blocked=0`、
  `installation_blocked=0`，规划前后均为 177/177、`unsupported=0`。模拟第二批不会
  重复第一批，会继续移动阁楼 key 316 并调入 key 283 Evolution +5 雕像。
- Debug 与 Release 完整构建均成功，两种配置 CTest 均 4/4 通过；Release DLL-only
  部署和安装校验通过。build、dist 与安装 DLL SHA-256 一致：
  `23DD099AB0B201F9ED5CA2263FB96C496B01308604A9BFD632960C1190DAA389`。
- 自动验收已通过，玩家实机执行、保存、退出重进持久化验证待完成。

## v0.5.45 多项属性替换回滚闪退修复

- 2026-08-12 23:53–23:55 玩家连续两次稳定复现：分析完成后点击“开始放置”立即
  闪退；两次运行分别产生 PID 19496、13928 的崩溃 dump。
- 两次日志路径一致：分析均生成 18 项属性替换，前 13 项成功，第 14 项
  `set_spider_tv key=435 -> set_90s_bed key=315` 被原生合法性检查拒绝。第 14 项自身
  已恢复原家具，但批次继续同 tick 逆序回滚前 13 项，结果从第一项开始即为
  `restored=0/13`，随后在游戏原生代码发生访问冲突。
- 当前 build 的家具替换会把旧 `FurniturePiece` 放入延迟删除队列。前 13 项成功后，
  旧 stable key 的 component 在本 tick 仍存活；失败回滚再以相同 key 创建旧家具，
  会与尚未清理的 component 冲突。因此不能把多个原生替换及其同 tick 逆序重建视为
  一个可安全回滚的事务。
- v0.5.45 将每次分析最多密封一项属性替换；只要本批存在属性替换，就不生成或执行
  布局移动。单项成功后重新分析下一项，单项被原生拒绝时只走该项已有的内部恢复。
  所有属性替换完成后，后续分析才恢复普通房局部压紧和仓库填充。
- `furniture_analysis_service_tests` 使用两个可执行属性升级验证本批只返回一项，并验证
  累计收益只包含该项且布局计划为空。Release 构建、CTest、部署与玩家实机复测状态
  见下方交付更新。
- 2026-08-13 最终验证：已生成的 Release `auto_cattery_tests.exe` 聚焦运行成功；随后
  `tools\build.ps1 -Configuration Release` 完整成功，CTest 4/4 通过，DLL exports 与
  x64 检查通过。`tools\deploy.ps1 -GameRoot
  D:\steam\steam\steamapps\common\Mewgenics -Configuration Release` 已部署 v0.5.45
  DLL 和 UI data mod，并保留玩家现有 20 次升级重投设置。未自动启动游戏；“连续重新
  分析并逐项替换，原生拒绝时不再闪退”的玩家实机验证待完成。

## v0.5.43 最新实机日志修复

- 2026-08-12 20:56:20–20:58:59 的 v0.5.42 最新日志已逐行核对。按钮并非没有收到
  点击：每次均出现 `AC3203`。第一次执行 6 项属性替换后，20:57:39、20:57:42、
  20:57:43 连续三次分析都以 `AC3205 current placed furniture coverage is incomplete`
  失败。根因是未保存存档仍把被替换的旧 stable key 记录为 placed，而完整运行时枚举
  已把仓库的新 stable key 放入房间；旧覆盖器错误要求所有存档 placed key 仍在 scene。
- 运行时家具枚举完整时，v0.5.43 以枚举结果作为当前房间成员权威：先把存档家具视为
  仓库，再逐项覆盖当前 scene 里的 room、坐标和朝向。仍严格拒绝未知 stable key、
  item 不一致、重复 key、无效房间或无效 scale；只是允许未保存的已替换旧 key 回到
  仓库状态。新增“旧 key 回仓库、新 key 在房间”的回归测试。
- 最新日志同时证明成功阁楼批次后再次分析返回 `rooms=0, moves=0,
  evacuation_blocked=1`；关闭再打开家具界面后又选择阁楼。v0.5.43 不再在家具界面
  开关时清空完成房间锁，锁保留到当前 House scene generation 结束；切换存档或离开
  House 后下一次分析才开始新的房间序列。
- 普通房原先只走整房重排，真实支撑链会让所有候选落入 `evacuation_blocked`。当前
  普通房复用有界单件局部压紧；碰撞和 Support 审计仍包含静态墙面、吊挂和不可移动
  家具，但“是否更紧凑”的比较只计算本批可移动家具，避免静态墙画撑满房间外框后
  永远掩盖地面家具的真实局部改善。
- 2026-08-12 20:58:58 最新 day 339 保存槽只读状态为 223 件家具、144 placed、79
  warehouse、五房均存在，当前 Support 182/182。最终 Debug probe 验证：未锁定时
  `target=Attic, moves=2`；锁定 Attic 后普通房产生非零安全批次；锁定
  `Attic;Floor1_Small;Floor2_Large` 后 `target=Floor1_Large, moves=1`；再锁定
  Floor1_Large 后 `target=Floor2_Small, moves=1`。所有展示批次均为
  `evacuation_blocked=0`、`installation_blocked=0`，规划前后 Support 保持 182/182。
- v0.5.43 最终 Debug/Release 构建、CTest、部署、安装 hash 与玩家实机复测状态见本节
  后续交付更新。

## v0.5.44 普通房持续优化、仓库填充与低属性替换

- 玩家 v0.5.43 实机结束画面和最新日志确认：普通房每次只执行 1 件局部移动，UI 随即
  把本批目标房加入锁定集合；五房各执行一批后所有房间被锁，后续分析直接返回
  `rooms=0, moves=0`。因此右侧房间即使只有 2 件家具、左侧仍有空位、仓库仍有 78 件，
  求解器也不再评估。
- v0.5.44 的布局计划新增显式 `exhausted_room_ids`。执行一批移动只代表取得进展，
  不再自动锁房；只有完整评估后没有仓库填充、局部压紧、整房候选或执行阻断的房间
  才标记为穷尽。`CurrentStateInvalid`、`FinalStateInvalid`、`EvacuationBlocked`、
  `InstallationBlocked` 或最终 Support 审计失败的房间均不得标记完成。
- 普通房先保留当前整屋占用，专门尝试一件 `warehouse -> target room` 安装，不撤离或
  重装目标房现有家具；仓库仍有可用家具时，候选截断优先保留仓库件，不让其他房间
  候选挤掉仓库填空机会。仓库件确实放不下后，才继续局部压紧和后续候选。
- 普通房属性替换不再只接受五项逐项占优；若替换能提高房间的
  `核心最低值 -> 核心总和 -> Appeal` 字典序排名，也允许小幅属性取舍。阁楼继续要求
  Comfort、Stimulation、Health、Mutation 核心逐项不降低，避免破坏平衡策略。
- 聚焦 Release 单元测试通过，覆盖：普通房连续局部移动不锁定、仓库候选超过搜索上限
  时仍优先填空、仓库安装不触碰现有布局、运行时网格异常时不返回 exhausted、普通房
  取舍型整体属性改善，以及阁楼拒绝核心属性下降的极端替换。
- 2026-08-12 最新 day 339 只读 probe：222 件家具、144 placed、78 warehouse、五房
  齐全，当前 Support `184/184`。传入当前实机网格 Attic `37x11`、四个普通房
  `18x9` 后，有界序列为：batch 1-5 连续选择 `Floor1_Large` 做房内压紧；batch 6
  将 key 323 `set_bone_table` 从仓库放入 `Floor1_Small`；batch 7 将 key 89
  `object_toxicwaste` 放入；batch 8 将 key 195 `object_radio_30s` 放入。每批均为
  `unsupported=0`、`current_blocked=0`、`evacuation_blocked=0`、
  `installation_blocked=0`，首批规划后 Support 仍为 `184/184`。
- 本次按玩家要求以功能完成为先，不执行 `tools\verify_install.ps1` 或安装 hash 检查。
  最终 Release `ALL_BUILD` 于 2026-08-12 23:42:48 完成；新 DLL 于 23:38:03
  生成。随后直接对该构建运行 Release CTest，4/4 全部通过；DLL exports 与 x64 检查
  通过。`tools\deploy.ps1 -GameRoot D:\steam\steam\steamapps\common\Mewgenics
  -Configuration Release` 成功部署 DLL 和 UI data mod，并保留玩家现有 20 次升级重投
  设置。未运行安装验证或 hash 审计，玩家实机功能复测待完成。

玩家完全退出游戏后重新通过 Mewtator 启动，进入同一测试存档和家具模式。重复点击
“开始分析”与“自动放置”：同一普通房应连续获得多批压紧或仓库填充，不应执行一件
后跳房或在仍有空位时返回 `rooms=0, moves=0`。完成一房后应自动推进下一房；关闭再
打开家具界面后，同一 House scene 的完成房间锁仍保留。观察低属性家具是否被整体
属性更优的仓库家具替换。成功后保存、完全退出并重进，确认布局和替换持久化。

## 风险与未做范围

- 当前原生 RVA、type/vtable 和对象布局只适用于已 gate 的当前
  `Mewgenics.exe`；签名不匹配时安全拒绝。
- 自动化测试无法代替真实游戏的仓库数量、画面、保存和重进持久化验证。
- v0.5.54 的状态边机制能通用识别有限长度布局闭环，但具体替代动作、原生接受情况和
  最终画面质量仍需玩家实机确认；`AC3924` 后应继续出现其他 key/坐标或推进其他房间。
- 首次选择存档后的偶发 `ntdll.dll` 访问冲突尚无可信符号化栈；若 v0.5.54 首次进入
  仍闪退，应以新日志边界和 dump 增加精确生命周期 probe，而不是把它与循环修复混为
  同一根因。
- v0.5.53 的用途满足系数是基于预计入住人数的通用、有界启发式，不是从当前单一存档
  反推的固定值；实际画面密度和各用途收益仍以本轮玩家实机复测为验收门。
- v0.5.52 只实现报告中已有代码/日志闭环支持的 P0：purpose precedence、用途/焦点
  调度、exact reverse tabu 与诊断日志。未实现报告的二步 look-ahead、purpose 完成后
  neutral fill、搜索 disposition 全量拆分、coherent micro-batch、繁育阈值校准、
  旋转、Anchor、墙面/天花板布局或跨会话撤销。
- 未自动移动猫、休息、结束一天、出征、组队或淘汰；第五房是否解锁仍不在本阶段
  范围，但当前 build 的第五房运行时身份一致性已在本次修复。

本地提交：本报告与实现位于同一本地任务提交，最终 hash 见交付回复。
是否 push：否

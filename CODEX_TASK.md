# CODEX CURRENT TASK - STAGE 45 AUTOMATIC WAREHOUSE ATTRIBUTE REPLACEMENT

## v0.5.53 state-bound room locks and purpose furnishing targets

- A completed-room lock now stores the room's exact live furniture identities,
  coordinates, and orientation. Player manual placement, removal, movement, or
  emptying invalidates only the changed room lock before the next analysis.
- Furniture-mode close/reopen still preserves unchanged completed rooms and
  retired-key quarantine inside the current House scene.
- Purpose-aware warehouse filling stops after a dynamic resident-scaled target
  is met instead of treating every marginal attribute increase as a reason to
  keep filling the room. Empty and under-target rooms remain fillable.
- Release unit tests and the Release DLL build must pass before DLL/data-only
  deployment. Do not launch or control the game.

## v0.5.52 purpose focus and exact reverse-move tabu

- A room with a `RoomPurposeAssignment` now fully uses purpose-aware layout
  ordering even when it is the Attic; the legacy balanced-Attic strategy is
  only a fallback when purpose analysis is unavailable.
- Furniture target rooms are ordered by purpose, with Breeding first, and the
  room that just committed a layout move remains the preferred focus until it
  is explicitly exhausted or safely deferred by native rejection.
- An immediate reverse replan no longer adds the entire target room to the
  completed-room lock. The exact reverse move enters a current-House-scene
  tabu set, and the solver keeps searching the same focused room for another
  furniture item or coordinate.
- Native-rejection rollback, whole-house Support gates, retired stable-key
  quarantine, 250 ms settling, and the 32-transaction checkpoint remain
  unchanged.
- Release unit tests and the Release DLL build must pass before deployment.
  Deploy DLL/data only and do not launch or control the game.

## v0.5.51 purpose filtering and bounded native transactions

- Purpose rooms no longer accept warehouse or cross-room furniture merely
  because it fits. A candidate must strictly improve the current room-purpose
  rank; neutral furniture remains in the warehouse.
- Combat staging therefore prefers furniture that further lowers effective
  Comfort while preserving the existing non-negative Health gate. Purpose-aware
  packing compares final purpose attributes before geometric fill quality.
- Continuous Auto Place waits 250 ms after each committed transaction and
  pauses after 32 transactions from one click, allowing native furniture
  objects and deferred deletion work to settle before the player continues.
- A furniture object newly created from the warehouse is protected from an
  immediate attribute replacement for the rest of the current furniture-mode
  opening. Existing in-room layout moves remain available.
- Release unit tests and the Release DLL build must pass before deployment.
  Deploy without hash verification at the player's request. Do not launch or
  control the game.

## v0.5.50 purpose-aware furniture optimization

- Furniture analysis now consumes the current five-room plan instead of
  optimizing every ordinary room toward the same high-comfort aggregate.
- Breeding rooms balance effective Comfort and Stimulation; combat staging
  deliberately lowers effective Comfort while keeping Health non-negative;
  mutation rooms prioritize Mutation with non-negative Health and effective
  Comfort above -10; kitten and recovery rooms prioritize Health and Comfort.
- Appeal is no longer a purpose objective for ordinary rooms. If purpose
  analysis is unavailable, the existing conservative general-room ordering is
  retained.
- Warehouse filling evaluates the complete warehouse for the best legal
  placement, counts only blocking geometry, and scores free-space components,
  isolated cells, contact edges, and deterministic tie breaks to reduce gaps.
- Release build and tests must pass before DLL/data deployment. Player validates
  the five role-specific attribute directions and confirms furniture-mode exit
  remains crash-free. Do not launch or control the game.

## v0.5.49 furniture-mode replacement lifetime guard

- v0.5.48 player runtime completed 17 attribute replacements and 26 layout
  moves, then reached `AC3922`. Exiting furniture mode one minute later crashed
  with an unhandled `std::bad_alloc` (`0xE06D7363`); the same run also produced
  an earlier `ntdll` access violation dump.
- The final replacements formed a native create/delete chain: a piece placed
  from the warehouse was immediately replaced, then its replacement was
  immediately replaced again. Every successful replacement queues the old
  `FurniturePiece` for deferred destruction until the game settles the
  furniture UI lifecycle.
- Each furniture-mode opening may now commit at most one attribute replacement.
  After that replacement, continuous Auto Place keeps layout moves and warehouse
  filling enabled but suppresses further attribute upgrades until furniture
  mode is closed and opened again.
- The guard is generic, is not keyed to a furniture name or stable key, and
  does not change analysis scoring or ordinary layout legality.

## v0.5.48 continuous-layout oscillation guard

- 最新 v0.5.47 实机日志确认 `wallmounted_cloud key=90` 在 `Floor1_Small` 的
  `(-10,-8)` 与 `(-10,-9)` 之间持续上下往返。原生移动每次均成功；根因是连续流程
  只执行 29 步整房方案的第一步，下一轮局部压紧又立即生成同一 stable key 的完全
  反向移动。
- 连续执行现在记录上一笔真实提交的普通布局移动。新分析若第一项是同一 item/key、
  同一房间和坐标的完全反向移动，则记录 `AC3924`，暂缓该目标房并继续其他房间；
  仓库放置、属性替换、手动新启动或非反向移动都会清空该记录。
- 修复不得按 `wallmounted_cloud` 名称硬编码，也不得影响玩家新一轮手动分析/放置。

## v0.5.47 continuous-run speed and recoverable native rejection

- v0.5.46 实机日志证明每项自动事务前出现两次完整快照/分析：自动 re-analyze 已生成
  新 preview，`StartFurnitureAutoPlacement` 又同步重复一次。自动续跑现在直接使用刚
  完成的 fresh preview；玩家首次手动启动仍保留 binding 防陈旧检查。
- 连续布局计划每轮只执行第一项，再刷新并重新规划，避免后续计划在运行时变化后继续
  使用旧序列。
- 原生移动若无 SEH 且已安全恢复，连续流程不再整体失败；将该目标房标为本次 House
  scene 暂缓并继续其他未锁定房间。SEH、恢复失败、场景变化仍立即停止。

## v0.5.46 current-scene stable-key quarantine

- 属性替换成功退回仓库的旧 stable key，以及原生拒绝后被回收的新 stable key，在
  当前 House scene generation 内进入 quarantine；分析器和原生 gateway 都不得再用
  它们调用仓库 create。离开并重新进入新的 House scene 后才清空。
- 玩家点击一次“自动放置”后，MOD 在每项已提交事务后自动刷新运行时快照、重新分析
  并继续其他未隔离的安全属性升级或布局移动，直到当前场景安全 fixpoint。
- 后续失败不再逆向重建先前已经提交、且旧 key 正在延迟删除的属性替换；只回滚当前
  密封计划中已提交的布局移动。任何原生 SEH 会让当前 House scene 停止后续写入。
- 本版不猜测同场景真正 create-safe ACK；quarantine 的释放边界保守固定为新 House
  scene。玩家实测重点是 key=36 类快速复用不再进入 create、一次点击可自动继续且不
  闪退。

## Current objective

在 Stage 43 已能分析仓库家具五属性升级、Stage 44 已由玩家真实取放证明仓库 stable
key 会生成新的 `FurniturePiece` 后，把属性升级接入现有“自动放置”批次。执行时先把
更优仓库家具替换到原家具位置，再按替换后的稳定 key 执行布局计划；任何一步失败都
停止后续操作并尽可能逆序恢复本批次。

## Accepted current-build evidence

- 玩家 Stage 44 两次手动仓库取放分别新增 stable key 474 和 467，证明仓库实例进入
  房间时会创建保留原存档 stable key 的新 scene piece。
- 当前 `Mewgenics.exe` 五个真实调用点确认 `0x1ABFF0` 的参数为 House scene
  manager、scene context、stable key 指针；该函数创建并初始化当前 build 的
  `FurniturePiece`。
- 当前游戏自身代码存在“创建 FurniturePiece -> 设置 transform -> 原生校验 ->
  提交 grid”的完整调用链。
- 已验证并复用移除占用 `0x2EE3D0`、合法性校验 `0x2EDE60`、提交
  `0x2EE230`；组件删除路径 `0x94A910` 会在 component context `+0x18` 设置
  删除状态，调用者不读取返回值。

## Stage 45 completion boundary

- `FurniturePlacementGateway` 支持按 warehouse stable key 创建家具，并在原家具
  房间/坐标通过原生校验后提交；提交读回成功才删除已清空占用的旧 scene piece。
- 创建、校验、提交或删除失败时，删除新对象并恢复旧家具的 transform/grid；批次
  后续失败时，先逆序恢复布局移动，再逆序交换已完成的属性替换。
- 已排队删除的 `FurniturePiece` 不再参与 scene 枚举、stable key 冲突检查或定位，
  防止同一 tick 的延迟清理干扰下一次替换及回滚。
- 分析器使用虚拟替换后的 stable key 生成布局计划；自动放置先逐 tick 执行全部属性
  替换，再执行布局移动。只存在属性升级时按钮也必须可用。
- v0.5.32 玩家实测发现删除状态过滤错误读取了 `context+0x18` 的两个字节，导致
  正常家具因 `+0x19` 非零被全部误判为待删除，运行时覆盖记录为 furniture=0。
  v0.5.33 必须只读取游戏实际比较的 `context+0x18` 单字节，并加入对应回归测试。
- v0.5.33 玩家实测已恢复 112 件运行时家具，但五房完成存档仍在房间覆盖阶段失败。
  通用根因有两处：运行时房间探针漏掉第五普通房间 `Floor2_Small`；家具分析路径
  没有按已解锁房间数补齐普通房间，却会把探针检测到的无人使用 `AdventureBox`
  辅助组件加入待映射快照。v0.5.34 必须统一普通房间集合，并覆盖 5 个快照房间、
  7 个原生组件、第五房无猫投票且 2 个组件无关的回归。
- 版本升级为 v0.5.34；Debug/Release 构建和 CTest 通过后 DLL-only 部署并验证安装；
  不 push。
- v0.5.34 首次后台部署使用 Windows PowerShell 5，旧脚本的
  `Set-Content -Encoding utf8` 给职业 GON 和 `modlist.txt` 写入 UTF-8 BOM，游戏启动
  报 `GON ERROR: More symbols exists after file completed parsing`。部署必须改为无 BOM
  UTF-8，并让安装校验主动拒绝 BOM 后再交付。
- v0.5.34 玩家已确认分析成功并生成 46 个属性升级、3 个布局移动，但第一次替换
  `special_fightidol -> set_bone_tv` 在原坐标被原生合法性校验拒绝，批次按设计回滚为
  0/49。v0.5.35 不缩减属性升级候选：分析时使用新家具的 24x24 放置网格和当前房间
  占用，先求出距离旧位置最近且不会破坏现有支撑关系的坐标；创建件采用旧家具朝向，
  原生执行再从该几何目标落地，若实际坐标有调整则后续布局从实际位置继续。
  Debug/Release 与部署通过后交给玩家复测，不 push。
- v0.5.35 玩家复测第一项 `special_foodbox key=13 -> set_90s_stove key=99`
  在 `target=(-10,-11)` 仍被原生合法性校验拒绝。当前存档只读网格确认 key 13
  直接或间接支撑 key 68、197、236、221、321；不能在这些上层件仍留在 scene 时从
  下方替换底座。当前 build 反汇编确认 `0x2EF0D0` 是完整家具栏收回路径：调用
  `0x2EE3D0` 清除 grid 并把 entry room 置空、从 scene 注册集合解绑、清空 piece 的
  entry 指针并排队删除 component。v0.5.36 必须按“最上层到最下层依赖件收回家具栏
  -> 旧底座收回家具栏 -> 新底座从 stable key 创建并提交 -> 最下层到最上层依赖件
  按 stable key 原位创建”的真实游戏流程执行；失败时按相反顺序恢复原底座和已收回
  依赖链。不得跨房间或在房间地面寻找临时落点。
- v0.5.36 玩家点击“开始分析”后两次出现 `AC3205 attribute upgrade support chain
  could not be resolved`，因此没有生成 `AC3901`，自动放置保持禁用。当前存档复现确认
  key 13 的初始依赖链可解析；根因是属性候选先按收益占用 placed/warehouse，再在后续
  阶段求几何目标。`key=13 -> set_90s_stove key=99` 没有完整合法落点，却以旧坐标
  `(-10,-11)` 写入虚拟家具集合，导致下一项 key 68 的依赖解析失败。v0.5.37 必须在
  贪心选择每个候选时同时验证当前虚拟状态的 Support 链和完整合法目标；不可执行候选
  不占用 placed/warehouse，继续尝试次优可执行配对。当前存档完整分析必须成功并让
  自动放置可点击。
- v0.5.37 玩家实测第一项无支撑替换成功；第二项
  `special_foodbox key=13 -> set_wooden_toilet key=160` 收回 5/5 支撑件并提交新底座后，
  同 tick 第一件支撑家具重建在 `Mewgenics.exe+0x5959A` 触发空指针，日志为
  `support=5/5->0`、`seh=0xC0000005`，随后回滚失败并卡死。根因是支撑件走完整家具栏
  收回，旧 component 尚在延迟删除队列时又用相同 stable key 创建新 component。
  v0.5.38 必须让支撑链和旧底座先只解除 grid 占用并保留原 component；新底座提交后
  按最下层到最上层校验并重新提交原支撑 component，全部成功后才把旧底座正式收入
  家具栏。失败回滚复用原底座和支撑 component，不得在同一 tick 为它们创建同 key
  component。
- v0.5.38 玩家实测 26 项属性替换全部成功，但完成后当前存档仍有 131 件仓库家具，
  阁楼及四个普通房间都有明显合法空位；再次分析返回 `rooms=0, moves=0,
  evacuation_blocked=1`，界面却显示“无需安全移动”。根因是全屋布局器对空 room_id
  只统计 warehouse 后直接跳过，且 UI 把布局受阻误报为已完成。v0.5.39 必须把仓库
  stable key 作为可创建的布局来源，优先保留目标房现有合法布局并把仓库件填入剩余
  空位；每批成功锁定一个目标房，下次分析继续其他房。失败时新建件回家具栏，批次
  失败时逆序回收已放入的仓库件。布局受阻不得再显示“无需移动”。

## Player validation gate

## v0.5.42 player-feedback correction

### Mandatory implementation contract

Stage 45 剩余布局工作必须严格按照
`docs/stage-45-layout-optimization-implementation-contract.md` 实施。该文档固定了目标房
可重排语义、未锁定房间候选、锁定房间保护、局部依赖感知压紧、Support/执行硬门、
day 339 验收条件以及构建部署顺序。不得退回“冻结目标房，只向空洞填充”的旧策略，
不得继续叠加递归完整全屋求解回退，也不得在真实非零安全计划产生前部署或宣布完成。

- v0.5.41 玩家复测发现完成阁楼后重复分析仍会选中 `moves=0` 的阁楼/大房间，
  阁楼现有布局有明显可压紧空隙，且 `Floor1_Small` 的四尊无负面单属性 `+5`
  雕像未参与候选。
- v0.5.42 取消“目标房现有家具近似冻结”的正式语义：目标房家具可以重排压紧，
  未锁定房间家具可以进入目标候选，只有已锁定完成房间不可拆；零移动候选不得阻断
  后续房间。
- 跨房候选执行前必须对整屋最终 Support 做硬审计，任一悬空方案安全拒绝。最新
  day 339 只读 probe 证明 stable key 145/265/283/321 分别为
  Appeal/Stimulation/Evolution/Comfort +5；修复候选截断后的来源房占用快照后，
  第一批会重排阁楼并调入 key 265，规划前后保持最新快照 177/177 全数 Support
  合法，模拟第二批继续调入 key 283。
- v0.5.43 玩家最新实机日志确认三处剩余回归：属性替换后未保存存档仍保留旧 placed
  stable key，导致后续分析连续 `AC3205 current placed furniture coverage is incomplete`；
  关闭再打开家具界面会清空已完成房间锁并重新选择阁楼；锁定阁楼后普通房只走整房
  重排，真实布局全部落入 `evacuation_blocked=1`。修复必须以完整运行时枚举覆盖未保存
  家具房间状态、让锁跨家具界面开关保留到当前 House scene 结束，并让普通房复用有界
  单件局部压紧，且紧凑评分只计算可移动家具、静态墙面/天花板对象只参与碰撞和
  Support 硬门。最新 day 339 保存槽 223 件、144 placed、79 warehouse、当前 Support
  182/182；只读锁定序列必须从 Attic 推进到 Floor1_Large、Floor1_Small、
  Floor2_Large、Floor2_Small，所有返回批次 moves>0、evacuation=0、Support 182/182。
- v0.5.44 玩家截图与最新 v0.5.43 日志确认，普通房每执行一件局部移动就被 UI 锁定，
  五房各做一批后直接返回 `rooms=0, moves=0`；同时普通房局部路径只压紧现有家具，
  不持续从 78 件仓库家具填空。修复必须仅在求解器完整确认房间无安全改善时返回
  `exhausted_room_ids`，执行一批移动不等于完成，任何 current/final state、evacuation、
  installation 或 Support 阻断都不得锁房。普通房优先从仓库单件安全安装，仓库不可用
  后再考虑跨房候选；低属性旧家具既接受逐项占优替换，也接受能提高房间
  `核心最低值 -> 核心总和 -> Appeal` 排名的取舍型替换，阁楼仍要求核心逐项不降。
  最新 day 339 只读状态为 222 件、144 placed、78 warehouse、Support 184/184；有界
  序列必须让 Floor1_Large 连续多批后再进入 Floor1_Small 仓库填充，且所有批次无
  current/evacuation/installation blocker。
- v0.5.45 玩家两次实测均在分析成功后点击“开始放置”立即闪退。两次日志和 dump
  对应同一确定路径：18 项属性替换中前 13 项成功，第 14 项
  `set_spider_tv key=435 -> set_90s_bed key=315` 被原生合法性检查拒绝；该项自身恢复
  成功后，批次又尝试同 tick 逆序恢复前 13 项，但游戏仍延迟删除被替换旧家具的
  component，使用相同 stable key 重建从第一项即失败为 `restored=0/13`，随后原生
  访问冲突。修复必须把每次分析密封为最多一项属性替换，并且有属性替换时不同时
  生成布局移动。成功后由玩家重新分析下一项；原生拒绝时只需该项内部恢复，不再
  存在多项同 tick 回滚。属性替换全部完成后，后续分析才恢复普通房连续压紧和仓库
  填充。版本升级为 v0.5.45，完成 Release 构建、CTest、DLL/data MOD 部署和本地提交
  后交给玩家复测；不自动启动游戏，不 push。

- 使用可恢复测试槽完全重启后进入家具模式，只点击一次“开始分析”；确认日志出现
  `AC14319`、`AC3201`、`AC3901` 且没有 `AC3205`，面板显示属性替换数和布局移动数。
- 点击“自动放置”，确认家具逐件替换/移动且没有崩溃；仓库填空日志应出现
  `AC3913`，属性替换日志为 `AC3912`，最终
  成功出现 `AC3904`，失败回滚出现 `AC3907`。
- 保存、完全退出并重进，确认新家具仍在目标房间，旧家具已回仓库；再次分析不应
  重复提出相同的已完成替换。

## Safety boundary

- 不自动启动、进入或控制游戏；实机验证由玩家操作。
- 不自动移动猫、休息、结束一天、出征、组队、淘汰或移动受保护对象。
- 不修改原游戏文件和活动存档；不删除家具存档记录。
- 第五房间未解锁问题已由玩家移出范围，不重新纳入本阶段。

# CODEX CURRENT TASK - STAGE 45 AUTOMATIC WAREHOUSE ATTRIBUTE REPLACEMENT

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

## Player validation gate

- 使用可恢复测试槽完全重启后进入家具模式，只点击一次“开始分析”；确认日志出现
  `AC14319`、`AC3201`、`AC3901` 且没有 `AC3205`，面板显示属性替换数和布局移动数。
- 点击“自动放置”，确认家具逐件替换/移动且没有崩溃；日志应出现 `AC3912`，最终
  成功出现 `AC3904`，失败回滚出现 `AC3907`。
- 保存、完全退出并重进，确认新家具仍在目标房间，旧家具已回仓库；再次分析不应
  重复提出相同的已完成替换。

## Safety boundary

- 不自动启动、进入或控制游戏；实机验证由玩家操作。
- 不自动移动猫、休息、结束一天、出征、组队、淘汰或移动受保护对象。
- 不修改原游戏文件和活动存档；不删除家具存档记录。
- 第五房间未解锁问题已由玩家移出范围，不重新纳入本阶段。

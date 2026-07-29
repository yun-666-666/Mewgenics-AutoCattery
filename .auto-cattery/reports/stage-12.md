阶段：12
状态：EligibilityAndLargeSaveValidationRequired；死亡过滤/大存档动态映射待实机验收

实际能力：
- 玩家点击 House 的 `Mark Combat Cats` 后才读取只读快照、验证当前
  scene generation、映射 CatId 并运行 Stage 6 独立猫评分。
- 2026-07-29 实机 `AC12105` 证明 8 个只读 CatId 与 8 个 rooted
  HouseCat 完整稳定双射：offset=128、width=8、matched=roots=8、
  consistent=stable_bijection=1。
- 玩家确认原纯文字列表在猫多时无法有效定位，不满足 Stage 12 的明显
  标记验收。
- 2026-07-29 第一版 8 按钮实机验收失败：8 个克隆按钮均带完整绳子；
  后绘制按钮的绳子/命中区域覆盖前面的按钮，清除后仍显示 8 个空牌，
  列表还会遮挡原生“出发”按钮。
- 2026-07-29 第二版 4 按钮实机验收仍失败：Mark 前四个 `Clean Up!`
  牌子已经显示并播放放大/缩小动画；Clear 只移除文字却不隐藏牌子；
  四行点击与 hover-gated 滚轮均没有可靠到达 MOD。
- 2026-07-29 第三版实机证明滚轮和行点击已到达 MOD；日志记录了多次
  正确 rank。但四行因 `goto-and-play` 在两帧间循环而持续闪烁，Mark 前
  即出现，Clear 后也不能保持隐藏。
- 同次 `AC12109` 显示 `signature=scene=house=cat=1 opened=0`：点击和
  目标猫验证成功，失败发生在原生详情调用。重新反汇编调用点确认第一
  参数是 `HouseDrawerUI`，不是旧适配器传入的 `House`。
- 第四版实机确认四行只在 Mark 后、Clear 前静态显示，停帧与显隐问题
  已解决。新日志每次点击均正确记录 rank，且
  `signature=scene=drawer=cat=1`，但仍为 `opened=0`。
- 继续沿原生调用点回溯确认：详情函数的第二参数也不是 HouseCat 组件；
  游戏先用 HouseCat 调用 RVA `0xEFCB0`，再把返回的内部详情目标传入。
  第五版复用相同转换，并新增转换函数/调用点签名和结果类型验证。
- 最新实机日志中第 1～8 名均正确到达，且每次都是
  `signature=scene=drawer=cat=target=1 opened=0`。因此按钮命中、rank、
  HouseCat 和第二参数已经排除，异常只在最终原生详情调用。
- 重新逐条复刻原生路径确认剩余差异：游戏对唯一
  `HouseCatClickManager` 调用 RVA `0x1A93F0`，把返回值作为最终函数第一
  参数；第五版仅枚举 scene 中同类型 `HouseDrawerUI`，并未证明对象身份。
  第六版改为调用精确 getter。
- 第六版实机日志稳定显示 `manager=1 drawer=0 failure=0`；同轮
  `AC12103` 又明确显示 `HouseDrawerUI components=1`。getter 没有异常，
  是后续把它返回的原生接口/子对象指针当作 scene 组件基址调用类型虚
  函数，导致错误拦截并提前返回，`cat/target` 根本未执行。
- 当前第七版保留唯一 scene 组件证据，但不再对接口指针调用
  `GetObjectTypeSTR`；改为验证签名所对应详情函数实际读取的
  `drawer+0x38` 与 `drawer+0x60` 均有效，再使用游戏 getter 原值调用。
- 最新跨存档日志证明“不显示”不是一次评分耗时过长：generation 4 的
  request 2～9 均立即返回 `snapshot_valid=0`；generation 6 的 request
  10～12 也立即失败，直到 request 13 时磁盘最新 `.sav` 才与当前 8 个
  HouseCat 一致并成功显示。旧实现把“最近修改存档”等同于“当前存档”。
- 最新另一个存档运行时与 `house_state` 均有 25 个当前 HouseCat。
  `house_state` 中普通房间、`AdventureBox` 和空 room_id 的 CatId 现在
  全部解析；空 room_id 只取消房间归属，不再删除这只当前猫。只存在于
  `cats` 表、不在当前 `house_state` 的历史猫才排除。
- 该存档 25 只当前猫中有 16 只 `Colorless`、9 只已有战斗 class；但
  第一个被推荐的 `Colorless` 猫有持久化 death day。修复后真实结果为
  15 只可用、1 只 dead、9 只已有 class，评分器明确排除后两类。
- 主存档的 `house_state` 有 74 只当前猫。只读 probe 复现两项独立失败：
  cat blob 在变长 `dex` descriptor 后错位，以及 HouseCat probe 固定容量
  64 导致运行时直接得到 0 个匹配。当前 reader 按长度前缀解析并读取
  birth/death day；组件和映射存储按当前数量动态分配。
- 23:54:33 离开 House 时，chainloader 在 `AC4101` 同一毫秒记录大量旧
  UI 地址访问异常。两个 view 在 scene 已卸载后仍触碰旧按钮/MovieClip，
  且重进 House 可能复用句柄，解释了闪烁、Clear 失效和上方按钮无反馈。
- 同次截图证明文字框偏左且宽于木牌。当前保持已验证的相对居中变换，
  但把四项改为两列两行：纸牌为 `(1025/1160, 175/235, 0.42)`，文字
  为 `(986/1121, 187/247, 0.25)`，顺序是左上、右上、左下、右下。
- 最终版不再创建推荐 Button 组件，只显示 4 个紧凑行：
  `排名 猫名 分数 ?`，排名前不再显示 `#`/星号形符号。
- 每行是私有三帧 SWF 白纸和独立文字：frame 0 为空，frame 1 是正常
  白纸，frame 2 是原生按钮 down 状态尺寸的按压白纸。生成器从固定 MIT
  示例纹理中遮罩出白纸并放大到原木牌边界；木板、垃圾桶、原 label、
  完整绳子 placement 和自动播放时间轴均未复制。
- `WM_LBUTTONDOWN` 立即切换并停在按压帧；`WM_LBUTTONUP` 后保持约
  90ms，再恢复正常帧并调用已经实机验证的详情回调。消息仍继续传给
  游戏，未改动 CatId→HouseCat→原生详情适配链。
- Mark 前和 Clear 后显式清空文字并切回 frame 0，因此不会出现默认
  `Clean Up!`、空牌或游戏按钮动画。显隐不再调用纯
  `goto-and-play`：按当前 EXE 原生停帧序列，在跳帧后立即清除
  MovieClip `+0x09` 的 `0x02` 播放位。
- 猫名仅作显示；行索引最终换算为稳定 CatId 对应的 HouseCat 组件。
- 点击某个猫名，打开游戏原生的对应猫详情抽屉，并显示原生绿色焦点
  轮廓，效果与玩家直接点击 House 中该猫一致。
- `?` 表示当前 reader 尚不能确认其余 life-stage 阈值和受伤；出战资格由
  持久化 death day 和 class 状态筛选。
- 主按钮第二次点击清除推荐按钮；退出 House 或 generation 变化也清除。
- 第一次点击现在会锁定为一个 pending 请求，每秒只读重试一次、最多
  30 秒；重复点击不会再创建并发请求。只有当前 generation 的快照、
  CatId 映射和 root 仍有效才显示。
- 一次请求会只读活动 Steam profile 的全部存档候选，再用当前 rooted
  HouseCat 的完整稳定 CatId 双射选择真实活动存档；8/25/74 猫的快照
  不能互相冒充，也不依赖哪个 `.sav` 最后修改。
- 推荐数量改为全部明确可出战猫，不再限制为 8。界面仍以四个物理行作为
  可滚动窗口；控制器测试以 12 条结果验证第 12 条可点击且第 13 条越界。
- 四行标签直接写入 UTF-8 文本，不再通过本地化数值占位符格式化空串，
  因此隐藏/Clear 不会残留 `0` 或 `.`。两个上方 MOD 按钮按 role 复用并
  刷新回调，家具界面进出不再累计重复 Button 组件。
- 离场时先核对保存的 scene 仍是当前 ready House；已卸载时不再调用旧
  UI 指针，只解除 hook 并丢弃句柄。新 generation 强制重新解析两个
  MOD 按钮与四个 MovieClip，并立即清空文字、停在隐藏帧。
- 输入观察使用 House UI 线程的 `WH_GETMESSAGE` hook，按当前窗口尺寸把
  鼠标映射到四个可见行的实际矩形；`WM_LBUTTONUP` 产生主动详情点击，
  `WM_MOUSEWHEEL` 逐项滚动。始终继续调用下一个 hook，不吞掉或改写
  游戏输入消息，也不再依赖失效的推荐 Button hover/click。

原生详情适配证据与安全：
- 当前 EXE 本地反汇编确认 HouseCatClickManager 的玩家点击路径先对
  click manager 调用 `0x1A93F0` 取得实际 `HouseDrawerUI`，再把 HouseCat
  转为内部详情目标，最后向详情函数传入该 drawer、内部详情目标和
  `show_drawer=1`。
- build-specific adapter 调用前验证：
  1. 当前 House scene ready 且未销毁；
  2. scene generation 与生成推荐时一致；
  3. 当前 EXE drawer getter/getter 调用点、转换函数/转换调用点、详情
     函数/详情调用点六段指令签名一致；
  4. scene 中 `HouseCatClickManager` 与 `HouseDrawerUI` 均唯一，getter
     返回接口指针的 `+0x38/+0x60` 必需字段有效；
  5. 目标组件仍归属该 scene 且类型仍为 HouseCat；
  6. 转换结果非空且能读取有效组件类型。
- 任一验证失败均不调用。`AC12109` 额外记录 manager、scene drawer
  存在/同一性，以及 SEH 失败阶段、异常码和模块内 RVA，不记录指针。
- 该玩家主动调用只改变 House 当前详情焦点；不调用冒险盒、出征队伍、
  确认、休息、日期推进、导航或存档写入接口。
- 不记录猫名、CatId、组件指针、存档名或个人路径。
- 未修改游戏原始文件、猫数据或玩家存档。

本次修改文件：
- `CMakeLists.txt`
- `CODEX_TASK.md`
- `docs/implementation-status.md`
- `.auto-cattery/state.json`
- `.auto-cattery/reports/stage-12.md`
- `src/snapshot/cat_blob_parser.cpp`
- `src/ui/mew_ui_bridge.cpp`
- `src/ui/mew_ui_house_cat_probe.c`
- `src/ui/mew_ui_house_cat_probe.h`
- `tests/cat_blob_parser_tests.cpp`
- `tests/mew_ui_house_cat_probe_tests.cpp`
- `tests/snapshot_probe.cpp`
- `tests/test_main.cpp`

验证：
- SWF 重新生成及结构检查：通过；包含且仅包含
  `recommend_row_1` 至 `recommend_row_4` 四个静态牌节点，以及
  `recommend_text_1` 至 `recommend_text_4` 四个独立文字节点。
- 新增私有白纸 bitmap character 147（330×140），透明区排除木板；
  character 148/149 分别是正常/按压白纸形状。
- 四行共同引用私有 character 150：frame 0 没有 placement；frame 1
  仅含 `(depth=5, character=148)`，frame 2 仅含
  `(depth=5, character=149)`；没有垃圾桶、rope depth 1、原始 label
  或自动播放时间轴。
- 四个文字节点共同引用私有 character 151，初始 HTML 为空，不会在
  attach 前闪现 source `Test`。
- Debug build：通过。
- Debug `phase12_unit_tests`：通过（4.15 秒）。
- Debug `phase12_dll_load_smoke`：通过（0.06 秒）。
- Release build：通过。
- Release `phase12_unit_tests`：通过（0.47 秒）。
- Release `phase12_dll_load_smoke`：通过（0.05 秒）。
- SWF 两列两行生成检查：通过；四个纸牌/文字 placement 与四个独立
  命中矩形按左上、右上、左下、右下对应。
- 控制器测试覆盖 stale generation、两秒状态、12 条数据、有效/
  越界项点击、详情回调 generation/rank、view poll、清除，以及 pending
  期间重复点击不会启动第二个请求。
- 主存档只读 `snapshot_probe`：`house_cats=74`、`assigned=74`、
  `combat_available=70`、`combat_spent=4`、`dead=0`、`stable_ids=1`、
  `stable_ranking=1`，未写入存档。
- 25 猫存档只读 `snapshot_probe`：`house_cats=25`、`assigned=25`、
  `combat_available=15`、`combat_spent=10`、`dead=1`、`stable_ids=1`、
  `stable_ranking=1`，未写入存档。
- 当前 8 猫存档只读 `snapshot_probe`：`house_cats=8`、`adventure=1`、
  `combat_available=8`、`combat_spent=0`、`dead=0`、`stable_ids=1`、
  `stable_ranking=1`，证明出战箱中的猫仍被解析。
- Release DLL 已部署：
  `D:\steam\steam\steamapps\common\Mewgenics\Mods\AutoCattery.dll`
- Mewtator UI 数据 MOD 已部署并启用：
  `D:\steam\steam\steamapps\common\Mewgenics\Mewtator\mods\AutoCattery`
- 构建与安装 DLL 均为 751616 bytes，SHA-256 均为
  `F570383303F3640E498E8FF1A923CA046D8810C4714DCD88F8F754681BA051F3`。
- 源与安装 SWF 均为 751211 bytes，SHA-256 均为
  `308D1185DAA03BA929E9156733AD6A50E823F7BE7C08EFCEAFD52753A90894AD`。
- 用户明确要求需要时允许联网；搜索了公开 Mewgenics/MOD 信息，但未
  找到可直接采用的 CatId→详情原始实现。实际接口证据来自当前本地 EXE
  与实机组件/日志。

玩家验收状态：
1. 玩家确认推荐行已经可以点击，并打开对应猫的原生详情界面。
2. 玩家此前已确认四行只在 Mark 后、Clear 前静态显示。
3. 玩家指定最终视觉收尾：增加点击动画、只显示放大白纸、移除排名前
   符号；此前实现已通过自动化/结构检查。
4. 最新跨存档验收继续暴露死猫误选、主存档无法映射、四个 `0`/空牌和
   家具界面触发重复组件；当前修复仍需玩家重启游戏后依次测试三档。

剩余事项：
- 验证 8 猫存档一次 Mark 能显示且出战箱猫仍被解析；25 猫存档显示
  全部 15 只可出战猫并排除 1 只死猫和 9 只已有 class 的猫；74 猫
  主存档能完整映射并显示全部 70 只可用猫。房间外猫仍在列表。
- 反复进出家具界面、Clear 和切换存档后不闪烁、不残留 `0`/`.`/空牌，
  两个上方按钮持续可点击且 Button role 计数不再增长。
- 验证两列两行与上方按钮、下方“出发”均有安全间距，四项点击对应
  正确猫详情。通过前 Stage 12 不恢复 Completed，Stage 13 继续 blocked。
- Stage 13～16 未实施。

本地 commit：本次实现提交见最终回复（报告与代码同一提交）
是否 push：否

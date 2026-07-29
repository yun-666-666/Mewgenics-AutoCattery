阶段：12
状态：Completed；玩家已确认推荐行可打开对应猫的原生详情

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
- 同次截图证明文字框偏左且宽于木牌。当前按 SWF 实际 bounds 计算：
  木牌/文字框中心为 1124.35/1124.16，宽为 114.89/107.33；text field
  使用 `(1071, 152 + row*42, scale=0.25)`，保持 HTML 居中对齐。
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
- `?` 表示当前 reader 尚不能确认年龄、受伤和出战资格。
- 主按钮第二次点击清除推荐按钮；退出 House 或 generation 变化也清除。
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
- `CODEX_TASK.md`
- `docs/implementation-status.md`
- `.auto-cattery/state.json`
- `.auto-cattery/reports/stage-12.md`
- `assets/swfs/auto_cattery_house.swf`
- `tools/build_house_ui_asset.py`
- `src/ui/mew_ui_bridge.cpp`
- `src/ui/mew_ui_recommendation_marker_view.cpp/.hpp`
- `tests/recommendation_marker_controller_tests.cpp`

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
- Debug `phase12_unit_tests`：通过（5.04 秒）。
- Debug `phase12_dll_load_smoke`：通过（0.09 秒）。
- Release build：通过。
- Release `phase12_unit_tests`：通过（0.46 秒）。
- Release `phase12_dll_load_smoke`：通过（0.05 秒）。
- SWF 几何检查：通过；木牌/文字框中心差 0.19，文字框宽小于木牌宽。
- 控制器测试覆盖 stale generation、两秒状态、最多 8 条数据、有效/
  越界项点击、详情回调 generation/rank、view poll、清除和不重复评分。
- Release DLL 已部署：
  `D:\steam\steam\steamapps\common\Mewgenics\Mods\AutoCattery.dll`
- Mewtator UI 数据 MOD 已部署并启用：
  `D:\steam\steam\steamapps\common\Mewgenics\Mewtator\mods\AutoCattery`
- 构建与安装 DLL 均为 726016 bytes，SHA-256 均为
  `906472F42D5E56B87734627DC17A10C8954F5437494C1B62ED25DFEB9A04DEE9`。
- 源与安装 SWF 均为 751211 bytes，SHA-256 均为
  `6AF896C5E91EB2DAFFD92644F4F57BB8A1460D0731013A7914EFE17D47439EF5`。
- 用户明确要求需要时允许联网；搜索了公开 Mewgenics/MOD 信息，但未
  找到可直接采用的 CatId→详情原始实现。实际接口证据来自当前本地 EXE
  与实机组件/日志。

玩家最终验收：
1. 玩家确认推荐行已经可以点击，并打开对应猫的原生详情界面。
2. 玩家此前已确认四行只在 Mark 后、Clear 前静态显示。
3. 玩家指定最终视觉收尾：增加点击动画、只显示放大白纸、移除排名前
   符号；本提交已按该范围完成并通过自动化/结构检查。

剩余事项：
- Stage 12 无剩余实现项。
- Stage 13～16 未实施。

本地 commit：本次实现提交见最终回复（报告与代码同一提交）
是否 push：否

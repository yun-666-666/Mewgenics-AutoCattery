阶段：12
状态：ScrollableDetailsValidationRequired；四行滚动详情列表待实机验收

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
- 修复版只显示 4 个无绳子的复用按钮：`#排名 猫名 分数 ?`。鼠标停在
  任一推荐行上滚轮，可逐项查看最多 8 条推荐。
- 推荐行使用私有 SWF 按钮角色：绳子层已删除，disabled 帧无画面并
  停止播放。未点击 Mark 及 Clear 后不会留下空牌。
- 猫名仅作显示；按钮保存的是稳定 CatId 对应的 HouseCat 组件。
- 点击某个猫名，打开游戏原生的对应猫详情抽屉，并显示原生绿色焦点
  轮廓，效果与玩家直接点击 House 中该猫一致。
- `?` 表示当前 reader 尚不能确认年龄、受伤和出战资格。
- 主按钮第二次点击清除推荐按钮；退出 House 或 generation 变化也清除。
- 滚轮观察使用 House UI 线程的 `WH_GETMESSAGE` hook，只在推荐列表可见
  且鼠标悬停某一行时累计 `WM_MOUSEWHEEL`，始终继续调用下一个 hook，
  不吞掉或改写游戏输入消息。

原生详情适配证据与安全：
- 当前 EXE 本地 RTTI/反汇编确认 HouseCatClickManager 的玩家点击路径
  向 House 详情函数传入 House 组件、HouseCat 组件和 `show_drawer=1`。
- build-specific adapter 调用前验证：
  1. 当前 House scene ready 且未销毁；
  2. scene generation 与生成推荐时一致；
  3. 当前 EXE 详情函数及原生调用点两段指令签名一致；
  4. scene 中 House 组件唯一；
  5. 目标组件仍归属该 scene 且类型仍为 HouseCat。
- 任一验证失败均不调用，并记录匿名 `AC12109` 技术结果。
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
- `assets/data/text/combined.csv.append`
- `assets/localization/strings.json`
- `assets/swfs/auto_cattery_house.swf`
- `include/auto_cattery/ui/mew_ui_bridge.hpp`
- `include/auto_cattery/ui/recommendation_marker_controller.hpp`
- `src/ui/mew_ui_bridge.cpp`
- `src/ui/mew_ui_house_detail_adapter.c/.h`
- `src/ui/mew_ui_recommendation_marker_view.cpp/.hpp`
- `src/ui/recommendation_marker_controller.cpp`
- `tests/recommendation_marker_controller_tests.cpp`
- `tools/build_house_ui_asset.py`

验证：
- SWF 重新生成及结构检查：通过；包含且仅包含
  `recommend_cat_1` 至 `recommend_cat_4` 四个推荐项节点。
- 四行共同引用私有 character 147；其 frame 0 不含 rope depth 1，
  disabled frame 59 无任何 placement，并包含停止动作。
- Debug build：通过。
- Debug `phase12_unit_tests`：通过（5.52 秒）。
- Debug `phase12_dll_load_smoke`：通过（0.08 秒）。
- Release build：通过。
- Release `phase12_unit_tests`：通过（0.51 秒）。
- Release `phase12_dll_load_smoke`：通过（0.02 秒）。
- 控制器测试覆盖 stale generation、两秒状态、最多 8 条数据、有效/
  越界项点击、详情回调 generation/rank、view poll、清除和不重复评分。
- Release DLL 已部署：
  `D:\steam\steam\steamapps\common\Mewgenics\Mods\AutoCattery.dll`
- Mewtator UI 数据 MOD 已部署并启用：
  `D:\steam\steam\steamapps\common\Mewgenics\Mewtator\mods\AutoCattery`
- 构建与安装 DLL 大小均为 723456 bytes，SHA-256 均为
  `32EE6F20C63FB8FBEF1C4EFC540DE0E2ED5E3243C2CC083FF38DF5765192DB9C`。
- 源与安装 SWF 大小均为 725652 bytes，SHA-256 均为
  `E01A7001D150D978E43F30FECCA3C1CC134FDAFFEA4C602801DB59733CED9910`。
- 用户明确要求需要时允许联网；搜索了公开 Mewgenics/MOD 信息，但未
  找到可直接采用的 CatId→详情原始实现。实际接口证据来自当前本地 EXE
  与实机组件/日志。

玩家最终验收：
1. 通过 Mewtator 启动游戏，进入 House，点击 `Mark Combat Cats`。
2. Mark 前确认没有空推荐牌；Mark 后确认只出现 4 个无绳子木牌。
3. 将鼠标停在任一推荐行上向下滚轮，确认可看到第 5～8 名；向上滚轮
   可回到第 1～4 名，且列表不遮挡或误触原生“出发”。
4. 点击例如第 3 名，确认左侧详情抽屉显示的名字与第 3 名完全一致，
   House 中同一只猫出现原生绿色焦点轮廓。
5. 滚动后点击例如第 6 名，确认打开的是当前第 6 名而非物理第 2 行。
6. 确认点击名字没有把猫放入/移出冒险盒，也没有改变出征队伍。
7. 点击 `Clear Recommendations`，确认 4 个按钮及文字完全消失；Mark
   主按钮保留；离开再进入
   House 后也无残留。

剩余事项：
- 玩家可见四行/滚轮/点击详情验收尚未返回；通过前 Stage 12 不标记 completed，
  Stage 13 继续 blocked。
- 若布局遮挡、按钮文字溢出、详情猫不匹配或 `AC12109 opened=0`，根据
  实机截图和日志做最小调整。

本地 commit：本次实现提交见最终回复（报告与代码同一提交）
是否 push：否

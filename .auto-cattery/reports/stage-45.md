# Stage 45 - 自动仓库属性替换与自动放置

日期：2026-08-12
版本：v0.5.38

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
- `tools\deploy.ps1 -GameRoot D:\steam\steam\steamapps\common\Mewgenics
  -Configuration Release`：DLL-only 部署成功。
- `tools\verify_install.ps1 -GameRoot D:\steam\steam\steamapps\common\Mewgenics`：
  通过；14 个职业重投配置均保持用户设置 20。
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

v0.5.38 已编译并部署测试 DLL，等待玩家实机复测。完全退出游戏后重新通过 Mewtator
启动，进入同一
测试存档和家具模式，点击一次“开始分析”，再点击一次“自动放置”。第一项成功日志
`AC3912` 应包含 `support=5/5->5`，表示五件依赖家具全部解除占用并以原 component
原位重新提交；批次成功最终出现 `AC3904`。若失败，`AC3907` 必须显示完整回滚。成功后保存、
完全退出并重进，确认新底座和上层链仍在房间、旧底座已回家具栏。

## 风险与未做范围

- 当前原生 RVA、type/vtable 和对象布局只适用于已 gate 的当前
  `Mewgenics.exe`；签名不匹配时安全拒绝。
- 自动化测试无法代替真实游戏的仓库数量、画面、保存和重进持久化验证。
- 未实现跨不同 item 的全局仓库分配、旋转、Anchor、墙面/天花板布局或跨会话撤销。
- 未自动移动猫、休息、结束一天、出征、组队或淘汰；第五房是否解锁仍不在本阶段
  范围，但当前 build 的第五房运行时身份一致性已在本次修复。

本地提交：本报告与实现位于同一本地任务提交，最终 hash 见交付回复。
是否 push：否

# Stage 45 - 自动仓库属性替换与自动放置

日期：2026-08-11
版本：v0.5.34

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

## 当前 build 证据

- `0x1ABFF0`：五个真实调用点确认参数为 House scene manager、scene context、
  stable key 指针；内部创建并初始化当前 `FurniturePiece`。
- `0x2EE3D0`：移除家具 grid 占用。
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

## 文件

- 原生适配器：`src/ui/mew_ui_furniture_move_adapter.c`、
  `src/ui/mew_ui_furniture_move_adapter.h`。
- Gateway：`src/ui/furniture_placement_gateway.cpp`、
  `src/ui/furniture_placement_gateway.hpp`。
- 分析与执行：`src/furniture_analysis/service.cpp`、`src/ui/mew_ui_bridge.cpp`、
  `include/auto_cattery/ui/mew_ui_bridge.hpp`。
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
- `tools\deploy.ps1 -GameRoot D:\steam\steam\steamapps\common\Mewgenics
  -Configuration Release`：DLL-only 部署成功。
- `tools\verify_install.ps1 -GameRoot D:\steam\steam\steamapps\common\Mewgenics`：
  通过；14 个职业重投配置均保持用户设置 20。
- v0.5.34 build、`dist\Release\AutoCattery.dll` 与实际安装的
  `Mewgenics\mods\AutoCattery.dll` SHA-256 一致：
  `A04F0B626D2618141659D65CBBD80F1D920B6A705892AC9235D98FBAD9AC5CE5`。
- 玩家首次启动暴露 BOM GON 错误后，已在同一 Windows PowerShell 5 后台环境重新
  执行修正后的部署与 `verify_install`：成功；AutoCattery 和
  SkillsPassivesFirstData 的四份职业 GON、`modlist.txt` 均确认 `BOM=False`，四份 GON
  各自花括号计数相等，14 个职业重投保持 20；DLL hash 未改变。

## 玩家验证状态

v0.5.33 玩家实测已确认仍显示“分析不可用”，不能算实机验收通过。v0.5.34 第一次
启动又暴露部署编码回归，现已修正并重新部署。玩家下一步先确认游戏能正常进入；进入
同一存档后只点击一次“开始分析”。本轮分析门必须同时满足：`AC14315` 家具数大于 0、
出现 `AC14319`、`AC3201` 和 `AC3901`，且没有 `AC3205`。只有该门通过后才允许点击
“自动放置”，观察 `AC3912` 和最终 `AC3904`，或失败时的 `AC3907`；随后保存、退出
并重进，确认新家具保持在房间、旧家具回到仓库，再次分析不重复提出相同替换。

## 风险与未做范围

- 当前原生 RVA、type/vtable 和对象布局只适用于已 gate 的当前
  `Mewgenics.exe`；签名不匹配时安全拒绝。
- 自动化测试无法代替真实游戏的仓库数量、画面、保存和重进持久化验证。
- 未实现跨不同 item 的全局仓库分配、旋转、Anchor、墙面/天花板布局或跨会话撤销。
- 未自动移动猫、休息、结束一天、出征、组队或淘汰；第五房是否解锁仍不在本阶段
  范围，但当前 build 的第五房运行时身份一致性已在本次修复。

本地提交：本报告与实现位于同一本地任务提交，最终 hash 见交付回复。
是否 push：否

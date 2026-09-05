# Stage 35：BETA 自动整理预览与战斗猫定位兼容修复

更新日期：2026-09-05

状态：实现、最终自动验证、本机部署、玩家实机验证及 v0.5.29 GitHub 发布均已完成。

## 问题与结论

- 玩家使用 v0.5.28 已能进入当前 BETA House，并看到两个普通按钮和 F10 管理面板。
- 同次运行记录 41 只猫，但每次自动整理点击后均为
  `native room components=0, available rooms=2`，只有 `AC3102`、`AC14315`、
  `AC14314`，没有 `AC11100`；F10 完整预览因此显示暂无预览。
- 当前 `AcMewReadHouseCatCurrentRoom()` 仍要求稳定版 HouseCat 固定 vtable RVA
  `0xEF4F58`。当前 BETA 的 41/41 猫身份和 `HouseCat` 类型已经由同次日志证明可
  稳定映射，故固定 vtable 校验是房间指针全部丢失的直接原因。
- 战斗推荐本身成功：`matched=41/41`、`marked=10`。点击条目均记录
  `AC12109 signature=0 scene=1 manager=0 ... opened=0`，说明详情适配器在搜索当前
  `HouseCatClickManager` 前就因稳定版固定 RVA 签名失败。

## 实现

- HouseCat 验证改为已有运行证据支持的引擎类型名 `HouseCat`，保留对象可读边界和
  SEH 安全返回，不再绑定稳定版 vtable 地址。
- 原生 room bucket 枚举不可用时，按当前猫的非空唯一房间指针推导 2–4 房的安全
  可用数量；现有映射器继续要求快照房间与运行时房间一一对应，缺失或歧义仍失败。
- 原生搬猫保留稳定版 `0x2E7DB0`，新增当前 BETA `0x2E88D0`；候选机器码不匹配、
  多匹配或猫/目标房间无效时均不调用。
- 战斗猫详情保留稳定版布局，新增当前 BETA 完整布局：详情打开 `0xEC7B0`、详情
  目标 `0xF0570`、抽屉解析 `0x1A9E10`，并同时验证三个对应调用点的前后机器码和
  相对调用目标。只有恰好一个完整布局匹配时才执行。
- 规划、评分、保护、淘汰、自动组队、结束一天和其他 MOD 均未修改。

## 修改文件

- `src/ui/mew_ui_house_move_invoke.c`
- `src/ui/mew_ui_house_move_adapter.h`
- `src/ui/mew_ui_house_detail_adapter.c`
- `src/ui/mew_ui_house_detail_adapter.h`
- `src/ui/runtime_house_state_capture.cpp`
- `src/ui/runtime_house_state_capture.hpp`
- `tests/mew_ui_house_move_probe_tests.cpp`
- `tests/mew_ui_house_detail_adapter_tests.cpp`
- `tests/runtime_house_state_tests.cpp`
- `tests/mew_ui_runtime_locator_tests.py`
- `tests/test_main.cpp`
- `CMakeLists.txt`
- `assets/description.json`
- `CHANGELOG.md`
- `docs/RELEASE_NOTES_v0.5.29.md`
- `CODEX_TASK.md`
- `docs/implementation-status.md`
- `.auto-cattery/state.json`
- `.auto-cattery/reports/stage-35.md`

## 构建与检查

- 当前 BETA 离线定位测试：PASS；30 个 MewUI 地址唯一解析，并确认 component
  bucket prepare `0x963030`、详情打开 `0xEC7B0`、详情目标 `0xF0570`、抽屉解析
  `0x1A9E10`、原生搬猫 `0x2E88D0`。
- 首轮 Debug 构建成功；CTest 5/5 与 DLL load smoke 通过。
- 首轮 Release 构建成功；CTest 5/5 与 DLL load smoke 通过。
- 单元测试覆盖稳定版/BETA 详情布局、稳定版/BETA 原生搬猫、签名拒绝和三占用房间
  回退计数。
- code-simplifier 仅将占用房间集合改用 `RuntimePointer` 并移除恒真的签名条件，未
  改变接口或行为。
- 最终 v0.5.29 Debug 构建成功；CTest 5/5 与 DLL load smoke 通过。
- 最终 v0.5.29 Release 构建成功；CTest 5/5 与 DLL load smoke 通过。

## 部署

- 部署前确认没有 `Mewgenics` 进程；没有启动或控制游戏。
- 执行 `./tools/deploy.ps1 -GameRoot
  D:/steam/steam/steamapps/common/Mewgenics -Configuration Release`，退出码 0，
  v0.5.29 DLL 与 Mewtator 数据 MOD 已部署。
- `user_config.json`、`protection.json`、`AutoCatteryData`、
  CombineDuplicateFurniture、SkillsPassivesFirst 均由部署脚本保留；AutoCattery 仍在
  Mewtator 加载列表末尾。

## 玩家验证状态

- 玩家已确认 BETA 与正式版游戏均通过测试。
- 第一次点击“自动整理猫舍”可以生成 F10 完整预览，不再显示预览不可用。
- 第二次点击可以执行实际搬猫。
- 点击战斗猫推荐名称可以打开并定位到对应猫的原生详情。
- Stage 35 的玩家实测接受边界已经满足。

## 风险与省略的后续工作

- 当前 BETA 的原生 room bucket 仍返回 0；本阶段只使用已有猫的真实房间指针完成
  当前占用房间映射。若未来出现已解锁但完全空置、且存档无法提供唯一映射的房间，
  现有一一映射门禁会安全拒绝，而不会猜测目标指针。
- 本阶段不启用真实淘汰，不自动组队、不结束一天，不修改家具 MOD 或技能 MOD。
- v0.5.29 已发布；本阶段不启用真实淘汰，不扩大到后续功能。

## GitHub 发布

- Windows x64 包含 21 个文件、恰好一个 `AutoCattery.dll`、不包含
  `AutoCatteryFurniture`，并包含 v0.5.29 发布说明；DLL 嵌入版本为 `0.5.29`。
- `main`、注释标签 `v0.5.29` 与 Windows x64 ZIP 均已推送。
- 注释标签解析到玩家验收提交
  `99ddb257c9f06b03c80309ae15f69550033d0ba3`。
- GitHub API 已确认 Release 公开、非草稿、非预发布并为 Latest：
  `https://github.com/yun-666-666/Mewgenics-AutoCattery/releases/tag/v0.5.29`。
- 发布资产：
  `AutoCattery-v0.5.29-Windows-x64.zip`，状态 `uploaded`。

## Git

- Stage 35 实现提交：`9ddb5533d3bea481f5a2d59de239efbf14eb5e2c`。
- 玩家验收与发布标签提交：`99ddb257c9f06b03c80309ae15f69550033d0ba3`。
- 是否 push：是。

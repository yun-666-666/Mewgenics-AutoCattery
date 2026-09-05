# Stage 35：BETA 自动整理预览与战斗猫定位兼容修复

更新日期：2026-09-05

状态：实现、最终自动验证和本机部署完成；玩家实机验证待完成。

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

玩家需在同一 41 猫存档验证：

1. 第一次点击“自动整理猫舍”，F10“完整预览”应出现房间汇总和计划移动猫，不再
   显示“暂无预览”。
2. 关闭 F10，第二次点击“自动整理猫舍”，应实际移动猫；若计划本来已满足，应明确
   显示 0 次移动而不是预览不可用。
3. 点击“标记战斗猫”，再点击任意推荐猫名，应打开/定位到对应猫的原生详情。
4. 提供完整预览截图、成功打开猫详情截图，以及同次
   `Mods/AutoCattery/logs/auto_cattery.log`。

自动构建和离线入口解析不能证明当前 BETA 中真实房间指针、移动提交或详情抽屉调用
最终成功，玩家实测仍是接受边界。

## 风险与省略的后续工作

- 当前 BETA 的原生 room bucket 仍返回 0；本阶段只使用已有猫的真实房间指针完成
  当前占用房间映射。若未来出现已解锁但完全空置、且存档无法提供唯一映射的房间，
  现有一一映射门禁会安全拒绝，而不会猜测目标指针。
- 本阶段不启用真实淘汰，不自动组队、不结束一天，不修改家具 MOD 或技能 MOD。
- 本阶段不发布、不 push；公开版本仍为 v0.5.24。

## Git

- Stage 35 顶层本地提交：本报告所在提交；精确哈希在最终交付中记录。
- 是否 push：否。

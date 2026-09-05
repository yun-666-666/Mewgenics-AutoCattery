# CODEX CURRENT STATUS

更新日期：2026-09-05

Stage 35 的 v0.5.29 BETA 房间指针、原生搬猫和战斗猫详情入口兼容修复已经完成。
玩家已确认 BETA 与正式版游戏中的自动整理预览、第二次点击实际搬猫，以及点击战斗
猫名称打开对应猫详情均通过。v0.5.29 已推送并发布为公开 GitHub Latest Release；
注释标签指向玩家验收提交 `99ddb257c9f06b03c80309ae15f69550033d0ba3`。

## Required reading

1. `AGENTS.md`
2. `README.md`
3. `docs/implementation-status.md`
4. `docs/pre-completion-functional-roadmap.md`
5. `.auto-cattery/state.json`
6. 当前代码、测试、最新运行日志和 `git status --short`

## Stage 35 scope

- v0.5.28 同次运行日志记录 `cats=41, native room components=0, available rooms=2`；
  自动整理点击后只出现 `AC3102`、`AC14315`、`AC14314`，没有 `AC11100`，证明
  预览在运行时房间映射阶段失败。
- `AcMewReadHouseCatCurrentRoom()` 仍以稳定版固定 vtable RVA `0xEF4F58` 验证
  `HouseCat`。当前 BETA 中 41/41 猫身份与类型均已可靠映射，但固定 vtable 不匹配，
  因而所有当前房间指针被错误清空。
- 战斗猫列表成功生成，日志为 `matched=41`、`marked=10`；每次点击均记录
  `AC12109 signature=0`，证明详情适配器仍使用稳定版固定 RVA 并在 BETA 安全拒绝。
- 修复必须保留稳定版入口，增加当前 BETA 的详情打开/目标/抽屉解析、原生搬猫入口，
  所有布局都需要机器码与相对调用目标一致；未知或歧义布局安全返回。

## Required verification

- 单元测试覆盖稳定版/BETA 详情布局、原生搬猫入口、签名不匹配和占用房间计数回退。
- 当前 BETA 离线特征必须唯一解析完整详情布局与原生搬猫入口。
- Debug/Release CTest 与 DLL load smoke 必须通过。
- 游戏退出后部署 v0.5.29，保留配置、保护规则、本地数据和其他 MOD。

## Verification completed

- 当前 BETA 离线解析确认详情打开 `0xEC7B0`、详情目标 `0xF0570`、抽屉解析
  `0x1A9E10`、原生搬猫 `0x2E88D0`，并保留稳定版布局。
- 首轮 Debug 与 Release 构建成功；CTest 5/5 与 DLL load smoke 均通过。
- code-simplifier 只简化本次房间集合类型和恒真条件；最终 v0.5.29 Debug/Release
  构建、CTest 5/5、DLL load smoke 与本机部署均已完成。
- 玩家已确认 BETA 与正式版的自动整理预览、实际搬猫和战斗猫点击定位均通过。

## Next work

Stage 35 已完成并发布。只有在用户明确提出下一项功能或新问题后才开始后续阶段。

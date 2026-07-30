# CODEX CURRENT TASK - STAGE 14

## Objective

只实施第 14 步：存档安全、原子写入与恢复工具。复用 Stage 10 已有备份、
journal、恢复包与事务边界，在测试副本上补齐稳定窗口、占用检测、manifest、
SHA-256 校验、原子发布、离线恢复及验证。不得提前实施第 15～16 步。

## Required reading

1. `AGENTS.md`
2. `AutoCatteryDocs/16_steps/14_存档安全与恢复.md`
3. `.auto-cattery/state.json`
4. Stage 10～13 报告中与真实写适配器、PreviewOnly、备份、恢复和存档格式
   有关的结论
5. 当前代码、测试、构建文件和 `git status --short`

## Evidence rules

- 事实优先级：当前仓库和当前实现 > 当前游戏 build > 真实运行日志 > 本地
  只读探针 > 玩家实机证据 > 文档和在线资料。
- 文档、示例、参考项目和网络资料只能辅助核实，不能提供未经本地证据确认的
  游戏字段、表、压缩结构、函数、偏移、房间 ID、UI 节点或恢复流程。
- 未知项必须保持在只读探针、build 专用 adapter 或安全失败路径中。
- 不修改玩家正在使用的存档、游戏原始文件、真实日志或 Steam Cloud 数据。

## Stage boundary

- 先在临时目录、合成数据和复制的测试存档上验证备份、校验、manifest、
  稳定窗口、文件占用、原子发布和恢复。
- 恢复前必须确认游戏未运行，并在覆盖目标前再次备份目标。
- 不控制、删除或改写 Steam Cloud 文件。
- 直接存档写入和真实 `MoveCat` 只能在当前 build 的容器、表、压缩块、房间
  关联、猫身份、写字段、独立读回与恢复均有完整证据时，放在显式开发/测试
  开关后启用。
- 证据不足时真实 adapter 保持 `Unsupported`，工作流保持 `PreviewOnly`；
  不实现批量房间分配或真实淘汰。

## Small-file rule

备份模型、manifest、定位/稳定窗口、哈希、原子替换、恢复、SQLite/LZ4 探针、
build 专用写 adapter、journal、CLI 和测试必须保持职责分离。每完成一个可编译
小块立即运行针对性测试和构建。

## Acceptance

- 覆盖备份损坏、文件占用、保存中、WAL/SHM、无权限、原子替换失败、读回
  不一致、恢复时游戏运行、重复恢复和路径逃逸。
- 运行 Debug 和 Release 构建及测试，检查 `git diff` 和 `git diff --check`。
- 填写 `.auto-cattery/reports/stage-14.md`，更新 `.auto-cattery/state.json`，
  仅提交 Stage 14 文件，创建一个本地 commit，绝不 push。
- 若单猫移动、独立读回和撤销/恢复仍缺少可靠证据，报告 blocker，保持
  PreviewOnly，并停止在 Stage 14。

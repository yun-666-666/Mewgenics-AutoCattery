# CODEX CURRENT TASK — STAGE 10

## Scope

只实施阶段 10：密封执行计划、执行前重检、离线一致备份、匿名 journal、
恢复包、注入式事务执行器和明确 Unsupported 的真实写适配器。

不实施阶段 11 流水线、复杂确认 UI、阶段 14 备份管理中心或任何自动组队、
自动休息、自动推进日期、自动出征选择。

## Evidence result

- 本地 SDK、已安装公开源码和当前仓库没有可验证的运行时移动、淘汰或
  RestoreCat API。
- 当前 SaveDatabase 只以 SQLite READONLY | NOMUTEX 打开，并持有
  BEGIN DEFERRED 读事务。
- 当前 SQLite 绑定没有 backup API；运行中只复制 `.sav` 主文件的一致性
  未被证明，WAL/SHM 可能遗漏已提交数据。
- 因此真实写适配器为 Unsupported，配置不能启用真实写入或淘汰。
- 未联网：活动 AGENTS.md 明确禁止 web research。

## Implemented boundary

- ApprovedExecutionPlan 与 ExecutionAuthorization 只能由重检器产生。
- 内容、分类、RoomPlan、保护摘要、HouseReady generation、游戏日、存档
  身份和 build 身份全部进入前置条件；snapshot_id 仅作捕获关联。
- Stage 9 的 `executable=false`、移动/淘汰权限恒 false 不被修改。
- 备份只接受游戏已确认静止且无 WAL/SHM 的离线 `.sav`，验证 SHA-256
  与大小后原子发布；拒绝穿越、重解析点、覆盖源和覆盖现有备份。
- journal 与恢复包使用临时文件和原子替换，不记录猫名或 CatId。
- fake 驱动事务器按备份、Prepared、逐移动验证、逐淘汰验证、Committed
  顺序执行；任一失败停止并反向恢复，恢复失败写
  ManualRecoveryRequired。
- 现有 UI 仍只生成预览；应用服务执行入口固定返回 PreviewOnly。

## Stage gate

阶段 10 的可测试安全基础设施已完成，但真实 adapter 仍为 Unsupported。
不得操作真实存档，不需要玩家进游戏测试，也不得开始阶段 11。

只有取得许可清楚、签名明确且可重复的移动/淘汰/恢复证据，并通过专用复制
测试存档的离线验证后，才能在新的明确任务中重新开启真实执行门。

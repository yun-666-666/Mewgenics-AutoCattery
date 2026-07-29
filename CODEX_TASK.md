# CODEX CURRENT TASK — STAGE 11

## Scope

只实施阶段 11：由 House 按钮明确触发的只读整理工作流编排、状态机、
短期预览、取消/过期/幂等保护、能力路由、结果摘要和推荐快照写入边界。

不实施阶段 12～16、后台自动触发、自动组队、自动休息、日期推进、自动
出征选择或任何未经验证的真实移动、淘汰、恢复和存档写入。

## Evidence result

- Stage 5～9 的不可变快照、七项属性评分、分类、保护和保守房间规划直接
  复用，没有复制 Toolkit 算法。
- Stage 10 的真实写适配器仍为 Unsupported；当前没有验证过的 move、
  cull、restore 或运行中一致备份接口。
- 活动 `AGENTS.md` 禁止 web research，因此本阶段没有联网。
- Toolkit 1.0.0 仅用于核对 MIT 许可、流水线顺序、保护优先级、确定性
  排序、部分成功和输出契约；其整数 RoomId、六属性模型、房间角色/容量
  和游戏接口假设均未进入实现。

## Implemented boundary

- 显式状态机覆盖捕获、评分、规划、等待确认、执行、验证、完成、失败和
  取消；PreviewOnly 不得进入 Applying。
- 预览绑定 HouseReady generation、game day、快照/分类/保护/RoomPlan/
  配置/候选顺序摘要、匿名 save identity 和 build identity。
- preview ID 由全部绑定与本地序列共同生成，不以 snapshot_id 单独生成。
- 短期预览支持过期、取消、重复 ID 拒绝、绑定变化重预览和一次性 claim。
- 预览摘要只记录匿名计数、能力、partial/invalid/Unknown 警告以及
  “未修改游戏数据”，不记录猫名、CatId、存档名或个人路径。
- PreviewOnly 执行在 Stage 10 gateway 前返回 NotAvailable，保持 0
  backup、journal、recovery package 和 write-adapter 调用。
- MoveOnly 合成边界拒绝淘汰选择；移动失败不会自动尝试淘汰。
- 推荐快照 writer 只接受 Committed/Completed 结果，写 MOD 自有 sidecar
  临时文件、校验和和原子替换；Stage 11 不读取或显示该数据。
- House 点击异步启动只读预览，由 UI tick 轮询完成；没有点击、进入 House
  或后台 timer 时不会捕获、评分或规划。

## Stage gate

`PreviewOnly complete; real execution blocked by Unsupported Stage 10 adapter`

Stage 12 继续 blocked。未部署 DLL，未读取或写入玩家真实存档，不要求玩家
手动测试。

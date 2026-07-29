阶段：11
状态：PreviewOnly complete; real execution blocked by Unsupported Stage 10 adapter

实现：
- House 按钮是唯一入口；没有点击时不捕获、评分、规划或执行。
- 新增显式工作流状态机、PreviewOnly/MoveOnly/MoveAndCull 能力模型。
- 直接编排 Stage 5～9 的快照、战斗/繁育评分、分类、保护和房间规划。
- 新增匿名预览摘要、两分钟默认 TTL、取消、过期、重复 ID、一次性 claim
  和并发保护。
- 预览绑定 scene generation、game day、快照/分类/ProtectionDigest/
  RoomPlan/配置/候选顺序摘要、匿名 save identity 和 build identity。
- PreviewOnly 在 Stage 10 gateway 前返回 NotAvailable，保持 0 写调用。
- MoveOnly 合成测试拒绝 Cull；Move 失败不自动触发 Cull。
- 推荐快照只在合成 Committed 结果后写 MOD sidecar，使用临时文件、
  校验和和原子替换；本阶段不读取、不展示、不生成真实 sidecar。
- House 点击的文件型只读分析在工作线程执行，MewUI tick 只轮询并更新
  现有摘要 UI；未新增确认 UI 或后台 timer。
- 测试进程统一捕获异常并禁用 Windows CRT 错误弹窗，避免失败测试阻塞。

Toolkit：
- 读取 Toolkit 1.0.0 MIT 许可证、README、接口契约、集成清单、C++ 核心
  README、分类/RoomPlan 实现与测试。
- 实际复用的是流程顺序、保护优先级、确定性排序、部分成功和输出契约。
- 拒绝整数 RoomId、六属性、房间名称/角色/容量、关系/繁育资格、游戏
  字段/API/偏移、snapshot 并发语义和任何真实写入假设。

验证：
- Debug `phase11_unit_tests`：通过（4.45 秒）。
- Debug `phase11_dll_load_smoke`：通过（0.07 秒）。
- Release `phase11_unit_tests`：通过（0.51 秒）。
- Release `phase11_dll_load_smoke`：通过（0.05 秒）。
- 覆盖空猫舍、无需动作、只读预览、Unknown/partial、取消、过期、消费、
  重复 ID、并发 claim、全部绑定变化、Unsupported 0 gateway、MoveOnly
  禁 Cull、移动失败不淘汰、推荐快照跳过/失败警告、100/1000 猫性能和
  无点击 0 调用。
- `git diff --check`：通过。
- 新增产品/测试文件均低于 250 行；最终最大值在最终回复报告。
- 隐私、密钥、存档/WAL/SHM、日志、二进制、个人路径和 Stage 12 越界
  扫描：通过。

真实游戏状态：
- 工作流能力：PreviewOnly。
- 未部署 DLL。
- 未打开、读取或写入玩家真实存档。
- 未执行真实移动、淘汰、恢复、备份或 sidecar 写入。
- 不需要玩家手动测试。

联网：
- 未进行；活动 AGENTS.md 明确禁止 web research。

未实施：
- Stage 12～16。
- 复杂确认/配置 UI、通用备份中心。
- 自动组队、自动休息、日期推进、自动出征选择。
- 任何未经验证的 move/cull/restore/实时一致备份适配器。

剩余 blocker：
- Stage 10 真实写适配器仍为 Unsupported。
- 缺少许可清楚、签名明确、可重复验证的 move/cull/restore API。
- 缺少运行中一致 SQLite backup 或官方保存快照证据。

本地 commit：由本阶段最终单一提交创建，哈希在最终回复报告。
是否 push：否

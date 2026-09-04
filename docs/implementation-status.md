# AutoCattery 当前实现状态

更新日期：2026-09-05

本文区分当前公开版本 v0.5.24 与已部署、等待玩家验证的 v0.5.27 测试候选。
历史阶段报告和旧版本故障用于追溯，不代表当前版本仍存在同类问题。

## v0.5.27 本地测试候选

- 保留 v0.5.26 的短管理面板节点名修复；玩家已确认该版本进入 House 不再闪退，
  但 F10 和两个普通 House 按钮全部缺失。
- 同次 BETA 启动 PID `28312` 中，CombineDuplicateFurniture v0.6.19 先通过
  Mewjector Hook scene-ready `0x96AC50`；AutoCattery 的延迟 MewUI 初始化随后持续
  报告 `Runtime UI locator could not uniquely resolve scene-ready update`，未到达
  `AC1202`、`AC18000`、`AC3100` 或 `AC4100`。
- 根因是先行 Hook 改写了进程内 `.text` 的 scene-ready 特征，不是按 BETA 版本号
  禁用 AutoCattery，也没有关闭 F10 或 House 按钮。
- 运行时改为从当前游戏进程实际 EXE 创建干净 `SEC_IMAGE` 映射，在未被其他 MOD
  改写的映像中唯一定位 30 个 UI RVA，再通过 Mewjector 加入已有 Hook 链。
- 干净映像使用 PE timestamp 与 image size 对照当前加载映像以确认身份；这是同一
  映像校验，不是版本白名单。仅在干净映像无法打开时回退到进程内映像。
- 当前 BETA 离线定位结果为 scene-ready `0x96AC50`、Button activate `0x97E8E0`、
  Button can-activate `0x97EAF0`。
- 新增回归测试模拟 scene-ready 特征首字节被先行 Hook 改写，确认旧的 live-image
  策略失效而磁盘干净映像仍可完成 30/30 唯一定位。
- Debug/Release CTest 5/5 与 DLL load smoke 均通过。
- v0.5.27 已部署到本机 Mewjector/Mewtator 目录，配置、保护规则和本地数据保留。
- 玩家状态：三个 MOD 同时启用时的启动、F10、两个 House 按钮、存档切换和 House
  重进测试待完成；
  beta 布局仍以玩家实机结果为接受边界。

## 当前发布状态

- 平台：Windows x64。
- Loader：Mewjector v3。
- UI：MewUI API 1.2.0。
- 当前版本：v0.5.24。
- 当前提交：`c33fe74bf140ff386c40f9c9bb73df60a4bb430e`。
- GitHub Latest Release：v0.5.24。
- 玩家状态：当前 House 按钮、F10、MoveOnly 与相关主流程测试通过。

## 当前能力

### UI 与配置

- House 自动整理按钮和次日战斗猫推荐按钮。
- F10 设置、猫保护和完整移动预览；Esc 可关闭面板。
- 中文/英文手动切换。
- 14 个普通/进阶职业的升级重骰次数设置与兼容数据 MOD 同步。
- 默认关闭、仅本地写入且不自动上传的技术数据收集。

### 快照、评分与规划

- 只读发现存档并解析 SQLite、LZ4 猫数据、房间、七项基础属性、遗传数据、性别、
  性取向、亲缘和游戏缓存 COI。
- 每次进入 House 和预览前刷新运行时 CatId、房间组件与当前房间。
- 支持当前已确认的 `Attic`、`Floor1_Large`、`Floor1_Small`、`Floor2_Large`。
- 支持 2、3、4 房人数均衡、房间属性用途、繁育房性别配额、战斗驻留和育幼目标。
- 未稳定全 7 时按基础属性与 COI 选择繁育组合；稳定后才启用技能、被动、疾病、
  普通变异和出生缺陷权重。
- 繁育房按整个群体的异性交叉质量构建稳健组合池，不假定游戏锁定某一对父母。
- 保护规则支持 `NoCull`、`NoMove`、`NoCullOrMove`、`FullyUnmanaged` 和固定房间。

### 当前 build 原生 MoveOnly

- 第一次点击生成预览，第二次点击前重新捕获并比较全体实时房间状态。
- 状态变化、换存档、离开 House 或 scene generation 改变会取消旧计划。
- 每个 UI tick 最多提交 8 次原生移动，批次间重新捕获、重验和重规划。
- 已到位的猫会跳过；重复执行保持 0 次移动。
- 真实淘汰保持关闭。

## 玩家验证状态

玩家已确认当前版本的以下流程通过：

- 两个普通 House 按钮和 F10 面板。
- 2、3、4 房自动分配。
- 预览后状态变化取消旧计划。
- 房外未分配猫进入普通房间。
- 自动分房后正常保存、退出并重新进入。
- 重复整理保持幂等。
- 保护规则的应用、固定房间和移除。

## 暂缓事项

- 玩家尚未产生更多游戏日，当前多日繁育样本不足以证明长期收益；暂不继续调参。
- 真实淘汰未启用，也不属于当前已完成能力。
- 游戏原生收藏、锁定、硬容量或未来 build 内部布局只有获得新证据后才处理。

## 证据解释

证据优先级为当前代码、当前构建、最新运行日志和玩家对对应版本的实际结果。
v0.5.24 的玩家通过结论不能代替 v0.5.27 的兼容性复测；同样，历史版本的崩溃、
闪烁、卡顿或待验证记录不能覆盖已经取得的对应版本玩家结果。

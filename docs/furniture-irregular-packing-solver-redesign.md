# AutoCattery 家具不规则排样与求解器重设计

更新日期：2026-08-14

状态：诊断与实施设计，尚未编码、构建、部署或完成玩家实机验证。

适用基线：AutoCattery v0.5.59，当前 Stage 45 家具自动放置链路。

## 1. 文档目的

本文档记录玩家在 v0.5.59 实机测试中发现的三个问题，并给出下一阶段可直接实施的
求解器设计：

1. F10 设置房间整体属性目标后，第一次“分析 + 自动放置”没有产生可见变化；
2. 搬空全部房间后，新一轮自动放置错误地从左下大房开始，而不是从阁楼开始；
3. 空房分析和点击自动放置后的重新计算耗时过长，当前布局质量仍不稳定。

本文档不把某一个存档中的房间家具、坐标或属性值编码为通用规则。日志中的数量和
时间只用于解释本次失败；算法和验收条件必须适用于动态的 1、2、3、4、5 以及更多
普通房间。

本文档提出的核心方案是：

```text
现有合法原点生成与游戏专属几何
    + Bottom-Left-Fill / 最大接触边快速初始解
    + bitset 带权 Set Packing
    + 限时 Branch-and-Bound
    + 小规模局部移除与重插
    + 现有整屋 Support 与密封执行验证
```

在替换求解器之前，必须先修复 `focus_room` 生命周期，否则即使新求解器得到更好的
房间布局，仍可能从错误房间开始。

## 2. 玩家要求与语义边界

F10“自动放置”页中的舒适、刺激、健康和变异设置是目标房间最终显示的整体属性，
不是每只猫的目标，也不乘房间预计居民数。

当前玩家配置为：

| 房间用途 | 舒适 | 刺激 | 健康 | 变异 |
|---|---:|---:|---:|---:|
| 繁育房 | 最低 7.5 | 最低 4 | 最低 5 | 最低 5 |
| 幼猫休养房 | 最低 8 | 最低 0 | 最低 3 | 最低 0 |
| 战斗房 | 最高 -8 | 最高 0 | 最低 0 | 最低 8 |
| 变异房 | 最低 10 | 最低 0 | 最低 0 | 最低 8 |
| 普通房 | 最低 4 | 最低 4 | 最低 4 | 最低 4 |

其他设置：

- 最低家具覆盖率：15%；
- 达标后继续填满：关闭。

安装配置中的旧 JSON 键名仍包含 `*_per_resident`，这是兼容保留字段；v0.5.59 的
实际含义已经是整个房间的最终目标。求解器和界面不得重新引入居民数缩放。

战斗房的舒适和刺激是上限，其余表格项是下限。特别是战斗房达到舒适 `-8` 后，不应
仅因为可以继续降低舒适而无界追求更负的数值；达到刺激上限 `0` 后，也不应为了几何
填充而加入额外正刺激家具。

## 3. 本次实机日志结论

最新日志：

```text
D:\steam\steam\steamapps\common\Mewgenics\Mods\AutoCattery\logs\auto_cattery.log
```

### 3.1 第一次设置后没有变化：方案被密封快照拒绝

本次时间线：

```text
17:12:13  玩家开始分析
17:12:34  分析生成方案，target=Floor1_Large
17:13:19  玩家点击自动放置
17:13:19  AC3906：分析后的整屋家具快照已经发生变化
```

`AC3906` 的含义是：执行前重新捕获的整屋家具绑定与分析时的密封绑定不一致，因此
本次方案被拒绝。该次没有执行任何家具移动。

所以这里不是“采用设置后仍生成相同布局”，而是“生成的布局从未被执行”。密封拒绝
本身必须保留，但界面应明确告诉玩家：

```text
家具状态已发生变化，本次分析结果已失效，请重新分析。
```

### 3.2 搬空后错误地先填左下大房

搬空房间后的时间线：

```text
17:15:55  当前运行时家具=0，开始分析
17:16:58  玩家关闭家具模式
17:16:59  玩家重新打开家具模式并再次开始分析
17:18:02  AC3901：moves=11，target=Floor1_Large，focus_room=Floor1_Large
17:18:29  玩家点击自动放置
17:20:25  第一件家具放入 Floor1_Large
17:20:33  11/11 完成
```

`Floor1_Large` 是玩家画面中的左下大房。当前固定的玩家可见房间顺序仍然是：

```text
Attic
→ Floor2_Large
→ Floor1_Large
→ Floor1_Small
→ Floor2_Small
```

完整重启后，焦点被清空，日志恢复为：

```text
21:49:14  开始分析
21:49:39  target=Attic，focus_room 为空
21:50:02  第一件家具放入 Attic
```

因此房间 ID、运行时房间映射和固定顺序本身没有错。错误是上一次没有执行成功的
`Floor1_Large` 分析方案提前写入了会话焦点，下一轮排序又让焦点房间优先于固定顺序。

### 3.3 分析慢，执行本身并不慢

搬空后的一次分析：

```text
17:16:59 → 17:18:02，约 63 秒
```

点击自动放置后的同步重新计算：

```text
17:18:29 → 17:20:25，约 116 秒
```

真正执行 11 件家具：

```text
17:20:25 → 17:20:33，约 8 秒
```

主要耗时发生在求解，而不是原生家具逐件放置。

## 4. `focus_room` 根因与必须先完成的修复

当前有三处行为共同造成焦点污染：

1. `FurnitureAnalysisService::Analyze()` 只要收到有效的
   `preferred_focus_room_id`，就直接把它选为 `active_room_id`；
2. `PlanWholeHouse()` 排序时让 `preferred_focus_room_id` 排在固定房间顺序之前；
3. `MewUiBridge` 在分析产生计划后就执行
   `furniture_focus_room_id_ = plan.target_room_id`，不等待任何实际移动提交成功。

相关代码接缝：

```text
src/furniture_analysis/service.cpp
src/furniture_planning/layout_solver.cpp
src/ui/mew_ui_bridge.cpp
```

### 4.1 正确的焦点状态机

建议把焦点分为两个不同概念：

- `preview_target_room_id`：当前只读分析结果的目标房间；
- `committed_focus_room_id`：至少有一项移动真实提交后，允许后续继续处理的房间。

二者不得共用同一个可持久会话字段。

状态转换：

```text
Idle
  └─ Analyze succeeds ─→ PreviewReady(preview target only)

PreviewReady
  ├─ first move commits ─→ Executing(committed focus = target)
  ├─ AC3906 ─────────────→ Idle(clear preview target)
  ├─ cancel ─────────────→ Idle(clear preview target)
  └─ close furniture UI ─→ Idle(clear uncommitted preview target)

Executing
  ├─ room completes ─────→ clear committed focus, lock exact completed binding
  ├─ safely deferred ────→ clear or explicitly defer committed focus
  └─ fatal native error ─→ stop writes for current scene
```

### 4.2 固定顺序的优先规则

对一次新的全屋循环，选择目标房间时应先找固定顺序中尚未完成的最早房间。
`committed_focus_room_id` 只有在下列条件全部满足时才能保持优先：

- 该房间在当前 House scene 中至少成功提交过一项本轮移动；
- 该房间尚未完成；
- 当前整屋 binding 与焦点建立时的有效状态兼容；
- 玩家没有手动搬空或修改该房间；
- 没有发生 `AC3906`、取消或未提交关闭。

当全部普通房间都为空时，必须无条件从固定顺序中的第一个可用房间开始。

### 4.3 焦点修复的回归测试

至少加入以下确定性测试：

1. 分析得到 `Floor1_Large`，未执行，下一轮空屋分析仍选 `Attic`；
2. 分析后触发 `AC3906`，焦点清空；
3. 分析后关闭再打开家具模式，未提交焦点不保留；
4. `Attic` 第一项提交成功后，刷新分析仍可继续聚焦 `Attic`；
5. `Attic` 完成并锁定后，下一轮选择 `Floor2_Large`；
6. 玩家手动搬空已完成房间后，只失效该房间的完成绑定，并重新按固定顺序选择；
7. 缺少某些房间时跳过，不使用固定数量假设。

## 5. 当前 Beam Search 的性能问题

当前 `src/furniture_planning/layout_solver.cpp` 中的主要上限：

```cpp
kWholeHousePackingBeamWidth = 256
kWholeHouseCandidateBranchesPerState = 16
kIncomingCandidatesPerRoom = 192
```

`PackingOrders()` 最多生成六种家具顺序。每种顺序又可能从普通初始状态、当前布局种子
和 Anchor/Support 链种子分别运行。

当前最大普通房间约为 `37 × 11 = 407` 个格子。对一件家具，现有算法会枚举房间内
几乎全部合法原点；对 Beam 中每个状态又重复扫描这些原点，最后才把局部分支裁剪为
16 个。

一轮粗略候选检查规模可能达到：

```text
192 件 × 256 个 Beam 状态 × 约 407 个原点
≈ 20,000,000 次候选检查
```

再乘最多六种顺序和多类种子，容易进入上亿次检查。当前实现还频繁复制完整
`PackState`，因此不仅碰撞检查多，内存复制和排序也占用大量时间。

仅仅缩小 Beam 宽度会变快，但会进一步降低布局质量；继续增加 Beam 宽度则会让等待
时间更差。因此问题不应通过继续调整三个常量解决，而应改变搜索表示和剪枝方式。

## 6. 问题的准确数学形式

当前问题不是连续多边形排样，而是离散格子上的可选家具放置问题。

对家具 `i` 的每一个合法原点 `p`，建立二进制变量：

```text
x[i,p] ∈ {0,1}
```

`x[i,p] = 1` 表示家具 `i` 最终放在原点 `p`。

这可以建模为：

```text
带权 0-1 Set Packing
= 合法放置候选冲突图上的最大权独立集
= 带可选项、属性目标和支撑依赖的广义 Exact Cover
```

### 6.1 硬约束

任何优化目标都不能违反以下条件：

1. 同一家具最多选择一个合法原点；
2. `Hitbox`、`Solid` 和已验证的 `PoopLogic` 冲突规则必须满足；
3. 家具不得越出房间或进入静态阻挡格；
4. `Support` 原点必须由房间边界、已有合法支撑或所选家具提供支撑；
5. `Surface` 按当前已验证语义处理，不能擅自当作普通占用或万能支撑；
6. 固定、未知语义、不可安全移动或受保护的家具维持原位；
7. 已 quarantine、retiring、稳定键冲突或执行状态不安全的对象不得进入计划；
8. 已完成房间的精确 live binding 不得被后续房间作为家具来源；
9. 整屋当前布局和最终布局都必须通过现有 Support 审计；
10. 计划必须能生成直接、可执行、不会依赖未验证临时缓冲的移动顺序。

### 6.2 词典序目标

不能简单把所有目标乘权重相加，否则一个极大的几何得分可能错误地补偿属性失败。
应使用严格的词典序比较：

1. 满足全部硬约束；
2. 优先满足该用途的四项整体属性目标；
3. 满足最低家具覆盖率；
4. 最小化与目标值的有界距离，避免超过目标后无界堆叠；
5. 在同等达标程度下优先使用更少家具；
6. 减少占用包围盒、内部空洞和离散区域；
7. 增加与地面、墙面和其他家具的有效接触边；
8. 用稳定 key、item ID 和坐标作最后的确定性排序。

如果“达标后继续填满”关闭，达到属性和最低覆盖率后，不再仅为了增加占地继续加入
中性家具。如果该设置开启，才在不破坏前述目标的条件下继续提高有效覆盖。

## 7. 相关成熟算法比较

### 7.1 Bottom-Left / Bottom-Left-Fill

BL/BLF 依次放置物体，并将其推向底部和左侧的最早可行位置。优点是快速、确定性强、
容易生成可用初始解；缺点是强依赖家具顺序，单独使用时容易产生无法回填的空洞。

当前 MOD 适合使用 BLF 的变体作为初始解，但评分不应只看“左下”：

```text
先贴地 → 再贴墙 → 最大接触边 → 最少新增空洞 → 稳定键排序
```

参考：

- [A New Bottom-Left-Fill Heuristic Algorithm for the Two-Dimensional Irregular Packing Problem](https://doi.org/10.1287/opre.1060.0293)

### 7.2 No-Fit Polygon

NFP 用多边形运算生成两个连续形状之间的不可重叠边界，适合连续坐标、任意角度旋转
和复杂多边形。

当前家具和房间已经是离散格子，现有 `GenerateCandidates()` 也已经能枚举合法原点。
当前瓶颈不在“生成哪里可以放”，而在“从大量合法放置中选哪一组”。因此不建议把
NFP 作为主替代方案。

### 7.3 Exact Cover / Algorithm X / Dancing Links

Exact Cover 适合每个目标格必须恰好覆盖一次的拼图。当前 MOD 允许空格、允许跳过家具、
同一家具最多选择一次，并有属性、覆盖率和支撑依赖等多目标，所以纯 DLX 不能直接
表达完整需求。

DLX 可以作为候选冲突枚举的参考，但最终需要广义或带权 Set Packing。

参考：

- [Donald Knuth: Dancing Links](https://arxiv.org/abs/cs/0011047)

### 7.4 CP-SAT / MILP

CP-SAT 能直接表示二进制候选、互斥、属性目标和部分依赖约束。OR-Tools 的
`NoOverlap2D` 主要面向轴对齐矩形；当前不规则格子家具更适合把每一个合法原点编码
成二进制变量，而不是用家具包围矩形代替真实 footprint。

CP-SAT 是很好的原型和离线基准工具，但第一步不建议把完整 OR-Tools 静态链接进游戏
DLL：依赖、体积、构建时间和运行时集成成本都明显增加。

参考：

- [Google OR-Tools C++ CP-SAT API](https://or-tools.github.io/docs/cpp/classoperations__research_1_1sat_1_1CpModelBuilder.html)

### 7.5 开源不规则排样实现

- [seanys/2D-Irregular-Packing-Algorithm](https://github.com/seanys/2D-Irregular-Packing-Algorithm)
  包含 NFP、BLF、TOPOS、遗传算法、模拟退火、邻域搜索和压紧/分离，适合研究算法
  结构，但主要是实验与教学实现，不应直接复制进 DLL；
- [fontanf/packingsolver](https://github.com/fontanf/packingsolver) 使用 MIT License，
  支持非凸物体、非凸容器、孔洞、旋转、镜像、间距、质量区域和时间限制，可作为
  成熟求解器架构参考，但仍需要适配游戏专属 tile、Support 依赖和属性词典序目标；
- [Two-dimensional irregular packing problems: A review](https://doi.org/10.3389/fmech.2022.966691)
  总结 NFP、BLF、局部搜索、遗传算法、模拟退火、数学规划和混合算法。

引入任何开源代码前必须单独核对许可证和第三方声明。本文推荐参考算法结构，不要求
复制第三方实现。

## 8. 推荐求解器架构

### 8.1 保留现有能力

以下部分应继续复用，不重新发明：

- 当前 build 的 24×24 家具 tile 解码；
- 房间运行时碰撞网格；
- `GenerateCandidates()` 或等价的合法原点枚举；
- Hitbox、Solid、Support、Surface、PoopLogic 的已验证语义；
- 固定家具、不可移动家具和 stable-key 生命周期过滤；
- 整屋 Support 验证；
- 密封 binding、防陈旧检查、逐 UI tick 原生执行和失败回滚。

主要替换“对合法候选进行组合选择”的 Beam Search 核心。

### 8.2 紧凑 bitset 表示

最大房间约 407 个格子，可以用七个 64 位字覆盖。每一个候选原点预计算：

```cpp
struct PlacementCandidate {
    FurnitureStableKey stable_key;
    ItemId item_id;
    RoomId target_room_id;
    GridOrigin origin;

    CellBitset blocking_cells;
    CellBitset hitbox_cells;
    CellBitset solid_cells;
    CellBitset support_cells;
    CellBitset surface_cells;
    CellBitset poop_logic_cells;

    CellBitset required_support;
    CellBitset provided_support;

    RoomAttributes attributes;
    std::uint16_t blocking_cell_count;
    std::uint16_t contact_score;
    std::uint16_t bounds_growth;
};
```

普通碰撞判断应尽量收敛为数个按位与：

```cpp
if ((candidate.blocking_cells & state.blocking_cells).none()) {
    // 继续检查 tile 特殊规则和支撑依赖
}
```

避免为每一个候选复制包含动态容器的完整 `PackState`。搜索状态只保存固定尺寸 bitset、
累计属性、覆盖率、少量索引和增量选择栈。

### 8.3 候选预处理

在搜索前完成：

1. 删除越界或静态冲突候选；
2. 删除违反 fixed/quarantine/stable-key 规则的候选；
3. 建立同一家具全部原点的互斥组；
4. 建立不同家具候选之间的冲突 bitset；
5. 建立 Support 需求与可提供支撑的候选关系；
6. 计算每个候选对当前房间属性缺口的收益；
7. 删除被严格支配的候选。

候选 A 在以下条件全部成立时可被候选 B 支配：

- A 和 B 属于同一家具；
- B 不引入更多冲突；
- B 的支撑条件不更困难；
- B 的属性贡献相同；
- B 的包围盒增长、空洞或接触评分至少一项更好，其他项不差。

### 8.4 BLF / 最大接触边初始解

先按用途属性缺口、有效占地和合法原点数量对家具排序，再贪心选择候选：

```text
属性目标迫切程度
→ 合法原点少的家具优先
→ 对目标属性收益高
→ 新增阻挡格少
→ 贴地/贴墙
→ 与已有家具接触边多
→ 新增空洞少
→ stable key 决定性排序
```

该初始解应在几十毫秒到数百毫秒内完成，为后续 Branch-and-Bound 提供一个较强下界。

### 8.5 限时 Branch-and-Bound

Branch-and-Bound 不需要穷举到证明全局最优。它在明确时间预算内持续改进当前最优解，
到期后返回已经通过硬约束验证的最优已知方案。

推荐分支策略：

- MRV：优先处理剩余合法原点最少的家具；
- 先尝试最能补齐当前属性缺口的候选；
- 每件家具始终保留 skip 分支；
- 用冲突 bitset 一次裁掉大量候选；
- 用剩余属性上界判断是否仍可能达到目标；
- 用剩余覆盖率上界判断是否仍可能达到最低覆盖；
- 达标后用最少家具、包围盒和空洞下界继续剪枝；
- 在相同输入和时间预算下保持确定性的分支顺序。

伪代码：

```text
best = BuildGreedySeed()
deadline = now + configured_budget

Search(state, remaining_candidates):
    if now >= deadline:
        return

    if CannotBeat(best, state, remaining_candidates):
        return

    if IsCompleteOrSufficient(state):
        best = BetterOf(best, state)

    item = SelectMRVItem(remaining_candidates)

    for candidate in OrderedLegalCandidates(item, state):
        Apply(candidate, state)
        Search(state, remaining_candidates - conflicts(candidate))
        Undo(candidate, state)

    Search(state, remaining_candidates - all_candidates(item))  // skip item
```

### 8.6 时间预算与返回规则

建议先固定一个玩家不可配置的保守预算，取得实机数据后再决定是否加入设置：

| 模式 | 单房搜索预算 | 用途 |
|---|---:|---|
| 快速初始解 | 100–300 ms | 必须始终产生可用基线 |
| 标准改进 | 1–2 s | 默认玩家流程 |
| 高质量上限 | 3–5 s | 仅当标准预算仍明显不足时评估 |

第一版目标不是保证数学上的全局最优，而是：

- 在预算内返回合法方案；
- 明显快于当前 25–116 秒；
- 同等属性下布局不劣于当前 Beam Search；
- 超时是正常终止，不报告为失败；
- 超时返回的方案仍通过完整 Support 和可执行性验证。

禁止到期后返回未完成的搜索状态或只通过局部碰撞检查的方案。

### 8.7 局部改善

Branch-and-Bound 后对当前最优解执行小规模、严格限时的局部搜索：

1. 移除一件家具并尝试重新插入；
2. 移除两件冲突或形成空洞的家具并重新插入；
3. 将家具向地面、墙和现有布局压紧；
4. 尝试用更少家具达到相同属性和覆盖率；
5. 尝试用同数量家具减少空洞或离散区域。

局部搜索不得改变硬约束和用途目标优先级。

## 9. Support 依赖的处理

纯格子互斥不能完整表达 Support。推荐将 Support 分为搜索期快速约束和最终严格验证：

### 9.1 搜索期

对每个候选记录：

- 哪些 Support 格必须被支撑；
- 哪些房间基础格直接提供支撑；
- 哪些已固定家具提供支撑；
- 哪些候选 Solid/Surface 按已验证规则能够提供支撑。

搜索状态维护当前已提供支撑和仍待满足的支撑集合。允许暂时选择一个待其他候选支撑
的家具，但剩余候选必须存在满足该需求的可能性，否则立即剪枝。

### 9.2 最终验证

搜索返回的每个候选布局必须继续通过现有：

- 当前布局 Support 审计；
- 最终整屋 Support 审计；
- `WholeHouseMovesPreserveSupport()` 或其当前等价门；
- 拆卸和安装依赖顺序生成；
- 原生目标坐标合法性和密封 binding 检查。

新求解器只负责决定终局，不得削弱执行层的安全验证。

## 10. 房间属性目标的比较方法

对每项最低值目标 `minimum`：

```text
deficit = max(0, minimum - actual)
excess  = max(0, actual - minimum)
```

对每项最高值目标 `maximum`：

```text
deficit = max(0, actual - maximum)
excess  = max(0, maximum - actual)
```

比较时先最小化是否未达标以及总缺口。所有目标达标后，再用有界的 excess/距离比较，
而不是继续无界奖励绝对值更大。

战斗房示例：

```text
目标：Comfort <= -8，Stimulation <= 0，Health >= 0，Mutation >= 8
```

- `Comfort=-8` 已达标；`-20` 不应仅因更负而压倒所有其他布局；
- `Stimulation=0` 已达标；正刺激是违反上限；
- `Health=0` 达标，但如果两个布局其他条件相同，可以用有限的次级规则比较更高健康；
- `Mutation=8` 达标；更高变异只能作为达标后的低优先级差异。

这保证“战斗房舒适已经很低，却继续塞大量刺激家具”的结果不会被错误奖励。

## 11. 空间质量指标

属性和最低覆盖率之后，建议使用以下几何指标：

1. `blocking_cell_count`：实际阻挡格数量；
2. `bounds_area`：全部阻挡格的最小包围盒面积；
3. `internal_holes`：被布局围住但无法容纳任何剩余候选的空格或空区；
4. `free_components`：剩余可用空间的连通分量数量；
5. `contact_edges`：家具与地面、墙、固定家具和其他家具的有效接触边；
6. `isolated_cells`：无法再被任何候选使用的孤立格；
7. `move_cost`：从当前布局变为目标布局的移动数量。

这些指标必须排在用途属性和最低覆盖率之后。不能为了包围盒更小而牺牲房间目标，也
不能因为大件家具占地多就错误地认为它比能精确补足属性的小件家具更优。

## 12. 建议新增性能与诊断日志

当前只有分析开始、结果和部分失败信息，无法直接区分候选生成、搜索和 Support 验证
各自耗时。建议每个房间记录一条结构化汇总：

```text
room=Attic
items_considered=183
items_after_filter=96
placements_generated=12480
placements_after_dominance=4380
conflict_edges=...
greedy_ms=...
search_ms=...
local_improvement_ms=...
support_validation_ms=...
nodes_visited=...
nodes_pruned_conflict=...
nodes_pruned_attribute_bound=...
nodes_pruned_coverage_bound=...
deadline_reached=true|false
best_attributes=comfort/stimulation/health/mutation
best_coverage=...
best_item_count=...
```

同时记录焦点来源：

```text
focus_source=none|committed_move|completed_binding|legacy_preview
```

正式修复后不应再出现 `legacy_preview`。

日志不得写入用户私密路径、存档内容或未经过滤的对象内存。

## 13. 实施阶段

### 阶段 A：焦点生命周期修复

目标：解决搬空后错误从左下房开始，不改变装箱算法。

工作：

- 分离 preview target 与 committed focus；
- `AC3906`、取消、未提交关闭时清除 preview target；
- 第一次真实提交后才建立 committed focus；
- 固定顺序优先于无提交焦点；
- 加入第 4.3 节测试和焦点来源日志。

完成门：空屋新循环稳定选择 `Attic`；分析失败或密封拒绝不会污染下一轮。

### 阶段 B：搜索统计与可重复基准

目标：在替换算法前得到确定性性能基线。

工作：

- 候选数、Beam 扩展数、状态复制量和分阶段耗时；
- 使用合成 1/2/3/4/5 房与当前只读快照；
- 不把当前存档具体家具值写入通用断言。

完成门：能够用一条日志解释时间消耗在哪个阶段。

### 阶段 C：bitset 候选与 BLF 初始解

目标：快速返回合法且确定性的初始布局。

工作：

- bitset tile 和冲突表示；
- 候选预处理与支配删除；
- 用途缺口感知的 BLF / 最大接触边初始解；
- 与现有最终 Support 门对接。

完成门：所有合成几何测试合法；标准大房在可接受时间内产生初始解。

### 阶段 D：限时 Branch-and-Bound

目标：在 1–2 秒标准预算内改进初始解。

工作：

- MRV 分支；
- conflict bitset；
- 属性和覆盖率上界；
- 词典序最优解比较；
- 明确 deadline 和合法 best-so-far 返回。

完成门：相同输入确定性；超时返回合法结果；不再出现几十秒无反馈搜索。

### 阶段 E：局部压紧与实机交付

目标：改善空洞、接触边和家具数量，完成玩家实机验证。

工作：

- 1-remove / 2-remove 重插；
- 完整拆装顺序和整屋 Support 验证；
- Debug、Release、CTest、DLL/data-only 部署；
- 由玩家操作游戏并返回截图和最新日志。

## 14. 自动化测试要求

### 14.1 几何

- 非矩形家具 footprint 在非矩形可用区域中合法放置；
- 包围盒重叠但真实 footprint 不冲突时允许共存；
- 真实 footprint 冲突时拒绝；
- 门口、房间边界和静态阻挡不可进入；
- 确定性同分布局保持稳定。

### 14.2 可选家具与属性

- 每件家具最多选择一次；
- skip 分支始终存在；
- 属性未达标时优先补缺口，不被纯几何大件挤掉；
- 达标后关闭填满时停止加入中性家具；
- 战斗房 Comfort/Stimulation 上限按方向正确比较；
- 居民数变化不改变相同房间属性与配置的家具排序。

### 14.3 Support

- 房间基础支撑；
- 家具 Solid 提供支撑；
- 多层依赖链；
- 缺少底座时拒绝；
- 搜索终局合法但无法生成安全拆装顺序时拒绝；
- 当前和最终整屋 Support 都通过。

### 14.4 性能与终止

- 大量候选在 deadline 到达后正常返回 best-so-far；
- 返回方案经过最终严格验证；
- 不复制无界动态状态；
- 不因某件家具原点过多而吞掉所有分支；
- 同一时间预算和输入保持确定性结果或至少保持确定性比较顺序。

### 14.5 焦点顺序

- 覆盖第 4.3 节全部状态转换；
- 空屋固定顺序；
- 缺失房间跳过；
- 已完成精确 binding 锁定；
- 手动修改只失效对应房间；
- `AC3906` 不保留未提交焦点。

## 15. 玩家实机验收步骤

游戏操作只能由玩家完成，MOD 开发流程不得启动、点击或控制游戏。

建议使用可恢复的家具测试存档：

1. 进入 House 家具模式，确认 F10 参数；
2. 搬空全部普通房间或使用玩家认可的测试起点；
3. 点击“开始分析”，记录分析耗时；
4. 确认第一目标是阁楼；
5. 点击“自动放置”，确认第一件家具进入阁楼；
6. 每一房完成后重新分析，确认顺序为：阁楼、二楼大房、一楼大房、一楼小房、
   二楼小房，缺失房间自动跳过；
7. 检查每个用途房最终显示的四项整体属性；
8. 检查关闭“达标后继续填满”时，没有仅为占空间继续加入中性家具；
9. 保存、完全退出、重新进入，确认家具布局持久化且没有重复提出已完成布局；
10. 返回完整截图以及最新 `auto_cattery.log`。

实机通过条件：

- 新的空屋循环没有焦点污染；
- 单房标准分析目标为约 1–2 秒，不再出现 25–116 秒级常规等待；
- 每个计划都通过 Support 和原生执行；
- 属性方向与 F10 房间整体目标一致；
- 布局明显减少无意义空洞和零散家具；
- 不崩溃、不重复反向移动、不复用退休 stable key；
- `AC3906` 发生时向玩家明确显示方案失效，而不是看起来无响应。

## 16. 预计修改接缝

后续实施应优先检查并最小修改：

```text
src/furniture_analysis/service.cpp
src/furniture_planning/layout_solver.cpp
src/furniture_planning/layout_solver.hpp
src/ui/mew_ui_bridge.cpp
tests/furniture_layout_solver_tests.cpp
```

如需要独立的新求解器组件，可增加：

```text
src/furniture_planning/discrete_packing_solver.cpp
src/furniture_planning/discrete_packing_solver.hpp
tests/discrete_packing_solver_tests.cpp
```

UI 不得直接写游戏状态；求解器只操作不可变快照和纯候选数据；原生执行仍由现有
gateway 完成。不得把评分、搜索和原生写入重新混在同一层。

## 17. 明确不采用的捷径

- 不把用户当前存档中的家具名、stable key、坐标或房间属性硬编码进算法；
- 不通过降低 Support、密封快照或回滚安全门换取布局成功；
- 不把每房整体属性重新解释为每猫属性；
- 不单纯减小 Beam 常量并声称问题已经解决；
- 不用家具包围矩形代替真实不规则 footprint；
- 不把 NFP 当作当前离散候选组合瓶颈的直接答案；
- 不第一步就把大型 OR-Tools 运行时静态链接到 MOD DLL；
- 不承诺数学全局最优后让游戏等待无上限时间；
- 不由自动化流程启动或操作游戏完成玩家验证。

## 18. 最终建议

最小、风险可控的实施顺序是：

```text
先修 focus_room 生命周期
→ 加搜索规模和耗时日志
→ 引入 bitset 候选表示
→ 用 BLF 生成快速合法初始解
→ 用限时 Branch-and-Bound 做带权 Set Packing 改进
→ 用小规模局部搜索压紧
→ 继续通过现有 Support、密封执行和回滚门
```

该路线直接针对本次日志证明的两个根因：错误的房间焦点状态，以及多顺序、多种子
Beam Search 的组合爆炸。它保留当前已经验证的游戏几何、Support 和原生执行成果，
只替换最不适合当前离散格子问题的组合搜索核心。

# CODEX CURRENT TASK - STAGE 41 NATIVE-VALID ATTIC STACKING AND ROLLBACK

## Current objective

修复 Stage 40 实机发现的假合法终局：阁楼方案不得把家具 Hitbox 塞进另一件家具
的 Solid/Surface；应利用真正可承重的 Solid/Surface 孔位向上叠放。任一步被原生
校验拒绝时，按已提交步骤的逆序恢复本批次，不能把临时缓冲家具留在楼下房间。

## Accepted foundation

- Stage 38 原生跨房间 `remove -> validate(target_grid,false) -> commit` 已验证可保存。
- Stage 40 可枚举空房间 grid、跨房间规划、严格坐标提交，并在后续批次排除已完成
  房间中的家具来源。
- 24 猫 v0.5.27 实机在第 16 个提交后停止：`set_spider_dresser` 计划到与
  `set_modern_couch` 同层的 `(5,-9)`，原生校验拒绝；此前移到楼下的家具是临时
  缓冲，并非最终房间分配。
- 当前资源网格证明该冲突中 dresser Hitbox 会覆盖 couch Solid/Surface；同时
  7 猫实机合法布局证明 Support 既可连接家具 Solid，也可连接 Surface。

## Stage 41 completion boundary

- 规划占用同时记录 Hitbox、Solid、Support、Surface。
- Solid/Surface 不得与另一件家具的 Hitbox、Solid 或 Surface 重叠；Hitbox 不得
  覆盖已有 Solid/Surface。
- Support 仍须唯一连接房间地面、家具 Solid 或家具 Surface；不得把家具脚之间的
  空隙误判为可把整件家具嵌入的空间。
- 紧凑包围盒包含 Hitbox 与 Surface，不再只按 Solid/Support 评价视觉占用。
- 24 猫失败前存档必须继续选择 `Attic`，并生成包含上层 y 坐标的完整方案；
  `set_spider_dresser` 不得再与 couch 同层同位。
- 当前 7 猫已成功布局重复分析必须保持 0 moves，不能被新碰撞规则打散。
- 自动执行记录实际提交的 move 索引；任一后续移动失败时，按逆序严格恢复所有已
  提交 move，并单独记录回滚完成数。场景已经失效时不得盲目回滚。
- Debug/Release 同阶段并行构建；单元测试串行运行；DLL smoke、7/24 猫只读探针、
  DLL-only 部署和安装校验通过；创建一个本地 commit，不 push。

## Current evidence boundary

- 24 猫失败前只读回归：目标仍为 37x11 `Attic`；新计划不再生成 couch/dresser
  同层重叠，并把 dresser、床和小件安排到多层承重点。
- 24 猫当前部分布局只读回归：仍优先 `Attic`，不把楼下临时缓冲当成已完成分配。
- 7 猫当前已完成布局：重新分析为 0 moves、10 kept。
- 主存档继续只读，禁止执行或写入测试。

## Out of scope

- 不从仓库取家具，不改变 scale 或旋转。
- 不自动结束一天、休息、出征、组队、淘汰猫或直接写 `.sav`。
- 不将未经过当前游戏原生校验的坐标声明为实机完成。

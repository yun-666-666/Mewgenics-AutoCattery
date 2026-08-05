# AutoCattery 游戏值名称参考

更新日期：2026-08-02

这些名称用于解释本 MOD 在当前验证过的运行时中实际读取或调用的值。它们不是
跨版本 API；游戏更新后仍必须重新验证，不能直接沿用偏移或 RVA。MOD 不再用
固定文件大小或 SHA-256 阻止启用；未知布局由原生适配器逐次检查并安全失败。

## 当前运行时证据边界

| 名称 | 当前值 | 用途 |
|---|---:|---|
| `Mewgenics.exe` | 正规游戏可执行文件 | 启动时确认目标文件存在；不绑定固定版本 |
| 已验证的文件大小/SHA-256 | 见历史日志与阶段报告 | 仅作为运行时证据，不再作为启用门 |
| `HouseCat` 对象大小 | `0x118` | 运行时对象边界检查 |
| `HouseCat` vtable RVA | `0xEF4F58` | 验证对象类型 |
| `HouseCat + 0x80` | CatId | 绑定存档猫与运行时猫 |
| `HouseCat + 0xE8` | 当前房间指针 | 读取并复核实时房间 |
| 原生 House 移动 RVA | `0x2E7DB0` | 玩家二次确认后移动猫 |

## 房间与家具属性

已验证的普通目标房间 ID 为 `Attic`、`Floor1_Large`、`Floor1_Small` 和
`Floor2_Large`。房外/null 房间只表示未分配来源，不会成为目标。

| 名称 | 含义 |
|---|---|
| `Comfort` | 舒适度；作为房间用途排序输入 |
| `Stimulation` | 刺激度；繁育技能/被动与培养偏好输入 |
| `Health` | 健康度；房间用途排序输入 |
| `Mutation` | 变异属性；稳定育种阶段偏好输入 |
| `Appeal` | 吸引力；作用于全屋流浪猫质量，不参与单房用途排序 |

## 猫快照值

| 名称 | 含义 |
|---|---|
| `CatId` | 游戏内猫标识；不等同于猫名 |
| `voice_id` | 当前样本中用于推导公母 |
| `tink_sexuality` | 性取向信息解锁进度门 |
| `tink_inbreeding` | 近亲信息解锁进度门 |
| `tink_relationships` | 关系信息解锁进度门 |
| `pedigree` / COI | 游戏缓存的配对近交系数 |
| `STR/DEX/CON/INT/SPD/CHA/LCK` | 七项可遗传基础属性 |
| ability/passive IDs | 技能与被动槽；稳定全 7 阶段后用于配对评分 |
| visual part / mutation IDs | 15 个主要外观部位与变异分类 |
| life stage / age | 幼年、成年、老年等繁育资格输入 |
| class / availability | 分类与当前可用性；普通分房不因职业或战斗经历排除 |

存档来源为只读 SQLite `files.house_state` 及相关猫 blob/进度/谱系数据。具体
结构通过当前仓库的解析器和 build 门使用；未知字段保持未知，不凭名称猜测。

## 数据收集边界

可选 `AutoCatteryData` 文件只保存以上与规划有关的技术值、分类结果、移动计划
和房间属性。明确排除猫显示名、存档名/路径、操作系统用户名、机器 ID、账号 ID
和网络标识；MOD 不包含上传代码。

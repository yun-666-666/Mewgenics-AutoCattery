# AutoCattery 操作手册 / User Guide

本文解释 F10 管理面板中每个按钮、数据行和开关的实际含义。面板只在 House
场景可用；离开 House、切换存档或场景刷新时，旧预览会失效。

## 中文

### 游戏外繁育工坊

发布包中的 `AutoCatteryWorkbench` 整个文件夹放到游戏目录，与 `Mewgenics.exe`
同级。保存并退出游戏后，双击其中的 `StartBreeding.cmd` 打开本地网页；随包已带
运行环境，无需项目源码或自行安装 Python。保留启动窗口，模拟和导入期间保持游戏关闭。

选择正确账户与槽位，核对猫群和天数，设置培养参数并开始。结束后“确认使用结果”
列出目标存档、新增猫、移出猫及七项遗传属性。勾选确认框后点击“确认使用此结果”，
自动保留原档恢复副本并写回原槽位；看到成功提示后再启动游戏加载。无需下载改名。
模拟后原档有变化或游戏运行时会拒绝写回。重复确认不会重复导入。

短程测试显示“尚未达到持续繁育标准”时，有可导出后代仍可使用；没有存活的新全七
后代则不能导入。导出保留原战役天数、家具和完整谱系，新猫保留模拟年龄。数量上限
可能移出原猫，确认前查看列表。辅助开关用于本次模拟，游戏内需单独开启相应设置。

成功提示给出原槽位与 `before-apply.sav` 恢复副本的完整路径。要撤销，先退出游戏，
将恢复副本复制回该槽位并改回原文件名。实际游戏加载、使用新猫及保存后重载由玩家验收。

### 面板按钮

| 控件 | 作用 | 会改变什么 |
| --- | --- | --- |
| `F10` | 打开或关闭管理面板 | 只改变面板可见性，不改变猫或配置 |
| `Esc` | 关闭面板 | 编辑数字时先取消编辑；再次按下才关闭面板 |
| `设置` | 打开设置页 | 只显示设置，不执行整理 |
| `猫保护` | 打开保护页 | 只读取本机存档列表和猫；只有点击应用才写规则 |
| `完整预览` | 打开最近一次有效预览 | 只读，不移动或淘汰猫 |
| `关闭` | 关闭面板 | 恢复 House 的鼠标和滚轮输入 |

设置行的操作方式：点击左侧减少数值，点击右侧增加数值，点击中间直接输入；
按 `Enter` 提交，按 `Esc` 取消。布尔开关点击任意侧都会切换。提交后写入
`user_config.json`，新预览会使用新值；已经生成的旧预览不会被悄悄改写。

### 年迈猫与死亡猫顺序交付（v0.5.33）

当前本地测试版修复了旧编译对象导致的“当前存档未确认”。若猫群确实与已保存状态
不匹配，面板现在保留具体提示；请正常保存后刷新，不按数量猜测其他存档。

F10 → 年迈猫：只显示当前匹配存档中仍活着、游戏自身标记“老了”的猫。
默认按七项基础属性总和降序排列，每页9只；点击底部排序行切换为年龄降序，
点击猫卡片打开游戏原生详情。属性顺序为力/敏/体/智/速/魅/运。
列表读取已保存状态；当日变化需正常保存后点击顶部刷新。

年迈页右下角“死亡猫交付：查看预览”进入独立预览，按编号显示当前屋内已死亡、
未设置MOD保护或固定房间的猫。此时不交付；返回年迈猫或关闭面板可取消预览。
先正常保存，确认列表后点击左下方“确认按编号顺序交付给接收死猫的NPC”。
MOD不创建自动备份，确认后逐只调用原生详情下水管道入口和Organ Grinder交付路径，
确认本只猫从屋内移除且NPC结果正确后才处理下一只。不是等待某段可见动画。
执行中按Esc或F10停止后续交付；已开始的单只原生交付可能完成。
完成后收起原生猫详情页，不自动弹回管理面板；之后可从F10死亡猫预览查看结果。
日志DeadCatDelivery记录每只请求和完成。执行仍使用原生NPC界面，暂不支持同时操作其他猫或NPC。
交付后正常保存，再刷新列表；尚未保存时可能提示当前存档未确认。

交付前请正常保存。若要撤销交付，需在游戏再次保存前重新载入先前存档；
游戏一旦保存了交付结果，就不能靠原存档撤销。MOD不自动保存或推进日期。
外部 `tools/lifecycle_probe.pyw` 是历史取证工具，使用顺序交付无需打开它。

### 设置页：战斗评分

| 面板行 | 含义和改变效果 |
| --- | --- |
| `推荐猫数量`（默认 8，1-100） | 战斗评分排序中标记为“推荐”的猫数。它不是自动组队数量，也不会自动让猫出征。 |
| `最低战斗分数`（默认 0） | 分数低于此值的猫不进入战斗合格列表；提高它会减少候选，降低它会放宽候选。 |
| `最少已知属性`（默认 7，0-7） | 一只猫至少要有多少项已确认的七项基础属性才可参与战斗评分。设为 7 时，缺任一项都会被排除。 |
| `排除幼猫`（默认开） | 开启时幼猫不进入战斗推荐；关闭只取消这一层年龄过滤。 |
| `排除受伤猫`（默认关） | 开启时已确认受伤的猫不进入战斗推荐；未知受伤状态在要求确认资格时仍会被拦截。 |
| `要求资格已确认`（默认开） | 未确认战斗可用性、年龄等字段时按不合格处理；关闭后仍会保留数据限制提示，不会把未知当成已确认事实。 |
| `缺失属性惩罚`（默认 0） | 每缺少一项战斗评分所需的遗传/遗传加成/装备来源，分数扣除此值。它不会补齐缺失数据。 |
| `受伤惩罚`（默认 0） | 不排除受伤猫时，对已确认受伤的猫额外扣分。 |
| `力量/敏捷/体质/智力/速度/魅力/幸运权重`（默认各 1） | 七项属性总值分别乘以对应权重后相加。权重为 0 表示该属性不影响分数，负值会反向偏好。 |
| `推荐显示分数`、`推荐显示排名`（默认开） | 控制 House 中推荐标记是否显示分数和名次；只影响显示，不改变排序。 |

战斗分数使用已确认的遗传值、遗传加成和装备加成；移动能力和基础攻击只会被
识别，当前没有通用默认分，因此不会凭空给它们加分。

### 设置页：繁育与分类

| 面板行 | 含义和改变效果 |
| --- | --- |
| `核心繁育猫数`（默认 4） | 从繁育合格排序的前 N 只建立核心池。推荐配对的两只猫也会被纳入核心判断；它不是自动繁育次数。 |
| `升级重骰次数`（默认 3） | 允许输入 0–99；应用到全部 7 个普通职业和 7 个进阶职业。保存后必须重启游戏。检测到同级 `SkillsPassivesFirstData` 时，会把这里的当前值动态同步到它的职业补丁；该值不是固定 11，改成多少就同步多少。 |
| `后备繁育猫数`（默认 4） | 排在核心池之后的 N 只建立后备池。它们是核心猫不足或需要替换时的保留对象。 |
| `最低繁育分数`（默认 0） | 低于此分数的猫不进入繁育合格排序。 |
| `繁育最少已知属性`（默认 7，0-7） | 繁育评分至少需要多少项已确认基础属性。**截图中的 `7` 表示七项全部已知；缺一项就会显示限制并不能成为合格繁育候选。** |
| `繁育资格已确认`（默认开） | 未确认成年、可繁育等字段时不把猫当成合格繁育候选；关闭只放宽资格门，不会伪造未知字段。 |
| `繁育缺失属性惩罚`（默认 0，步进 0.25） | 每缺少一项基础属性，繁育分数扣除此值。**截图中的 `0.25` 意味着每缺一项扣 0.25 分；但当“繁育最少已知属性”为 7 时，缺失属性的猫已经因资格不足被排除，所以该惩罚主要在你把门槛调低后才会影响排序。** |
| `繁育力量/敏捷/体质/智力/速度/魅力/幸运权重`（默认各 1） | 用于单猫基础属性加权和，以及配对中逐项取父母较高基础属性后的加权分。配对仍包含全 7 覆盖、双方共同全 7 项数、COI 和性向分；稳定全 7 后才会叠加已确认的技能槽和特征权重。默认等权保持原有评分。 |
| `战斗分类优先`（默认关） | 一只猫同时进入战斗推荐和核心繁育池时，开启则把主角色显示为战斗推荐，关闭则显示为繁育核心；两个池仍都会保留。 |
| `最低战斗保留池`（默认 8） | 分类器至少保留多少只**战斗评分合格且排序靠前**的猫不作为普通候选。**截图中的 `8` 不是强制组成 8 人队，也不是当前猫数；它是安全保留下限。**实际保留数为 `max(推荐猫数量, 此值)`，但不会把不合格或未知猫硬塞进池子；合格猫不足时只给出警告并停止危险淘汰。** |
| `最低繁育保留池`（默认 8） | 与战斗保留池相同，但对象是繁育评分合格的猫。它保证繁育候选不足时不继续把普通猫当成可淘汰对象。 |
| `最低普通保留数`（默认 4） | 在战斗池和繁育池之外，按综合分数再保留的普通猫数量，用于避免把所有非核心猫都视为淘汰候选。 |
| `禁止淘汰置信度`（默认 0.85，0-1） | 数据置信度低于此值的猫不得进入淘汰预览。它只提高安全门槛，不会提高评分。 |

“保留池”是原整理分类器的边界，不是立即执行的动作；独立数量管理使用下面的
择优保留规则，不把这些分类器数值当作强制配额。

### 猫群上限与自动淘汰（当前本地测试版）

设置“猫群上限（新一天自动淘汰）”，并关闭只读模式；安全模式也必须关闭。
进入猫舍或新一天、且没有正在执行的整理批次时，MOD检查总猫数（包含屋内死亡猫）。
超过上限则读取当前存档和保护规则，自动显示超额名单，10秒倒计时结束后逐只送入
当前愿意接收该猫的NPC；所有NPC都不接收时才送原生垃圾桶，不需要再点一键整理。
接收条件由游戏原生判断，每次交付前重新读取当前进度与额度；多个NPC同时接收时
按下面的玩家优先级选择。条件读取失败则停止队列，不视为无人接收。老猫列表与死亡猫
交付仍是不同页面。

| 图示优先级（小者优先） | 接收方 | 选择规则 |
| --- | --- | --- |
| 1 | 弗兰克 | 愿意接收退休猫时优先；该猫也满足布奇条件时，布奇优先于弗兰克。 |
| 2 | 布奇、比尼斯博士 | 两者都接收时先布奇；其余按原生接收条件选择。 |
| 3 | 汀可 | 前面的NPC都不接收时选择。 |
| 4 | 特蕾西、杰克宝宝 | 前面的NPC都不接收时选择；同级任一均可，当前稳定先特蕾西。 |

死猫固定送未编号的原接收死猫NPC，不参与以上活猫排序；接收方暂不可用时停止，
不改送垃圾桶。退休资格及各NPC接收要求由游戏判断，不用年龄代替退休状态。

2026-10-01 本地修复：Tink 的性取向／族谱显示尚未解锁，也能读取存档里已经存在的
性欲、性取向和族谱数据，用于繁育配对和数量筛选；不修改游戏解锁进度。
开启避近亲时，超额筛选中的种猫保留也排除已知近亲配对，包括无法找到两组独立
血线时的后续选对。读取失败或数据确实缺失时仍显示原因，不把未知亲缘当作无亲缘。

- 优先保留保护猫、固定房间猫、冒险箱猫与身份/属性不明的猫，再保留可用种猫配对、
  新血线种源、战斗推荐猫，其余按存活、全七、遗传属性等质量排序。
  保护猫超过上限时不会强行删到上限。
- 在保护和种猫保留后，上限内最多预留两只尚未留下在屋存活后代的族谱始祖猫，尽量
  一公一母。要求族谱明确记录无父母且COI为0，与已保留种猫和另一预留猫均已知无近亲
  关系，且与保留种猫有可用异性组合；成年猫可繁育，幼猫允许保留等待成熟，低性欲、
  受伤和资料不足的猫不占该预留位置。优先年轻种源，同龄按稳定编号选择，属性不参与
  这两个位置的排序。未变化的候选群不会因为天数增加就轮换；没有候选或位置不足时
  不强行增加猫数。无父母字段或出生日期本身不能证明某只猫是今天新来的。
- 新血线预留防止低属性独立种源全部被质量排序挤出，实际繁育仍遵守原有配对和避近亲
  设置；保留不等于强制配种，不保证全七出生率或所有未来世代必然无近亲。
- Esc、F10、关闭面板或切换/编辑页面会取消本轮倒计时；翻页查看名单不取消。
  执行中Esc/F10停止后续交付，已开始的一只可能完成。
- 取消后本次猫舍访问不反复弹出；下次进入猫舍或修改上限会重新检查。
  手动入口仍是“年迈猫 → 死亡猫交付预览 → 超额猫淘汰预览”。
- 2026-10-02：房间识别改为读取已核实的房间自身名称，避免交付后的相邻内存变化
  被误认成额外房间。交付中断后留在HousePipe的猫作为待安置猫参与整理，管道不算
  居住房；无需先手动拖回，点自动整理查看预览，再确认整理即可通过原生搬猫流程
  安置回居住房。尚在冒险箱或设置不移动保护的猫仍遵守原规则。已开始的一只交付
  需先完成游戏原生界面的收尾。若仍显示预览不可用，查看同次启动的AC3105具体原因。
- 存档未唯一匹配时不自动淘汰；正常保存后可从手动入口刷新。
- MOD不自动保存、不推进天数、不创建自动备份。要撤销交付，需在游戏再次保存前
  重新加载交付前的存档；保存后的结果无法靠该存档撤销。

日志`AC4200`记录自动预览、数量与上限，`AC4201`记录无法执行的原因；
`AC19205`记录单只猫的当前接收方预览，`AC19201/AC19203`逐只记录请求与完成及
接收方编号（0至6为NPC，7为垃圾桶）。自动化回归已通过，实机效果待玩家测试。

### 设置页：房间、安全与 MOD

| 面板行 | 含义和改变效果 |
| --- | --- |
| `默认软容量`（默认 4） | 没有实测房间软容量时用于规划的舒适人数参考，不覆盖已确认的硬容量。 |
| `不限制房间人数`（默认开） | 不因软容量阻止继续分配猫；仅遵守游戏已确认的硬容量。关闭才按默认软容量限制自动分配。 |
| `软容量超额提醒`（默认 2） | 超过软容量加此值时提示拥挤，不阻止整理、不删除猫。 |
| `优先单一战斗房`（默认开） | 尽量把战斗推荐猫集中到一个已验证普通房，减少多房分散；不是自动选择出征队。 |
| `繁育配对保持同房`（默认开） | 推荐配对存在时，在阁楼建立繁育群。 |
| `繁育房猫数`（默认 4，2–1000） | 选择阁楼实际目标人数；保护与繁育资格优先。 |
| `避免近亲配对`（默认开） | 开启时推荐对和新增繁育群配对仅接受已知后代 COI 为 0 的组合；没有符合组合则不强选。关闭后允许已知非零 COI 组合参与原评分，原 COI 扣分仍保留；亲缘未知不会被当作安全。 |
| `尽量分开幼猫`（默认开） | 繁育房和战斗驻留房之外仍有可用房间时，把可移动幼猫集中到高健康、高舒适的育幼目标；固定猫优先。 |
| `只读模式`（默认开） | 只生成预览和诊断，不提交原生移动。需要实际移动时必须明确关闭。 |
| `应用前创建备份`（默认开） | 执行写操作前创建备份；备份失败会阻止执行。 |
| `单击执行模式`（默认关） | 关闭时第一次点击预览、第二次点击执行；开启会把两步合成一次，风险更高。 |
| `界面语言`（默认中文） | 只切换 AutoCattery 面板文字，不翻译游戏本体；选择会持久化。 |
| `收集猫数据（默认关闭）` | 开启后在本机生成去除猫名、存档路径、用户名、机器 ID 和账号 ID 的技术快照；不会自动联网。 |

### House 自动整理按钮

| 点击次数 | 作用 | 结果 |
| --- | --- | --- |
| 第一次 | 刷新当前运行时猫、房间、保护规则和配置，生成预览 | 不移动猫，可在“完整预览”查看 |
| 第二次 | 再次校验实时状态后执行 MoveOnly | 每个 UI tick 最多移动 8 只；每批自动刷新并继续，已到位的猫跳过，离开 House 即停止 |

只有确认一公一母推荐繁育对后，独立繁育目标房才会按实际可用公母做最佳可达
基本均衡。战斗驻留房、育幼房和普通功能房完全忽略性别；没有可靠繁育对时不会因
性别移动任何猫。固定房和 `NoMove` 居民会优先保留，比例无法均衡时预览安全降级。

关闭“繁育配对保持同房”会关闭集中繁育房及其繁育群筛选，不强制拆开原本同房的猫。
核心/后备数量和最低繁育保留池控制分类标记与保留名单，自动整理不再清空这些标记；
它们不是繁育房住猫数量的硬上限。
自动整理默认不限制每房人数，繁育群之外的猫继续安排在其他可用普通房。
“软容量超额提醒”只提示拥挤，不能阻止猫群增长后的整理。只有主动关闭“不限制房间人数”
时才使用默认软容量作为分配上限；游戏已确认的硬容量始终保留。限制开启后总空间不足时显示
`configured-or-hard-room-capacity-insufficient`，不生成超出设置的移动。
受保护居民已经超过配置人数时保留原位并报告限制，不为满足人数设置移动受保护猫。
原配置中的允许溢出开关继续兼容，默认开启即不限制人数，无需随猫群增长反复调大容量。

开启集中繁育后，阁楼固定为繁育房。F10 设置新增“繁育房猫数”（默认 4，范围 2–1000），
控制实际入房人数，与核心/后备保留数量分开。先保留最佳已知配对，再兼顾公母数量和群内
交叉配对评分补足目标人数；不再要求所有交叉组合都和首选对一样好。资格、近亲开关、
保护猫及真实容量仍有效，无法凑齐时会记录限制，不强塞不合格猫或移走保护猫。
默认 4 是扩群与遗传质量的折中建议，不是经实测证明的全7产出最优值；玩家可以调整。

可用房间读取存档实际解锁记录，空房也保留，不按有猫房间数或固定解锁顺序推算。
房间用途按家具合计的空房属性判断，不扣猫口拥挤，也不计大便。最后一只猫移出后，
有家具的空房仍保留真实属性。战斗房从阁楼以外选空房舒适最低者；同分时优先突变较高、
健康较低、刺激较低，最后按房间标识确定。只改变居住安排，不自动出征、过日或改变家具。
开启“优先单一战斗房”时，未入繁育群的可移动成年猫归战斗房。育幼房从剩余房间优先选
舒适高于 -10 的房间，再按健康、舒适、刺激从高到低比较。房间同分规则不依赖当前猫数。

| 可用房间 | 用途安排（集中繁育、单一战斗房、幼猫分离开启时） |
| --- | --- |
| 2 | 阁楼繁育；另一间战斗及幼猫共用，没有独立育幼空间。 |
| 3 | 阁楼繁育；另一间按空房属性选战斗；余下一间育幼。 |
| 4 | 阁楼繁育；选一间战斗、一间育幼；余下一间普通备用，可能为空。 |
| 5 | 阁楼繁育；按空房属性选一间战斗、一间育幼；另外两间普通备用。二楼左侧小房同样参与选择。 |

普通备用房保留固定/保护居民，其他猫按可行空间安排；不会为占满每间房而稀释繁育群。
关闭集中繁育时保留普通规划和核心/后备标记；关闭单一战斗房时不强制剩余成年猫集中。
同房不保证游戏采用某个配对或后代必出全7。出生与成长后再次整理会按新猫群重新选种。

玩家手动搬猫、换存档、场景刷新或配置/保护规则变化后，旧预览会失效并要求重新
预览。真实淘汰、自动推进天数和自动选择出征队不属于此按钮。

### 猫保护页

选择存档只用于列出猫，不会自动把它设为执行来源。必须先点击一只猫，再选择保护
等级或固定房间，最后点击“应用保护”才会写入规则：

- `禁止淘汰`：不列入淘汰候选。
- `禁止移动`：整理时不移动。
- `禁止淘汰和移动`：同时禁止两项。
- `完全不管理`：自动流程跳过该猫。
- `固定房间`：把目标房锁为当前存档中已存在的普通房；房间不存在时安全停止。

`移除保护`只删除当前选中猫的规则。规则写入
`Mewgenics\\Mods\\AutoCattery\\config\\protection.json`；默认记录为空。

### 完整预览页和数据反馈

完整预览只显示房间人数变化、性别比例、每只猫的来源/目标/性别/潜力/原因，不能
直接编辑或执行。遇到卡顿或错误时，打开“收集猫数据”，复现问题，退出游戏后把
`Mewgenics\\Mods\\AutoCattery\\AutoCatteryData` 压缩为 ZIP，上传到
[GitHub Issues](https://github.com/yun-666-666/Mewgenics-AutoCattery/issues)。Issue
请写游戏版本、猫数量、复现步骤、预期和实际结果；只上传该目录中的 JSON 技术快照，
不要上传存档、整个游戏目录、账号截图或其他个人文件，上传后关闭收集开关。

### 游戏版本

MOD 不再用固定文件大小或 SHA-256 阻止启用。启动只确认存在正规的
`Mewgenics.exe`；原生适配器运行时仍逐项检查指针、组件和调用结果。新版内部布局
变化时，移动/探针会安全失败并记录日志，面板和只读预览仍可用。

## English

This guide mirrors the Chinese section. The panel is available only in House. Leaving
House, changing saves, or refreshing the scene invalidates an old preview.

### Panel buttons

| Control | Action | What changes |
| --- | --- | --- |
| `F10` | Open or close the management panel | Visibility only; cats and settings do not change |
| `Esc` | Close the panel | Cancels numeric editing first; press again to close |
| `Settings` | Open Settings | Display only; no organization is executed |
| `Cat Protection` | Open protection controls | Reads local saves and cats; writes only after Apply |
| `Full Preview` | Open the latest valid preview | Read-only; no movement or culling |
| `Close` | Close the panel | Restores House mouse and wheel input |

For a setting row, click the left side to decrease, the right side to increase, or the
middle to type a value. Press `Enter` to commit or `Esc` to cancel. Boolean switches
toggle from either side. Values are saved to `user_config.json`; a new preview uses
them, while an existing preview is never silently rewritten.

### Settings: Combat Scoring

| Row | Meaning and effect |
| --- | --- |
| `Recommended cats` (default 8, 1-100) | Number of top eligible combat cats marked as recommended. It is not an auto-created team or expedition size. |
| `Minimum combat score` (default 0) | Cats below this score are excluded from the eligible combat ranking. |
| `Minimum known stats` (default 7, 0-7) | Required count of confirmed base stats. At 7, any missing one excludes the cat. |
| `Exclude kittens` (on) | Excludes kittens from combat recommendations. |
| `Exclude injured cats` (off) | Excludes confirmed injured cats when enabled; unknown injury remains blocked when eligibility confirmation is required. |
| `Require confirmed eligibility` (on) | Treats unconfirmed combat availability or age as ineligible; it never turns unknown data into a confirmed fact. |
| `Missing stat penalty` (0) | Subtracts this amount for each missing genetic, heredity, or equipment source. It does not invent the value. |
| `Injury penalty` (0) | Extra score deduction for confirmed injuries when injured cats are not excluded. |
| `Strength/Dexterity/Constitution/Intelligence/Speed/Charisma/Luck weight` (1 each) | Multiplies each stat's total before adding it to the combat score. Zero ignores a stat; a negative value reverses its preference. |
| `Show recommendation score` and `Show recommendation rank` (on) | Controls marker text only; it does not change ranking. |

Combat scoring uses confirmed genetic values, heredity bonuses, and equipment bonuses.
Movement and basic-attack slots are identified but have no universal default score.

### Settings: Breeding and Classification

| Row | Meaning and effect |
| --- | --- |
| `Core breeders` (default 4) | Takes the first N eligible cats in the breeding ranking as the core pool. A recommended pair is also included in the core decision; this is not a breeding count. |
| `Reserve breeders` (default 4) | Takes the next N eligible cats after the core as a replacement pool. |
| `Minimum breeding score` (0) | Excludes cats below this score from the eligible breeding ranking. |
| `Breeding known stats` (default 7, 0-7) | Required count of confirmed base stats. **The screenshot value `7` means all seven must be known; a cat missing one is limited and cannot be an eligible breeder.** |
| `Confirmed breeding eligibility` (on) | Requires confirmed adult/breeding availability. Disabling the gate does not fabricate unknown fields. |
| `Breeding missing penalty` (0, step 0.25) | Subtracts this amount for every missing base stat. **The screenshot value `0.25` means minus 0.25 per missing stat. With known stats set to 7, missing-stat cats are already excluded, so this mainly affects ordering after lowering that gate.** |
| `Breeding Strength/Dexterity/Constitution/Intelligence/Speed/Charisma/Luck weight` (1 each) | Weights individual base stats and each pair's higher parental value per stat. Pair scores also include seven-stat coverage, jointly stable stats, COI, and orientation; confirmed ability and trait weights are added at the stable all-seven stage. Default equal weights preserve previous scores. |
| `Combat role first` (off) | For a cat in both the combat recommendation and breeding core, chooses Combat as the displayed primary role when enabled; both pools still retain the cat. |
| `Minimum combat pool` (default 8) | Keeps at least this many **eligible, highest-ranked combat cats** out of the general pool. **The screenshot value `8` is not an eight-cat team and not the current cat count.** The actual target is `max(Recommended cats, this value)`. Ineligible or unknown cats are never forced into the pool; too few eligible cats produces a warning and blocks unsafe culling. |
| `Minimum breeding pool` (default 8) | The same safety boundary for eligible breeding cats. |
| `Minimum general reserve` (default 4) | Keeps this many additional cats outside both specialized pools, ordered by combined score. |
| `No-cull confidence` (default 0.85, 0-1) | Cats below this confidence cannot enter a cull preview. It raises a safety threshold, not a score. |

A retained pool is a classification and safety boundary, not an immediate action. Real
culling is disabled; even a listed candidate cannot bypass preview, protection, or the
execution gates.

### Settings: Rooms, Safety, and MOD

| Row | Meaning and effect |
| --- | --- |
| `Default soft capacity` (4) | Planning reference when no measured soft capacity is available; it never overrides known hard capacity. |
| `Allow soft overflow` (on) | Allows planning above soft capacity while still respecting hard capacity. |
| `Maximum room overflow` (2) | Maximum soft-capacity overflow per room before reporting a capacity gap. |
| `Prefer one combat room` (on) | Tries to stage combat recommendations in one verified ordinary room; it does not select an expedition team. |
| `Keep breeding pair together` (on) | Tries to place a recommended pair in the same room. |
| `Avoid inbreeding pairs` (on) | Avoids confirmed close relatives; unknown relationships are not treated as safe. |
| `Separate kittens when possible` (on) | When a room remains after breeding and combat staging targets are selected, movable kittens receive a high-health/high-comfort nursery target; fixed cats remain authoritative. |
| `Read-only mode` (on) | Generates previews and diagnostics only; native movement is not submitted. |
| `Create backup before apply` (on) | Creates a backup before a write; backup failure blocks the write. |
| `Single-click execution` (off) | Off means first click previews and second click applies; on combines both steps and is riskier. |
| `Interface language` (Chinese) | Changes AutoCattery text only, not the base game; the choice is persisted. |
| `Collect cat data (off by default)` | Writes local technical snapshots without names, save paths, usernames, machine IDs, or account IDs; nothing is uploaded automatically. |
| `Level-up rerolls` (3) | Accepts 0–99 for all seven base and seven advanced player classes. Restart the game after saving. If sibling `SkillsPassivesFirstData` is installed, its class patches are dynamically synchronized to the current value; the value is not fixed at 11. |

### Auto-Organize Cattery

| Click | Action | Result |
| --- | --- | --- |
| First | Refresh live cats, rooms, protection, and settings, then build a preview | No cat moves; inspect it in Full Preview |
| Second | Revalidate live state and run MoveOnly | Moves at most eight cats per UI tick, automatically refreshes between batches, skips cats already in place, and stops after leaving House |

Only a confirmed female/male breeding pair enables basic sex balancing in the
separate breeding target room. Combat staging, nursery, and ordinary rooms
ignore sex completely. Breeding balances viable comfort with stimulation;
combat staging prefers health and comfort instead of taking the highest-
stimulation room. Fixed-room and `No Move` residents are kept first, and an
unreachable 1:1 mix degrades safely instead of invalidating the preview.
Remaining breeding-room slots form one cross-compatible cohort. Each new cat
must have an eligible pairing with every already selected opposite-sex cat;
the planner maximizes the weakest cross-pair score, then minimizes worst cached
offspring COI and maximizes average score. Sharing a room still does not
guarantee which pair the game will choose.

Manual movement, save changes, scene refreshes, or changed settings/protection invalidate
the old preview. Real culling, automatic day advance, and automatic expedition selection
are outside this button.

### Cat Protection

Selecting a save only chooses which cats to list. Select one cat, choose a level or fixed
room, then click `Apply Protection` to write a rule:

- `No Cull`: never include the cat in cull candidates.
- `No Move`: never move the cat during organization.
- `No Cull or Move`: both restrictions.
- `Fully Unmanaged`: skip the cat in the automatic workflow.
- `Fixed room`: lock the target to an existing ordinary room; a missing room stops safely.

`Remove Protection` removes only the selected cat's rule. Rules are stored in
`Mewgenics\\Mods\\AutoCattery\\config\\protection.json`; the default record list is empty.

### Full Preview and issue feedback

Full Preview shows room totals, sex ratios, and each planned cat's source, target, sex,
potential, and reason. It cannot edit or execute a plan. For lag or errors, enable
`Collect cat data`, reproduce the issue, exit the game, compress
`Mewgenics\\Mods\\AutoCattery\\AutoCatteryData` into a ZIP, and attach it to a new
[GitHub Issue](https://github.com/yun-666-666/Mewgenics-AutoCattery/issues). Include game
version, cat count, reproduction steps, expected result, and actual result. Upload only
the JSON technical snapshots from that directory, never saves, the whole game directory,
account screenshots, or other personal files; turn collection off afterward.

### Game version

The MOD no longer refuses to enable because `Mewgenics.exe` has a fixed size or SHA-256.
Startup only checks for a regular executable; native adapters still validate pointers,
components, and call results at runtime. A changed internal layout can safely reject a
move/probe and log the reason while the panel and read-only preview remain available.

### 全七之后的技能、被动和变异繁育

本版修正 format-19 被动/疾病读取：前十个字符串的最后四项是主动技能缓存，真正的两个被动和两个疾病来自后续四个字符串/等级记录。配对、种猫排名、战斗推荐现在读取实际特性。

部署时 `tools/breeding_trait_profile.py` 从本机当前 `resources.gpak` 生成效果评分，写入 MOD 的 `config/default_config.json`；用户 `user_config.json` 的单项权重仍优先。技能参考伤害、已识别的控制/增益、范围和消耗；被动参考一级属性/效果；变异参考属性净收益与已识别效果。无已识别收益的外观变异不因稀有或数量多自动加分。这是通用效果估分，不能覆盖所有职业、装备、条件效果和技能组合；并非绝对强度榜。

开启且实际可写的“后代遗传全七辅助”时，亲本可以以遗传特性优先选择，允许引入属性稍低但特性更好的种源；关闭辅助、只读、安全模式时仍优先原有全七/属性配对。避近亲、独立血系、玩家保护和预览确认继续生效。繁育模拟也按该效果模型每日重选亲本。

技能、被动、疾病和身体仍按游戏原生规则随机遗传；新生辅助只保证七项基因属性，不保证技能/被动/变异必传或将升级等级传下去。整理后的配对与数量保留同步使用特性优先级，繁育环境在舒适度相当时优先有利于遗传的刺激度。

验证：重启游戏，保持原辅助开关，重新生成整理预览，确认有较好特性的猫进入繁育组；推进一天后看新生猫的七项基础属性与实际技能、被动、变异。连续几天记录亲本与后代才可判断实际保留效果。开启“收集猫数据”后，新快照的 `ability_slots` 第 7–10 项应为真正的被动/疾病，不再重复主动技能。

# AutoCattery

[English](README_EN.md)

AutoCattery 是一个面向 Windows x64 Mewgenics 的自动猫舍管理 MOD。
本项目由 **OpenAI GPT-5.6** 根据项目需求、代码审查和玩家实机反馈制作；
它不是游戏官方组件，不修改游戏原始文件，也不直接操作 Steam Cloud。
游戏内 MOD 通过原生运行时操作猫舍；游戏外繁育工坊仅在玩家确认使用结果后写回所选本地存档，并保留恢复副本。

当前发布版本为 **v0.5.35 正式版（2026-10-07）**，包含自 v0.5.34 以来的最新 MOD 和繁育工坊。
下载与完整变更见 [GitHub Release](https://github.com/yun-666-666/Mewgenics-AutoCattery/releases/tag/v0.5.35)
和 [发布说明](docs/RELEASE_NOTES_v0.5.35.md)。玩家已于 2026-10-07 确认当前 MOD 实际游玩正常，本版作为正式 Latest Release 发布。

## 当前能力

- 确认整理后同时清除各房间大便；没有猫需要换房时也会清理，普通家具不受影响。

- 在稳定的 `House` 场景显示“自动整理猫舍”按钮。
- 第一次点击生成预览，第二次点击通过当前游戏运行时可用的原生 House 房间路径
  执行移动。大计划每个 UI tick 最多提交 8 次原生移动，每批后刷新并重验实时
  房间；离开 House 会取消尚未执行的批次。
- 预览使用当前游戏内 CatId→房间映射覆盖尚未保存的旧存档分布；若玩家在
  预览后手动搬猫，旧预览会失效且不执行任何移动。
- 每次进入 House 和每次预览前刷新运行时数据，按当前猫数量选择相应存档，
  避免换存档后继续使用上一个存档。
- 暂时位于普通房间外、但仍属于当前 House 的猫会作为“未分配来源”参与
  下一次预览，并只移动到已验证的普通房间。
- 支持从当前存档与运行时确认的 2–5 房布局：
  - `Attic`
  - `Floor1_Large`
  - `Floor1_Small`
  - `Floor2_Large`
  - `Floor2_Small`
- 按实际可用房间规划人数，并读取当前游戏家具表计算每个房间的舒适、
  刺激、健康、变异和吸引力；具体猫按实际房间用途分配，不按固定房序。
- 只有存在已确认的一公一母推荐繁育对时，才会在独立繁育目标房内按全屋实际
  可用公母做到最佳可达的基本均衡；固定或不可移动猫会被计入并安全降级。
  战斗驻留、育幼和普通功能房完全忽略性别，不会为了公母比例替换高潜力猫或
  额外搬猫。战斗驻留房优先健康与舒适，不再错误占用最高刺激房；开启“尽量
  分开幼猫”且存在额外房间时，会建立独立的健康/舒适育幼目标。人数相同但
  具体猫被手动换房后，重复整理仍会恢复属性目标。
- 读取当前已验证存档格式中的七项基础属性、性欲、性取向及实际族谱；Tink 的
  显示解锁不会阻断已有数据读取，也不会被 MOD 自动解锁。未知亲缘仍不当作安全。
- 从已知一公一母组合中按七维基础属性、游戏缓存 COI 和性取向选择成年配对。
  全七辅助实际可写时可优先遗传特性；否则先保证属性覆盖，稳定后再考虑主动技能、
  被动、疾病、正负变异与出生缺陷。各正向特性类别采用有界评分，避免单类淹没其他类别。
  繁育房不固定为阁楼：排除禁止繁育房，优先舒适 `> -10` 的环境，按舒适、刺激、健康、
  变异四项等权合计选房；稀有家具及已安装合并家具 MOD 的强化倍率计入效果。首选配对之外
  的繁育房槽位按“整个房内任意异性交叉组合”的最弱配对分数、最坏后代 COI 和
  平均配对分数逐猫选择，不再只挑若干彼此独立的高分配对；这能适应游戏在同房
  猫之间实际交叉选配，但仍不声称能锁定某一对。
- F10 面板中的“猫保护”会自动列出本机存档与猫；只有玩家点击
  “应用保护”才写入 `NoCull`、`NoMove`、`NoCullOrMove`、
  `FullyUnmanaged` 或 `fixed_room`。规则绑定稳定猫指纹，不绑定某个存档文件。
- 保护规则在预览和执行前各读取一次，期间规则发生变化会取消执行并要求
  重新预览。
- 不以战斗状态、是否战斗过、职业或受伤状态排除猫。
- 8 猫和 25 猫两房存档已通过玩家实机自动分房验证，目标分别为 `4/4`
  和 `13/12`。
- 玩家已确认当前 v0.5.24 的两个普通 House 按钮、F10、2/3/4 房 MoveOnly、
  状态变化取消、房外猫、保存后重进和重复执行等当前主流程通过。
- 提供次日战斗猫推荐、猫详情入口、游戏内设置、猫保护和离线恢复源码工具。
- House 内按 `F10` 打开管理面板；“完整预览”与“设置”“猫保护”“关闭”同级，
  显示逐猫来源、目标、性别、潜力、移动原因，以及各房间整理前后人数和公母比例。
- 中文和英文界面可在 F10 设置页最下方切换；默认中文。当前 build 没有已验证的
  游戏语言读取接口，因此不猜测游戏设置，使用明确的手动选择并持久化。
- F10 设置页可把普通与进阶职业猫的升级重骰次数设为 `0`–`99`，默认 `3`；
  保存后需重启游戏。检测到同级的 `SkillsPassivesFirstData` 时，会把控制面板
  当前值动态同步到它已验证有效的普通与进阶职业补丁；改成 7、11 或其他值时，
  两个数据 MOD 都使用同一当前值，不固定为某个数字，也不会再被旧的 3 覆盖。
- 可在 F10 设置页最下方选择是否收集猫数据；默认关闭。开启后只在本机
  `Mods\AutoCattery\AutoCatteryData` 写入用于优化规划的技术数据，不记录猫名、
  存档名/路径、系统用户名、机器 ID 或账号 ID，也不会自动联网上传。需要帮助时，
  玩家可按下方“数据反馈”流程手动压缩并附加到本仓库的 GitHub Issue。

一键整理继续执行 `MoveOnly`。房间属性、第一阶段繁育配对、基础属性默认
读取、房间身份缓存、房外猫原生搬入和详细预览均已由玩家确认。
v0.5.35 另有独立数量管理：关闭只读/安全模式后，进入猫舍或新一天时
按配置上限检查超额猫，显示名单并倒计时 10 秒后送入垃圾桶；Esc/F10/关闭可取消
本轮，玩家保护、固定房间和数据不明的猫保留。取消后本次猫舍访问不重复启动，
下次进入猫舍或更改上限后重新检查。玩家已确认当前 MOD 实际游玩正常。见
[`docs/USER_GUIDE.md`](docs/USER_GUIDE.md) 和
[`docs/pre-completion-functional-roadmap.md`](docs/pre-completion-functional-roadmap.md)。

### F10 管理面板

在 House 场景按 `F10` 打开或关闭面板，`Esc` 关闭。三个功能页为：

- **设置**：修改全部规划参数、界面语言和可选猫数据收集。
- **猫保护**：为猫设置 `NoCull`、`NoMove`、`NoCullOrMove`、
  `FullyUnmanaged` 或固定房间。
- **完整预览**：点击一次“自动整理猫舍”生成计划后，可在这里查看房间汇总与
  每只计划移动猫的详情；该页本身只读，不会移动猫。

为解决 25/79 猫存档的面板卡顿，面板会在挂载时缓存原生 MewUI 文字节点，并
只提交发生变化的文字和帧。推荐列表也缓存文字节点；当天推荐不可用时的状态
同步是幂等的，不再每帧清空列表或扫描整个 House 场景。House UI 根只从当前
场景挂载一次：去重组件根后在首个包含 MOD 资源的根停止，不再读取全部组件
类型，也不会在正常渲染期间重复扫描。挂载失败最多每 500 ms 重试一次。

## 安装

请在 [v0.5.35 Release](https://github.com/yun-666-666/Mewgenics-AutoCattery/releases/tag/v0.5.35)
选择 `AutoCattery-v0.5.35-MOD-only.zip` 可只安装 MOD、配置、数据补丁和文档，不包含繁育工坊或 Python 运行环境。
这个包保留完整 F10 控制面板，包括“后代遗传全七辅助”和“供食辅助”，无需工坊即可使用。
需要游戏外培养功能时，再自行下载 `AutoCattery-v0.5.35-Workbench.zip`；
也可下载 `AutoCattery-v0.5.35-Windows-x64.zip` 一次取得 MOD 与繁育工坊。
`AutoCattery-v0.5.35-source.zip` 包含源码和第三方源码依赖，供修改或自行构建。

### 安装前置 MOD

AutoCattery 需要先安装并启用 [Mewjector](https://github.com/githubuser508/mewjector)
和 [Mewtator](https://github.com/dancomstock/mewtator)：

1. 按 [Mewjector](https://github.com/githubuser508/mewjector) 的发布说明将它安装到
   Mewgenics 游戏根目录，确认
   `version.dll` 与 `Mewgenics.exe` 同级，并保留游戏根目录的 `mods\\` 文件夹。
2. 按 [Mewtator](https://github.com/dancomstock/mewtator) 的发布说明安装并运行一次，确认游戏根目录存在
   `Mewtator\\config.json`。将其中的 `mod_folder` 设置为 Mewtator 的
   `mods\\` 文件夹（通常是 `Mewtator\\mods`），并在该目录使用
   `modlist.txt` 管理数据 MOD。
3. 确认两个前置 MOD 可以正常工作后，再按下面步骤安装 AutoCattery；完成后
   必须通过 Mewtator 启动 Mewgenics，直接从 Steam 启动不会加载 Mewtator 数据 MOD。

Windows 发布包安装步骤：

1. 解压 `MOD-only.zip` 或 `Windows-x64.zip`。不要再套一层压缩包目录到 `mods` 中。
2. 将压缩包内 `Mewjector\\mods\\AutoCattery.dll` 和
   `Mewjector\\mods\\AutoCattery\\` 复制到游戏根目录的 `mods\\`（或
   Mewjector 实际扫描的同名目录）。
3. 将压缩包内 `Mewtator\\AutoCattery\\` 复制到 Mewtator 的 `mods\\` 文件夹，
   使它成为 `Mewtator\\mods\\AutoCattery\\`；并在该目录的 `modlist.txt` 中
   加入一行 `AutoCattery`。
4. **可选**：需要游戏外繁育工坊时，将独立 `Workbench.zip` 或完整包中的整个
   `AutoCatteryWorkbench` 文件夹复制到游戏根目录，与 `Mewgenics.exe` 同级。
   工坊已带运行环境，无需安装 Python；只使用游戏内 MOD 时跳过此步。
5. 通过 Mewtator 启动游戏；Mewjector 和兼容的数据 MOD 加载器是前置条件，
   只从 Steam 直接启动不会加载 Mewtator 数据 MOD。

进入 `House` 后按 `F10` 打开面板。第一次点击“自动整理猫舍”只生成预览，确认
房间和猫的来源/目标后再次点击才执行 MoveOnly；按 `Esc` 或“关闭”退出面板。
设置、猫保护、完整预览和每个设置项的影响见
[`docs/USER_GUIDE.md`](docs/USER_GUIDE.md)。

运行时 DLL 位于 `Mewgenics\\Mods\\AutoCattery.dll`，配置和日志位于
`Mewgenics\\Mods\\AutoCattery\\`。按钮 SWF 和文本补丁由已启用的 Mewtator
数据 MOD 提供。

### 游戏外繁育模拟与确认使用

保存并退出游戏后，双击 `AutoCatteryWorkbench/StartBreeding.cmd` 打开本地网页。
保持启动窗口开启，核对账户和存档槽位后开始培养。模拟及导入期间保持游戏关闭。
结束后查看新增猫、移出猫和写回槽位，勾选确认框，再点击“确认使用此结果”。
工具自动保留原档恢复副本并写回所选槽位；成功提示后再启动游戏加载，无需手动
下载、改名或替换存档。网页会拒绝游戏运行期间的模拟/导入，以及模拟后已变化的原档。
短程结果未达到连续30日标准时仍可使用；没有存活新全七猫时不会提供导入按钮。
恢复副本位置显示在成功提示中，详细步骤见随包 `AutoCatteryWorkbench/README.md`。

两个辅助开关首次默认勾选，后续记住玩家的选择；网页设置不改变游戏内 MOD 设置。
“实际安排 / 目标配对”显示每日执行情况，房间不足时多对可共用繁育房，仍为其他猫保留空间；
种猫资格和原生随机繁育继续生效，目标 4 对不保证每日 4 窝。新培养的猫使用游戏原生随机名，
预览与导入名称一致，已有猫不会被重命名。

## 可选猫数据和数据反馈

此功能默认关闭。开启后，每次成功生成新预览会按快照摘要写入一份 JSON，重复
快照不会无限重复写入。数据包括 CatId、房间、性别、年龄阶段、基础属性、技能/
被动/变异 ID、分类、保护结果、计划移动和房间属性，用来复现和优化自动规划。
关闭后不会继续写入；已有文件由玩家自行保留或删除。

### 数据反馈（GitHub Issues）

1. 在 F10“设置”页打开“收集猫数据（默认关闭）”。
2. 重新进入 House，复现一次卡顿、错误预览、无法移动或版本更新后的兼容问题。
3. 退出游戏后，把 `Mewgenics\Mods\AutoCattery\AutoCatteryData` 整个文件夹压缩
   为 ZIP。只上传这个压缩包，不要上传存档、`user_config.json`、日志目录以外的
   个人文件、截图中的账号信息或整个游戏目录。
4. 打开 [GitHub Issues](https://github.com/yun-666-666/Mewgenics-AutoCattery/issues)，
   新建 Issue，说明游戏版本、猫数量、复现步骤和预期/实际结果，并把 ZIP 拖到
   Issue 编辑框上传。上传后可在 F10 设置中关闭数据收集。

收集文件是为了让我分析规划和性能路径；它不包含猫名、存档名/路径、系统用户名、
机器 ID 或账号 ID。上传前仍请玩家自行检查压缩包内容，确认没有额外文件。

## 管理猫保护

进入 House 后按 `F10`，选择“猫保护”。选择一份存档只是为了列出其中的猫，
不会把该存档自动设为保护来源；选择猫和保护级别后，必须点击“应用保护”才会
写入规则。“移除保护”只删除当前猫的规则。项目不再构建或发布辅助 EXE。

规则保存在 `Mods\AutoCattery\config\protection.json`。仓库和安装目录的默认
`records` 永远为空；MOD 不会自动保护或针对玩家的任何存档、CatId 或猫名。
同一 CatId 出现在不同存档时，会以出生信息等稳定指纹区分；改名或搬房不会
丢失保护。

手动格式示例：

```json
{
  "schema_version": 1,
  "records": [
    {
      "cat_id": 123,
      "level": "NoMove",
      "identity_token": "由保护管理器生成"
    },
    {
      "cat_id": 456,
      "level": "NoCull",
      "identity_token": "由保护管理器生成",
      "fixed_room": "Attic"
    }
  ],
  "blacklist": []
}
```

建议使用保护管理器生成真实 `identity_token`，不要照抄示例文本。
`fixed_room` 必须是当前存档实际存在的房间 ID；文件损坏或目标房不存在时，
本次自动移动会停止。

## 构建

```powershell
.\tools\build.ps1 -Configuration Debug
.\tools\build.ps1 -Configuration Release
```

## 部署

```powershell
.\tools\deploy.ps1 -GameRoot '<GAME_ROOT>'
.\tools\verify_install.ps1 -GameRoot '<GAME_ROOT>'
```

Mewjector 只扫描游戏目录下即时的 `Mods`/`mods` DLL，因此运行 DLL 位于：

```text
Mewgenics\Mods\AutoCattery.dll
```

配置和日志位于：

```text
Mewgenics\Mods\AutoCattery\
```

按钮 SWF 与文本补丁仍由已启用的 Mewtator 数据 MOD 提供。

## 致谢与参考

感谢 Mewgenics MOD 社区以及下列项目。它们只作为接口、结构、兼容性或规划思路
参考，AutoCattery 没有复制其闭源二进制、SWF、FLA、游戏资源或个人数据：

- [Mewjector](https://github.com/githubuser508/mewjector)：运行时 DLL 加载和模块
  注册接口；AutoCattery 通过它进入游戏进程。
- [MewUI API](https://github.com/Pseudonym-Tim/mewgenics-ui-api)：House 场景发现、
  MewUI 生命周期、节点查找、文字写入和输入拦截接口。
- [JSON for Modern C++](https://github.com/nlohmann/json)：配置、保护规则和本地
  诊断 JSON 的解析与序列化。
- **AutoCattery Codex Toolkit**（用户提供的 MIT 参考工具包）：确定性评分、保留池
  分类、保护权限交集和预览/执行边界；游戏字段和容量示例均未直接照抄。
- **Push To Meow**：作为 MOD 目录结构、加载兼容性和发布形态的参考。
- **Quick-Cleanup**：作为房间整理工具的用户流程和安全提示参考。

本 MOD 的代码和文档由 **OpenAI GPT-5.6** 在项目方向和实机验证指导下
制作。完整第三方许可与版本记录见 [`ACKNOWLEDGEMENTS.md`](ACKNOWLEDGEMENTS.md)
和 [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md)。按钮逐项说明见
[`docs/USER_GUIDE.md`](docs/USER_GUIDE.md)。

# AutoCattery

[English](README_EN.md)

AutoCattery 是一个面向 Windows x64 Mewgenics 的自动猫舍管理 MOD。
本项目由 **OpenAI GPT-5.6** 根据项目需求、代码审查和玩家实机反馈制作；
它不是游戏官方组件，也不会修改游戏原始文件、Steam Cloud 或玩家存档数据库。

## 当前能力

- 在稳定的 `House` 场景显示“自动整理猫舍”按钮。
- 第一次点击生成预览，第二次点击通过当前游戏运行时可用的原生 House 房间路径
  执行移动。
- 预览使用当前游戏内 CatId→房间映射覆盖尚未保存的旧存档分布；若玩家在
  预览后手动搬猫，旧预览会失效且不执行任何移动。
- 每次进入 House 和每次预览前刷新运行时数据，按当前猫数量选择相应存档，
  避免换存档后继续使用上一个存档。
- 暂时位于普通房间外、但仍属于当前 House 的猫会作为“未分配来源”参与
  下一次预览，并只移动到已验证的普通房间。
- 支持当前已确认的 2、3、4 房布局：
  - `Attic`
  - `Floor1_Large`
  - `Floor1_Small`
  - `Floor2_Large`
- 按实际 2、3、4 房均衡人数，并读取当前游戏家具表计算每个房间的舒适、
  刺激、健康、变异和吸引力；具体猫按实际房间用途分配，不按固定房序。
- 公母足够时，每个至少 2 只的目标房保留一公一母；人数相同但具体猫被
  手动换房后，重复整理仍会恢复属性目标。
- 七项基础属性在早期存档中已经存在，因此默认读取；性取向和亲缘只在
  `tink_sexuality`、`tink_inbreeding`、`tink_relationships` 对应进度
  实际解锁后读取，未解锁维度不参与评分。
- 解锁完整繁育信息后，按七维基础属性缺口、游戏缓存 COI 和性取向选择成年
  配对；稳定全 7 前不启用技能/变异权重，稳定后才按技能、被动、疾病、普通
  变异和出生缺陷优化配对，并继续选择高刺激、其次高变异属性的繁育房。
- 外部配置编辑器中的“管理猫保护”会自动列出本机存档与猫；只有玩家点击
  “应用保护”才写入 `NoCull`、`NoMove`、`NoCullOrMove`、
  `FullyUnmanaged` 或 `fixed_room`。规则绑定稳定猫指纹，不绑定某个存档文件。
- 保护规则在预览和执行前各读取一次，期间规则发生变化会取消执行并要求
  重新预览。
- 不以战斗状态、是否战斗过、职业或受伤状态排除猫。
- 8 猫和 25 猫两房存档已通过玩家实机自动分房验证，目标分别为 `4/4`
  和 `13/12`。
- 提供次日战斗猫推荐、猫详情入口、外部配置编辑器、备份和离线恢复工具。
- House 内按 `F10` 打开管理面板；“完整预览”与“设置”“猫保护”“关闭”同级，
  显示逐猫来源、目标、性别、潜力、移动原因，以及各房间整理前后人数和公母比例。
- 中文和英文界面可在 F10 设置页最下方切换；默认中文。当前 build 没有已验证的
  游戏语言读取接口，因此不猜测游戏设置，使用明确的手动选择并持久化。
- 可在 F10 设置页最下方选择是否收集猫数据；默认关闭。开启后只在本机
  `Mods\AutoCattery\AutoCatteryData` 写入用于优化规划的技术数据，不记录猫名、
  存档名/路径、系统用户名、机器 ID 或账号 ID，也不会自动联网上传。需要帮助时，
  玩家可按下方“数据反馈”流程手动压缩并附加到本仓库的 GitHub Issue。

当前实时执行能力是 `MoveOnly`。房间属性、第一阶段繁育配对、基础属性默认
读取、房间身份缓存与房外猫原生搬入均已由玩家确认。真实淘汰、
详细预览已完成；真实淘汰仍未完成。见
[`docs/pre-completion-functional-roadmap.md`](docs/pre-completion-functional-roadmap.md)。

### F10 管理面板

在 House 场景按 `F10` 打开或关闭面板，`Esc` 关闭。三个功能页为：

- **设置**：修改全部规划参数、界面语言和可选猫数据收集。
- **猫保护**：为猫设置 `NoCull`、`NoMove`、`NoCullOrMove`、
  `FullyUnmanaged` 或固定房间。
- **完整预览**：点击一次“自动整理猫舍”生成计划后，可在这里查看房间汇总与
  每只计划移动猫的详情；该页本身只读，不会移动猫。

为解决 25/79 猫存档的面板卡顿，面板会在挂载时缓存原生 MewUI 文字节点，并
只提交发生变化的文字和帧，不再为每一行、每一次刷新扫描整个 House 场景。

### 游戏版本兼容策略

MOD 不再用固定文件大小或 SHA-256 阻止启用。启动时只确认游戏目录包含正规的
`Mewgenics.exe`；运行时原生适配器仍会对指针、组件和调用结果逐项检查。若新版
游戏改变了内部布局，相关原生移动或探针会安全失败并记录日志，面板、只读预览
和外部编辑器仍可使用。版本更新后请先预览，不要在未确认日志和结果前连续执行
移动；遇到异常请按“数据反馈”提交压缩包。

## 安装

请在 GitHub Releases 下载 `AutoCattery-vX.Y.Z-Windows-x64.zip`。
`AutoCattery-vX.Y.Z-source.zip`：`source.zip` 是源码内容，方便对MOD做出更改。

Windows 发布包安装步骤：

1. 解压 `Windows-x64.zip`。不要再套一层压缩包目录到 `mods` 中。
2. 将压缩包内 `Mewjector\\mods\\AutoCattery.dll` 和
   `Mewjector\\mods\\AutoCattery\\` 复制到游戏根目录的 `mods\\`（或
   Mewjector 实际扫描的同名目录）。
3. 将压缩包内 `Mewtator\\AutoCattery\\` 复制到 Mewtator 的 `mod_folder`，
   并在该目录的 `modlist.txt` 中加入一行 `AutoCattery`。
4. 通过 Mewtator 启动游戏；Mewjector 和兼容的数据 MOD 加载器是前置条件，
   只从 Steam 直接启动不会加载 Mewtator 数据 MOD。

进入 `House` 后按 `F10` 打开面板。第一次点击“自动整理猫舍”只生成预览，确认
房间和猫的来源/目标后再次点击才执行 MoveOnly；按 `Esc` 或“关闭”退出面板。
设置、猫保护、完整预览和每个设置项的影响见
[`docs/USER_GUIDE.md`](docs/USER_GUIDE.md)。

运行时 DLL 位于 `Mewgenics\\Mods\\AutoCattery.dll`，配置和日志位于
`Mewgenics\\Mods\\AutoCattery\\`。按钮 SWF 和文本补丁由已启用的 Mewtator
数据 MOD 提供。

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

运行 `Mods\AutoCattery\AutoCatterySettings.exe`，点击“管理猫保护”。选择一份
存档只是为了列出其中的猫，不会把该存档自动设为保护来源；选择猫和保护级别
后，必须点击“应用保护”才会写入规则。“移除保护”只删除当前猫的规则。

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

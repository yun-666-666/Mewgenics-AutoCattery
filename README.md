# AutoCattery

AutoCattery 是一个面向 Windows x64 Mewgenics 的自动猫舍管理 MOD。

## 当前能力

- 在稳定的 `House` 场景显示“自动整理猫舍”按钮。
- 第一次点击生成预览，第二次点击通过当前游戏 build 的原生 House 房间路径
  执行移动。
- 预览使用当前游戏内 CatId→房间映射覆盖尚未保存的旧存档分布；若玩家在
  预览后手动搬猫，旧预览会失效且不执行任何移动。
- 每次进入 House 和每次预览前刷新运行时数据，按当前猫数量选择相应存档，
  避免换存档后继续使用上一个存档。
- 支持当前已确认的 2、3、4 房布局：
  - `Attic`
  - `Floor1_Large`
  - `Floor1_Small`
  - `Floor2_Large`
- 按实际 2、3、4 房均衡人数，并以最少移动为第一优先。
- 把潜力前列猫分散到各房；公母足够时，每个至少 2 只的目标房保留一公一母。
- 不以战斗状态、是否战斗过、职业或受伤状态排除猫。
- 8 猫和 25 猫两房存档已通过玩家实机自动分房验证，目标分别为 `4/4`
  和 `13/12`。
- 提供次日战斗猫推荐、猫详情入口、外部配置编辑器、备份和离线恢复工具。

当前实时执行能力是 `MoveOnly`。真实淘汰、完整繁育配对、房间实际属性读取
和详细预览界面仍未完成。权威状态与剩余功能见
[`docs/pre-completion-functional-roadmap.md`](docs/pre-completion-functional-roadmap.md)。

## 构建

```powershell
.\tools\build.ps1 -Configuration Debug
.\tools\build.ps1 -Configuration Release
```

## 部署

```powershell
.\tools\deploy.ps1 -GameRoot 'D:\steam\steam\steamapps\common\Mewgenics'
.\tools\verify_install.ps1 -GameRoot 'D:\steam\steam\steamapps\common\Mewgenics'
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

## 当前验证证据

- 当前支持的 `Mewgenics.exe`：
  - 大小：`21,981,184` bytes
  - SHA-256：`C3A41E436A93FA58CD386EC46DAD5C2A6F21A583D33C3A57A15A2604C726439E`
- 2026-07-30 玩家验证：
  - 8 猫存档正确选择 8 猫快照，原生移动提交 6 只。
  - 25 猫存档正确选择 25 猫快照，原生移动提交 10 只。
  - 重复执行提交 0 只，不重复移动已到位的猫。
- 2026-08-01 玩家与自动化验证：
  - 25 猫两房目标 `13/12`，8 猫两房目标 `4/4`。
  - 10 猫四房目标 `3/3/2/2`。
  - 两份两房存档首次均提交 2 次移动，重复执行提交 0 次。
  - 玩家手动搬猫暴露旧预览仍使用未保存 `house_state` 的问题；实时房间覆盖
    和执行前全量失效门已通过自动化验证并部署，待玩家复测。
  - Debug/Release CTest 均为 5/5 通过。

历史阶段文档仅记录当时证据，不应覆盖本 README 和当前路线图。

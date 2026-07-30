# AutoCattery

AutoCattery 是一个面向 Windows x64 Mewgenics 的自动猫舍管理 MOD。

## 当前能力

- 在稳定的 `House` 场景显示“自动整理猫舍”按钮。
- 第一次点击生成预览，第二次点击通过当前游戏 build 的原生 House 房间路径
  执行移动。
- 每次进入 House 和每次预览前刷新运行时数据，按当前猫数量选择相应存档，
  避免换存档后继续使用上一个存档。
- 支持当前已确认的 2、3、4 房布局：
  - `Attic`
  - `Floor1_Large`
  - `Floor1_Small`
  - `Floor2_Large`
- 按潜力选择高潜力猫，不以战斗状态、是否战斗过、职业或受伤状态排除猫。
- 在公母数据可用时，避免高潜力目标组全为同一性别。
- 8 猫和 25 猫两房存档已通过玩家实机自动分房验证。
- 提供次日战斗猫推荐、猫详情入口、外部配置编辑器、备份和离线恢复工具。

当前实时执行能力是 `MoveOnly`。真实淘汰、完整繁育配对、全房间人数/性别
优化和详细预览界面仍未完成。权威状态与剩余功能见
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

历史阶段文档仅记录当时证据，不应覆盖本 README 和当前路线图。

# AutoCattery v0.5.34

## 中文

- 自动整理猫舍现在会在确认后，一并清除各房间的大便；没有猫需要换房时也会清理。
- 首次点击仍生成预览，确认按钮仍显示“确认整理”。普通家具与其他物品不受影响。
- 修复自动整理与推荐战斗猫按钮在临时禁止交互后可能无法继续点击的问题，保持原生按钮更新，仅控制交互权限。
- 清理结果记录在 `AC19300` 日志中。

玩家已使用最终版本游玩多次，反馈未再出现问题。本次清便实现和按钮修复已完成 Release 构建及对应测试，作为正式版发布。

安装或升级前退出游戏。将压缩包 `Mewjector/mods/AutoCattery.dll` 放入游戏 `Mods` 目录；不要仅放入 `Mods/AutoCattery/` 子目录。数据文件按附带 README 安装，保留已有用户配置。无需安装 Quick-Cleanup；其 MIT 许可参考声明已包含在包内。

## English

- Confirmed cattery organization now clears poop from House rooms, including when no cat moves are needed.
- The first click remains a preview; confirmation uses the existing organization label. Normal furniture and other items are unaffected.
- Fixes House organization and combat-recommendation buttons becoming unresponsive after temporary interaction blocking by keeping native button updates active.
- Cleanup counts are logged under `AC19300`.

The player reported several successful play sessions with the final version. Release builds and relevant tests passed. This is a stable release.

Exit the game before upgrading. Install `Mewjector/mods/AutoCattery.dll` into the game's `Mods` directory, not only its `Mods/AutoCattery/` subdirectory. Follow the included README for data files and preserve your user configuration. Quick-Cleanup is not required; its MIT reference notice is included.

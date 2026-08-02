# Stage 24 Report

## 范围

- 针对玩家确认“进入游戏后仍出现整面板闪烁”继续收敛隐藏路径。

## 实际修改

- `src/ui/in_game_panel_controller.cpp`：恢复 House 就绪时的一次性隐藏初始化。
  进入场景后立即 Attach、解析并切换全部面板节点到隐藏状态；没有用户按键时
  不打开、不渲染内容，也不会每帧重复解析。
- 保留 Stage 23 的 SWF 默认空帧和 F10 显示帧修复，作为资源层和运行时层双重
  防护。

## 验证

- `tools\build.ps1 -Configuration Debug`：成功，CTest `5/5`。
- `tools\build.ps1 -Configuration Release`：成功，CTest `5/5`。
- Release 部署与 `verify_install.ps1`：成功。

## 游戏状态

- 最新 Release 已部署；本地没有游戏渲染线程，仍需玩家完全重启游戏后确认
  House 进入时不再出现面板。
- 如果仍闪，下一步将只采集一次进入 House 时的 `AC18000` 时间点和截图，不再
  继续猜测 SWF 帧编号。

## Git

- 本地提交：`505e6bd fix: prime panel hidden on house entry`。
- 是否 push：否。

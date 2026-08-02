# Stage 22 Report

## 范围

- 根据玩家新截图修复面板仍闪烁和关闭后文字残留。

## 根因与修复

- 设置行/按钮的文本是独立 SWF 文本节点，不会随背景 MovieClip 的隐藏帧一起消失；
  关闭时现在明确写入空字符串，并把每个控件切到隐藏帧。
- `Show()` 开始先隐藏背景，完成文本和控件帧更新后才显示背景，避免空框或黄色
  中间态被绘制。

## 文件与验证

- 文件：`src/ui/mew_ui_management_panel_view.cpp`
- `tools\build.ps1 -Configuration Debug`：成功，CTest `5/5` 通过。
- `tools\build.ps1 -Configuration Release`：成功，CTest `5/5` 通过。
- Release 部署与 `verify_install.ps1`：成功。
- 未修改原始游戏文件、存档或 Steam Cloud。

## 游戏状态

- 修复版已部署，需玩家重新确认进入 House、首次 F10、再次 F10 三个状态。
- 目前尚无新的实机日志证明游戏内渲染中间帧已经消失。

## Git

- 本地提交：`a8070aa fix: clear panel text on close`。
- 是否 push：否。

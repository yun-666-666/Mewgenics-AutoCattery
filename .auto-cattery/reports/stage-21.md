# Stage 21 Report

## 范围

- 修复 F10 面板首次打开时空白框短暂闪现。
- 修复关闭面板后背景消失但设置/按钮边框仍残留的问题。

## 实际文件

- `src/ui/mew_ui_management_panel_view.cpp`

## 根因与修复

- 面板背景在 `Show()` 开始时就切到可见帧，而各文本和控件随后才更新，
  因此会出现截图中的空框/黄色区块中间态；现在先更新所有缓存文本和控件帧，
  最后才发布背景可见帧。
- 之前为降低重开成本只隐藏背景，没有隐藏每个独立的 SWF 控件；现在关闭时
  保留文本缓存，但把固定按钮、保护行、分组标题、设置行、标题和状态节点的
  控件帧全部切回隐藏帧，避免残留边框和文字。

## 构建与部署

- `tools\build.ps1 -Configuration Debug`：成功，CTest `5/5` 通过。
- `tools\build.ps1 -Configuration Release`：成功，CTest `5/5` 通过。
- `tools\deploy.ps1 -GameRoot D:\steam\steam\steamapps\common\Mewgenics
  -Configuration Release`：成功。
- `tools\verify_install.ps1 -GameRoot D:\steam\steam\steamapps\common\Mewgenics`：成功。

## 游戏验证状态

- 修复版已部署，等待玩家实机确认：进入 House 不闪、第一次 F10 无空框、
  再次按 F10 后所有控件完整隐藏。
- 未修改原始游戏文件、存档或 Steam Cloud。

## Git

- 本地提交：`d15b932 fix: hide panel controls atomically`。
- 是否 push：否。

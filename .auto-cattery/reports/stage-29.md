# Stage 29 Report

## 范围

- 修复三个存档进入 House 时面板根层持续闪烁，直到按 F10 才停止的问题。
- 保留 79 猫存档的崩溃修复，不在 HouseReady 阶段执行面板原生时间轴调用。

## 根因与修复

- 之前的 SWF 只把背景和按钮做成隐藏帧，但它们仍直接放在 House 根时间轴的
  第一帧；游戏会先实例化并绘制这些子节点，因此按 F10 后的 `Hide()` 才能让
  闪烁停止。
- 资源生成器现在把全部 `panel_*` 控件和文字节点移入一个 `panel_root` 包装
  MovieClip。House 根时间轴只保留这个包装实例，包装实例的初始帧没有任何子
  节点，面板内容位于显式显示帧。
- F10 Attach 时先把 `panel_root` 切到显示帧，再解析子节点；关闭或离开 House
  时先清空子节点并隐藏包装层。进入 House 不再扫描或操作几十个面板节点。

## 修改文件

- `tools/build_house_ui_asset.py`
- `tools/swf_panel_shapes.py`
- `assets/swfs/auto_cattery_house.swf`
- `src/ui/mew_ui_management_panel_view.cpp`
- `src/ui/mew_ui_management_panel_view.hpp`

## 构建与检查

- SWF 重新生成：`python tools/build_house_ui_asset.py third_party/mew_ui_api/swfs/house_ui_test.swf assets/swfs/auto_cattery_house.swf`。
- 静态 SWF 检查：House 根只包含 `panel_root`；包装层 5 帧，面板子节点只在显示帧存在。
- `tools\\build.ps1 -Configuration Debug`：成功，CTest `5/5`。
- `tools\\build.ps1 -Configuration Release`：成功，CTest `5/5`。
- Release 部署与 `tools\\verify_install.ps1`：通过。
- `git diff --check`：通过。

## 游戏验证状态

- Release DLL 和新的 SWF 已部署到当前 Mewtator 数据 MOD。
- 尚需玩家完全退出并重启游戏，分别进入三个存档确认：进入 House 不再出现
  面板闪烁；按 F10 后面板背景、按钮、文字正常显示；关闭 F10 后再次保持隐藏。
- 未修改原始游戏文件、活动存档或 Steam Cloud。

## Git

- 本地提交：以最终回复中的提交哈希为准。
- 是否 push：否

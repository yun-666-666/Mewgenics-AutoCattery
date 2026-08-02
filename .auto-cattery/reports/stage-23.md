# Stage 23 Report

## 范围

- 修复玩家连续反馈的“进入游戏/House 后面板自己闪出”的问题。

## 根因与修复

- F10 面板是叠加到 House SWF 的独立 MovieClip。懒加载后，C++ 不会在进入
  House 时立即 Attach/Hide；而原面板子时间轴的初始第 1 帧是可见帧，资源加载
  期间就会先绘制整套面板。
- 新增面板专用四帧时间轴：第 0、1 帧为空，第 2 帧正常，第 3 帧按下。这样
  游戏创建 MovieClip 时默认落在第 1 帧也保持不可见；C++ 显示帧同步改为
  2/3，隐藏仍为 0。
- 推荐标记使用的原三帧资源不变；本次只改变管理面板背景和管理面板控件。

## 文件与验证

- `tools/swf_panel_shapes.py`
- `tools/build_house_ui_asset.py`
- `assets/swfs/auto_cattery_house.swf`
- `src/ui/mew_ui_management_panel_view.cpp`
- `python -m py_compile tools/build_house_ui_asset.py tools/swf_panel_shapes.py`：通过。
- Debug/Release 构建：通过；CTest 均 `5/5`。
- Release 部署与安装检查：通过；部署 SWF 与 Release SWF SHA-256 一致：
  `61ACA1594CE1724C3047DAD0AE7FC5EFF1E1D4683A096DF6B95E1C5A681DC989`。

## 游戏验证状态

- 修复版已部署，需玩家重新启动游戏并进入 House 验证进入时是否完全不再闪出。
- 本地无法替代游戏渲染线程的实机验收。

## Git

- 本地提交：`7c33759 fix: keep panel hidden before F10 attach`。
- 是否 push：否。

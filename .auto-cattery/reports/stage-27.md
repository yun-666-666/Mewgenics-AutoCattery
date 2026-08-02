# Stage 27 Report

## 范围

- 修复 Stage 26 按需挂载后，79 猫存档进入 House 时重新出现整面板闪烁。

## 新证据与根因

- 最新 79 猫进入日志只出现 `HouseReady`、`AC14315`、House 按钮和推荐按钮，
  没有 `AC18000`；因此本次闪烁不是 F10 控制器自动 Attach。
- 未挂载期间，Mewtator 加载的 SWF 仍会先显示面板 MovieClip 的默认 artwork；
  资源的空帧不能覆盖当前游戏 UI 初始化时序，截图中的整块背景和空按钮就是该
  默认状态。
- 需要在 House 首个就绪 tick 锁住可见 MovieClip 的隐藏帧，同时继续避免大存档
  进入时解析全部文字节点和安装输入钩子。

## 实际修改

- `src/ui/mew_ui_management_panel_view.hpp`：增加一次性
  `PrimeHidden()` 接口和场景代次缓存。
- `src/ui/mew_ui_management_panel_view.cpp`：首次 House 就绪时只查找并切换
  背景、固定按钮、保护行、设置分组和设置行的 MovieClip 到隐藏帧 0；不读取
  文字节点、不安装输入钩子。场景代次内不重复扫描，离开场景时重置。
- `src/ui/in_game_panel_controller.cpp`：在按需 Attach 保护之前调用轻量隐藏
  初始化；只有 F10 仍会触发完整 Attach 和面板打开。
- `.auto-cattery/reports/stage-27.md`：记录本次回归证据和验证。

## 构建与检查

- `tools\\build.ps1 -Configuration Debug`：成功，CTest `5/5`。
- `tools\\build.ps1 -Configuration Release`：成功，CTest `5/5`。
- `tools\\deploy.ps1 -GameRoot D:\\steam\\steam\\steamapps\\common\\Mewgenics -Configuration Release`：成功。
- `tools\\verify_install.ps1 -GameRoot D:\\steam\\steam\\steamapps\\common\\Mewgenics`：通过。
- `git diff --check`：通过。

## 游戏验证状态

- 修复版 Release DLL 和已启用的 Mewtator UI 数据 MOD 已部署。
- 请完全重启游戏并进入 79 猫主存档，确认进入 House 不再出现整面板；然后按
  F10，确认面板仍能正常打开、显示背景和文字、关闭后保持隐藏。
- 本阶段未修改原始游戏文件、存档或 Steam Cloud。

## 风险与省略的后续工作

- 本地自动化无法替代游戏渲染线程验收；若仍闪烁，应提供新的时间点日志和截图，
  再判断是否存在第二个数据 MOD 重复加载同一 SWF。
- 未改变 79 猫规划、面板性能缓存、真实淘汰、撤销或存档身份匹配逻辑。

## Git

- 本地提交：以最终回复中的提交哈希为准。
- 是否 push：否

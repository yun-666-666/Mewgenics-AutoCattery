# AutoCattery v0.5.0

本版完成 F10 管理面板的“完整预览”：逐猫显示为何移动、从哪到哪、性别与潜力，
并展示整理前后各房间人数和公母比例。

同时修复大量猫时的主要卡顿路径。面板现在只在挂载时解析 MewUI 节点，刷新时
仅更新变化的文字和帧，不再按每一行重复扫描整个 House 场景。

新增功能：

- 中文/英文切换，默认中文。
- 默认关闭的本地猫技术数据收集；排除猫名、存档名/路径和用户/机器/账号标识，
  不含任何上传功能。
- 双语使用说明、当前 build 游戏值名称参考、致谢。

The release adds a complete per-cat move preview, before/after room and sex
ratios, cached F10 panel rendering, Chinese/English selection, and opt-in,
privacy-minimized local planning diagnostics.

本 MOD 的实现由 OpenAI GPT-5.6 在 wordy 的需求与实机验证指导下独立完成。
感谢 Mewjector、MewUI API、JSON for Modern C++、AutoCattery Codex Toolkit、
Push To Meow、Quick-Cleanup 与 Mewgenics MOD 社区。

支持的游戏 build 与安装方法见压缩包内 `Documentation`。真实淘汰仍未启用。

# AutoCattery v0.5.0

本版完成 F10 管理面板的“完整预览”：逐猫显示为何移动、从哪到哪、性别与潜力，
并展示整理前后各房间人数和公母比例。

同时修复大量猫时的主要卡顿路径。面板现在只在挂载时解析 MewUI 节点，刷新时
仅更新变化的文字和帧，不再按每一行重复扫描整个 House 场景。

新增功能：

- 中文/英文切换，默认中文。
- 默认关闭的本地猫技术数据收集；排除猫名、存档名/路径和用户/机器/账号标识。
  玩家可按双语手册手动压缩并附加到 GitHub Issues，MOD 不会自动上传。
- 双语使用说明、版本兼容策略、游戏值参考、详细致谢和 GPT-5.6 制作声明。
- 不再用固定游戏文件大小或 SHA-256 阻止 MOD 启用；未知运行时布局由适配器安全
  失败并保留只读功能。

The release adds a complete per-cat move preview, before/after room and sex
ratios, cached F10 panel rendering, Chinese/English selection, and opt-in,
privacy-minimized local planning diagnostics.

本 MOD 的实现由 OpenAI GPT-5.6 在 wordy 的需求与实机验证指导下独立完成。
感谢 Mewjector、MewUI API、JSON for Modern C++、AutoCattery Codex Toolkit、
Push To Meow、Quick-Cleanup 与 Mewgenics MOD 社区。

安装方法和逐按钮说明见 `README.md`、`README_EN.md` 与 `docs/USER_GUIDE.md`。
真实淘汰仍未启用。

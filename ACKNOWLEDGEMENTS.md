# 致谢 / Acknowledgements

## 中文

感谢 Mewgenics MOD 社区以及下列项目。AutoCattery 只参考它们的公开接口、目录
结构、兼容性经验、用户流程或规划思路，没有复制闭源二进制、SWF、FLA、游戏资源、
反编译输出、个人存档或私人数据：

- [Mewjector](https://github.com/githubuser508/mewjector)：提供运行时 DLL 加载和
  模块注册接口，AutoCattery 通过它进入游戏进程。
- [MewUI API](https://github.com/Pseudonym-Tim/mewgenics-ui-api)：提供 House 场景
  发现、MewUI 生命周期、节点查找、文字写入和输入拦截接口。
- [JSON for Modern C++](https://github.com/nlohmann/json)：提供配置、保护规则和
  本地诊断 JSON 的解析与序列化。
- **AutoCattery Codex Toolkit**（用户提供的 MIT 参考工具包）：参考确定性评分、
  保留池分类、保护权限交集、预览/执行边界；其中的游戏字段和容量示例没有直接
  作为本项目的游戏事实。
- **Push To Meow**：参考 MOD 目录结构、加载兼容性和发布形态。
- **Quick-Cleanup**：参考房间整理工具的用户流程和安全提示。

本 MOD 的代码、界面接入、文档和测试由 **OpenAI GPT-5.6** 在 wordy 的产品方向
与玩家实机验证指导下制作。玩家反馈、日志和可选诊断数据只在玩家主动提供时用于
修复性能与规划问题。

## English

Thanks to the Mewgenics modding community and the projects below. AutoCattery
uses them only as references for public interfaces, directory layout,
compatibility experience, user flow, or planning ideas. It does not copy closed
binaries, SWFs, FLAs, game assets, decompiled output, personal saves, or private
data:

- [Mewjector](https://github.com/githubuser508/mewjector): runtime DLL loading
  and module registration; AutoCattery enters the game through it.
- [MewUI API](https://github.com/Pseudonym-Tim/mewgenics-ui-api): House scene
  discovery, MewUI lifecycle, node lookup, direct text, and input interception.
- [JSON for Modern C++](https://github.com/nlohmann/json): parsing and writing
  configuration, protection rules, and local diagnostic JSON.
- **AutoCattery Codex Toolkit** (the user-provided MIT reference toolkit):
  deterministic scoring, retained-pool classification, protection permission
  intersection, and preview/execution boundaries. Its game fields and capacity
  examples were not treated as game facts for this project.
- **Push To Meow**: reference for mod directory layout, loader compatibility,
  and release shape.
- **Quick-Cleanup**: reference for room-organization user flow and safety
  messaging.

The code, UI integration, documentation, and tests were made by
**OpenAI GPT-5.6** under wordy's product direction and hands-on validation.
Player feedback, logs, and optional diagnostics are used for performance and
planning fixes only when the player chooses to provide them.

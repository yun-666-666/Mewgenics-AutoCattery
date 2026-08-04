# Stage 22 当前报告

更新日期：2026-08-04

状态：F10 可配置升级重骰次数已实现、构建、打包并部署；等待玩家在游戏中确认
设置交互与重启后的升级界面次数。

## 玩家功能

- F10 设置页新增“升级重骰次数（重启生效）”。
- 可选范围为 `0`–`99`，默认 `3`；`0` 表示不增加升级重骰次数。
- 左右点击按 1 调整，中键可直接输入整数。
- 设置覆盖 14 个玩家职业：7 个普通职业与 7 个进阶职业。
- Mewtator 在游戏启动时合并职业数据，因此保存设置后必须重启游戏才会生效。
- 不要同时启用旧的独立 `SkillsPassivesFirstData`，两者会修改相同职业条目。

## 实现边界

- 旧的固定 3 次来自 `SkillsPassivesFirstData` 的 `AddLevelUpRerolls 3`；本阶段
  将同一已确认数据条目迁入 AutoCattery 自己的 Mewtator 数据 MOD。
- 配置新增版本化的 `level_up.reroll_count`，解码、迁移、校验、schema、默认值
  和设置文件编辑器保持一致。
- 保存时先用临时文件和原子替换生成两份职业 merge 文件；第二份失败会回滚
  第一份，配置保存失败也会恢复旧职业数据。
- Mewtator `mod_folder` 支持绝对路径与相对 `config.json` 的路径。
- F10 SWF 设置节点从 47 个扩展到 48 个，第三列从 12 行扩展到 13 行；新增行
  位于底部按钮上方，无重叠。结构检查确认恰好 48 个 clip 节点和 48 个文字节点。
- 新玩家发行物保持 DLL-only；不处理或清理历史版本中的 EXE。

## 自动化与发行验证

- Debug 目标编译成功，CTest `4/4` 通过。
- Release 完整构建成功，CTest `4/4` 通过。
- 配置测试覆盖默认 3、最小 0、最大 99、拒绝 100，以及保存/重载值 7。
- F10 模型测试覆盖 48 项、末页 5 行、重启提示、写入值 9、拒绝 100，以及
  Fighter/Jester 条目和两份文件各 7 个职业。
- 虚拟视口命中测试覆盖第 48 项 `(944, 503)`，并在解引用 optional 前显式
  检查是否存在，避免 Debug Assertion 弹窗。
- `dist\Release` 含两份职业数据文件且 EXE 数量为 0。
- `AutoCattery-v0.5.6-Windows-x64.zip` 含两份职业数据文件且 EXE 数量为 0。
- Release 已部署；安装验证确认 DLL 架构、Mewtator 启用状态和全部 14 个职业
  当前均为 3 次。
- 玩家实机验证待完成：F10 调整/直接输入、重启游戏、进入猫升级界面核对次数。

## 实际修改范围

- 配置：`include/auto_cattery/config.hpp`、`src/config/*`、
  `src/settings_file_editor.cpp`、`config/*`。
- 职业数据：`level_up_reroll_data.*`、`assets/data/classes/*`。
- F10 面板：`src/ui/in_game_settings_*`、panel controller/settings、输入命中范围、
  48 节点 SWF 与布局生成脚本。
- 构建发行：`tools/build.ps1`、`tools/deploy.ps1`、
  `tools/verify_install.ps1`、`tools/package_release.ps1`。
- 测试和中英文玩家文档、变更记录、0.5.6 发布说明。

本轮本地 commit：以最终回复中的提交哈希为准。

是否 push：否

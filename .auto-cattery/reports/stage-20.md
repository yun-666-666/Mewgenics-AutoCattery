# Stage 20 Report

## 范围

- 降低外部 `AutoCatterySettings.exe` 被启发式杀毒引擎误报的概率。
- 保留外部设置/猫保护编辑器及全部现有 MOD 行为。
- 发布 `v0.5.4` Windows x64 与源码压缩包。

## 根因证据与修改

- v0.5.3 VirusTotal 截图显示 2/70 引擎检出；Microsoft 使用泛化机器学习标签
  `Program:Win32/Wacapew.C!ml`，其余 68 家未检出。
- 旧 EXE 未签名、没有产品版本资源，并通过 `ShellExecuteW` 启动自身以打开猫
  保护窗口；这些特征可能增加新文件的启发式误报概率，但不能证明单一根因。
- 猫保护窗口现改为同进程模态窗口，不再启动第二个 EXE 进程。
- 新 EXE 包含产品名、说明、原始文件名、`0.5.4.0` 版本资源，并嵌入
  `asInvoker`、Per-Monitor-V2 DPI、long-path-aware manifest。
- 构建脚本现在检查 EXE 为 x64、产品元数据存在，且不导入 `ShellExecuteW`、
  `CreateRemoteThread`、`VirtualAllocEx` 或 `WriteProcessMemory`。

## 实际文件

- `CMakeLists.txt`
- `src/settings_app/settings_app.cpp`
- `src/settings_app/protection_editor.cpp`
- `src/settings_app/protection_editor.hpp`
- `src/settings_app/settings_app.rc.in`
- `src/settings_app/settings_app.manifest`
- `tools/build.ps1`
- `tools/package_release.ps1`
- `CHANGELOG.md`
- `docs/RELEASE_NOTES_v0.5.4.md`
- `.auto-cattery/state.json`
- `.auto-cattery/reports/stage-20.md`

## 命令与结果

- `tools/build.ps1 -Configuration Debug`：成功，CTest 5/5 通过；EXE x64、
  元数据和禁止导入检查通过。
- `tools/build.ps1 -Configuration Release`：成功，CTest 5/5 通过；EXE x64、
  元数据和禁止导入检查通过。
- Release EXE SHA-256：
  `C917678EFED18E389F16017B6151BEFB057889495C99BAD5D7145A371B0CB12E`。
- Release EXE 导入表不含 `SHELL32.dll`、`ShellExecuteW`、`OpenProcess`、
  `CreateRemoteThread`、`VirtualAllocEx` 或 `WriteProcessMemory`。
- 当前 Microsoft Defender 自定义扫描：未发现威胁，退出码 0。
- 包内 EXE SHA-256 与已验证的 Release EXE 完全一致；二进制 ZIP 和源码 ZIP
  的 Defender 扫描均未发现威胁。
- 任务公开文件严格秘密扫描：无命中；发布包清单只包含预期 DLL、设置 EXE、
  配置、本地化、Mewtator 资源与文档。
- 两个 ZIP 的最终哈希和 GitHub Release 状态：最终发布记录与最终回复为准；
  哈希不能写入会被打包的本报告，否则会形成自引用并改变压缩包哈希。

## 游戏验证状态

- 设置验证 CLI 和全部自动化测试通过。
- 本轮没有改变 F10、MoveOnly、规划、保护持久化或游戏运行时 DLL 行为。
- 同进程猫保护窗口仍需玩家做一次外部编辑器视觉点击确认。

## 风险与剩余问题

- 项目没有可信代码签名证书，因此 EXE/DLL 仍为未签名文件；这会继续影响新
  文件的云端信誉，无法保证所有启发式引擎立即不报毒。
- 本机 Defender 结果不能替代 VirusTotal/Nexus Mods 云端重新扫描。
- 真实淘汰、同猫数存档身份匹配等后续业务功能未在本阶段实现。

## Git

- 本地提交：本报告所在的 Stage 20 发布提交；最终哈希见 GitHub 标签和最终回复。
- 是否 push：是（用户明确要求推送 GitHub 并创建 Release）。

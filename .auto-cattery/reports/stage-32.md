# Stage 32：BETA House 管理面板短节点名修复

更新日期：2026-09-04

状态：实现、自动验证和本机部署完成；玩家 BETA 实机验证待完成。

## 问题与结论

- 两次独立 BETA 启动分别在 PID `26384` 和 `2780` 中进入 `HouseReady`，完成
  `AC18000` 管理面板隐藏挂载后约两秒终止。
- 两次终止性故障均为 `0xC0000005`、`Mewgenics.exe+0x963041`；较早但可继续运行的
  `0xC0000094` 不是本次重复终止点。
- `CombineDuplicateFurniture.dll` 在当前 BETA 明确没有匹配布局并未安装原生 Hook；
  `SkillsPassivesFirst.dll` 只安装技能/奖励路径 Hook。本次证据不支持归因给另外两个
  MOD，因此没有向对应项目创建修复任务。
- AutoCattery 管理面板在首次挂载时会查找 145 个 MOD 自有 artwork/text 实例。
  原节点名大量超过 15 字节，使原生 `MewNarrowString` 进入 heap-backed 所有权转移
  路径；相同 House 初次挂载路径已有堆损坏历史，这是当前最具体且可消除的风险。

## 实现

- 将管理面板背景、8 个固定控件、12 个保护项、3 个设置组、48 个设置项、标题、
  状态以及全部配套文字节点改为唯一、ASCII 且不超过 15 字节的名称。
- 文字节点后缀由 `_text` 改为 `_t`，最大索引 48 的设置节点及其文字节点仍满足
  15 字节限制。
- 原生 C++ 查找同步使用相同短名称；仍先通过短 `test_button` 定位 MOD 自有 overlay，
  不扫描无关 House 根。
- 新增资产契约检查，验证 145 个名称唯一、长度受限、完整存在于生成 SWF，并与
  C++ 查找字面量/前缀一致。
- 将测试候选版本更新为 v0.5.26。完整 F10、Esc、两个普通 House 按钮和 MoveOnly
  均保留；没有用禁用 BETA UI 作为绕过。

## 修改文件

- `CMakeLists.txt`
- `assets/description.json`
- `CHANGELOG.md`
- `docs/RELEASE_NOTES_v0.5.26.md`
- `tools/swf_panel_layout.py`
- `tools/build_house_ui_asset.py`
- `src/ui/mew_ui_management_panel_view.cpp`
- `tests/house_ui_asset_tests.py`
- `CODEX_TASK.md`
- `docs/implementation-status.md`
- `.auto-cattery/state.json`
- `.auto-cattery/reports/stage-32.md`

## 构建与检查

- `python tools/build_house_ui_asset.py third_party/mew_ui_api/swfs/house_ui_test.swf build-ninja/generated/auto_cattery_house.swf`
  - 资产生成成功。
- `python tests/house_ui_asset_tests.py build-ninja/generated/auto_cattery_house.swf`
  - PASS：145 个短面板实例、4 个唯一 AS3 first-frame stop，按钮 setup 顺序正确。
- `python -m py_compile tools/swf_panel_layout.py tools/build_house_ui_asset.py tests/house_ui_asset_tests.py`
  - 通过。
- `./tools/build.ps1 -Configuration Debug`
  - 构建成功；CTest 5/5 通过；DLL load smoke 通过。
- `./tools/build.ps1 -Configuration Release`
  - 构建成功；CTest 5/5 通过；DLL load smoke 通过。
- `git diff --check`
  - 通过。
- `code-simplifier`
  - 最终复核限定于本阶段代码；没有发现可在不扩大 diff 或削弱契约测试的前提下
    进一步简化的内容，因此代码保持不变并复用已经通过的构建/测试结果。

## 部署

- 部署前确认没有 `Mewgenics` 进程；没有启动或控制游戏。
- `./tools/deploy.ps1 -GameRoot D:/steam/steam/steamapps/common/Mewgenics -Configuration Release`
  - Release DLL 和 Mewtator 数据 MOD 部署成功，版本为 v0.5.26。
- `./tools/verify_install.ps1 -GameRoot D:/steam/steam/steamapps/common/Mewgenics`
  - 退出码 0。
- `Mods\AutoCattery.dll`、`user_config.json`、`protection.json` 和
  `AutoCatteryData` 均存在；`modlist.txt` 中 AutoCattery 恰好一次且位于最后。
- 其他 MOD、日志和诊断文件未删除。

## 玩家验证状态

待玩家使用此前闪退的同一 BETA 存档验证：

1. 通过 Mewtator 启动游戏并选择同一存档。
2. 进入 House，停留至少 10 秒，确认不再在 `AC18000` 后闪退。
3. 按 F10 打开/关闭面板，并用 Esc 关闭。
4. 点击两个普通 House 按钮，确认正常显示和响应。
5. 离开并重新进入 House，或切换存档后再次进入 House，重复上述检查。

自动检查不能证明当前 BETA 中的实际 UI 对象生命周期；玩家实机结果仍是本阶段
接受边界。若仍崩溃，需提供同一次启动的完整 chainloader、AutoCattery 日志和
最新 WER/崩溃文件。

## 风险与后续边界

- 本阶段消除了已知 heap-backed 节点名查找路径，但没有新的当前 BETA crash dump
  或完整调用栈；若相同终止点仍出现，需要基于新日志继续缩小 UI 生命周期问题。
- 本阶段不修改存档、游戏原文件、保护规则、规划算法或其他 MOD。
- 本阶段不发布、不 push；是否制作公开 Release 需在玩家验证后另行决定。

## Git

- Stage 32 本地提交：本报告所在的顶层提交；精确哈希在任务最终交付中记录。
- 是否 push：否。

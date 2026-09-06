# Stage 36 — 战斗回家时 UI 根对象类型验证修复

日期：2026-09-06。版本：v0.5.30。状态：本地构建、测试、部署及玩家实机验证完成；GitHub Release 发布已获授权。

## 证据与判断

- 最新崩溃报告 `mod_logs/crashes/23024-20260906-171118.txt`：17:11:18，PID 23024，0xC0000374 堆损坏；对应 dmp 为 0 字节。
- AutoCattery 同秒记录 AC1204 恢复和 AC2100 HouseReady，尚未记录面板附加完成 AC18000。
- 原始栈含游戏 0xE8FF7；当前游戏离线反汇编确认它位于 FindChildByName，函数在检查类型返回值前调用 root+0x80 对象的虚表槽 3、0。原始栈中的 AutoCattery+0x120C88 是 FindChildByName 字符串，不是调用帧。
- 原验证只检查内存可读及槽地址可执行，确实允许非 UI 对象通过。此缺陷与本次路径吻合，但近似栈及空转储不能证明唯一根因或完全排除其他 MOD。

## 改动文件

- src/ui/mew_ui_scene_components.c：调用原生查找前只读验证主程序镜像中的 MSVC RTTI，接受当前游戏已确认的 MovieClip、DisplayObjectContainer、DisplayObjectContainer_DynamicBatched；拒绝无容器及其他对象。
- tests/mew_ui_safe_node_lookup_tests.cpp：覆盖上述有效类型、非 UI 多态对象、仅具备可执行槽的伪对象及空容器。
- CMakeLists.txt、assets/description.json：版本 0.5.30。

## 验证与部署

- 命令：`tools/build.ps1 -Configuration Release`。原会话结束后句柄已不可用；从保存的 LastTest.log 和产物确认构建已生成 DLL，18:43 的 CTest 5/5 通过，包括 DLL load smoke。没有重跑已通过测试，也没有将不可取回的脚本退出码称为已知。
- `git diff --check` 通过。
- code-simplifier 已完成，仅审阅本次差异，无需额外简化，未改变已验证代码。
- 确认游戏未运行后，只复制 Release DLL 至 mods/AutoCattery.dll，并更新现有数据 MOD 的 description.json；配置、存档、其他 MOD 未改动。
- 已安装 DLL 中版本文本为 0.5.30，数据 MOD 描述为 0.5.30。首次部署检查错误地期待 AutoCatteryVersion= 前缀；现有构建仅嵌入版本值，随后按实际构建定义完成内容确认，没有重复部署。
- 构建监控 autocattery 已暂停。

## 玩家验证与剩余边界

- 玩家已确认使用原存档完成战斗并回家后不再闪退，本阶段的实机接受边界已满足。
- 该结果验证的是本次战斗后返回 House 的崩溃路径，不扩大为对未来游戏版本或其他未知 MOD 组合的保证。

没有实现后续功能，也没有转派其他 MOD 项目，因为当前证据首先指向本 MOD 的查找路径。

本地实现提交：`96eb0c8fd80d8be4b4b19b9daa2dcf8b46e7dbe7`（fix(ui): validate display container types before House node lookup）。
玩家已授权发布 v0.5.30；远端 Release 结果将在发布后补记。
是否 push：实现提交已推送；发布标签待创建。

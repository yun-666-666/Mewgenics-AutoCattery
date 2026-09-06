# CODEX CURRENT STATUS

更新日期：2026-09-06

Stage 36 的 v0.5.30 House 子对象类型验证修复已经完成。玩家已确认在原先失败的
存档中完成战斗并回家后不再闪退。实现提交
`96eb0c8fd80d8be4b4b19b9daa2dcf8b46e7dbe7` 已推送到 `main`，用户已明确授权
发布 v0.5.30 GitHub Release。

## Required reading

1. `AGENTS.md`
2. `README.md`
3. `docs/implementation-status.md`
4. `docs/pre-completion-functional-roadmap.md`
5. `.auto-cattery/state.json`
6. 当前代码、测试、最新运行日志和 `git status --short`

## Stage 36 scope

- 最新崩溃发生在战斗后恢复 House UI、面板完成附加之前，原始栈落在游戏
  `FindChildByName` 路径。
- 原验证只确认 root+0x80 对象可读且虚表槽可执行，无法证明它属于可安全查找子节点的
  UI 容器类型。
- 调用原生子节点查找前验证主程序镜像中的 MSVC RTTI，仅接受当前已确认的
  `MovieClip`、`DisplayObjectContainer` 与 `DisplayObjectContainer_DynamicBatched`；
  空容器、无关对象或歧义布局安全返回。

## Required verification

- 单元测试覆盖有效显示容器、无关多态对象、伪造可执行虚表槽及空容器。
- Release 构建、CTest 5/5 与 DLL load smoke 必须通过。
- 游戏退出后部署 v0.5.30，保留配置、保护规则、本地数据和其他 MOD。
- 玩家必须用原先失败的存档完成一次战斗并回家，确认不再闪退。

## Verification completed

- Release 构建成功；CTest 5/5 与 DLL load smoke 均通过。
- code-simplifier 已审阅本次差异，无需额外简化。
- 游戏退出后已部署 v0.5.30，未修改配置、存档或其他 MOD。
- 玩家已确认完成战斗并返回 House 后不再闪退。

## Next work

按用户授权发布 v0.5.30：生成并验证 Windows x64 发布包，创建并推送注释标签
`v0.5.30`，创建公开 Latest GitHub Release、上传二进制资产并远端复核。

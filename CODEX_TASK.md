# CODEX CURRENT TASK - STAGE 43 FIVE-ROOM CORRECTION, BUTTON LABEL SYNC, AND FURNITURE ATTRIBUTE UPGRADES

## Current objective

修复 v0.5.29 实机暴露的三个玩家可见问题：主存档仍只有 4 个实际房间；House
按钮在进入游戏或从家具界面返回时会先显示原版 `Clean Up!`，直到鼠标悬停才恢复
AutoCattery 中文文案；旧家具布局只要几何合法就会显示“无需移动”，完全没有比较
仓库中后来获得的更优家具属性。

## Accepted runtime evidence

- 主存档家具界面截图明确显示阁楼下左上区域仍为封板，实际只有阁楼、右上、左下、
  右下 4 个房间；`properties.house_storage_upgrades=4` 与该画面一致。
- House 初次出现以及从家具界面返回时，AutoCattery 按钮会显示 `Clean Up!`；鼠标
  悬停后才显示 `自动整理猫舍`，证明按钮状态标签没有覆盖完整的初始/返回更新窗口。
- 主存档有 257 件家具，其中 142 件已摆放、115 件在仓库；当前原生 House scene
  只枚举到 142 个 FurniturePiece，未发现仓库家具对应的 grid-null scene piece。
- 当前家具属性目录已完整覆盖 257/257 件家具，包含 Comfort、Stimulation、Health、
  Mutation、Appeal；旧布局合法不能再等同于属性组合已最优。

## Stage 43 completion boundary

- 复用现有修改前主存档备份，不创建第二份；把主存档
  `house_storage_upgrades` 从 4 修正为 5，只允许该次事务改变这一字段，并完成 SQLite
  完整性校验。最终 5 房画面由玩家亲自进入游戏确认，Codex 不自主启动或操作游戏。
- House 按钮在 attach、家具模式切换和返回正常 House 模式后的初始可见帧持续重同步
  当前状态标签，不能再依赖 hover 才从 `Clean Up!` 变成 AutoCattery 文案。
- 家具分析必须比较已摆家具与仓库家具的五项属性。至少识别“仓库家具五项属性均
  不低于某件已摆家具、且至少一项更高”的无损升级候选，唯一匹配家具实例，汇总
  各属性净增，并在 UI/日志中明确显示“检测到更优属性组合”，不能继续显示“无需
  移动”。
- 只读记录存档仓库数量、当前 scene FurniturePiece 总数、已摆 piece、grid-null
  仓库 piece 及与存档仓库 key 的匹配数。仓库 piece/原生取放路径未证明前，不执行
  仓库替换，不把属性候选混入可执行原生 move 批次。
- 版本升级为 v0.5.30；新增按钮重同步、唯一属性升级匹配和仓库 scene probe 回归。
  Debug/Release 构建与单元测试通过后 DLL-only 部署；不 push。

## Safety boundary

- 不自动启动、进入或控制游戏；需要实机验证时只给玩家清晰测试步骤。
- 不自动移动猫、休息、结束一天、出征、组队或淘汰。
- 仓库原生取放路径未由当前 build 的真实证据证明前，不调用 remove/validate/commit
  处理仓库家具，也不把分析候选声明为已自动替换。
- 不修改除主存档 `house_storage_upgrades` 之外的存档数据；现有备份必须保留在原位。

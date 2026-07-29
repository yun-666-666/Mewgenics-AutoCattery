# CODEX CURRENT TASK — STAGE 05

## Scope

只实施阶段 05：建立猫、房间与运行时数据的只读不可变快照，并把脱敏
摘要接入阶段 03 的家园按钮。禁止实施评分、规划、移动、淘汰或存档写入。

阶段 04 已由玩家确认完成，提交为 `d83629c`。阶段 05 的代码、自动化测试、
真实存档只读探针、Release 部署和玩家真实 House 点击验收均已完成。
除非玩家明确要求，不得开始阶段 06。

## Evidence rules

- `AutoCatteryDocs/16_steps/05_猫与房间数据快照.md` 只作范围参考，字段和
  数值不得照搬。
- 用户已明确允许联网核对真实游戏资料；文档中的“不联网”是旧生成约束，
  本阶段不再适用。
- 本地真实存档、当前游戏文件和当前 SDK 资料优先于参考文档。
- 未证实字段必须保持 unavailable/Unknown，不得以 0、空 ID、猜测坐标或
  猜测枚举替代。

## Implemented boundary

- 稳定猫 ID：SQLite `cats.key` 的 64 位整数。
- 房间 ID：`house_state` 中的真实字符串，如 `Floor1_Large` 和
  `AdventureBox`。
- 真实属性：STR/DEX/CON/INT/SPD/CHA/LCK，共 7 项；分别保留遗传基础、
  heredity bonus 和 equipment bonus。
- 只读取当前 SDK 已确认的 9 个能力槽。
- 职业、年龄、关系、房间容量和未出现猫的房间归属仍为 unavailable。
- Windows 系统 SQLite 以 read-only 模式打开；仅使用只读事务。
- 每次按钮点击重新定位最新 `.sav` 并生成新快照，不缓存游戏对象或指针。
- 日志与按钮结果只输出猫数、房间数、可靠归属数和告警数。

## Required validation

- Debug/Release `phase05_unit_tests`。
- Debug/Release `phase05_dll_load_smoke`。
- 临时 SQLite fixture 验证只读查询和缺失文件不创建。
- 空猫舍、1 只猫以及 100/500/1000 只猫组装测试。
- 当前真实存档端到端探针：0 校验错误；连续拍摄稳定 ID 一致。
- 玩家在真实 House 点击按钮，确认按钮完成且最新 AutoCattery 日志出现
  `AC5100` 脱敏摘要，无 WARN/ERROR。

以上验证均已通过。2026-07-29 的玩家会话记录 10 次 `AC5100` 成功摘要、
5 次推荐标记切换、0 WARN、0 ERROR，并在离开 House 时完成两个按钮清理。

## Stop condition

阶段 05 已完成。阶段 06 只能在玩家明确要求后开始，永不自动 push。

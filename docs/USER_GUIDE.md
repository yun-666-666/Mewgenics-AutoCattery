# AutoCattery 操作手册 / User Guide

本文与 README 同步说明 F10 面板的每个按钮、行和输入方式。面板只在 House
场景可用；离开 House、读取存档或保存进行中时，面板会自动隐藏。

## 中文

### 打开与关闭

| 控件 | 作用 | 改变了什么 |
| --- | --- | --- |
| `F10` | 打开或关闭面板 | 只改变面板可见性，不改变猫或设置 |
| `Esc` | 关闭面板 | 取消正在输入的数值；再次按下才关闭面板 |
| `设置` | 显示规划和安全设置 | 切换页不会执行移动 |
| `猫保护` | 读取本机存档并管理玩家保护规则 | 只写入明确应用的保护规则 |
| `完整预览` | 查看最近一次有效预览 | 只读，不会移动猫 |
| `关闭` | 关闭面板 | 恢复 House 的鼠标和滚轮输入 |

### 设置页

设置按“战斗评分与推荐”“繁育评分与分类”“房间、安全与 MOD”分组。每行的
数值或开关会立即保存到 `user_config.json`，游戏规则随后热更新。

| 操作 | 作用 | 改变了什么 |
| --- | --- | --- |
| 点击行左侧 | 数值减小；开关切换一次 | 改变该行配置并保存 |
| 点击行右侧 | 数值增大；开关切换一次 | 改变该行配置并保存 |
| 点击行中间 | 开始直接输入数值 | 只进入编辑状态，不立即写入 |
| 输入数字后按 `Enter` | 提交数值 | 校验范围后保存；无效输入不会覆盖旧值 |
| 编辑时按 `Esc` | 取消输入 | 恢复编辑前的值 |
| `上一页` / `下一页` | 浏览设置分组的分页 | 只改变当前显示页 |

重点设置包括推荐猫数量、七项属性权重、繁育保留池、房间软容量、只读模式、
应用前备份、单击执行模式、界面语言和“收集猫数据”。“单击执行模式（危险）”
会把原本的“第一次预览、第二次执行”缩短为一次点击；除非你明确理解风险，建议
保持关闭。

### 猫保护页

1. 点击顶部“存档”行左侧或右侧，选择要列出猫的本机存档。选择存档不会自动
   把它设为执行来源。
2. 点击一张猫卡片，明确选择一只猫。没有选猫时，保护按钮不会写入任何规则。
3. 点击“保护等级”，选择以下级别之一：
   - `禁止淘汰`：规划器不得把这只猫列为淘汰对象。
   - `禁止移动`：自动整理不得移动这只猫。
   - `禁止淘汰和移动`：同时禁止淘汰和移动。
   - `完全不管理`：自动流程完全跳过这只猫。
4. 点击“固定房间”，选择 `不固定` 或当前存档已有的普通房间。固定房间会把
     这只猫的目标锁在指定房间；目标房不存在时本次操作会安全停止。
5. 点击“应用保护”才写入规则。点击“移除保护”只移除当前已选猫的玩家规则。
6. 猫超过 9 只时，用“上一页”“下一页”或鼠标滚轮翻页；翻页后需要重新点击
   猫卡片，避免把保护等级应用到另一只猫。

保护规则写入 `Mewgenics\\Mods\\AutoCattery\\config\\protection.json`。默认规则为空，
不会自动保护任何 CatId、猫名或存档。

### 完整预览页

| 控件 | 作用 | 改变了什么 |
| --- | --- | --- |
| `上一页` / `下一页` | 浏览房间汇总和逐猫移动详情 | 只改变显示内容 |
| 猫详情行 | 显示来源、目标、性别、潜力和原因 | 不可编辑，不会移动猫 |
| `关闭` | 返回 House | 不执行预览中的动作 |

如果显示“暂无预览”，请关闭 F10，在 House 点击一次“自动整理猫舍”生成预览，
再重新打开 F10。预览后如果玩家手动搬猫、换存档或场景刷新，旧预览会失效，
执行按钮不会绕过这个检查。

### House 中的自动整理按钮

| 点击次数 | 作用 | 改变了什么 |
| --- | --- | --- |
| 第一次点击 | 读取当前运行时猫、房间、保护和设置，生成预览 | 不移动猫；可回到 F10 查看 |
| 第二次点击 | 重新校验预览和实时状态后执行 MoveOnly | 只移动需要改变房间的猫；已在目标房的猫跳过 |

只读模式或原生适配器失败时，执行会被拒绝并写日志，不会偷偷修改存档。真实淘汰、
自动推进天数和自动组建出征队伍不属于此按钮。

### 数据收集和 GitHub Issue

数据收集默认关闭。需要协助分析卡顿或错误时：打开 F10 设置中的“收集猫数据”，
复现问题，退出游戏，把 `Mewgenics\\Mods\\AutoCattery\\AutoCatteryData` 压缩为 ZIP，
在 [GitHub Issues](https://github.com/yun-666-666/Mewgenics-AutoCattery/issues) 新建
Issue 并附加 ZIP。说明游戏版本、猫数量、复现步骤、预期结果和实际结果，然后关闭
数据收集。上传前检查压缩包，只保留该目录中的 JSON 技术快照；不要上传存档、整个
游戏目录、账号截图或其他个人文件。

## English

This guide mirrors the Chinese section. The F10 panel is available only in House;
it hides automatically while leaving House, loading a save, or saving.

### Open and close

| Control | Action | What changes |
| --- | --- | --- |
| `F10` | Open or close the panel | Visibility only; cats and settings do not change |
| `Esc` | Close the panel | Cancels numeric input first; press again to close |
| `Settings` | Show planning and safety settings | Page changes never move cats |
| `Cat Protection` | Read local saves and manage player rules | Only explicitly applied rules are written |
| `Full Preview` | View the latest valid preview | Read-only; no cat is moved |
| `Close` | Close the panel | Restores House mouse and wheel input |

### Settings page

Settings are grouped as Combat Scoring & Recommendations, Breeding &
Classification, and Rooms, Safety & MOD. Every row is saved to `user_config.json`
immediately and game rules hot-reload afterward.

| Operation | Action | What changes |
| --- | --- | --- |
| Click the left side of a row | Decrease a number; toggle a switch | Saves that setting |
| Click the right side of a row | Increase a number; toggle a switch | Saves that setting |
| Click the middle of a row | Start direct numeric input | Enters edit mode only |
| Type a number and press `Enter` | Commit the number | Range-checks and saves; invalid input is rejected |
| Press `Esc` while editing | Cancel input | Keeps the value from before editing |
| `Previous` / `Next` | Browse setting pages | Changes only the visible page |

Important settings include recommended count, seven-stat weights, breeding
reserve pools, room soft capacity, read-only mode, backup-before-apply,
single-click execution, interface language, and **Collect cat data**. The
**Single-click execution (dangerous)** option changes the normal two-step
preview-then-apply flow into one click; keep it off unless you understand the
risk.

### Cat Protection page

1. Click the left or right side of the top **Save** row to choose a local save to
   list. This does not make it the execution source.
2. Click one cat card to select exactly one cat. Protection buttons write nothing
   until a cat is selected.
3. Click **Protection** and choose one level:
   - `No Cull`: never include this cat in a cull candidate.
   - `No Move`: never move this cat during automatic organization.
   - `No Cull or Move`: both restrictions apply.
   - `Fully Unmanaged`: skip this cat in the automatic workflow.
4. Click **Fixed room** and choose `Not fixed` or an ordinary room present in the
     selected save. A fixed target that does not exist stops the operation safely.
5. Click **Apply Protection** to write the rule. **Remove Protection** removes
   only the selected cat's player rule.
6. With more than nine cats, use `Previous`, `Next`, or the mouse wheel to page;
   select the cat again after paging so a rule cannot be applied to another cat.

Rules are stored in `Mewgenics\\Mods\\AutoCattery\\config\\protection.json`. The default
records list is empty; no CatId, cat name, or save is protected automatically.

### Full Preview page

| Control | Action | What changes |
| --- | --- | --- |
| `Previous` / `Next` | Browse room summaries and cat-move details | Display only |
| Cat detail row | Shows source, target, sex, potential, and reason | Not editable; no movement |
| `Close` | Return to House | Does not execute preview actions |

If it says **No preview**, close F10, click **Auto-Organize Cattery** once in
House, and reopen F10. If the player moves a cat, changes saves, or the scene
refreshes after preview, the preview becomes stale and cannot bypass validation.

### Auto-Organize Cattery in House

| Click | Action | What changes |
| --- | --- | --- |
| First click | Read live cats, rooms, protection, and settings and create a preview | No cat moves |
| Second click | Revalidate the preview and live state, then run MoveOnly | Moves only cats whose room must change; skips cats already in target rooms |

Read-only mode or a failed native adapter rejects execution and writes a log; it
does not silently edit saves. Real culling, automatic day advance, and automatic
expedition-team selection are outside this button.

### Data collection and GitHub Issues

Collection is off by default. To request help with lag or incorrect behavior,
enable **Collect cat data** in F10 Settings, reproduce the problem, exit the
game, and compress `Mewgenics\\Mods\\AutoCattery\\AutoCatteryData` into a ZIP. Open a
[GitHub Issue](https://github.com/yun-666-666/Mewgenics-AutoCattery/issues), include
the game version, cat count, reproduction steps, expected result, and actual
result, and attach the ZIP. Turn collection off afterward. Inspect the archive
first and keep only JSON technical snapshots from that directory; do not upload
saves, the whole game directory, account screenshots, or other personal files.

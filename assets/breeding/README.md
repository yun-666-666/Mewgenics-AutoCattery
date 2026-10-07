# AutoCattery 繁育工坊

随 AutoCattery v0.5.35 正式版发布；也可下载独立的 `AutoCattery-v0.5.35-Workbench.zip`。
繁育工坊是可选组件；`AutoCattery-v0.5.35-MOD-only.zip` 不含工坊，但完整保留游戏内F10遗传与供食辅助。

将整个 `AutoCatteryWorkbench` 文件夹放到游戏目录，与 `Mewgenics.exe` 同级。
无需安装 Python，也不需要源码项目；运行环境和模拟器已经随包提供。

1. 正常保存并完全退出游戏。
2. 双击本文件夹中的 `StartBreeding.cmd`，浏览器会自动打开“繁育工坊”。
   保留启动窗口；若浏览器未自动打开，将窗口中的本机网址复制到浏览器。
3. 核对账户、存档槽位、天数和猫群，选择培养设置，点击“开始培养”。
4. 完成后查看“确认使用结果”：写回路径、增加猫、移出猫和遗传属性。
   短程模拟未达到连续30日标准时，仍可确认使用已生成的结果。
5. 勾选确认框，点击“确认使用此结果”。工具会自动保留原档恢复副本并写回原槽位。
6. 页面提示写回成功后，再启动游戏加载该槽位，检查新猫及保存后重载。

模拟和导入期间保持游戏关闭。网页会检查游戏进程；原档在模拟后发生变化时，
需要重新培养，避免覆盖新的进度。模拟不上传存档，不会自动替你确认使用。

结果、记录和恢复副本位于用户的 `Documents/AutoCattery/本次任务目录`，
网页成功提示中会给出原档及恢复副本的完整位置。如需撤销，先退出游戏，
将 `before-apply.sav` 复制到提示的原存档位置，并改回原槽位文件名。

网页辅助开关仅用于这次模拟。继续在游戏里使用遗传或供食辅助时，
需要在 MOD 设置中单独开启相应开关。

两个辅助开关首次默认勾选，之后记住上次选择。偏好存储在用户
`Documents/AutoCattery/workbench-preferences.json`，关闭窗口再打开仍保留。
“实际安排 / 目标配对”显示每日实际选对数，房间不足时多对可共用繁育房；
目标配对数不保证每日同样数量的出生。新培养猫使用游戏原生随机名，已有猫不改名。

随包运行环境：Python 3.12.10、pefile 2024.8.26、Unicorn 2.1.4。
原始许可保留在 runtime 及各包的 dist-info 目录中。
Python：https://www.python.org/downloads/release/python-31210/
pefile 源码：https://github.com/erocarrera/pefile
Unicorn 源码及许可：https://github.com/unicorn-engine/unicorn/tree/2.1.4
运行时读取玩家本机游戏资源；不包含游戏文件、游戏资产或存档。

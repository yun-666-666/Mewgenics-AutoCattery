阶段：04
状态：根据 2026-07-28 玩家截图和运行日志完成纠正实现与自动化验证；游戏内复验待执行，阶段门禁保持未通过
读取的关键文件：CODEX_TASK.md；AGENTS.md；AutoCatteryDocs/16_steps/04_次日推荐标记按钮.md；AutoCatteryDocs/contracts/自动化核心接口契约.md；阶段 02 场景服务；阶段 03 按钮控制器、MewUI 视图、测试、构建与部署脚本
修改的文件：阶段初始实现文件；本次纠正另修改 tools/build_house_ui_asset.py、assets/swfs/auto_cattery_house.swf、src/ui/mew_ui_scene_probe.h/.c、include/auto_cattery/ui/scene_context.hpp、src/ui/scene_context.cpp、config/scene_signatures.json、src/ui/mew_ui_house_button_view.cpp、推荐按钮控制器/视图/桥接、对应测试、构建脚本、实施状态、手工验收说明、资源版本和本报告
新增测试：错误/不安全场景拒绝；幂等挂载；50 次标记开关；视觉节点失败安全回退；scene_generation 更换；先清标记后卸载；幂等卸载；配置默认值
执行的命令：py tools/build_house_ui_asset.py third_party/mew_ui_api/swfs/house_ui_test.swf assets/swfs/auto_cattery_house.swf；.\tools\build.ps1 -Configuration Debug；.\tools\build.ps1 -Configuration Release；.\tools\deploy.ps1 -GameRoot 'D:\steam\steam\steamapps\common\Mewgenics' -Configuration Release；.\tools\verify_install.ps1 -GameRoot 'D:\steam\steam\steamapps\common\Mewgenics'；git diff --check；git diff；git status --short
构建结果：Debug 与 Release x64 DLL 构建和资源打包通过；导出检查通过；固定 MIT 按钮资源可重复生成，并包含家具模式使用的真实空白帧
测试结果：Debug 与 Release 的 phase04_unit_tests 与 phase04_dll_load_smoke 全部通过
游戏内验证状态：初次验证失败。截图确认普通 House 缺少推荐按钮且家具摆放模式残留 Stage 03 按钮/文字；日志确认 ClassChooser 识别成功但 AC4105 表明 House SWF 节点不存在。已据此纠正，等待玩家按 docs/phase04-manual-test.md 复验；通过前不得开始阶段 05
安全边界：仅写入 MOD 自己的按钮和文本节点；家具模式只禁用 MOD Button 组件、清空 MOD 文本并切到 MOD 按钮空白帧；演示标记不绑定真实猫；未读取评分、猫、房间或存档；未调用选队、移动、淘汰、休息、日期推进或出征选择写接口
已知风险：普通 House 中第二按钮实际布局、家具模式隐藏、10 次往返、50 次开关和不同分辨率/UI 缩放仍需玩家复验；SWF 字体继续使用已验证的 ASCII 回退文本
省略的后续工作：阶段 05 真实猫与房间快照及其后全部评分、规划和写入功能；本阶段未实现真实推荐结果
本地 commit：由本报告所在的阶段提交记录，精确哈希见 Codex 最终报告
是否 push：否

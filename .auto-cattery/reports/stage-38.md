# Stage 38 — 游戏年迈状态筛选与死亡猫交付探针

日期：2026-09-08。候选版本：0.5.32。
状态：年迈页与只读探针已构建、测试、部署；玩家验收待完成。死亡猫顺序批量交付仍未实现，等待用户已同意的探针取证。

## 实现与文件

- src/snapshot/cat_blob_parser.cpp、tests/cat_blob_parser_tests.cpp：格式19中old_state >= 2解析为Senior，死亡优先；Senior不进入繁育资格。
- src/ui/in_game_panel_senior.cpp：当前匹配存档年迈猫、9只每页、基础属性总值/年龄排序、原生猫详情。
- src/ui/in_game_panel_controller.cpp/.hpp、in_game_panel_protection.cpp：页面事件与异步加载。
- src/ui/mew_ui_management_panel_input.cpp、mew_ui_management_panel_view.cpp/.hpp、tools/swf_panel_layout.py：年迈猫标签与按钮。
- tools/lifecycle_probe.pyw：SQLite只读事务采集手动单次交付前后猫记录、house_state及npc_progress差异，不调用游戏交付函数。
- CMakeLists.txt、assets/description.json：构建源及版本；docs/USER_GUIDE.md：功能与探针操作；CODEX_TASK.md、.auto-cattery/state.json：实际状态。

## 证据与验证

当前游戏详情UI函数RVA 0xE1A00在0xE5297比较CatData+0xC34与2，控制old节点；序列化函数0x22F410在0x22FE85写该int，对应格式19尾部79字节处。不是人为年龄门槛。

- tools/build.ps1 -Configuration Release：原会话68239退出0，221步完成。
- CTest 5/5通过：资源检查、单元测试、DLL加载及两个CLI smoke；测试耗时2.65秒。
- 合成用例：old_state=2为Senior；同龄未标记保持Adult；死亡优先于Senior。
- 只读实档探针捕获913条猫数据库记录（不是屋内猫数量）；无变化对照和合成死亡记录移除/NPC字节变化验证通过。
- code-simplifier仅检查本次变更，无必要简化，代码未改变，复用已完成验证。
- 部署前确认游戏未运行；精确复制DLL、新SWF、description和探针至已安装MOD，文件存在与版本描述检查通过。未改玩家配置、存档或其他MOD。

## 玩家验证与剩余工作

1. 启动游戏，在House按F10进入年迈猫页，确认猫原生详情标记“老了”；测试排序、翻页和打开详情。
2. 列表读取保存状态：猫群变化后正常保存，再点击顶部刷新。
3. 双击已安装MOD下tools/lifecycle_probe.pyw，选择当前存档。正常保存→记录前→手动交付一只死猫给截图Organ Grinder→正常保存→记录后。期间不睡觉、不换档、不交付其他猫。
4. 返回工具旁lifecycle-probe-results中的JSON及交付前后人物计数。死亡历史记录不能直接视为屋内可交付尸体。报告是持久化状态差异，不是原生调用跟踪，不能单凭报告推定调用签名。

当前未验证游戏内新页面的真实显示与交互，不能将编译通过视为玩家验收；批量交付接口签名未确认，不调用未知函数。继续原始需求需玩家交付证据。
未实现自动休息、出征或额外房间策略；保留此前通用繁育权重修复。

## Git

本次本地提交消息：feat: add game senior filter and dead cat delivery probe。
本报告随任务代码同一提交；准确提交ID见最终任务回复或 git log -1（避免自引用提交ID）。
是否 push：否。
原有build-ninja/、tests/__pycache__/、tools/__pycache__/保持未跟踪。

## 玩家反馈修正：F10资源部署遗漏

玩家实测F10无法打开。当前启动日志11:46:05报面板节点缺失；Mewtator配置的mod_folder为Mewtator/mods，与上一轮复制资源的Mods不同。实际加载的旧SWF缺少ac_tab_old及ac_tab_old_t，两节点在新构建产物中存在。
游戏已退出，按配置指定目录补部署SWF和description；解压SWF内容确认两个节点已存在。保留所有配置和其他MOD。
独立探针已通过pythonw启动，并新增已安装MOD根目录“死亡猫交付探针.lnk”入口，不依赖游戏F10。
此修正仅部署和说明，未修改代码；复用已完成构建测试，不进行无关简化或重编译。F10修复效果仍待玩家重启确认。
本地修正提交消息：fix: correct active Mewtator UI deployment record；准确提交ID见交付。是否 push：否。

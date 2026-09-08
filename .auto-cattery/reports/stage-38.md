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

## 年迈验收与交付运行时探针

玩家确认F10恢复及年迈筛选正常。已有手动交付报告：同日277，房屋160→159，移除死亡猫748，死亡历史198不变，NPC进度8→9。当前存档身份字段与报告变更位置一致；没有把当前已变化的位置浮点数当成原始记录。
用户明确交付无可见动画，通过按钮送猫；原生代码事件名称不等于玩家可见动画。不得依靠播放时长判断完成。

新增src/ui/mew_ui_delivery_trace.c/.h，接入in_game_panel_controller.cpp/.hpp及CMakeLists.txt：按标记启用的只读NPCMapDrawer状态采样，记录屋内数量、当前猫弱引用是否有效、ID、NPC状态、回调RVA及是否绑定此drawer。仅状态变化写日志；异常或128条后停止本次启动采集。没有调用原生交付函数。
当前布局读取受0x27E820回调入口指令匹配约束；本机exe检查通过。未知版本不采样，不修改游戏数据。

最新tools/build.ps1 -Configuration Release会话3804退出0，CTest 5/5通过（2.86秒）；此前会话13983也通过，其后增加屋内数量字段而进行必要增量构建。
code-simplifier只检查当前差异，没有必要简化，代码未改，不重跑已完成验证。
游戏未运行时已精确部署DLL及本机启用标记diagnostics/delivery_trace.enabled。未修改SWF、玩家配置或存档。此诊断候选仍为0.5.32，不表示批量功能发布。
下一步玩家使用原按钮手动交付一次，日志自动记录；无需外部工具的前后按钮。逐帧采样可能无法捕获同帧内部短暂回调，需根据实际日志判断，不预先声称完成边界已验证。
本次本地提交消息：feat: add opt-in read-only NPC delivery runtime trace；准确ID见交付。是否 push：否。

## v0.5.33 顺序死亡猫交付候选

玩家截图确认详情topipe→“把猫送给”NPC地图→Organ Grinder。运行时12:57:57选择猫750；12:58:36回调0x27E820，house14→13、选择清空、npc_result6，无异常。当前游戏原生topipe注册回调0xEDF00；读取CatStatsDrawer弱引用，调用原生管道移动路径0x2E88D0。未直接改写选中字段。

实现文件：新增src/ui/dead_cat_delivery_service.hpp/.cpp、tests/dead_cat_delivery_tests.cpp；扩展mew_ui_delivery_trace.c/.h原生适配、in_game_panel_controller.cpp/.hpp、in_game_panel_senior.cpp、mew_ui_management_panel_input.cpp；ProtectionSaveOption保存实际来源路径（editor_model.hpp/.cpp）；CMakeLists.txt及test_main.cpp接入；description版本0.5.33；操作手册、任务、状态文件同步。

UI预览与确认分开。只选当前匹配存档的屋内死亡猫，排除MOD保护或固定房间猫，按稳定编号排序。
异步创建执行前已保存状态的恢复副本，复用StableSaveGuard锁定原存档，复制后用现有快照读取器验证可读及猫群一致；不新增哈希检查。失败不交付。
执行服务每只重新验证完整运行时身份，原生适配重新检查该猫死亡状态，打开原生详情并调用topipe，然后选择NPC。结果6、选中清空及剩余猫完整身份匹配后进入下一只。Esc/F10取消后续；已开始原生交付可能完成。场景不安全时停止。不自动保存、不睡觉、不出征。
日志AC19200记录恢复副本相对目录，AC19201请求ID，AC19203完成ID，AC19202完成/停止计数。临时自动只读采样标记已关闭；保留服务调用所需的状态读取适配。

验证：89541 Release223步构建、CTest5/5通过（2.42秒），新增测试覆盖稳定顺序、活猫排除、保护排除、固定房间排除、缺失保护信息排除、服务拒绝非死亡猫。随后完成状态显示修复及一次code-simplifier移除重复namespace，必要增量11步与CTest5/5通过（2.35秒）。当前exe两个新原生入口布局检查通过；git diff --check通过。

部署：游戏未运行，精确复制DLL及Mods/AutoCattery和Mewtator配置mod_folder下description，版本0.5.33。复用既有SWF节点；未改用户配置、存档或其他MOD。没有在本次工具操作中实际交付猫或创建玩家存档副本（副本由玩家确认执行时生成）。

游戏验收仍待玩家完成：预览正确、自动一只/连续多只交付、NPC进度及尸体移除、取消。无需再运行外部取证工具。若有问题根据DeadCatDelivery日志修复，不把候选标为玩家已通过。
本次本地任务提交消息：feat: deliver dead cats sequentially through native NPC flow。准确ID见交付或git log -1。是否 push：否。

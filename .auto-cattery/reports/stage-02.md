阶段：02
状态：完成（工具包接入时根据既有提交和实施记录追溯登记）
读取的关键文件：docs/implementation-status.md；docs/phase02-manual-test.md；config/scene_signatures.json；include/auto_cattery/ui/scene_context.hpp；src/ui/scene_context.cpp；src/ui/mew_ui_scene_probe.c；src/ui/mew_ui_scene_probe.h；tests/scene_context_tests.cpp
修改的文件：阶段实现已由既有提交 78613cb 和 1fa3e93 完成；本次只新增本阶段追溯报告及工具包控制元数据
新增测试：House/Embark 进入退出、重复回调、短暂节点闪烁、连续十帧丢失、快速切换、保存场景、订阅者异常隔离
执行的命令：历史验收执行 .\tools\build.ps1 -Configuration Debug；.\tools\build.ps1 -Configuration Release；按 docs/phase02-manual-test.md 完成玩家操作的五轮以上场景往返与日志检查
构建结果：Debug 与 Release 构建通过
测试结果：Debug 与 Release 的阶段 02 单元测试和 DLL load smoke test 通过；探针为显式 opt-in，空签名安全失败
游戏内验证：已做；确认家园场景为 House、出征选择场景为 ClassChooser，House 生命周期 generation 多轮递增且无重复 ready、悬空访问或崩溃
已知风险：场景签名和游戏 build 绑定；游戏更新或识别失败时必须重新采集只读诊断并安全降级
未实现且留给后续阶段：家园按钮、出征推荐按钮、猫数据读取、评分、移动、淘汰和存档写入
本地 commit：1fa3e93
是否 push：否

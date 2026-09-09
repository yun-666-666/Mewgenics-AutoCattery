# Stage 39 — 整理猫舍清便

- 用户需求：确认自动整理时一并清除房间大便，优先参考现有MOD。
- 参考：https://github.com/Pseudonym-Tim/mewgenics-quick-cleanup，已下载，MIT许可保留在THIRD_PARTY_NOTICES.md，未安装参考DLL或替换其UI。
- 改动文件：CMakeLists.txt、assets/description.json、THIRD_PARTY_NOTICES.md、include/auto_cattery/execution/domain.hpp、src/ui/mew_ui_poop_adapter.c/.h、src/ui/runtime_house_move_gateway.cpp、src/ui/in_game_preview_model.cpp、src/ui/mew_ui_house_button_view.cpp、src/workflow/execution_router.cpp、src/workflow/preview_builder.cpp、tests/poop_cleanup_tests.cpp、CODEX_TASK.md。
- 行为：首次点击只预览；二次确认后完成猫移动并清便，零移动也清便。原生Pickup类型与精确poop键过滤，跳过已经销毁的对象；原生函数负责移除占格和记录。记录AC19300结果/数量/异常，不直接修改存档，不自动保存。可在确认前放弃操作；保存前可使用整理前正常存档恢复。
- 技术证据：当前EXE反汇编验证0x2EF9D0及其清除item指针/延迟销毁调用；Pickup RTTI对应vtable0x10BDE28，键poop及item字段布局一致。
- 构建：tools/build.ps1 -Configuration Release，原会话49126退出0，CTest7/7通过（3.22秒）。
- 专项测试：多堆/无大便/重复清理/不影响其他类型与物品/跳过已销毁/heap key/原生未提交；测试警告修正后只重建poop_cleanup_tests，CTest1/1通过（0.03秒），无警告。初次增量命令缺少MSVC环境，vcvars初始化后成功。
- code-simplifier：仅本次差异检查，无必要生产代码简化。
- 部署：游戏未运行时复制0.5.34 DLL及两处description、第三方许可说明。未启动游戏，未改配置或其他MOD。
- 游戏验证状态：等待玩家验证所有房间大便消失、家具猫正常、重复执行和保存重载。构建/替身测试不能证明真实游戏效果。
- 风险：当前原生布局依赖游戏版本；运行时不匹配将拒绝清理并记录失败。
- 未做：无其他拾取物清理、无死亡猫交付改动、无远程发布。
- 本地提交：1daf94f — feat: clear house poop after confirmed cattery organization。此编号为提交后补记。
- 是否 push：否


2026-09-09 玩家反馈清便未执行：18:05:13加载日志明确扫描Mods并加载根目录AutoCattery.dll，实际版本0.5.33；18:06:17-18猫移动49只完成，之后两次零移动，没有AC19300。此前误将新DLL只复制到Mods/AutoCattery/AutoCattery.dll，未更新真正加载文件。游戏已退出时将已验证0.5.34产物精确复制到Mods/AutoCattery.dll；内容检查确认0.5.34、清便日志文本和“确认整理”，不含“确认整理并清便”。本次无源码变化、不重复构建，沿用已通过Release/CTest与最新DLL加载检查。tools/deploy.ps1第60行原本即使用正确根路径，后续部署必须遵循。下一步玩家启动游戏，点击自动整理、确认整理；读取新启动版本与AC19300验证清便。未启动或控制游戏。


2026-09-09 18:10玩家复测仍有大便：本次确实加载0.5.34，五次AC19300均completed=1 cleaned=0 exception=0，零移动确认也进入清理。因此部署问题已修复，但旧筛选类型错误。原先根据Quick-Cleanup注释误将房间大便限定为战斗Pickup（vtable0x10BDE28），实际原生清便函数属于FurniturePiece流程。当前EXE的FurniturePiece RTTI/vtable为0xEE6858，虚表槽0返回FurniturePiece，槽48位于0x2ED450；家具流程0x2EE750以同一对象调用0x2EF9D0。已把类型筛选及RTTI校验更正为FurniturePiece，保留精确poop键判断，普通家具/其他物品不清除。按钮仍为确认整理。
仅修正适配器c/h及测试对象类型说明；Release增量6步成功，清便专项与DLL加载2/2通过（0.11秒）。code-simplifier限本次差异检查，无额外改动。游戏未运行时部署到真正加载的Mods/AutoCattery.dll，并同步dist与子目录副本，仍0.5.34。未启动/控制游戏。下一步玩家复测并读取AC19300实际清理数量；此前的测试仅证明筛选算法，不证明生产对象类型正确，本次游戏验收仍待确认。


2026-09-09 清便后按钮偶发无响应：18:18:11日志确认清理12堆成功，之后无新的整理点击回调，F10仍可用；用户随后继续多局未复现，原因尚未确证，不把偶发问题视为稳定复现。限定修改：确认点击仅排队，由后续UI Poll执行原生操作，避免点击回调中直接清除场景对象；IsAttached同时检查MewUI追踪记录，丢失时走已有绑定流程。按钮文字仍“确认整理”。增加连续两次零移动确认回归，更新批次/离场测试以覆盖延迟执行。Release增量6步通过，单元测试和DLL加载2/2通过（2.55秒）。code-simplifier限本次差异检查，无额外简化；游戏退出时部署到Mods/AutoCattery.dll并同步副本。仍0.5.34，未启动或控制游戏。玩家后续继续正常使用观察；若再现，需同次启动的点击/追踪状态进一步取证，不能声称已确认根因或彻底修复。


2026-09-09 两按钮无响应再次反馈：23:18本次启动日志只有F10开关，无整理/清便回调。用户反馈上一版更差；撤回bb43300的延迟执行和追踪记录判断，保留清便FurniturePiece实现。当前两view在Running/临时不可用（含F10 suppression）时调用SetButtonEnabled(0)，API写原生enabled/activate字节；历史同类问题也指向停用原生Button后无法恢复。现两个view临时状态仅用SetButtonInteractable控制交互，SetButtonEnabled保持1，Detach仍可停用。未变按钮文字，未新增原生入口。根因尚需当前玩家复测确认，不将历史记录当作当前验证。
Release增量7步通过，单元与DLL加载2/2通过（2.60秒）；回归覆盖整理一次、暂禁点击、恢复、第二次整理。code-simplifier限本次差异检查，无必要简化。游戏未运行时部署Mods/AutoCattery.dll并同步副本，版本仍0.5.34。下一步玩家测试两按钮连续使用及F10开关后恢复。未启动/控制游戏，未push。


2026-09-10 最终验收：玩家确认最终修正版多次游玩无问题，授权GitHub推送与正式Release。实现提交d668f99，既有Release构建与对应检查通过。发布版本0.5.34；是否 push：是（用户本次明确授权，发布流程中）。

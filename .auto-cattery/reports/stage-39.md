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
- 本地提交：本阶段提交，提交后在本报告补记实际编号。
- 是否 push：否

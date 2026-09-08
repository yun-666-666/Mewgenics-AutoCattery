# CODEX CURRENT TASK

更新日期：2026-09-08

## Stage 38：死亡猫交付与游戏年迈状态筛选

用户要求：1.已死猫按顺序交付截图红框人物；原生交付接口不确定时允许制作探针，玩家测试取证。2.筛选游戏自身标记“老了”的猫，查看优质猫，不能用自设年龄门槛代替。

当前资源确认红框人物为玩家命名的 Organ Grinder；NPC_MAP_TOOLTIP_ORGANGRINDER明确接收死猫。
本机当前exe详情UI函数0xE1A00在0xE5297比较CatData+0xC34>=2决定old节点显示。序列化函数0x22F410在0x22FE85处理该int，位于格式19尾部79字节处（现有115字节post-class段+36）。尾部后续为11字节元数据和64字节数组，已核对。
死亡猫仍以death_day为准且优先于Senior。没有调用未知原生交付函数。

## 已做与下一步

v0.5.32：Senior解析、F10年迈猫页（基础总值/年龄排序、分页、原生详情）及只读交付前后GUI探针完成。
Release构建会话68239退出0，CTest 5/5通过；只读实档捕获与合成差异检查已通过。
code-simplifier完成一次范围内检查，无需改动。DLL、新SWF、description及探针已部署，安装描述版本0.5.32；未启动游戏。当前等待玩家测试。
玩家需确认年迈页与游戏“老了”一致，并提供单只死猫手动交付前后报告。
批量死亡猫交付尚未实现，原生调用签名仍未确认；收到证据后继续，不重复当前已通过检查。
上一轮v0.5.31繁育权重修复保留；未使用个人猫群或固定房间模板。
原有build-ninja和缓存未跟踪目录必须保留。禁止push。

## 2026-09-08 玩家反馈后的部署修正

玩家反馈v0.5.32无法打开F10。11:46:05日志明确报F10 panel nodes are missing from the House SWF。
核实Mewtator/config.json的mod_folder指向Mewtator/mods；上一轮只更新Mods/AutoCattery，漏了实际数据MOD目录。
实际加载目录的旧SWF缺ac_tab_old及ac_tab_old_t，新构建产物两者齐全。
游戏未运行时，已向配置指定的AutoCattery数据MOD精确复制新SWF和description，验证两个节点存在。
未修改DLL或源码，无需重编译。已直接启动独立探针窗口，并在Mods/AutoCattery创建“死亡猫交付探针.lnk”。
当前下一步：玩家重启游戏验证F10恢复，再验证年迈页；独立探针无需F10。

## 当前前沿：原生运行时绑定探针

玩家确认F10和年迈页正常。单次交付报告证明同日277，house 160→159，死亡历史198不变，移除已死猫748，NPC进度8→9。
当前exe定位Organ Grinder入口0x27C500；动画事件DonateCat分发0x27DAC0调用std::function，完成回调0x27E820更新NPC6并移除屋内对象。
NPCMapDrawer+0x48/+0x50为待交付弱引用，+0xB0为回调、+0xB8为NPC结果；调用时绑定入口仍未确认。
继续完整需求前增加按需只读运行时观察，不调用未知交付函数。通过diagnostics/delivery_trace.enabled启用；观察下一次玩家手动交付，不需要外部探针按钮。

用户澄清无可见动画，实际是找到猫后点击按钮送NPC；不得以动画播放作为完成门槛。
只读运行时观察已完成并部署（仍标识0.5.32诊断候选），最新构建会话3804退出0，CTest 5/5通过。
观察启用标记已写到已安装MOD diagnostics/delivery_trace.enabled；在House tick记录变化到DeliveryTrace/AC19100，最多128条。无交付调用，无游戏状态写入。
code-simplifier本次范围检查无必要修改。下一步：用户说明按钮实际位置，并手动交付一只死猫；读取原日志新DeliveryTrace记录确认绑定与完成边界。原始批量交付需求仍未完成。

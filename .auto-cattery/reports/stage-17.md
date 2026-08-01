# Stage 17 当前 MoveOnly 保护规则

更新日期：2026-08-01

状态：通用 sidecar、玩家保护管理器、自动化、目视检查和 Release 部署完成；
默认规则为空。等待玩家使用测试存档做游戏内验收，本阶段尚不能关闭。

## 已实现

- 移除 MoveOnly 对全部猫统一 `move_allowed=true` 的临时覆盖，重新使用既有
  ProtectionPolicy 决策。
- `NoMove`、`NoCullOrMove` 和 `FullyUnmanaged` 猫不会进入移动计划。
- sidecar 支持 `fixed_room`；目标人数分配会先预留固定猫位置，具体猫指派与
  执行前置条件再次检查目标房。
- 保护管理器自动发现本机存档、猫和房间；只有玩家点击“应用保护”才会写入
  个体规则，也可以移除当前猫的规则。
- 新规则绑定由 CatId、出生日、品种和声音组成的稳定猫指纹，不绑定存档文件；
  改名和搬房不会丢失保护，同一 CatId 在不同存档中可以分别管理。
- sidecar 仍兼容旧 `source_save_name` 规则；身份冲突或重复匹配继续
  fail-closed。
- 第一次点击后、第二次点击前重新读取保护摘要。保护级别或固定房变化时取消
  旧预览，不执行移动。
- 缺少 sidecar 等同空规则以保持原有自动整理；文件损坏、未知固定房或身份
  冲突继续 fail-closed。
- 真实淘汰仍关闭，MoveOnly 基线继续为所有猫合并 `NoCull`。

## 当前 build 边界

- 当前快照没有已验证的游戏原生收藏、锁定和特殊猫字段，因此本阶段不伪造
  这些值；需要保护的猫使用 MOD sidecar 明确设置。
- 房间没有虚构硬容量；只有已有的、经过确认的容量门才参与固定房校验。
- 默认仓库与安装配置都为空，不绑定任何个人存档、CatId 或猫名。
- 存档选择只用于列出猫，不会自动创建、继承或推测保护规则。

## 通用性修正

- 不再向安装目录写入针对测试存档或具体猫的预设规则。
- 自动化使用虚构 fixture 身份验证 NoMove、固定房和摘要变化，不读取或依赖
  玩家存档内容。
- 当前实现不能因为缺少原生收藏/锁定数据就把某份玩家存档当成规则来源。

## 自动化与命令

- `auto_cattery_tests.exe`：通过。
- `.\tools\build.ps1 -Configuration Debug`：CTest 5/5 通过。
- `.\tools\build.ps1 -Configuration Release`：CTest 5/5 通过。
- `.\tools\deploy.ps1 -GameRoot <Mewgenics> -Configuration Release`：通过。
- 安装版外部编辑器目视检查：主窗口入口可打开保护管理器，自动发现
  25/8/79/9 猫存档；初始无猫被选中、无猫被保护。
- 安装目录 `protection.json`：`records=0`、`blacklist=0`。
- `git diff --check`：提交前执行。

## 主要文件

- `include/auto_cattery/protection/{sidecar,identity,editor_model}.hpp`
- `src/protection/{sidecar,sidecar_writer,identity,editor_model}.cpp`
- `src/classification/protection_adapter.cpp`
- `src/workflow/move_only_protection.cpp`
- `src/settings_app/protection_editor*.{hpp,cpp}`
- `src/settings_app/{settings_app,settings_form}.cpp`
- `tests/{protection_sidecar,classifier,protection_editor}_tests.cpp`
- `config/protection.schema.json`

## 风险与后续

- 当前 build 没有已验证的游戏原生收藏/锁定/特殊状态字段；在出现真实证据前
  不读取或伪造这些来源。
- 需要玩家在测试存档验证 NoMove、fixed_room 和移除规则。真实淘汰必须等
  这项验收通过后开始。
- 后续业务项：真实淘汰与 journal/撤销、相同猫数存档身份匹配、详细预览。
- 本阶段本地 commit：见最终回复。

是否 push：否

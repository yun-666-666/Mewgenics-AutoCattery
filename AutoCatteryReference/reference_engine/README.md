# Python 参考引擎

只读、确定性、不接触游戏进程或存档。用途：

- 用 JSON fixture 验证评分、分类和房间规划规则；
- 给 Codex 的 C++ 实现提供对照结果；
- 快速复现边界条件。

运行：

```bash
python autocattery_reference.py plan --house ../examples/house_fixture.json --config ../examples/config_balanced.json --output ../examples/generated_plan.json
python autocattery_reference.py validate --house ../examples/house_fixture.json --config ../examples/config_balanced.json
```

# DatabaseCheckPlugin ETS 测试材料

- [README](README.md)：构建环境、全量执行命令、报告和插件安装包位置。
- [ETS测试用例设计](ETS测试用例设计.md)：插件安装使用方法，以及67个用例的“用例名、测试点、预期结果”表。
- [ETS测试验证结果](ETS测试验证结果.md)：逐例告警结果及验证范围。
- `EtsAcceptance/Rule*/`：67个独立ETS输入文件，自动化验证直接读取此目录。
- `src/test/kotlin/com/example/databasecheck/inspections/EtsAcceptanceInspectionTest.kt`：自动验收入口。

在当前 `DatabaseCheckPlugin` 目录执行：

```bash
gradle cleanTest test buildPlugin --no-daemon --console=plain
```

全量111个JUnit方法包含10组ETS验收方法，覆盖67个ETS输入。
结果见 `build/reports/tests/test/index.html`，安装包直接生成于 `DatabaseCheck-1.0.0.zip`。
本套材料验证插件静态告警，不执行输入中的数据库、文件或网络操作。

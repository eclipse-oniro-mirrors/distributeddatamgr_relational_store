# DatabaseCheckPlugin ETS 测试用例设计与使用说明

## 1. 测试范围与交付物

本套材料验证 DatabaseCheckPlugin 对 ETS 源码的静态检查结果，基于提交 `096ad675714b17bfc840fa6050e97231f0fa4c94`。
覆盖 Rule1、2、3、4、5、6、7、9、12、14，共 **67 个独立 ETS 文件**，同时提供应告警的反例和不应误报的正例。
重点回归路径变量、别名、重新赋值、作用域遮蔽、事务变量名、模板字符串、嵌套参数以及错误码对应处理动作。

- 用例目录：`EtsAcceptance/Rule*/`。
- 自动验证入口：`src/test/kotlin/com/example/databasecheck/inspections/EtsAcceptanceInspectionTest.kt`。
- 每个 ETS 文件包含导入、完整函数，以及 `Case/Rule/Expected/Message/Point` 元数据。
- `Expected` 表示本文件在**目标规则单独启用**时的告警条数；`Message` 表示告警应包含的关键内容。
- 本套文件是插件输入材料，不是 Hypium 运行测试，不实际创建数据库、调用文件接口或发送网络请求。
  函数没有顶层调用，手工验收时不需要运行应用或接入设备。

## 2. 插件如何编译与打包

### 2.1 环境与版本

| 项目 | 配置与要求 |
| --- | --- |
| Java | 本地验证使用 JDK 21；生成 JVM 17 字节码 |
| Gradle | 使用 8.12.1，与 `gradle/wrapper/gradle-wrapper.properties` 一致 |
| 插件构建配置 | `build.gradle.kts`，Kotlin 1.9.25，Gradle IntelliJ Plugin 1.17.3 |
| 编译/自动化测试平台 | IntelliJ IDEA Community 2023.3.6 |
| IDE 安装范围 | IntelliJ 平台构建号至少 233，不设置 `until-build` 上限 |
| 网络依赖 | 首次构建需要取得 Gradle、Maven 依赖和 IntelliJ 平台 SDK，后续可复用缓存 |

IDE 的产品版本号和 IntelliJ 平台构建号不是同一个数字。请在 IDE 的 About/关于页面查看平台构建号。
不设置版本上限只移除安装时的上限限制；目标 DevEco Studio 版本的运行兼容性仍需按本说明验证。

### 2.2 构建步骤

在 `relational_store/DatabaseCheckPlugin` 目录执行。以下命令中的 JDK 目录应替换为实际安装路径，
并把 Gradle 8.12.1 的 `bin` 目录加入 `PATH`：

```bash
cd /path/to/relational_store/DatabaseCheckPlugin
export JAVA_HOME=/path/to/jdk-21
export PATH="$JAVA_HOME/bin:/path/to/gradle-8.12.1/bin:$PATH"
java -version
gradle --version

# 只检查插件源码编译
gradle compileKotlin --no-daemon --console=plain

# 全部自动化测试和可安装 ZIP；推荐交付前执行
gradle test buildPlugin --no-daemon --console=plain
```

当前仓库含 `gradlew` 和 Wrapper 配置，但缺少 `gradle/wrapper/gradle-wrapper.jar`，
因此直接执行 `./gradlew` 可能报 `Could not find or load main class org.gradle.wrapper.GradleWrapperMain`。
上述步骤使用已安装的 Gradle 8.12.1，不依赖缺失的 JAR。Gradle 下载地址由 Wrapper 配置指定：
[Gradle 8.12.1](https://services.gradle.org/distributions/gradle-8.12.1-bin.zip)。

构建在当前 `DatabaseCheckPlugin` 目录执行，`buildPlugin.destinationDirectory` 设置为项目根目录。成功后得到：

```text
DatabaseCheck-1.0.0.zip       可安装插件包
build/reports/tests/test/index.html              自动化测试 HTML 报告
build/test-results/test/TEST-*.xml                JUnit XML 报告
```

### 2.3 单独执行本套 ETS 验证

```bash
gradle test --tests '*EtsAcceptanceInspectionTest' --no-daemon --console=plain

# 只验证某条规则，例如 Rule1
gradle test --tests '*EtsAcceptanceInspectionTest.testRule1EtsCases' --no-daemon --console=plain
```

验证器加载交付的 ETS 原文件，走 `buildVisitor(...).visitFile(...)` 检查入口，
逐文件断言目标规则的告警条数、消息关键内容及源码范围。10 个 JUnit 方法分别执行10组规则用例，
合计验证67个 ETS 文件，不能把“JUnit 方法数10”误读为“只有10个 ETS 场景”。

`build.gradle.kts` 的 `test` 任务已将这批 ETS 目录声明为输入，修改文件后会重新验证。
若只把材料复制到其他分支或项目，也应在 `test` 配置内加入以下声明，或执行 `gradle cleanTest test` 强制重跑：

```kotlin
inputs.dir("EtsAcceptance")
```

如构建停留在 `initializeIntelliJPlugin` 的在线自更新查询，可将以下内容保存为插件目录之外的
`local.init.gradle`，再使用 `gradle -I /path/to/local.init.gradle test buildPlugin --no-daemon --console=plain`。
该设置仅关闭构建插件自身的版本查询，不跳过编译或测试，也不替代下载所需依赖：

```groovy
allprojects {
    tasks.matching { it.name == 'initializeIntelliJPlugin' }.configureEach {
        selfUpdateCheck.set(false)
    }
}
```

### 2.4 开发时运行沙箱 IDE

```bash
gradle runIde --no-daemon --console=plain
```

该命令启动带有本插件的 IntelliJ IDEA 沙箱，便于开发检查；它不等同于目标 DevEco Studio 的手动安装验收。

## 3. 插件如何安装

1. 先按第2节生成 `DatabaseCheck-1.0.0.zip`，保留 ZIP 原样。
2. 打开 DevEco Studio 的 `Settings/设置 → Plugins/插件`；macOS 可从应用的 Settings/Preferences 进入。
3. 点击插件页面的齿轮菜单，选择 `Install Plugin from Disk.../从磁盘安装插件`。
4. 选择上述 ZIP，按 IDE 提示安装并重启。
5. 返回已安装插件列表，确认 `DatabaseCheck` 已启用。插件 ID 为 `com.example.DatabaseCheck`。
6. 在 `Settings → Editor → Inspections` 搜索并展开 `Database Robustness Rules`，勾选待测规则并应用设置。

更新同为 `1.0.0` 的包时，按 IDE 提示替换；若仍加载旧包，先卸载旧插件、重启，再从磁盘安装新 ZIP。
如果 IDE 报版本不兼容，先核对 About 中的平台构建号至少为233，并确认安装的是本次新打包的 ZIP。
当前包的声明应为 `<idea-version since-build="233" />`，没有 `until-build`。

## 4. 插件如何使用和验收用例

### 4.1 手工检查步骤

1. 在目标 DevEco Studio 中打开一个已配置对应 OpenHarmony/HarmonyOS SDK 的 ArkTS 工程。
2. 将 `EtsAcceptance` 目录复制到该工程的 `entry/src/main/ets/plugin_checks/`，保留 `Rule*` 子目录及 `.ets` 后缀。
   每个文件本身就是一个模块，不需要添加到页面、Ability 或应用启动流程，也不要调用其中的反例函数。
3. 在检查设置中仅启用当前待测的 `DatabaseRuleN`，例如 Rule1 对应 `DatabaseRule1`。
   若同时启用多条规则，只统计本设计表中列出的目标规则结果；其他规则的伴随告警单独记录。
4. 打开目标 ETS 文件，查看编辑器的黄色警告与悬浮说明。也可使用 IDE 的 Find Action/查找操作搜索
   `Inspect Code`，选择该文件或规则目录，查看检查结果窗口。
5. 按第5节逐行核对告警条数和内容。`0` 表示目标规则不能误报；不表示该示例已经满足所有生产代码规范。
6. 记录实际结果、插件构建来源及 IDE 平台构建号；告警位置应落在对应操作或 try/catch 的源码范围内。

### 4.2 用例独立性与约定

- 不把多个文件拼接成一个文件。Rule14 在当前文件内按数据库名分组，拼接会引入无关的配置比较。
- `delay: () => Promise<void>` 是 Rule9 正例传入的退避回调契约，应由应用提供实际延迟实现；没有空实现冒充退避。
  示例仅重试一次，第二次失败直接向调用方传播。自动验证不执行该回调，不使用固定等待。
- 恢复正例展示真实 `store.restore` 调用；有效备份、磁盘清理成功与执行顺序需要应用运行测试另行验证。
- Rule7 仅覆盖既有 `@ohos.fileio` 兼容模块的 `chmod`，不新增独立 `chmodSync` 用例。
  该导入不等同于 `@ohos.file.fs`；若目标 SDK 已移除旧模块，这两个文件只用于现有词法规则验收，
  不能当成目标 SDK 的可运行 API 示例。
- 路径跟踪限当前文件的可见赋值、别名、拼接、简单模板和文件句柄；任意函数返回路径、对象字段赋值、
  跨文件数据流及完整条件分支合流不在本轮验收承诺范围内。
- 本轮没有修改插件检测逻辑；本套材料与既有101个测试互为补充。自动验收直接读取插件根目录的 ETS 文件。

## 5. 用例设计表

表中“告警”均指文件名所属的目标规则。用例名与 ETS 文件名一一对应；点击用例名可打开源码。

### 5.1 Rule1：数据库路径和文件接口（12例）

| 用例名 | 测试点 | 预期结果 |
| --- | --- | --- |
| [R01_DirectDatabasePath_Warn](EtsAcceptance/Rule1/R01_DirectDatabasePath_Warn.ets) | 直接向 fileIo.openSync 传入 databaseDir 路径 | Rule1 告警 1 条，包含 `Prohibited use` |
| [R01_StaticPathVariable_Warn](EtsAcceptance/Rule1/R01_StaticPathVariable_Warn.ets) | 静态数据库路径先存入变量 | Rule1 告警 1 条，包含 `Prohibited use` |
| [R01_TypedPathAlias_Warn](EtsAcceptance/Rule1/R01_TypedPathAlias_Warn.ets) | 带类型的路径变量经两层别名与拼接传给接口 | Rule1 告警 1 条，包含 `Prohibited use` |
| [R01_OuterPath_Warn](EtsAcceptance/Rule1/R01_OuterPath_Warn.ets) | 函数内使用可见的外层数据库路径 | Rule1 告警 1 条，包含 `Prohibited use` |
| [R01_AliasBeforeReassignment_Warn](EtsAcceptance/Rule1/R01_AliasBeforeReassignment_Warn.ets) | 别名保留赋值时的数据库路径，原变量后改普通路径 | Rule1 告警 1 条，包含 `Prohibited use` |
| [R01_TemplatePath_Warn](EtsAcceptance/Rule1/R01_TemplatePath_Warn.ets) | 简单模板插值中的路径变量 | Rule1 告警 1 条，包含 `Prohibited use` |
| [R01_FileDescriptor_OpenClose_Warn](EtsAcceptance/Rule1/R01_FileDescriptor_OpenClose_Warn.ets) | 数据库路径打开并关闭返回文件描述符，每个操作报告一次 | Rule1 告警 2 条，包含 `Prohibited use` |
| [R01_OrdinaryPath_Pass](EtsAcceptance/Rule1/R01_OrdinaryPath_Pass.ets) | 普通文件路径变量及文件描述符不误报 | Rule1 无告警 |
| [R01_FutureAssignment_Pass](EtsAcceptance/Rule1/R01_FutureAssignment_Pass.ets) | 普通文件调用不借用之后的数据库路径赋值 | Rule1 无告警 |
| [R01_OrdinaryReassignment_Pass](EtsAcceptance/Rule1/R01_OrdinaryReassignment_Pass.ets) | 数据库路径变量已在调用前重新赋为普通路径 | Rule1 无告警 |
| [R01_ParameterShadowing_Pass](EtsAcceptance/Rule1/R01_ParameterShadowing_Pass.ets) | 同名函数参数遮蔽外层数据库路径 | Rule1 无告警 |
| [R01_SiblingFunction_Pass](EtsAcceptance/Rule1/R01_SiblingFunction_Pass.ets) | 相邻函数的数据库路径局部变量不污染普通文件函数 | Rule1 无告警 |

### 5.2 Rule2：文件复制（5例）

| 用例名 | 测试点 | 预期结果 |
| --- | --- | --- |
| [R02_SourceAlias_Warn](EtsAcceptance/Rule2/R02_SourceAlias_Warn.ets) | 复制源路径来自数据库路径变量 | Rule2 告警 1 条，包含 `file interface` |
| [R02_DestinationAlias_Warn](EtsAcceptance/Rule2/R02_DestinationAlias_Warn.ets) | 复制目标路径来自数据库路径变量 | Rule2 告警 1 条，包含 `file interface` |
| [R02_OrdinaryCopy_Pass](EtsAcceptance/Rule2/R02_OrdinaryCopy_Pass.ets) | 普通文件源和目标不误报 | Rule2 无告警 |
| [R02_FutureAssignment_Pass](EtsAcceptance/Rule2/R02_FutureAssignment_Pass.ets) | 复制操作不借用后续路径赋值 | Rule2 无告警 |
| [R02_RdbBackup_Pass](EtsAcceptance/Rule2/R02_RdbBackup_Pass.ets) | 使用 RDB backup 进行备份 | Rule2 无告警 |

### 5.3 Rule3：事务使用优化（6例）

| 用例名 | 测试点 | 预期结果 |
| --- | --- | --- |
| [R03_RenamedStore_SingleCrud_Warn](EtsAcceptance/Rule3/R03_RenamedStore_SingleCrud_Warn.ets) | store 命名及类型注解下的单操作事务 | Rule3 告警 1 条，包含 `only one CRUD operation` |
| [R03_MultipleCrud_Pass](EtsAcceptance/Rule3/R03_MultipleCrud_Pass.ets) | 包含多个 CRUD 的已提交事务 | Rule3 无告警 |
| [R03_LoopCrud_Pass](EtsAcceptance/Rule3/R03_LoopCrud_Pass.ets) | 事务循环执行同一 CRUD，不作为单操作事务 | Rule3 无告警 |
| [R03_TemplateLoop_Warn](EtsAcceptance/Rule3/R03_TemplateLoop_Warn.ets) | 模板内伪循环不能取消单操作事务告警 | Rule3 告警 1 条，包含 `only one CRUD operation` |
| [R03_NetworkInsideTransaction_Warn](EtsAcceptance/Rule3/R03_NetworkInsideTransaction_Warn.ets) | 多操作事务中插入真实网络请求 | Rule3 告警 1 条，包含 `Time-consuming operation` |
| [R03_MultilineCreation_Warn](EtsAcceptance/Rule3/R03_MultilineCreation_Warn.ets) | 跨行创建事务，参数内嵌套函数调用 | Rule3 告警 1 条，包含 `only one CRUD operation` |

### 5.4 Rule4：事务提交与回滚（6例）

| 用例名 | 测试点 | 预期结果 |
| --- | --- | --- |
| [R04_MissingCommit_Warn](EtsAcceptance/Rule4/R04_MissingCommit_Warn.ets) | store 创建的事务没有提交 | Rule4 告警 1 条，包含 `Missing transaction commit` |
| [R04_TypedCommit_Pass](EtsAcceptance/Rule4/R04_TypedCommit_Pass.ets) | 带类型事务执行多操作并提交 | Rule4 无告警 |
| [R04_MissingRollback_Warn](EtsAcceptance/Rule4/R04_MissingRollback_Warn.ets) | catch 只有日志，无事务回滚 | Rule4 告警 1 条，包含 `rollback` |
| [R04_RollbackInCatch_Pass](EtsAcceptance/Rule4/R04_RollbackInCatch_Pass.ets) | catch 中真实回滚对应事务 | Rule4 无告警 |
| [R04_UncalledArrowCommit_Warn](EtsAcceptance/Rule4/R04_UncalledArrowCommit_Warn.ets) | 未调用的箭头函数内提交不能算已提交 | Rule4 告警 1 条，包含 `Missing transaction commit` |
| [R04_ReassignmentCannotBorrowCommit_Warn](EtsAcceptance/Rule4/R04_ReassignmentCannotBorrowCommit_Warn.ets) | 第二个事务不能借用同名变量之前的提交 | Rule4 告警 1 条，包含 `Missing transaction commit` |

### 5.5 Rule5：旧式事务接口（4例）

| 用例名 | 测试点 | 预期结果 |
| --- | --- | --- |
| [R05_RenamedLegacyBegin_Warn](EtsAcceptance/Rule5/R05_RenamedLegacyBegin_Warn.ets) | 任意 store 变量名使用旧 beginTransaction | Rule5 告警 1 条，包含 `Deprecated beginTransaction` |
| [R05_TypedStoreCommit_Warn](EtsAcceptance/Rule5/R05_TypedStoreCommit_Warn.ets) | RdbStore 类型参数上调用旧 commit | Rule5 告警 1 条，包含 `Deprecated commit` |
| [R05_TransactionNamedRdbStore_Pass](EtsAcceptance/Rule5/R05_TransactionNamedRdbStore_Pass.ets) | 来自 createTransaction 的对象即使叫 rdbStore 也不误报 | Rule5 无告警 |
| [R05_TypedTransactionCommit_Pass](EtsAcceptance/Rule5/R05_TypedTransactionCommit_Pass.ets) | Transaction 类型参数的 commit/rollback 不误报 | Rule5 无告警 |

### 5.6 Rule6：删除前关闭句柄（4例）

| 用例名 | 测试点 | 预期结果 |
| --- | --- | --- |
| [R06_MissingClose_Warn](EtsAcceptance/Rule6/R06_MissingClose_Warn.ets) | 删除前未关闭数据库句柄 | Rule6 告警 1 条，包含 `Missing rdbStore.close` |
| [R06_RenamedOptionalClose_Pass](EtsAcceptance/Rule6/R06_RenamedOptionalClose_Pass.ets) | 不同变量名且可选访问的 close 位于删除前 | Rule6 无告警 |
| [R06_FutureClose_Warn](EtsAcceptance/Rule6/R06_FutureClose_Warn.ets) | 删除之后的 close 不能抵消告警 | Rule6 告警 1 条，包含 `Missing rdbStore.close` |
| [R06_SiblingClose_Warn](EtsAcceptance/Rule6/R06_SiblingClose_Warn.ets) | 相邻函数内的 close 不算当前函数已关闭 | Rule6 告警 1 条，包含 `Missing rdbStore.close` |

### 5.7 Rule7：既有权限接口（2例）

| 用例名 | 测试点 | 预期结果 |
| --- | --- | --- |
| [R07_LegacyChmod_PathAlias_Warn](EtsAcceptance/Rule7/R07_LegacyChmod_PathAlias_Warn.ets) | 旧 fileio 模块的 chmod 接收数据库路径变量 | Rule7 告警 1 条，包含 `Permission modification` |
| [R07_LegacyChmod_OrdinaryPath_Pass](EtsAcceptance/Rule7/R07_LegacyChmod_OrdinaryPath_Pass.ets) | 旧 fileio 模块 chmod 操作普通文件路径不误报 | Rule7 无告警 |

### 5.8 Rule9：错误码处理动作（18例）

| 用例名 | 测试点 | 预期结果 |
| --- | --- | --- |
| [R09_Busy_LogOnly_Warn](EtsAcceptance/Rule9/R09_Busy_LogOnly_Warn.ets) | busy=14800024 仅日志 | Rule9 告警 1 条，包含 `14800024` |
| [R09_Busy_RetryWithDelay_Pass](EtsAcceptance/Rule9/R09_Busy_RetryWithDelay_Pass.ets) | busy 分支先退避后重试一次，失败向调用方传播 | Rule9 无告警 |
| [R09_Locked_RetryWithoutDelay_Warn](EtsAcceptance/Rule9/R09_Locked_RetryWithoutDelay_Warn.ets) | locked=14800025 有重试无退避 | Rule9 告警 1 条，包含 `14800025` |
| [R09_Locked_RetryWithDelay_Pass](EtsAcceptance/Rule9/R09_Locked_RetryWithDelay_Pass.ets) | locked 分支退避并重试 | Rule9 无告警 |
| [R09_Corrupt_RetryOnly_Warn](EtsAcceptance/Rule9/R09_Corrupt_RetryOnly_Warn.ets) | corrupt=14800011 盲目重试无恢复 | Rule9 告警 1 条，包含 `14800011` |
| [R09_Corrupt_Restore_Pass](EtsAcceptance/Rule9/R09_Corrupt_Restore_Pass.ets) | corrupt 分支从备份恢复 | Rule9 无告警 |
| [R09_IoError_LogOnly_Warn](EtsAcceptance/Rule9/R09_IoError_LogOnly_Warn.ets) | I/O=14800028 仅日志 | Rule9 告警 1 条，包含 `14800028` |
| [R09_IoError_Restore_Pass](EtsAcceptance/Rule9/R09_IoError_Restore_Pass.ets) | I/O 错误通过恢复满足规则 | Rule9 无告警 |
| [R09_DiskFull_RetryOnly_Warn](EtsAcceptance/Rule9/R09_DiskFull_RetryOnly_Warn.ets) | 磁盘满=14800029 有重试无清理 | Rule9 告警 1 条，包含 `14800029` |
| [R09_DiskFull_CleanupAndRetry_Pass](EtsAcceptance/Rule9/R09_DiskFull_CleanupAndRetry_Pass.ets) | 磁盘满分支清理非数据库缓存后重试一次 | Rule9 无告警 |
| [R09_WalTooLarge_CloseOnly_Warn](EtsAcceptance/Rule9/R09_WalTooLarge_CloseOnly_Warn.ets) | WAL=14800047 只关闭结果集，缺 checkpoint | Rule9 告警 1 条，包含 `14800047` |
| [R09_WalTooLarge_CloseAndCheckpoint_Pass](EtsAcceptance/Rule9/R09_WalTooLarge_CloseAndCheckpoint_Pass.ets) | WAL 超限分支关闭结果集并执行 TRUNCATE checkpoint | Rule9 无告警 |
| [R09_EmptyCatch_Warn](EtsAcceptance/Rule9/R09_EmptyCatch_Warn.ets) | 空 catch 不能作为错误处理 | Rule9 告警 1 条，包含 `No verifiable` |
| [R09_SwitchMergedBusyLocked_Pass](EtsAcceptance/Rule9/R09_SwitchMergedBusyLocked_Pass.ets) | switch 连续 busy/locked case 共享退避和重试 | Rule9 无告警 |
| [R09_AdjacentBranchCannotSupplyRetry_Warn](EtsAcceptance/Rule9/R09_AdjacentBranchCannotSupplyRetry_Warn.ets) | busy 不能借用 else 中的重试和退避 | Rule9 告警 1 条，包含 `14800024` |
| [R09_TemplateFakeRecovery_Warn](EtsAcceptance/Rule9/R09_TemplateFakeRecovery_Warn.ets) | 模板中的 restore 文本不能满足恢复动作 | Rule9 告警 1 条，包含 `14800011` |
| [R09_EmptyNamedRecovery_Warn](EtsAcceptance/Rule9/R09_EmptyNamedRecovery_Warn.ets) | 同文件空 recover 辅助函数不能只凭名称通过 | Rule9 告警 1 条，包含 `14800011` |
| [R09_NamedRecoveryBody_Pass](EtsAcceptance/Rule9/R09_NamedRecoveryBody_Pass.ets) | 同文件 recover 辅助函数内实际 restore 可被展开 | Rule9 无告警 |

### 5.9 Rule12：SQLite PRAGMA（3例）

| 用例名 | 测试点 | 预期结果 |
| --- | --- | --- |
| [R12_SynchronousOffVariable_Warn](EtsAcceptance/Rule12/R12_SynchronousOffVariable_Warn.ets) | SQL 变量中设置 synchronous=OFF | Rule12 告警 1 条，包含 `Prohibited PRAGMA` |
| [R12_WalCheckpoint_Pass](EtsAcceptance/Rule12/R12_WalCheckpoint_Pass.ets) | TRUNCATE checkpoint 不属于禁用 PRAGMA | Rule12 无告警 |
| [R12_OrdinarySql_Pass](EtsAcceptance/Rule12/R12_OrdinarySql_Pass.ets) | 普通建表 SQL 不误报 | Rule12 无告警 |

### 5.10 Rule14：数据库配置一致性（7例）

| 用例名 | 测试点 | 预期结果 |
| --- | --- | --- |
| [R14_SecurityMismatch_Warn](EtsAcceptance/Rule14/R14_SecurityMismatch_Warn.ets) | 同名数据库 securityLevel 不一致 | Rule14 告警 1 条，包含 `configuration inconsistency` |
| [R14_EncryptMismatch_Warn](EtsAcceptance/Rule14/R14_EncryptMismatch_Warn.ets) | 同名数据库 encrypt 不一致 | Rule14 告警 1 条，包含 `configuration inconsistency` |
| [R14_IdenticalConfig_Pass](EtsAcceptance/Rule14/R14_IdenticalConfig_Pass.ets) | 同名数据库配置完全相同 | Rule14 无告警 |
| [R14_DifferentDatabases_Pass](EtsAcceptance/Rule14/R14_DifferentDatabases_Pass.ets) | 不同数据库允许配置不同 | Rule14 无告警 |
| [R14_NestedCallAndComma_Warn](EtsAcceptance/Rule14/R14_NestedCallAndComma_Warn.ets) | 嵌套 context 调用与数据库名中的逗号不截断参数 | Rule14 告警 1 条，包含 `configuration inconsistency` |
| [R14_CrossFunctionTypedConfig_Warn](EtsAcceptance/Rule14/R14_CrossFunctionTypedConfig_Warn.ets) | 同文件不同函数分别解析带类型配置后比较 | Rule14 告警 1 条，包含 `configuration inconsistency` |
| [R14_SiblingConfigDoesNotShadow_Pass](EtsAcceptance/Rule14/R14_SiblingConfigDoesNotShadow_Pass.ets) | 相邻函数局部配置不能覆盖当前可见的全局配置 | Rule14 无告警 |

## 6. 验证记录与能力边界

验证环境：Gradle 8.12.1、JDK 21、JVM目标17，IntelliJ IDEA Community 2023.3.6。
本地构建使用第2.3节的 init 脚本关闭在线自更新查询。

本次结果见 [ETS测试验证结果](ETS测试验证结果.md)：67个 ETS 场景均符合预期，全套111个 JUnit 方法通过，插件构建成功。验证项分别为：

- ETS 验收：67个文件的目标规则结果与设计表一致，正例不误报、反例有实际告警。
- 插件构建与全套单测：运行 `test buildPlugin`，检查完整 JUnit 报告及 ZIP。
- ETS 语法辅助检查：TypeScript 解析/转译检查可排除基础语法错误，但不等同于 ArkTS 编译器或 SDK 类型检查。
- 目标 IDE：本轮未执行 DevEco Studio 手工安装和应用工程 Hvigor 编译，不将本地 fixture 结果描述为这两项通过。

本插件采用词法/语法级启发式检查，不是完整的 ArkTS 类型、跨文件符号或控制流证明。
Rule6 只检查同一函数中删除之前出现的关闭调用，不证明被关对象与被删库的身份相同。
Rule9 的动作识别不证明处理成功或每个执行路径都有效；Rule14 只比较当前文件内的配置文本。
这些边界不应被写成已支持的正例期望。

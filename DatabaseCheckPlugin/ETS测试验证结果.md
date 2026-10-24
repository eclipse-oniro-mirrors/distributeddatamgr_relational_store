# ETS 测试验证记录

验证时间：2026-10-24 09:43:44 UTC+08:00（北京时间）。
插件源码基线：`096ad675714b17bfc840fa6050e97231f0fa4c94`。
本轮新增测试输入、验证器、Gradle测试输入目录声明和说明文档，未修改插件检测实现。

| 验证项目 | 结果 | 说明 |
| --- | --- | --- |
| ETS 源文件 | 67/67 预期一致 | 38 个应告警反例、29 个不误报正例；实际调用插件 visitor |
| 全套 JUnit | 111/111 通过 | 0失败、0错误、0跳过；原101方法加10组ETS验收方法 |
| 插件编译及打包 | 通过 | `test buildPlugin` 成功；未改实现的任务可复用已验证产物 |
| 基础语法辅助检查 | 67/67 无解析错误 | TypeScript 4.9.5 `transpileModule`，不等同于 ArkTS/SDK 编译 |
| 文档与源码对应 | 通过 | 67个唯一用例名、元数据、表格及相对链接核对通过 |
| DevEco 手动安装与展示 | 未执行 | 需在实际目标 IDE 按用例文档验收 |
| ArkTS 应用工程 Hvigor 编译 | 未执行 | 不将本轮语法辅助检查或 fixture 验证描述为应用编译通过 |

本地执行命令（在 `DatabaseCheckPlugin` 目录，Gradle 8.12.1 / JDK21）：

```bash
gradle -I /path/to/local.init.gradle test buildPlugin --no-daemon --console=plain
```

`local.init.gradle` 内容见主文档，也随测试材料压缩包提供；只关闭在线自更新查询。

下表记录每个 ETS 文件的实际目标规则告警条数，均与预期一致：

| 用例名 | 目标规则 | 预期告警数 | 实际告警数 | 结果 |
| --- | --- | --- | --- | --- |
| R01_AliasBeforeReassignment_Warn | Rule1 | 1 | 1 | PASS |
| R01_DirectDatabasePath_Warn | Rule1 | 1 | 1 | PASS |
| R01_FileDescriptor_OpenClose_Warn | Rule1 | 2 | 2 | PASS |
| R01_FutureAssignment_Pass | Rule1 | 0 | 0 | PASS |
| R01_OrdinaryPath_Pass | Rule1 | 0 | 0 | PASS |
| R01_OrdinaryReassignment_Pass | Rule1 | 0 | 0 | PASS |
| R01_OuterPath_Warn | Rule1 | 1 | 1 | PASS |
| R01_ParameterShadowing_Pass | Rule1 | 0 | 0 | PASS |
| R01_SiblingFunction_Pass | Rule1 | 0 | 0 | PASS |
| R01_StaticPathVariable_Warn | Rule1 | 1 | 1 | PASS |
| R01_TemplatePath_Warn | Rule1 | 1 | 1 | PASS |
| R01_TypedPathAlias_Warn | Rule1 | 1 | 1 | PASS |
| R02_DestinationAlias_Warn | Rule2 | 1 | 1 | PASS |
| R02_FutureAssignment_Pass | Rule2 | 0 | 0 | PASS |
| R02_OrdinaryCopy_Pass | Rule2 | 0 | 0 | PASS |
| R02_RdbBackup_Pass | Rule2 | 0 | 0 | PASS |
| R02_SourceAlias_Warn | Rule2 | 1 | 1 | PASS |
| R03_LoopCrud_Pass | Rule3 | 0 | 0 | PASS |
| R03_MultilineCreation_Warn | Rule3 | 1 | 1 | PASS |
| R03_MultipleCrud_Pass | Rule3 | 0 | 0 | PASS |
| R03_NetworkInsideTransaction_Warn | Rule3 | 1 | 1 | PASS |
| R03_RenamedStore_SingleCrud_Warn | Rule3 | 1 | 1 | PASS |
| R03_TemplateLoop_Warn | Rule3 | 1 | 1 | PASS |
| R04_MissingCommit_Warn | Rule4 | 1 | 1 | PASS |
| R04_MissingRollback_Warn | Rule4 | 1 | 1 | PASS |
| R04_ReassignmentCannotBorrowCommit_Warn | Rule4 | 1 | 1 | PASS |
| R04_RollbackInCatch_Pass | Rule4 | 0 | 0 | PASS |
| R04_TypedCommit_Pass | Rule4 | 0 | 0 | PASS |
| R04_UncalledArrowCommit_Warn | Rule4 | 1 | 1 | PASS |
| R05_RenamedLegacyBegin_Warn | Rule5 | 1 | 1 | PASS |
| R05_TransactionNamedRdbStore_Pass | Rule5 | 0 | 0 | PASS |
| R05_TypedStoreCommit_Warn | Rule5 | 1 | 1 | PASS |
| R05_TypedTransactionCommit_Pass | Rule5 | 0 | 0 | PASS |
| R06_FutureClose_Warn | Rule6 | 1 | 1 | PASS |
| R06_MissingClose_Warn | Rule6 | 1 | 1 | PASS |
| R06_RenamedOptionalClose_Pass | Rule6 | 0 | 0 | PASS |
| R06_SiblingClose_Warn | Rule6 | 1 | 1 | PASS |
| R07_LegacyChmod_OrdinaryPath_Pass | Rule7 | 0 | 0 | PASS |
| R07_LegacyChmod_PathAlias_Warn | Rule7 | 1 | 1 | PASS |
| R09_AdjacentBranchCannotSupplyRetry_Warn | Rule9 | 1 | 1 | PASS |
| R09_Busy_LogOnly_Warn | Rule9 | 1 | 1 | PASS |
| R09_Busy_RetryWithDelay_Pass | Rule9 | 0 | 0 | PASS |
| R09_Corrupt_Restore_Pass | Rule9 | 0 | 0 | PASS |
| R09_Corrupt_RetryOnly_Warn | Rule9 | 1 | 1 | PASS |
| R09_DiskFull_CleanupAndRetry_Pass | Rule9 | 0 | 0 | PASS |
| R09_DiskFull_RetryOnly_Warn | Rule9 | 1 | 1 | PASS |
| R09_EmptyCatch_Warn | Rule9 | 1 | 1 | PASS |
| R09_EmptyNamedRecovery_Warn | Rule9 | 1 | 1 | PASS |
| R09_IoError_LogOnly_Warn | Rule9 | 1 | 1 | PASS |
| R09_IoError_Restore_Pass | Rule9 | 0 | 0 | PASS |
| R09_Locked_RetryWithDelay_Pass | Rule9 | 0 | 0 | PASS |
| R09_Locked_RetryWithoutDelay_Warn | Rule9 | 1 | 1 | PASS |
| R09_NamedRecoveryBody_Pass | Rule9 | 0 | 0 | PASS |
| R09_SwitchMergedBusyLocked_Pass | Rule9 | 0 | 0 | PASS |
| R09_TemplateFakeRecovery_Warn | Rule9 | 1 | 1 | PASS |
| R09_WalTooLarge_CloseAndCheckpoint_Pass | Rule9 | 0 | 0 | PASS |
| R09_WalTooLarge_CloseOnly_Warn | Rule9 | 1 | 1 | PASS |
| R12_OrdinarySql_Pass | Rule12 | 0 | 0 | PASS |
| R12_SynchronousOffVariable_Warn | Rule12 | 1 | 1 | PASS |
| R12_WalCheckpoint_Pass | Rule12 | 0 | 0 | PASS |
| R14_CrossFunctionTypedConfig_Warn | Rule14 | 1 | 1 | PASS |
| R14_DifferentDatabases_Pass | Rule14 | 0 | 0 | PASS |
| R14_EncryptMismatch_Warn | Rule14 | 1 | 1 | PASS |
| R14_IdenticalConfig_Pass | Rule14 | 0 | 0 | PASS |
| R14_NestedCallAndComma_Warn | Rule14 | 1 | 1 | PASS |
| R14_SecurityMismatch_Warn | Rule14 | 1 | 1 | PASS |
| R14_SiblingConfigDoesNotShadow_Pass | Rule14 | 0 | 0 | PASS |

## 当前仓库目录构建复核

时间：2026-10-24 09:51:36 UTC+08:00（北京时间）。

在当前 `relational_store` 仓库指定 `DatabaseCheckPlugin` 项目执行 `test buildPlugin`，
已同步本次修复源码及测试，构建成功；全套111项JUnit测试通过，67个ETS场景与预期一致。
Gradle 的 `buildPlugin.destinationDirectory` 已设为项目根目录，安装包直接生成于
`DatabaseCheckPlugin/DatabaseCheck-1.0.0.zip`，构建日志为 `DatabaseCheckPlugin/build-current-directory.log`。
安装包没有 `until-build` 上限，最低构建号233。

## 根目录用例提交前全量复核

时间：2026-10-24 10:10:58 UTC+08:00（北京时间）。

自动验收器直接读取 `EtsAcceptance/Rule*/`，该目录已声明为 Gradle test 输入。
在当前仓库的 `DatabaseCheckPlugin` 目录执行 `cleanTest test buildPlugin`（本地 init 脚本仅关闭在线自更新查询）：
全量111项JUnit通过，0失败、0错误、0跳过；67个ETS文件预期和实际告警数一致，插件构建成功。
安装包生成在项目根目录 `DatabaseCheck-1.0.0.zip`。

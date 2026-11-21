# RDB 故障注入使用指导

> 本文面向**使用** RDB（`OHOS::NativeRdb::RdbStore` / `RdbStoreManager` / `RdbHelper`）的应用开发者，只讲怎么配置、怎么触发、怎么恢复。
> 注入机制本身的实现原理与扩展方式，见维护者指南 [`fault_injection_guide.md`](fault_injection_guide.md)。

## 概述

应用需要正确处理 RDB 接口返回的数据库损坏错误码 `E_SQLITE_CORRUPT`（27394104），但通过手工破坏数据库文件来构造损坏，既无法按接口定向，也难以覆盖全部场景。RDB 故障注入 mock 层通过一个**放在数据库同目录的配置文件**，让指定数据库的 RDB 接口按配置返回 `E_SQLITE_CORRUPT`（查询类接口返回 `nullptr`）——不需要真的破坏数据库文件，就可以逐接口、逐次数、按概率地验证应用的损坏处理逻辑。注入命中**只影响返回值**：SQL 不执行、数据不落库、文件不被改动；未配置的数据库行为与未开启该能力时完全一致。

## 快速开始

以验证 `Insert` 返回损坏错误码为例，三步完成。

**第 1 步：写配置文件。** 在数据库文件所在目录创建固定名称为 `rdb_fault_inject_config.json` 的文件，`databases` 的键是你的数据库文件名：

```json
{
    "databases": {
        "app.db": {
            "operations": { "insert": { "count": 1 } }
        }
    }
}
```

把 `app.db` 替换为真实库名。在 root/调试版设备上可用 `hdc` 推送，例如：

```bash
hdc file send rdb_fault_inject_config.json /data/app/el2/<userId>/database/<bundleName>/
```

**第 2 步：正常打开数据库。** 只配置 `operations` 条目时打开不受影响，按既有方式调用即可：

```cpp
#include "rdb_helper.h"
#include "rdb_store_config.h"
#include "values_bucket.h"

using namespace OHOS::NativeRdb;

class SampleCallback : public RdbOpenCallback {
public:
    int OnCreate(RdbStore &store) override
    {
        return store.ExecuteSql(
            "CREATE TABLE IF NOT EXISTS test (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT NOT NULL)");
    }
    int OnUpgrade(RdbStore &store, int oldVersion, int newVersion) override
    {
        return E_OK;
    }
};

RdbStoreConfig config("/data/app/el2/<userId>/database/<bundleName>/app.db"); // 替换为真实路径
SampleCallback callback;
int errCode = E_ERROR;
auto store = RdbHelper::GetRdbStore(config, 1, callback, errCode); // errCode == E_OK
```

**第 3 步：调用接口收到 `E_SQLITE_CORRUPT`。** 配置了 `count: 1`，第一次插入被注入，预算耗尽后自然恢复：

```cpp
ValuesBucket row;
row.PutString("name", "alice");
int64_t rowId = 0;
int ret = store->Insert(rowId, "test", row); // ret == E_SQLITE_CORRUPT (27394104)，数据未写入
ret = store->Insert(rowId, "test", row);     // ret == E_OK，预算耗尽，真实插入
```

## 配置文件详解

### 文件位置

- 文件名固定为 `rdb_fault_inject_config.json`，放在**数据库文件同目录**：`/path/to/app.db` 对应 `/path/to/rdb_fault_inject_config.json`。应用数据库目录通常形如 `/data/app/el1~el5/<userId>/database/<bundleName>/`（目录路径含 `userId`，见"注意事项"）。
- **按库文件名定向**：`databases` 对象的键是数据库文件名（如 `"app.db"`）。同一目录下未列名的数据库完全不受影响，一份配置可以同时服务多个库。
- **内存库**（`file:name?mode=memory&cache=shared` 形式的 URI）：`databases` 的键用库名（如 `"mem_mock.db"`），配置文件按**进程工作目录**相对路径查找。
- 该能力依赖构建开关，仅在以 `rdb_fault_injection_enable = true` 编译的 root/调试版系统上生效（详见"注意事项"）。

### 配置语法

顶层只有一个有效对象 `databases`，键为数据库文件名；每个库下可配置三类条目（`open` / `operations` / `methods`）。完整可运行示例（摘自 [`rdb_fault_inject_config.example.json`](rdb_fault_inject_config.example.json)）：

```json
{
    "databases": {
        "app.db": {
            "open": { "count": 1 },
            "operations": {
                "insert": { "count": 3 },
                "update": { "count": 2, "probability": 50 },
                "delete": { "probability": 30 },
                "query": { "count": 1 },
                "executeSql": { "count": 2, "probability": 100 },
                "transaction": { "count": 1 },
                "resultSetRead": { "count": 2, "probability": 50 }
            },
            "methods": {
                "Backup": { "count": 1 }
            }
        },
        "cache.db": {
            "open": { "mode": "error" }
        }
    }
}
```

每个条目有两个可选参数：

| 参数 | 类型 | 缺省 | 语义 |
| --- | --- | --- | --- |
| `count` | 整数 | `-1` | 注入次数预算：`-1` 不限次数；`0` 永不注入；`N` 注入 `N` 次后自然恢复 |
| `probability` | 整数 | `100` | 0-100 的命中概率（百分比）；**未命中不消耗次数**，因此 `count: N` 最终恰好命中 `N` 次 |

容错规则（异常输入一律向"不注入"方向收敛，不影响正常库）：

- 配置文件缺失、JSON 整体非法、缺少 `databases` 键：视为未配置，所有判定不命中；
- 条目内的缺省、`null`、非整数字段：取缺省值（如 `"count": "abc"` 等价于不限次数）；
- 未知顶层字段静默容忍（向前兼容）；未知的 `operations` 键打告警日志并忽略；
- `open` 条目参数整体非法时视为未配置，打开正常进行。

### 三类注入条目

**`open` —— 打开链路**

```json
{
    "databases": {
        "app.db": { "open": { "count": 1 } }
    }
}
```

效果：`RdbHelper::GetRdbStore` / `RdbStoreManager::GetRdbStore` 直接以 `errCode = E_SQLITE_CORRUPT` 失败并返回 `nullptr`，**不执行打开、不创建数据库文件**；`RdbHelper::GetRdb` 返回 `nullptr`。判定发生在每次打开入口（先于缓存查询），`count` 按打开次数消耗——`count: 1` 表示第一次打开失败、第二次即恢复。`mode` 字段目前仅支持 `"error"`（缺省值，显式写与不写行为相同）。

**`operations` —— 操作族**

```json
{
    "databases": {
        "app.db": { "operations": { "query": { "count": 1 } } }
    }
}
```

效果：按操作族批量定向一类接口。共七个键：`insert` / `update` / `delete` / `query` / `executeSql` / `transaction` / `resultSetRead`，各自覆盖的接口见"注入效果"的映射表。上例中第一次 `Query` / `QuerySql` / `QueryByStep` 返回 `nullptr`、`Count` 返回 `E_SQLITE_CORRUPT`，之后恢复。

**`methods` —— 方法点名**

```json
{
    "databases": {
        "app.db": { "methods": { "Commit": { "count": 1 } } }
    }
}
```

效果：按**最终调用点的方法名**定向单个接口，命中**优先于**其所属操作族（同名时方法条目先判定）。适合只注入某一个调用而放过同族其他接口的场景，也可以点名不属于任何操作族的方法（如结果集的 `Close`，以及文件级的 `Backup` / `Restore` / `ForceRestore`——它们没有所属操作族，`methods` 点名是唯一的注入方式）。注意点名的是最终调用点（sink）方法名而非全部公开重载名——例如 `InsertWithConflictResolution` 内部汇入 `Insert`，应写 `"Insert"`；有效名单即"注入效果"映射表中的接口名，点名当前没有调用点覆盖的名字（如 `InterruptBackup` / `GetBackupStatus`）语法合法但不会命中。

## 注入效果

### 操作族与接口映射

| 条目 / 操作族 | 受影响的接口（公共 API） | 命中时的表现 |
| --- | --- | --- |
| `open` | `RdbHelper` / `RdbStoreManager` 的 `GetRdbStore`、`GetRdb` | `errCode` 置为 `E_SQLITE_CORRUPT`，返回 `nullptr`，不创建数据库文件 |
| `insert` | `Insert`（含 `InsertWithConflictResolution`）、`BatchInsert`（全部重载） | 返回 `E_SQLITE_CORRUPT`，rowId 为 `-1`，数据不落库 |
| `update` | `Update`（含 `UpdateWithConflictResolution`）全部重载 | 返回 `E_SQLITE_CORRUPT`，改动不生效 |
| `delete` | `Delete` 全部重载 | 返回 `E_SQLITE_CORRUPT`，删除不生效 |
| `query` | `Query`、`QuerySql`、`QueryByStep`、`Count` | `Query` / `QuerySql` / `QueryByStep` 返回 `nullptr`；`Count` 返回 `E_SQLITE_CORRUPT` |
| `executeSql` | `ExecuteSql`、`Execute`、`ExecuteExt`、`ExecuteAndGetLong`、`ExecuteAndGetString`、`ExecuteForLastInsertedRowId`、`ExecuteForChangedRowCount` | 返回 `E_SQLITE_CORRUPT`，SQL 不执行 |
| `transaction` | store 上：`CreateTransaction`、`BeginTransaction`、`Commit`、`RollBack`；事务对象上：`Commit`、`Rollback`、`Close` | `CreateTransaction` 返回 `{E_SQLITE_CORRUPT, nullptr}`；其余返回 `E_SQLITE_CORRUPT` |
| `methods` 点名（文件级） | `Backup` / `Restore` / `ForceRestore` | 返回 `E_SQLITE_CORRUPT`，不触碰数据库与备份文件；无所属操作族，仅可经 `methods` 点名 |
| `resultSetRead` | `QuerySql` / `QueryByStep`（含事务内查询）返回的结果集的读取方法：`GoTo` / `GoToRow` / `GoToFirstRow` / `GoToNextRow` / `GoToPreviousRow` / `GoToLastRow`、`GetRowCount`、`GetInt` / `GetLong` / `GetDouble` / `GetString` / `GetBlob`、`GetColumnCount` / `GetColumnName` / `GetColumnIndex` / `GetColumnType`、`IsEnded` / `IsStarted` / `IsAtFirstRow` / `IsAtLastRow` / `IsColumnNull` / `IsClosed` 等 | 读取方法返回 `E_SQLITE_CORRUPT`，游标不前进；`Close` 不在读取族内（保证资源可释放），但可经 `methods` 点名 |

补充说明：

- **事务对象上的 CRUD 同样可注入**：事务上的 `Insert` / `BatchInsert` 消耗 `insert` 族预算，`Update`、`Delete`、`QueryByStep`、`Execute` / `ExecuteExt` 依此类推。同一 store 及它创建的事务、结果集**共享同一份预算**：多线程并发消耗是原子的、总数精确不超发；预算不跨进程、不跨 store 实例共享。
- **Init 门控**：注入仅在数据库打开（`Init`）完成后生效，`OnCreate` / `OnUpgrade` 回调与打开期间的内部语句不消耗预算——不会把建表逻辑打断。
- **真实错误原样透传**：注入不会改写真实错误。例如查询一个不存在的表，返回的是真实 SQL 错误而非 `E_SQLITE_CORRUPT`，两者可区分。

### count / probability 语义与自然恢复

- `count` 是次数预算：`-1` 不限、`0` 永不、`N` 注入 `N` 次。**预算耗尽后接口自然恢复真实行为**，无需删除配置或重启进程。
- `probability` 按次采样，**未命中不消耗次数**：`"count": 2, "probability": 50` 表示最终恰好命中 2 次，只是命中位置随机。
- 恢复手段（按需选用）：等预算自然耗尽；删除（或改小）配置文件后重建 store；`open` 条目无需重建 store，下一次打开就是新预算。
- 演练时给 `open` 配**有限**的 `count`：配成 `-1` 会让应用永远打不开该库。

## 使用场景

### 场景 1：验证应用的数据库损坏处理

目标：确认每个接口返回 `E_SQLITE_CORRUPT` 时，应用走的是损坏处理分支（提示用户、触发备份、重建库、上报等），而不是崩溃或误当成功。

逐族单独配置、逐轮验证（示例为 `query` 族）：

```json
{
    "databases": {
        "app.db": {
            "operations": { "query": { "count": 3 } }
        }
    }
}
```

一轮通过后换下一个族，直到七个族与 `open` 都覆盖。也可一次配齐多族做综合演练（此时注意各族预算独立消耗，故障顺序不可控）：

```json
{
    "databases": {
        "app.db": {
            "open": { "count": 1 },
            "operations": {
                "insert": { "count": 1 },
                "update": { "count": 1 },
                "delete": { "count": 1 },
                "query": { "count": 1 },
                "executeSql": { "count": 1 },
                "transaction": { "count": 1 },
                "resultSetRead": { "count": 1 }
            }
        }
    }
}
```

验证要点：查询类接口要同时处理"`nullptr` 返回值"与"结果集读取返回 `E_SQLITE_CORRUPT`"两种形态；`resultSetRead` 命中时 `Close` 仍可用，资源能正常释放。

### 场景 2：验证损坏后重试/恢复逻辑

目标：确认应用在偶发损坏后能重试成功、在打开失败后能恢复，而不是一次性失败到底。

用 `probability` 模拟偶发损坏（预算仍由 `count` 封顶，最终恰好命中 `N` 次后永久恢复）：

```json
{
    "databases": {
        "app.db": {
            "operations": { "insert": { "count": 5, "probability": 30 } }
        }
    }
}
```

验证重试收敛：前若干次 `Insert` 可能返回 `E_SQLITE_CORRUPT`，应用的退避重试应在预算耗尽后拿到 `E_OK`，且注入期间的数据没有落库（不产生脏数据）。

验证打开失败后的恢复（应用重启 / 重新打开场景）：

```json
{
    "databases": {
        "app.db": { "open": { "count": 1 } }
    }
}
```

第一次 `GetRdbStore` 失败（`errCode = E_SQLITE_CORRUPT`、返回 `nullptr`、无数据库文件产生），应用的降级路径（提示、修复、重建）执行后，第二次打开应成功。

### 场景 3：灰度期间监控格式选择（osVersion 相关）

目标：演练/灰度期间，监控口径不为"注入的损坏"另建格式，同时不污染真实损坏统计。

- **复用既有监控格式**：注入返回的错误码与真实损坏完全相同（同一个 `E_SQLITE_CORRUPT` = 27394104），会自然流经应用既有的损坏处理与上报链路。因此直接沿用按错误码聚合的既有损坏监控格式即可，不需要为注入单独定义事件或字段。
- **区分注入与真实损坏**：注入命中伴随专属日志 TAG `RDB_FAULT_INJECT`（限频输出），真实损坏没有这条日志；排障与数据清洗时以此甄别。
- **按 osVersion 维度解读数据**：注入只存在于以 `rdb_fault_injection_enable = true` 编译的 root/调试版镜像，user 版（正式灰度版本）零行为。因此按 osVersion 聚合监控数据时：**正式灰度版本上的 `E_SQLITE_CORRUPT` 全部是真实损坏**，可直接用于损坏率统计；调试/测试版本上的 `E_SQLITE_CORRUPT` 需先用 `RDB_FAULT_INJECT` 日志剔除演练注入，避免演练流量抬高真实损坏率。

## 注意事项

- **构建开关**：能力由根构建 gn 参数 `rdb_fault_injection_enable`（`relational_store.gni`，缺省 `false`）控制。`true`（root/调试版）注入可用——注入实现随 rdb 源码一并编入 `libnative_rdb.z.so`；`false`（user 版）经**编译期裁剪**：守卫宏空展开、不创建注入器对象（判定恒为未命中），**零行为零代码**——不读配置、不打日志，不编入任何注入实现。应用侧无需任何代码改动，user 版上的行为与未提供该能力时完全一致。
- **新增注入点约束（维护者）**：为注入面新增接口时，必须使用 `rdb_fault_injector.h` 中的 `RDB_FAULT_INJECT_HIT` / `RDB_FAULT_INJECT_METHOD` 宏（user 版自动空展开），不得在守卫之外直接调用注入器判定方法——违反时 user 版构建在链接期即报错暴露。
- **配置文件读取时机**：store 打开（`Init`）开始时读取一次并持有快照（操作族/方法预算随之定档，`OnCreate` / `OnUpgrade` 回调期内已生效）。运行中修改配置：`open` 条目下一次打开即生效；`operations` / `methods` 条目需**重建 store** 后生效——释放全部 store 引用（必要时调用 `RdbHelper::ClearCache()`），再重新 `GetRdbStore`。重复 `GetRdbStore` 返回的是缓存中的同一对象，不会重读配置。
- **userId 隐私**：数据库目录路径形如 `/data/app/el1~el5/<userId>/database/<bundleName>/`（路径模式已有文档涉及，见 [`../DatabaseCheckPlugin/Doc/Rule7_数据库目录文件权限检测.md`](../DatabaseCheckPlugin/Doc/Rule7_数据库目录文件权限检测.md)），其中 `userId` 属隐私信息。注入日志中的库路径已做匿名化输出，但在工单、群聊中共享配置路径、`hdc` 命令或排查截图时，注意不要泄露完整路径中的 `userId`。
- **日志 TAG**：`RDB_FAULT_INJECT`，限频输出（每 100 次命中一条，首条必出）。注入 `Commit` / `Rollback` / `Close` 等收敛类方法时会额外输出告警：事务与结果集的收敛依赖调用方回滚或析构——请确保应用在这些调用失败时仍能正确收尾，不要泄漏资源。
- **失败安全**：命中只改返回值，无任何副作用；配置文件缺失或非法一律视为未配置；同目录未列名的数据库不受影响。

## FAQ

**Q1：配置了但没有生效，怎么排查？**

按顺序检查：

1. 系统镜像是否以 `rdb_fault_injection_enable = true` 编译（user 版零行为，这是最常见原因）；
2. store 是否在写入配置**之前**已打开——缓存中的 store 不会重读配置，需释放引用并重建（`open` 条目除外）；
3. `databases` 的键是否与数据库**文件名**完全一致（含扩展名，如 `app.db`）；
4. 配置文件是否在数据库同目录、JSON 是否合法（非法文件视为未配置，hilog 中有告警）；
5. 是否处在 `OnCreate` / `OnUpgrade` 执行期间（Init 完成前不注入）。

**Q2：注入会破坏我的数据或留下脏状态吗？**

不会。命中只影响返回值：写入类接口的 SQL 不执行、数据不落库；查询类接口返回 `nullptr`；结果集读取命中时游标原地不动。数据库文件本身不被触碰，预算耗尽后接口自然恢复。

**Q3：`count` 用完了，还想再触发一轮怎么办？**

重建 store（配置不变，重新打开即重新加载 `N` 次预算）；或把 `count` 改大后重建。`open` 条目最简单：下一次打开就是新预算，无需重建。

**Q4：怎么区分注入命中和真实损坏？**

错误码相同（`E_SQLITE_CORRUPT` = 27394104），单看返回值无法区分。依据是日志：注入命中伴随 TAG 为 `RDB_FAULT_INJECT` 的限频日志，真实损坏没有。另外注入不会改写真实错误——返回了其他错误码的，都是真实错误。

**Q5：用 `methods` 点名 `InsertWithConflictResolution` 为什么不生效？**

`methods` 点名的是**最终调用点（sink）方法名**，公开重载都会汇入 sink：`InsertWithConflictResolution` 应写 `"Insert"`，事务提交应写 `"Commit"`，结果集翻行应写 `"GoToNextRow"`。有效名单就是"注入效果"映射表中列出的接口名；点名当前没有调用点覆盖的名字（如 `InterruptBackup` / `GetBackupStatus`）不会命中。

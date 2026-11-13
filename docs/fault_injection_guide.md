# RDB 故障注入使用指导

本文件是 relational_store 接口故障注入 mock 层的使用指导。需要**使用**该能力验证损坏处理逻辑，或**修改**注入机制本身时，先完整阅读本文件。

---

## 1. 概述

业务方需要处理所有 RDB 接口返回的数据库损坏错误码，但直接构造数据库文件损坏无法按接口定向、也难以覆盖全部场景。故障注入 mock 层通过**数据库同目录的配置文件**让指定 DB 的 RDB 接口返回 `E_SQLITE_CORRUPT`，无需破坏真实文件即可按接口验证损坏处理逻辑。

- 注入命中只影响返回值（`E_SQLITE_CORRUPT` = 27394104，经既有 RdbHelper 损坏回调与故障上报链路自然触发），不产生任何副作用
- 未配置的数据库行为与上游完全一致（装饰器在注入器未生效时与裸实现等价）

## 2. 构建开关

```gn
# 根构建 gn 参数（relational_store.gni declare_args）
rdb_fault_injection_enable = false   # 缺省
```

开关跟 `build_variant` 联动（dsoftbus 同模式）：`rdb_fault_injection_enable = (build_variant == "root")`。

| build_variant | 编入产物 | 行为 |
| --- | --- | --- |
| `root` | `rdb_fault_injector.cpp` + 装饰器家族 | 注入能力可用 |
| `user` | `rdb_fault_injector_noop.cpp` + 装饰器家族 | 零行为：无配置读取、无日志、所有判定恒为未命中 |

注意：**代码中没有任何特性宏**。开关在 `relational_store.gni` 里做源级选择，仅切换注入器的真实现与 no-op 实现；装饰器两个版本都编入（继承自被装饰类，注入器未生效时等价于裸实现）。root 版构建自动可用，无需手动传 `--gn-args`。

## 3. 配置文件

- 文件名固定：`rdb_fault_inject_config.json`
- 位置：与数据库文件同目录（按数据库路径推导，如 `/data/app/.../mydb.db` → `/data/app/.../rdb_fault_inject_config.json`）
- 内存库（`file:name?mode=memory&cache=shared` URI）：剥离 `file:` 前缀与查询串后取 name 作为库名，配置文件按**进程工作目录**相对路径查找
- **按 DB 名定向**：`databases` 对象的键为数据库文件名；同目录下未列名的数据库不受影响
- **读取时机**：`RdbStoreDecorator` 构造时按库路径读取一次并持有快照（操作族/方法预算随之定档）；open 判定由 `RdbStoreManager` 打开入口的短生命周期实例承担，天然按打开周期重读——运行中修改配置，open 下次打开即生效，操作族预算需重建 store 后生效

## 4. 配置语法

```json
{
    "databases": {
        "mydb.db": {
            "open":       { "count": 3, "probability": 100 },
            "operations": { "insert": { "count": 5 }, "query": { "probability": 50 } },
            "methods":    { "Backup": { "count": 1 } }
        }
    }
}
```

以上为最小片段；完整可运行的示例见 [`rdb_fault_inject_config.example.json`](rdb_fault_inject_config.example.json)（覆盖三类条目、全部操作族键与参数缺省语义；使用时复制到数据库所在目录并重命名为 `rdb_fault_inject_config.json`）。

### 条目参数

| 参数 | 缺省 | 语义 |
| --- | --- | --- |
| `count` | `-1` | `-1` 不限次数；`0` 永不注入；`N` 注入 N 次后自然恢复 |
| `probability` | `100` | 0-100 命中概率；**未命中不消耗次数** |

条目内缺省、`null`、非整数或非对象值视为取缺省值（与库内其他可选 Serializable 字段一致）；越界整数按原值生效（负 `count` 即不限次、负 `probability` 永不命中）；`open` 条目整条非法时视为未配置。未知顶层字段静默容忍；未知操作族键告警并忽略（向前兼容）。

### 三类条目

| 条目 | 作用域 | 说明 |
| --- | --- | --- |
| `open` | 打开链路 | Manager 层直接返回 `E_SQLITE_CORRUPT`，不执行打开；`count`/`probability` 同上 |
| `operations` | 操作族 | 七个键：`insert` / `update` / `delete` / `query` / `executeSql` / `transaction` / `resultSetRead` |
| `methods` | 方法点名 | 按方法名定向单个最终调用点（如 `"Backup"`）；命中优先于其所属操作族 |

## 5. 注入行为要点

- **Init 门控**：注入仅在打开（`Init`）完成后生效——`OnCreate` / `OnUpgrade` 回调与打开期内部语句不消耗预算
- **结果集包裹**：`QuerySql` / `QueryByStep`（含事务内）返回的结果集被包裹，`resultSetRead` 命中时读取方法返回 `E_SQLITE_CORRUPT`；`Close` 不在读取族内（保证资源可释放），但可被 `methods` 点名
- **方法级仅覆盖最终调用点**：`methods` 点名的是 sink 方法名（`Insert` / `BatchInsert` / `Update` / `Delete` / `QuerySql` / `QueryByStep` / `Execute` / `ExecuteExt` / `Commit` 等），非全部公开重载名
- **命中日志**：TAG `RDB_FAULT_INJECT`，限频输出（每 100 次命中一条，首条必出）
- **预算随 store 实例**：同一 store 及其事务/结果集装饰器共享同一注入器实例，多线程消耗原子精确不超发；计数不跨进程、不跨 store 共享

## 6. 架构

| 组件 | 文件 | 职责 |
| --- | --- | --- |
| `RdbFaultInjector` | `rdb_fault_injector.h/.cpp` / `rdb_fault_injector_noop.cpp` | 按库实例化的判定对象：`RdbStoreDecorator` 持有并传递给事务/结果集装饰器，open 判定用管理器入口的短生命周期实例；配置经 `Serializable` 的 `Marshal`/`Unmarshal` 解析（schema 结构体也在 cpp 内） |
| `RdbStoreDecorator` | `rdb_store_decorator.h/.cpp` | `: public RdbStoreImpl`，默认 store 对象（不只为故障注入）；只覆盖各操作族的最终 sink（基类模板方法把全部公开重载漏斗到这些 sink，见 `rdb_store.cpp`）；承载逐调用埋点（HITRACE / 统计上报 / SQL 统计 / 性能统计）后判定注入再调 parent |

| `TransactionImplDecorator` | `transaction_impl_decorator.h/.cpp` | `: public TransactionImpl`，事务对象（`Commit`/`Rollback`/`Close` 与全部 CRUD sink；内部 TransDB 门面留给基类） |
| 结果集装饰器 | `rdb_result_set_decorator.h`（头文件模板，无 cpp） | 单模板双实例化（`ResultSet` / `AbsSharedResultSet`），包裹查询返回值；`innerHolder_`+`inner_` 与 `injectorHolder_`+`injector_` 引用成员（`configHolder_`/`config_` 约定） |

store 身份恒定：装饰器**就是**缓存中的 store 对象，重复 `GetRdbStore` 返回同一对象，向下转型不受影响。

## 7. 边界与约束

- 同一 path 的 `open` 条目与操作条目互不干扰（`ConsumeOpen` 对仅配置操作条目的库恒为 false）
- 上游实现文件保持精简：`rdb_store_impl.cpp` / `trans_db.cpp` 的 sink 方法只留功能实现，埋点 RAII 已上移装饰器

## 8. 扩展指引

新增可注入能力时：

1. 新增操作族：`RdbFaultOp` 枚举 + `OpKeyToEnum` 键映射 + 对应 sink 覆盖 + 漏斗覆盖用例
2. 新增方法点名：在对应装饰器覆盖该方法并调用 `RdbFaultInjector::Hit`（方法名 + 所属族）
3. 配置 schema 变更：只改 `rdb_fault_injector.cpp` 内的 `Serializable` 结构体，业务逻辑不得直接操作 json 对象（规范 R12）
4. 同步更新 `docs/fault_injection_guide.md` 与 `test/interface/rdb_mock_inject*_test.cpp`

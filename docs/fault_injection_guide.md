# RDB 故障注入使用指导

本文件是 relational_store 接口故障注入 mock 层的使用指导。需要**使用**该能力验证损坏处理逻辑，或**修改**注入机制本身时，先完整阅读本文件。

---

## 1. 概述

业务方需要处理所有 RDB 接口返回的数据库损坏错误码，但直接构造数据库文件损坏无法按接口定向、也难以覆盖全部场景。故障注入 mock 层通过**数据库同目录的配置文件**让指定 DB 的 RDB 接口返回 `E_SQLITE_CORRUPT`，无需破坏真实文件即可按接口验证损坏处理逻辑。

- 注入命中只影响返回值（`E_SQLITE_CORRUPT` = 27394104，经既有 RdbHelper 损坏回调与故障上报链路自然触发），不产生任何副作用
- 未配置的数据库行为与上游完全一致（装饰器在注入器未生效时与裸实现等价）

## 2. 构建开关

```gn
# relational_store.gni（非 declare_args，随构建变体联动）
rdb_fault_injection_enable = (is_ohos && build_variant == "root")
```

开关跟 `build_variant` 联动（dsoftbus 同模式），且仅 OHOS 构建可开启——跨平台（SDK）变体永远关闭。

| build_variant | 编入产物 | 行为 |
| --- | --- | --- |
| `root`（OHOS） | 装饰器家族 + 注入器实现（随 rdb 源码编入 `libnative_rdb.z.so`） | 注入能力可用 |
| `user` / 跨平台 | 仅装饰器家族 | 零行为零代码：守卫宏空展开、不创建注入器对象；不读配置、不打日志，不编入任何注入实现 |

机制为**编译期裁剪**：`RDB_FAULT_INJECTION_ENABLE` 有定义时（root）守卫宏展开为判定调用，注入器实现随 `rdb_fault_mock_sources` 编入；无定义时（user/跨平台）守卫宏空展开、不创建注入器对象，任何翻译单元都不引用注入器符号。root 版构建自动可用，无需手动传 `--gn-args`。

## 3. 配置文件

- 文件名固定：`rdb_fault_inject_config.json`
- 位置：与数据库文件同目录（按数据库路径推导，如 `/data/app/.../mydb.db` → `/data/app/.../rdb_fault_inject_config.json`）
- 内存库（`file:name?mode=memory&cache=shared` URI）：剥离 `file:` 前缀与查询串后取 name 作为库名，配置文件按**进程工作目录**相对路径查找
- **按 DB 名定向**：`databases` 对象的键为数据库文件名；同目录下未列名的数据库不受影响
- **读取时机**：`Init` 开始时（先于 base `Init`，`OnCreate` / `OnUpgrade` 回调期内已生效）按库路径读取一次并持有快照（操作族/方法预算随之定档）；open 判定由 `RdbStoreManager` 打开入口的短生命周期实例承担，天然按打开周期重读——运行中修改配置，open 下次打开即生效，操作族预算需重建 store 后生效

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

- **生效时机**：注入器在 `Init` 开始（base `Init` 之前）幂等创建（`RdbFaultInjector::CreateOnce`），`OnCreate` / `OnUpgrade` 回调中的操作同样消耗预算；打开期 PRAGMA 类内部语句不经守卫，不消耗预算
- **结果集包裹**：`QuerySql` / `QueryByStep`（含事务内）返回的结果集被包裹，`resultSetRead` 命中时读取方法返回 `E_SQLITE_CORRUPT`；`Close` 不在读取族内（保证资源可释放），但可被 `methods` 点名
- **方法级仅覆盖最终调用点**：`methods` 点名的是 sink 方法名（`Insert` / `BatchInsert` / `Update` / `Delete` / `QuerySql` / `QueryByStep` / `Execute` / `ExecuteExt` / `Commit` 等），非全部公开重载名
- **命中日志**：TAG `RDB_FAULT_INJECT`，限频输出（每 100 次命中一条，首条必出）
- **预算随 store 实例**：同一 store 及其事务/结果集装饰器共享同一注入器实例，多线程消耗原子精确不超发；计数不跨进程、不跨 store 共享，**不同 path 的数据库各持独立注入器实例与预算**（`CreateOnce` 按成员幂等创建，不按路径缓存、不跨成员共享）
- **并发打开**：缓存命中时 manager 会让第二个打开方对**同一 store 对象**再次执行 `Init`——注入器的创建（`RdbFaultInjector::CreateOnce`）是幂等且串行化的，两个并发 `Init` 收敛到同一注入器（一份配置快照、一份预算），不会劈分；尚未完成创建的注入器判空恒为未命中（不报错）

## 6. 架构

| 组件 | 文件 | 职责 |
| --- | --- | --- |
| `RdbFaultInjector` | `rdb_fault_injector.h`（无构建变体形状的纯接口类 + `RDB_FAULT_INJECT_*` 宏）/ `rdb_fault_injector.cpp`（root 版实现，随 rdb 源码编入 `libnative_rdb`） | 按库实例化的判定对象；配置经 `Serializable` 的 `Marshal`/`Unmarshal` 解析（schema 结构体也在 cpp 内）；命中日志中的库路径经 `SqliteUtils::Anonymous` 匿名化输出。装饰器家族持注入器 `shared_ptr`、仅经宏消费——store 的创建经 `CreateOnce`（幂等、并发 `Init` 收敛单实例），事务/结果集装饰器持注入器指针副本共享预算，open 判定用管理器入口的短生命周期实例（user 版宏空展开，判定恒为未命中）；单测直用本类 |
| `RdbStoreDecorator` | `rdb_store_decorator.h/.cpp` | `: public RdbStoreImpl`，默认 store 对象（不只为故障注入）；只覆盖各操作族的最终 sink（基类模板方法把全部公开重载漏斗到这些 sink，见 `rdb_store.cpp`）；承载逐调用埋点（HITRACE / 统计上报 / SQL 统计 / 性能统计）后经 `RDB_FAULT_INJECT_*` 宏判定注入再调 parent |
| `TransactionImplDecorator` | `transaction_impl_decorator.h/.cpp` | `: public TransactionImpl`，事务对象（`Commit`/`Rollback`/`Close` 与全部 CRUD sink；内部 TransDB 门面留给基类）；持 store 的注入器指针 |
| 结果集装饰器 | `rdb_result_set_decorator.h`（头文件模板，无 cpp） | 单模板双实例化（`ResultSet` / `AbsSharedResultSet`），包裹查询返回值；`innerHolder_`+`inner_` 引用成员（`configHolder_`/`config_` 约定）+ 持 store 的注入器指针；user 版守卫空展开，模板零实例化 |

store 身份恒定：装饰器**就是**缓存中的 store 对象，重复 `GetRdbStore` 返回同一对象，向下转型不受影响。

## 7. 边界与约束

- 同一 path 的 `open` 条目与操作条目互不干扰（`ConsumeOpen` 对仅配置操作条目的库恒为 false）
- 上游实现文件保持精简：`rdb_store_impl.cpp` / `trans_db.cpp` 的 sink 方法只留功能实现，埋点 RAII 已上移装饰器

## 8. 扩展指引

新增可注入能力时：

1. 新增操作族：`RdbFaultOp` 枚举 + `OpKeyToEnum` 键映射 + 对应 sink 覆盖 + 漏斗覆盖用例
2. 新增方法点名：在对应装饰器覆盖该方法，入口处使用 `RDB_FAULT_INJECT_HIT`（方法名 + 所属操作族）或 `RDB_FAULT_INJECT_METHOD`（仅方法名，无操作族）宏——user 版自动空展开；**不得在守卫之外直接调用注入器判定方法**（违反时 user 版链接期即报错暴露）
3. 配置 schema 变更：只改 `rdb_fault_injector.cpp` 内的 `Serializable` 结构体，业务逻辑不得直接操作 json 对象（规范 R12）
4. 同步更新 `docs/fault_injection_guide.md` 与 `test/interface/rdb_mock_inject*_test.cpp`

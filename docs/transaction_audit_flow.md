# 事务审计流程图

## 审计开关判定

审计是否启用由 `RdbStoreConfig::IsAuditEnabled()` 决定，在两处创建 logger 时传入：

```
config_.IsAuditEnabled()
        │
        ├─ true  ─→ RdbAuditLogger::Create(true)  ─→ RdbAuditLoggerImpl (真实审计，IsActive()=true)
        │                                              │
        │                                              ├─ 单例初始化: RdbAuditLoggerManager / RdbDbLoggerManager
        │                                              └─ 所有 On* 方法执行真实逻辑
        │
        └─ false ─→ RdbAuditLogger::Create(false) ─→ RdbAuditLogger (no-op 基类)
                                                       │
                                                       └─ 所有 On* 方法为空虚函数，调用直接穿透，零开销
```

**两处创建点：**
1. `RdbStoreImpl` 构造函数 — `auditLoggerHolder_(RdbAuditLogger::Create(config_.IsAuditEnabled()))`
2. `TransactionImpl` 构造函数 — `auditLoggerHolder_(RdbAuditLogger::Create(auditEnabled))`，`auditEnabled` 由 `Transaction::Create(type, conn, path, config_.IsAuditEnabled())` 传入

---

## 两条事务路径总览

```
                         RdbStoreImpl
                          (持有 auditLogger_)
                              │
              ┌───────────────┴───────────────┐
              │                               │
     Path 1: BeginTransaction           Path 2: CreateTransaction
     (栈式事务，共享连接池)              (连接式事务，独占连接)
              │                               │
              ▼                               ▼
     auditLogger_.OnTransaction       Transaction::Create(
     (管理 inTransaction_/txnBuffer_)    type, conn, path,
     (仅最外层 outer=true 生效)           config_.IsAuditEnabled())
              │                               │
              │                          TransactionImpl 构造
              │                          (自建 auditLoggerHolder_)
              │                               │
              ▼                               ▼
     SQL 走缓冲路径                     SQL 走限流/直写路径
     (inTransaction_=true 时             (inTransaction_ 始终 false
      聚合到 txnBuffer_)                  因为 OnTransactionEvent
      COMMIT 时 flush                     不设置 inTransaction_)
      ROLLBACK 时丢弃)
```

---

## Path 1: BeginTransaction / Commit / RollBack（栈式）

```
BeginTransaction()
    │
    ├─ pool->GetTransactionStack().push(transaction)
    │
    └─ auditLogger_.OnTransaction(path, "BEGIN", E_OK, id==1)
            │
            ├─ !IsActive() → return
            │
            ├─ outer == true (最外层)
            │   └─ inTransaction_ = true
            │      txnBuffer_.clear()
            │      → 不写 events.log（BEGIN 无审计行）
            │
            └─ outer == false (嵌套)
                └─ 无操作


Insert / Update / Delete / BatchInsert
    │
    └─ auditLogger_.OnSqlAudit(path, op, table, rows)
            │
            ├─ !IsActive() → return
            │
            ├─ lock(txnMutex_)
            │   └─ inTransaction_ == true
            │       └─ txnBuffer_[{op, tbl}] += rows  ← 聚合缓冲
            │          return                           ← 不写 events.log
            │
            └─ inTransaction_ == false
                └─ 走限流/直写路径（见下方 OnSqlAudit 决策图）


Commit()
    │
    ├─ pool->GetTransactionStack().pop()
    │
    └─ auditLogger_.OnTransaction(path, "COMMIT", E_OK, outer)
            │
            ├─ !IsActive() → return
            │
            └─ outer == true (最外层)
                ├─ FlushTxnBuffer(path)
                │   └─ 遍历 txnBuffer_，逐条 EmitSqlAudit()
                │      → 异步写入 events.log（SQL: op=X tbl=Y rows=N）
                │
                ├─ EmitTransaction(path, "COMMIT", rc)
                │   └─ 异步写入 events.log（TRX: op=COMMIT rc=0）
                │
                └─ inTransaction_ = false

            outer == false (嵌套) → 无操作


RollBack()
    │
    ├─ pool->GetTransactionStack().pop()
    │
    └─ auditLogger_.OnTransaction(path, "ROLLBACK", rc, outer)
            │
            ├─ !IsActive() → return
            │
            └─ outer == true (最外层)
                ├─ txnBuffer_.clear()        ← 丢弃所有缓冲 SQL
                └─ inTransaction_ = false
                   → 不写 events.log（ROLLBACK 无审计行）

            outer == false (嵌套) → 无操作
```

---

## Path 2: CreateTransaction（连接式）

```
RdbStoreImpl::CreateTransaction(type)
    │
    └─ Transaction::Create(type, conn, path, config_.IsAuditEnabled())
            │
            └─ creator_(type, conn, path, auditEnabled)
                    │
                    └─ TransactionImpl::Create(type, conn, path, auditEnabled)
                            │
                            └─ TransactionImpl(conn, path, auditEnabled)
                                    │
                                    └─ auditLoggerHolder_(RdbAuditLogger::Create(auditEnabled))
                                       auditLogger_(*auditLoggerHolder_)
                                        ├─ auditEnabled=true  → RdbAuditLoggerImpl
                                        └─ auditEnabled=false → RdbAuditLogger (no-op)


TransactionImpl::Begin()
    │
    └─ auditLogger_.OnTransactionEvent(path, "BEGIN", E_OK)
            │
            ├─ !IsActive() → return
            │
            └─ EmitTransaction(path, "BEGIN", rc)
               └─ 异步写入 events.log（TRX: op=BEGIN rc=0）

            注意: OnTransactionEvent 不管理 inTransaction_ / txnBuffer_
                  SQL 事件不走缓冲，直接走限流/直写路径


Insert / Update / Delete / BatchInsert
    │
    └─ AuditSql(op, tbl, rows)
            │
            └─ auditLogger_.OnSqlAudit(path, op, tbl, rows)
                    │
                    └─ inTransaction_ 始终 false
                       → 走限流/直写路径（见下方 OnSqlAudit 决策图）


TransactionImpl::Commit()
    │
    ├─ 成功: auditLogger_.OnTransactionEvent(path, "COMMIT", E_OK)
    │        └→ events.log: TRX: op=COMMIT rc=0
    │
    └─ 失败: auditLogger_.OnTransactionEvent(path, "ROLLBACK", errorCode)
             └→ events.log: TRX: op=ROLLBACK rc=<err>


TransactionImpl::Rollback()
    │
    ├─ 成功: auditLogger_.OnTransactionEvent(path, "ROLLBACK", E_OK)
    │        └→ events.log: TRX: op=ROLLBACK rc=0
    │
    └─ 失败: auditLogger_.OnTransactionEvent(path, "ROLLBACK", errorCode)
             └→ events.log: TRX: op=ROLLBACK rc=<err>

TransactionImpl 析构（未显式 Commit/Rollback）
    │
    └─ Rollback() → 同上
```

---

## OnSqlAudit 决策流程（两条路径共用）

```
OnSqlAudit(dbPath, op, tbl, rows)
    │
    ├─ !IsActive() → return（审计未启用）
    │
    ├─ op 为空？
    │   ├─ 是 → DDL 路径: ParseDropTruncateOp(sql) / ParseDropTruncateTable(sql)
    │   │       └─ 解析失败 → return
    │   └─ 否 → CRUD 路径: tbl = sql 参数（AUD-9，调用方传表名）
    │
    ├─ alwaysLog = (op ∈ {DELETE, DROP, TRUNCATE, BatchInsert})
    │
    ├─ 行数过滤:
    │   ├─ alwaysLog && op=="DELETE" && rows<=0 → return
    │   ├─ !alwaysLog && rows<=0 → return
    │   └─ 通过 → 继续
    │
    ├─ lock(txnMutex_) → 检查 inTransaction_
    │   ├─ true → txnBuffer_[{op, tbl}] += rows → return（缓冲，等 COMMIT flush）
    │   └─ false → 继续（非事务或 CreateTransaction 路径）
    │
    ├─ alwaysLog == true
    │   └─ EmitSqlAudit(dbPath, op, tbl, rows)
    │       └→ executor 异步写入 events.log: SQL: op=X tbl=Y rows=N
    │
    └─ alwaysLog == false（INSERT/UPDATE）
        └─ AccumulateOrFlush(dbPath, op, tbl, rows)
                │
                ├─ lock(mutex_) → 查 throttleMap_[key]
                │
                ├─ 窗口内 (< 60s)
                │   ├─ accumulatedRows >= 1000 (FLUSH_THRESHOLD)
                │   │   └─ 提前 flush: EmitSqlAudit(flushRows)
                │   │      reset entry
                │   │
                │   ├─ 首次抑制
                │   │   └─ EmitThrottleLog() → events.log: THROTTLE: op=X tbl=Y accumulated=N
                │   │      throttleLogged = true
                │   │
                │   └─ 后续抑制
                │       └─ 无输出（静默累积）
                │
                ├─ 窗口过期 (>= 60s)
                │   └─ flush 旧累积: EmitSqlAudit(flushRows)
                │      新建窗口: throttleMap_[key] = {now, rows, ...}
                │
                └─ throttleMap_ 超 64 条
                    └─ 驱逐过期条目
```

---

## 两条路径对比

| 维度 | Path 1: BeginTransaction | Path 2: CreateTransaction |
|------|--------------------------|---------------------------|
| 审计接口 | `OnTransaction` | `OnTransactionEvent` |
| 缓冲管理 | 管理 `inTransaction_`/`txnBuffer_` | 不管理（始终 false） |
| SQL 审计 | 缓冲→COMMIT 时 flush / ROLLBACK 时丢弃 | 直接走限流/直写 |
| BEGIN 审计行 | 不写 | 写 `TRX: op=BEGIN` |
| ROLLBACK 审计行 | 不写 | 写 `TRX: op=ROLLBACK` |
| COMMIT 审计行 | 写 `TRX: op=COMMIT` + flush SQL | 写 `TRX: op=COMMIT` |
| outer 判定 | 栈空=最外层 | 无（每次都是独立事务） |
| logger 来源 | `RdbStoreImpl` 的 `auditLogger_` | `TransactionImpl` 自建（`auditEnabled` bool） |
| 失败路径 | ROLLBACK 丢弃缓冲 | 写 `TRX: op=ROLLBACK rc=<err>` |

---

## 关键代码位置

| 文件 | 位置 | 说明 |
|------|------|------|
| `rdb_store_impl.cpp` 构造函数 | `auditLoggerHolder_(RdbAuditLogger::Create(config_.IsAuditEnabled()))` | Path 1 logger 创建 |
| `rdb_store_impl.cpp` BeginTransaction | `auditLogger_.OnTransaction(path, "BEGIN", E_OK, id==1)` | Path 1 BEGIN |
| `rdb_store_impl.cpp` Commit | `auditLogger_.OnTransaction(path, "COMMIT", E_OK, outer)` | Path 1 COMMIT |
| `rdb_store_impl.cpp` RollBack | `auditLogger_.OnTransaction(path, "ROLLBACK", rc, outer)` | Path 1 ROLLBACK |
| `rdb_store_impl.cpp` CreateTransaction | `Transaction::Create(type, conn, path, config_.IsAuditEnabled())` | Path 2 入口 |
| `transaction_impl.cpp` 构造函数 | `auditLoggerHolder_(RdbAuditLogger::Create(auditEnabled))` | Path 2 logger 创建 |
| `transaction_impl.cpp` Begin | `auditLogger_.OnTransactionEvent(path, "BEGIN", E_OK)` | Path 2 BEGIN |
| `transaction_impl.cpp` Commit | `auditLogger_.OnTransactionEvent(path, "COMMIT"/"ROLLBACK", rc)` | Path 2 COMMIT/失败 |
| `transaction_impl.cpp` Rollback | `auditLogger_.OnTransactionEvent(path, "ROLLBACK", rc)` | Path 2 ROLLBACK |
| `transaction_impl.cpp` AuditSql | `auditLogger_.OnSqlAudit(path, op, tbl, rows)` | Path 2 SQL 审计 |
| `rdb_audit_logger.cpp` OnTransaction | 管理 `inTransaction_`/`txnBuffer_` | Path 1 缓冲逻辑 |
| `rdb_audit_logger.cpp` OnTransactionEvent | 仅 `EmitTransaction()` | Path 2 直发 TRX 行 |
| `rdb_audit_logger.cpp` OnSqlAudit | 缓冲判定 + 限流/直写决策 | 两条路径共用 |
| `rdb_audit_logger.cpp` AccumulateOrFlush | 60s 窗口限流 | INSERT/UPDATE 限流 |
| `rdb_audit_logger.cpp` ~RdbAuditLoggerImpl | flush 未写限流数据 | 析构兜底 |

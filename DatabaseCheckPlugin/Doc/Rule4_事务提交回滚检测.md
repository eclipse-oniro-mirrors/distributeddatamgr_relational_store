# Rule4: 事务提交回滚检测

## 1. 规则描述

Rule4用于检测基于 OpenHarmony 平台的应用中事务的正确提交和回滚处理，确保每个事务都有适当的生命周期管理。该规则防止事务资源泄露和数据不一致问题。

**规则要求**：
- 每个事务创建都必须有对应的提交操作
- 异常处理必须包含回滚操作进行事务清理
- 事务不应该在没有适当关闭的情况下被丢弃

## 2. 规则配置

```xml
<localInspection 
    displayName="Rule 4: Transaction Commit and Rollback"
    groupName="Database Robustness Rules"
    shortName="DatabaseRule4"
    enabledByDefault="true"
    level="WARNING"
    implementationClass="com.example.databasecheck.inspections.Rule4TransactionCommitInspection"/>
```

## 3. 检测方法

### 3.1 检测架构
- **事务生命周期跟踪**: 从创建到提交/回滚的完整跟踪
- **异常处理分析**: 检查catch块中的回滚操作
- **作用域感知**: 以最内层函数和源码位置隔离命名/匿名/箭头函数及类/对象方法，支持简单返回类型，避免借用相邻函数或前一事务的提交

### 3.2 检测流程
1. **事务创建识别**: 按 `createTransaction/beginTransaction` 方法名提取完整调用，不限定变量名为 `rdbStore`，主流程与回退流程共用词法提取器
2. **变量跟踪**: 跟踪事务变量的赋值和使用；`const txn: relationalStore.Transaction = ...` 对应的变量名为 `txn`
3. **提交检测**: 验证每个事务是否有对应的`commit()`调用
4. **回滚检测**: 用括号配对定位异常块，检查真实的事务 `rollback()` 调用；字符串、模板及未调用的嵌套函数体不能作为回滚
5. **违规报告**: 标记缺失提交或回滚的事务

提交和回滚检查按源码顺序与局部作用域关联，并非所有控制流路径证明；不跨文件解析，
不推断对象真实类型。注释与字符串过滤保留源码偏移，支持嵌套参数和跨行事务赋值。

## 4. 正确与错误示例

### 4.1 错误示例

```javascript
// ✗ 错误：缺少提交操作
async function missingCommit() {
    let trans = await rdbStore.createTransaction();
    await trans.insert("users", {name: "test"});
    // 缺少 await trans.commit();
    return "done";
}

// ✗ 错误：异常处理缺少回滚
async function missingRollback() {
    try {
        let trans = await rdbStore.createTransaction();
        await trans.insert("users", {name: "test"});
        await trans.commit();
    } catch (err) {
        console.error("Error:", err);
        // 缺少 trans.rollback();
    }
}
```

### 4.2 正确示例

```javascript
// ✓ 正确：完整的事务处理
async function correctTransaction() {
    let trans;
    try {
        trans = await rdbStore.createTransaction();
        await trans.insert("users", {name: "test"});
        await trans.commit();
    } catch (err) {
        if (trans) {
            await trans.rollback();
        }
        throw err;
    }
}
```

## 5. 修复建议

### 5.1 基本修复原则
1. **确保提交**: 每个事务创建后必须调用`commit()`
2. **异常回滚**: 在catch块中添加`rollback()`调用
3. **资源管理**: 使用try-catch-finally模式确保事务正确关闭
4. **变量检查**: 在回滚前检查事务变量是否存在

### 5.2 推荐实践
- **统一模式**: 使用一致的事务处理模式
- **错误传播**: 在回滚后重新抛出异常
- **资源清理**: 确保所有事务资源都被正确释放

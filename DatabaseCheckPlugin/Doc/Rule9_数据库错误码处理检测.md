# Rule9: 数据库错误码处理检测

## 1. 规则描述

Rule9 检查包含数据库操作的 `try/catch`，识别错误码对应分支中的重试或恢复调用。
非空 catch、日志、重新抛出异常和回滚事务本身均不代表已处理特定错误码。

**规则要求**：

以下为 JS/TS 层错误码，依据 `frameworks/js/napi/relationalstore/src/napi_rdb_error.cpp`，
应以该接口映射表为准，不能直接套用底层返回值。

- **14800047**：关闭 ResultSet，并执行 `PRAGMA wal_checkpoint(TRUNCATE)`。
- **14800024、14800025**：实现重试调用及延迟或退避调用，处理 busy / locked 错误。
- **14800028**：实现重试加延迟，或者恢复调用，处理 I/O 错误。
- **14800029**：清理磁盘空间并重试。
- **14800011**：执行 restore 或 recover 类恢复调用，处理 corrupt 错误；仅重试不满足要求。

规则默认启用，严重级别为 WARNING。未识别出任何受支持错误码分支时提示缺少可验证的
错误码处理；已识别的分支分别检查动作。不会强制每个 catch 穷举表中所有错误码。
分支中出现错误码数字、动作名称字符串或注释，均不能代替实际条件和调用。

## 2. 规则配置

与 `src/main/resources/META-INF/plugin.xml` 中的配置一致：

```xml
<localInspection
    displayName="Rule 9: Database Error Code Handling and Retry Logic"
    groupName="Database Robustness Rules"
    shortName="DatabaseRule9"
    enabledByDefault="true"
    level="WARNING"
    implementationClass="com.example.databasecheck.inspections.Rule9DatabaseErrorHandlingInspection"/>
```

## 3. 检测方法

### 3.1 检测架构

- **IDE 检查入口**：`Rule9DatabaseErrorHandlingInspection` 通过既有文件类型过滤进入分析器，
  将分析结果转换为 IDE 告警。
- **词法与结构识别**：`JavaScriptCode` 对源代码分词，跳过注释并将字符串及可识别的正则字面量作为整体处理，
  使用括号配对确定代码块边界，同时保留原文偏移。
- **错误分支分析**：`DatabaseErrorHandlingAnalyzer` 识别包含数据库调用的 try/catch，
  提取捕获变量、可见别名及对应的 if、switch 错误码分支。
- **处理动作分析**：收集分支中的调用，按错误码匹配重试、延迟、恢复、清理或 checkpoint 动作；
  复用 `JavaScriptAnalyzer` 提取同文件命名函数作用域，按需展开辅助函数。
- **诊断输出**：输出缺失动作及错误码，使用原文范围定位对应 try/catch。

### 3.2 检测流程

1. **源码扫描**：生成词法单元和括号配对信息，排除注释、字符串中的伪代码干扰。
2. **数据库操作识别**：定位 try/catch，检查 try 内是否存在支持的数据库调用。
3. **错误码分支提取**：回溯捕获变量的直接别名和 `as BusinessError` 类型断言，
   识别对应 `.code` 的数字比较以及 switch case，过滤不支持的错误码。
4. **动作收集**：分析对应分支的调用，排除匹配不同错误码的分支和仅声明的函数体；
   对同文件命名辅助函数进行有限展开，保留当前错误码及可识别异常实参的绑定。
5. **规则判定**：依据第 1 节的错误码与动作要求，检查是否缺少必要动作；
   未找到受支持错误码分支时，报告缺少可验证的错误码处理。
6. **违规报告**：将错误码、缺失动作和原文位置提交给 IDE；不会强制每个 catch 穷举所有错误码。

### 3.3 核心检测逻辑

- **错误码与分支关联**：检查 `error.code ==/=== 数字` 或对应 switch case，
  错误码常量集合本身不代表应用已处理错误。
- **逐码匹配处理动作**：busy/locked 检查重试加延迟；corrupt 检查恢复；I/O 错误检查
  重试加延迟或恢复；磁盘满检查清理加重试；WAL 超限检查 close 加执行 TRUNCATE checkpoint。
- **分支动作隔离**：当前错误码不能借用相邻 else/case 或嵌套且匹配不同错误码的分支中的动作。
- **平凡处理判定**：空 catch、仅日志、仅重新抛出或仅回滚，均不能单独满足错误码对应动作。
- **辅助函数展开**：同文件命名函数检查可见函数体，避免空 retry/recover 函数仅凭名称通过；
  展开后重新筛选当前错误码的 if/case 动作，不借用不同错误码或 default 的动作；
  异常对象实参与形参关联，参数遮蔽不会直接引用外层同名异常；检测循环调用并限制展开深度。
- **排除非执行内容**：注释、字符串和未调用的函数声明不能作为处理动作，
  checkpoint 必须出现在支持的执行调用参数中。

**支持的语法**：

- `if/else if` 中捕获变量的 `.code` / `?.code` 与数字错误码的 `==` / `===` 比较，
  包括反向比较、OR 合并错误码以及额外的重试次数条件。
- `switch (error.code)`，包括多个相邻空 case 共享一个处理块。
- 嵌套代码块、含基本自动分号插入的单语句 if、带类型的 catch、可选 catch 绑定。
- 捕获异常的直接别名、别名链和 `as BusinessError` / 限定类型名断言；
  按赋值位置处理重新赋值和局部变量遮蔽，循环回溯最多访问 64 个绑定。
- 词法扫描跳过注释和字符串，使用括号配对识别嵌套结构，保留原文偏移用于 IDE 定位。
- 数据库调用识别沿用 CRUD 名称集合，并包括 getRdbStore、backup、restore、querySql
  及 insertSync/updateSync/deleteSync/querySync。

**动作识别约定与检测边界**：

这是语法级检查，不是完整的控制流或跨文件语义证明：

- 重试识别 `retry*()`、再次调用原数据库操作，或再次调用包围 try 的命名函数。
- 延迟识别 `sleep()`、`delay()`、`backoff()`、`setTimeout()`。
- 恢复识别 `restore()`、`recover*()`；清理空间识别 `cleanupDiskSpace()`、
  `cleanDiskSpace()`、`freeDiskSpace()`、`clearCache()`、`unlink()`、`unlinkSync()`。
- 同文件的命名 function 声明会展开检查函数体，空函数或只有日志的函数不能仅凭 retry/recover
  名称通过检查；循环调用会终止展开，最多展开 16 层。
- 无可见函数体的辅助函数名称表示应用提供的处理约定。跨文件函数、对象方法及未展开的函数
  封装不做符号解析，业务名称不同可能收到告警，需要人工核查。调用存在也不证明操作对象、
  备份有效性、处理顺序或执行成功。
- 检查动作限制在对应错误码分支中，不借用相邻 else/case 的动作，也不将仅声明的
  function/箭头函数体视为已执行。嵌套且匹配不同错误码的分支不能替代当前分支的处理。
- 不证明分支内所有控制流路径均执行动作，也不证明重试有界或退避时长合理。
  应用应自行设置重试次数上限，避免无限重试。
- 当前范围为 `.js/.ts/.jsx/.tsx/.ets` 文件中的 try/catch；`.ets` 复用文本扫描，
  不提供完整的 ArkTS 语义分析。不扩展 Promise `.catch()`、错误回调、错误码常量别名、
  负向 guard 或解构捕获变量。
  正则字面量在赋值、参数、return 等明确表达式起始位置按整体跳过，保护内部括号、转义和字符类；
  不提供完整的正则与除法上下文判定，也不解析正则内容或模板字符串插值的语义。
- corrupt 不应通过盲目重试解决。业务恢复应使用有效备份和 RDB 接口，并核查恢复失败的处理，
  不建议由本规则自动生成删除数据库等操作。

## 4. 正确与错误示例

示例中的 `rdbStore`、`resultSet` 和数据由应用提供并管理。`delay()` 表示应用的异步延迟，
`cleanupDiskSpace()` 表示真实的空间清理，`retryOperation()`、`recoverDatabase()` 等辅助函数
需要由应用实现；示例不通过声明空函数来代替处理。重试应考虑操作幂等性及实际故障类型。

### 4.1 错误示例

**错误：数据库操作使用空 catch。**

```javascript
async function emptyErrorHandling() {
    try {
        await rdbStore.insert('test', valueBucket);
    } catch (err) {
        // 缺少错误码判断和对应处理动作。
    }
}
```

**错误：仅打印日志并重新抛出，未处理特定错误码。**

```javascript
async function genericErrorHandling() {
    try {
        await rdbStore.insert('test', valueBucket);
    } catch (err) {
        console.error('Database error:', err);
        throw err;
    }
}
```

**错误：识别了 busy 错误码，但没有重试和延迟。**

```javascript
async function busyWithoutRetry() {
    try {
        await rdbStore.insert('test', valueBucket);
    } catch (err) {
        if (err.code === 14800024) {
            console.error('Database busy:', err);
        }
        throw err;
    }
}
```

**错误：locked 分支仅重试，没有延迟或退避。**

```javascript
async function lockedWithoutDelay() {
    try {
        await rdbStore.insert('test', valueBucket);
    } catch (err) {
        if (err.code === 14800025) {
            await retryOperation();
        }
        throw err;
    }
}
```

**错误：corrupt 分支只有重试，没有恢复。**

```javascript
async function corruptWithoutRecovery() {
    try {
        await rdbStore.executeSql('UPDATE test SET age = 18 WHERE id = 1');
    } catch (err) {
        if (err.code === 14800011) {
            await delay(100);
            await retryOperation();
        }
        throw err;
    }
}
```

**错误：I/O 错误分支仅记录日志，没有重试或恢复路径。**

```javascript
async function ioWithoutHandling() {
    try {
        await rdbStore.executeSql('UPDATE test SET age = 18 WHERE id = 1');
    } catch (err) {
        if (err.code === 14800028) {
            console.error('Database I/O error:', err);
        }
        throw err;
    }
}
```

**错误：磁盘满时直接重试，注释不能代替空间清理。**

```javascript
async function diskFullWithoutCleanup() {
    try {
        await rdbStore.insert('test', valueBucket);
    } catch (err) {
        if (err.code === 14800029) {
            // 应清理磁盘空间，但此处没有清理调用。
            await retryOperation();
        }
        throw err;
    }
}
```

**错误：WAL 超限分支只关闭 ResultSet，没有执行 checkpoint。**

```javascript
async function walWithoutCheckpoint(resultSet) {
    try {
        await rdbStore.insert('test', valueBucket);
    } catch (err) {
        if (err.code === 14800047) {
            resultSet?.close();
        }
        throw err;
    }
}
```

### 4.2 正确示例

**正确：busy / locked 分支使用带次数上限的延迟重试。**

```javascript
async function handleBusyDatabaseError(attempt = 0) {
    const maxRetries = 3;
    try {
        await rdbStore.insert('test', valueBucket);
    } catch (err) {
        if ((err.code === 14800024 || err.code === 14800025) && attempt < maxRetries) {
            await delay(100);
            return await handleBusyDatabaseError(attempt + 1);
        }
        throw err;
    }
}
```

**正确：corrupt 分支使用有效备份恢复，保留原操作失败的错误传播。**

以下示例针对已取得数据库实例后发生的损坏错误；应用需要核查备份有效性及 restore 失败路径。

```javascript
async function handleCorruptError() {
    try {
        await rdbStore.executeSql('UPDATE test SET age = 18 WHERE id = 1');
    } catch (err) {
        if (err.code === 14800011) {
            await rdbStore.restore('backup.db');
        }
        throw err;
    }
}
```

**正确：确认属于可重试的 I/O 故障后，对幂等操作进行有限重试。**

`isTransientIoError()` 由应用根据实际故障判断；不能仅凭 14800028 断定故障可重试。

```javascript
async function handleIoError(attempt = 0) {
    const maxRetries = 3;
    try {
        await rdbStore.executeSql('UPDATE test SET age = 18 WHERE id = 1');
    } catch (err) {
        if (err.code === 14800028 && isTransientIoError(err) && attempt < maxRetries) {
            await delay(100);
            return await handleIoError(attempt + 1);
        }
        throw err;
    }
}
```

**正确：磁盘满分支执行空间清理后有限重试。**

```javascript
async function handleDiskFullError(attempt = 0) {
    const maxRetries = 3;
    try {
        await rdbStore.insert('test', valueBucket);
    } catch (err) {
        if (err.code === 14800029 && attempt < maxRetries) {
            await cleanupDiskSpace();
            return await handleDiskFullError(attempt + 1);
        }
        throw err;
    }
}
```

**正确：WAL 超限分支关闭 ResultSet，并执行 TRUNCATE checkpoint。**

```javascript
async function handleResultSetError(resultSet) {
    try {
        await rdbStore.insert('test', valueBucket);
    } catch (err) {
        if (err.code === 14800047) {
            resultSet?.close();
            await rdbStore.execute('PRAGMA wal_checkpoint(TRUNCATE)');
        }
        throw err;
    }
}
```

**正确：使用 switch 区分错误类型，并为各分支提供相应动作。**

`recoverDatabase()` 表示应用针对 I/O 故障提供的恢复路径；以下示例不将 I/O 错误并入 busy 分支。
恢复或 checkpoint 完成后仍抛出原错误，由调用者决定失败操作的后续处理。

```javascript
async function comprehensiveErrorHandling(resultSet, attempt = 0) {
    const maxRetries = 3;
    try {
        await rdbStore.insert('test', valueBucket);
    } catch (err) {
        switch (err.code) {
            case 14800047:
                resultSet?.close();
                await rdbStore.execute('PRAGMA wal_checkpoint(TRUNCATE)');
                break;
            case 14800024:
            case 14800025:
                if (attempt < maxRetries) {
                    await delay(100);
                    return await comprehensiveErrorHandling(resultSet, attempt + 1);
                }
                break;
            case 14800028:
                await recoverDatabase();
                break;
            case 14800029:
                if (attempt < maxRetries) {
                    await cleanupDiskSpace();
                    return await comprehensiveErrorHandling(resultSet, attempt + 1);
                }
                break;
            case 14800011:
                await rdbStore.restore('backup.db');
                break;
            default:
                console.error('Unhandled database error:', err);
        }
        throw err;
    }
}
```

## 5. 修复建议

### 5.1 基本修复原则

1. 按错误码区分临时锁冲突、数据库损坏、I/O 错误、空间不足和 WAL 超限，提供对应处理路径。
2. 对可重试操作设置次数上限和延迟策略，并保留重试耗尽后的错误传播或降级处理。
3. 对 corrupt 核查有效备份与恢复失败路径；日志或盲目重试不能替代恢复。
4. 补充真实清理和恢复调用，避免只写注释或空的辅助函数。

### 5.2 修复步骤

1. 根据告警中的错误码定位对应 catch 分支。
2. 对照第 1 节补充缺失动作，确保动作属于该错误码的处理路径。
3. 检查操作对象、资源清理、处理顺序及辅助函数实现；这些语义不能仅靠本规则证明。
4. 运行自动测试；对跨文件封装或不支持语法导致的告警进行人工复核。

### 5.3 错误码处理指南

#### 14800047 - WAL 超限

- 关闭打开的 ResultSet，并执行 TRUNCATE checkpoint。
- 检查资源对象和 checkpoint 执行结果，避免仅声明 SQL 字符串或只关闭资源。

#### 14800024/25 - 数据库忙碌或锁冲突

- 对可重试操作实现延迟或退避策略。
- 设置合理的重试次数上限，并处理重试耗尽场景。

#### 14800028 - I/O 错误

- 根据实际故障选择重试加延迟或恢复路径，避免将所有 I/O 错误都视为锁冲突。
- 核查存储状态及操作结果。

#### 14800029 - 磁盘空间不足

- 清理临时文件或缓存后重试；仅打印清理提示或添加注释不满足要求。
- 处理清理失败或空间仍不足的情况。

#### 14800011 - 数据库损坏

- 使用有效备份和 RDB 恢复接口，检查恢复失败后的处理。
- 不用盲目重试替代恢复，不由本规则自动生成删除数据库等操作。

### 5.4 推荐实践

- **错误分类**：区分锁冲突、数据库损坏、I/O 错误、空间不足与 WAL 超限。
- **有限重试**：明确次数上限和退避策略；本规则不证明重试有界。
- **人工复核**：检查操作对象、备份有效性、处理顺序及跨文件封装的实际实现。
- **自动验证**：使用以下自动断言测试验证检测行为，避免只在 IDE 中人工观察。

在 DatabaseCheckPlugin 目录运行 Gradle 8.12.1（JDK 17 或以上）：

```bash
./gradlew test buildPlugin
```

仓库若未提供 `gradle-wrapper.jar`，可用已安装的 Gradle 运行 `gradle test buildPlugin`。
JUnit 用例验证错误码与动作配对、缺失动作、嵌套结构、switch 合并 case、注释/字符串排除、
未执行函数声明排除、诊断原始偏移和真实 IDE visitor。正反例 JS 文件也由自动断言校验，
无需依赖人工在 IDE 中观察。

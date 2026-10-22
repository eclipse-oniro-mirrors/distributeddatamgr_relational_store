# Rule9: 数据库错误码处理检测

Rule9 检查包含数据库操作的 `try/catch`，识别错误码对应分支中的重试或恢复调用。
非空 catch、日志、重新抛出异常和回滚事务本身均不代表已处理特定错误码。

## 错误码及检查动作

以下为 JS/TS 层错误码，依据 `frameworks/js/napi/relationalstore/src/napi_rdb_error.cpp`，
不能直接使用 Native 或 NDK 的数值替代。

| 错误码 | 含义 | 检查动作 |
| --- | --- | --- |
| 14800024 / 14800025 | busy / locked | 重试调用以及延迟或退避调用 |
| 14800011 | corrupt | restore 或 recover 类恢复调用；仅重试不满足要求 |
| 14800028 | I/O 错误 | 重试加延迟，或者恢复调用 |
| 14800029 | 磁盘满 | 清理空间调用以及重试调用 |
| 14800047 | WAL 超限 | close 调用以及执行 `PRAGMA wal_checkpoint(TRUNCATE)` |

规则默认启用，严重级别为 WARNING。未识别出任何受支持错误码分支时提示缺少可验证的
错误码处理；已识别的分支分别检查动作。不会强制每个 catch 穷举表中所有错误码。
分支中出现错误码数字、动作名称字符串或注释，均不能代替实际条件和调用。

## 支持的语法

- `if/else if` 中捕获变量的 `.code` / `?.code` 与数字错误码的 `==` / `===` 比较，
  包括反向比较、OR 合并错误码以及额外的重试次数条件。
- `switch (error.code)`，包括多个相邻空 case 共享一个处理块。
- 嵌套代码块、单语句 if、带类型的 catch、可选 catch 绑定。
- 词法扫描跳过注释和字符串，使用括号配对识别嵌套结构，保留原文偏移用于 IDE 定位。
- 数据库调用识别沿用 CRUD 名称集合，并包括 getRdbStore、backup、restore、querySql
  及 insertSync/updateSync/deleteSync/querySync。

## 动作识别约定与边界

这是语法级检查，不是完整的控制流或跨文件语义证明：

- 重试识别 `retry*()`、再次调用原数据库操作，或再次调用包围 try 的命名函数。
- 延迟识别 `sleep()`、`delay()`、`backoff()`、`setTimeout()`。
- 恢复识别 `restore()`、`recover*()`；清理空间识别 `cleanupDiskSpace()`、
  `cleanDiskSpace()`、`freeDiskSpace()`、`clearCache()`、`unlink()`、`unlinkSync()`。
- 同文件的命名 function 声明会展开检查函数体，空函数或只有日志的函数不能仅凭 retry/recover
  名称通过检查；循环调用会终止展开，最多展开 16 层。
- 无可见函数体的辅助函数名称表示应用提供的处理约定。跨文件、对象方法或其他形式的函数
  封装不做符号解析，业务名称不同可能收到告警，需要人工核查。调用存在也不证明操作对象、
  备份有效性、处理顺序或执行成功。
- 检查动作限制在对应错误码分支中，不借用相邻 else/case 的动作，也不将仅声明的
  function/箭头函数体视为已执行。嵌套的其他错误码分支不能替代当前分支的处理。
- 不证明分支内所有控制流路径均执行动作，也不证明重试有界或退避时长合理。
  应用应自行设置重试次数上限，避免无限重试。
- 当前范围为现有插件支持的 `.js/.ts/.jsx/.tsx` 文件中的 try/catch；不扩展
  Promise `.catch()`、错误回调、`.ets`、错误码常量别名、负向 guard 或解构捕获变量。
  正则字面量和模板字符串插值中的代码也不做语义解析。
- corrupt 不应通过盲目重试解决。业务恢复应使用有效备份和 RDB 接口，并核查恢复失败的处理，
  不建议由本规则自动生成删除数据库等操作。

## 示例

仅打印 busy 错误会告警，即使 catch 非空：

```javascript
try {
    await rdbStore.insert('users', values);
} catch (error) {
    if (error.code === 14800024) {
        console.error(error);
    }
}
```

带次数限制的重试示例：

```javascript
async function insertWithRetry(attempt = 0) {
    try {
        await rdbStore.insert('users', values);
    } catch (error) {
        if ((error.code === 14800024 || error.code === 14800025) && attempt < 3) {
            await delay(100);
            return await insertWithRetry(attempt + 1);
        }
        throw error;
    }
}
```

损坏错误需要恢复路径：

```javascript
try {
    await relationalStore.getRdbStore(context, config);
} catch (error) {
    if (error.code === 14800011) {
        await rdbStore.restore('backup.db');
    }
    throw error;
}
```

## 自动验证

在 DatabaseCheckPlugin 目录运行 Gradle 8.12.1（JDK 17 或以上）：

```bash
./gradlew test buildPlugin
```

仓库若未提供 `gradle-wrapper.jar`，可用已安装的 Gradle 运行 `gradle test buildPlugin`。
JUnit 用例验证错误码与动作配对、缺失动作、嵌套结构、switch 合并 case、注释/字符串排除、
未执行函数声明排除、诊断原始偏移和真实 IDE visitor。正反例 JS 文件也由自动断言校验，
无需依赖人工在 IDE 中观察。

/*
 * Copyright (c) 2024-2026 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.example.databasecheck.utils

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import java.nio.file.Files
import java.nio.file.Paths

class DatabaseErrorHandlingAnalyzerTest {
    private fun source(handler: String, operation: String = "await rdbStore.insert('t', values);"): String =
        "try { $operation } catch (error) { $handler }"

    private fun warnings(handler: String): List<DatabaseErrorHandlingAnalyzer.Violation> =
        DatabaseErrorHandlingAnalyzer(source(handler)).analyze()

    @Test
    fun emptyAndGenericHandlersAreReported() {
        // Arrange: handlers that previously slipped through the non-empty catch check.
        val handlers = listOf("", "/* recover(error); */", "console.error(error); throw error;",
            "recordFailure(error);", "return;", "throw error;")
        for (handler in handlers) {
            // Act / Assert: each handler must produce the actionable generic warning.
            val result = warnings(handler)
            assertEquals(handler, 1, result.size)
            assertTrue(handler, result.single().reason.contains("No verifiable"))
        }
    }

    @Test
    fun busyAndLockedRequireBothRetryAndDelay() {
        for (error in listOf(14800024, 14800025)) {
            val incomplete = listOf("console.error(error);", "await retryOperation();", "await delay(10);")
            for (body in incomplete) {
                val result = warnings("if (error.code === $error) { $body }")
                assertEquals("$error: $body", 1, result.size)
                assertTrue(result.single().reason.contains("$error"))
            }
            assertTrue(warnings("if (error.code === $error) { await delay(10); await retryOperation(); }").isEmpty())
        }
    }

    @Test
    fun corruptRequiresRecoveryRatherThanRetry() {
        assertEquals(1, warnings("if (error.code === 14800011) { await delay(10); await retryOperation(); }").size)
        assertTrue(warnings("if (error.code === 14800011) { await rdbStore.restore('backup.db'); }").isEmpty())
        assertTrue(warnings("if (error.code === 14800011) { await recoverDatabase(); }").isEmpty())
    }

    @Test
    fun ioErrorAcceptsRetryWithDelayOrRecovery() {
        assertEquals(1, warnings("if (error.code === 14800028) { await retryOperation(); }").size)
        assertTrue(warnings("if (error.code === 14800028) { await delay(1); await retryOperation(); }").isEmpty())
        assertTrue(warnings("if (error.code === 14800028) { await recoverDatabase(); }").isEmpty())
    }

    @Test
    fun diskFullRequiresCleanupAndRetry() {
        assertEquals(1, warnings("if (error.code === 14800029) { await retryOperation(); }").size)
        assertEquals(1, warnings("if (error.code === 14800029) { await cleanupDiskSpace(); }").size)
        assertTrue(warnings("if (error.code === 14800029) { await cleanupDiskSpace(); await retryOperation(); }").isEmpty())
    }

    @Test
    fun walLimitRequiresCloseAndAnExecutedCheckpoint() {
        val close = "resultSet?.close();"
        val checkpoint = "await rdbStore?.execute('PRAGMA wal_checkpoint(TRUNCATE)');"
        assertEquals(1, warnings("if (error.code === 14800047) { $close }").size)
        assertEquals(1, warnings("if (error.code === 14800047) { $checkpoint }").size)
        assertTrue(warnings("if (error.code === 14800047) { $close $checkpoint }").isEmpty())
        assertEquals(1, warnings("if (error.code === 14800047) { $close const sql = 'PRAGMA wal_checkpoint(TRUNCATE)'; }").size)
        assertEquals(1, warnings("if (error.code === 14800047) { $close await rdbStore.execute('PRAGMA wal_checkpoint(PASSIVE)'); }").size)
    }

    @Test
    fun groupedSwitchCasesShareTheirActions() {
        val handler = """
            switch (error.code) {
                case 14800024:
                case 14800025:
                    await delay(10);
                    await retryOperation();
                    break;
                case 14800011:
                    await rdbStore.restore('backup.db');
                    break;
                default: throw error;
            }
        """.trimIndent()
        assertTrue(warnings(handler).isEmpty())
        assertEquals(2, warnings(handler.replace("await retryOperation();", "console.error(error);")).size)
    }

    @Test
    fun actionsCannotBeBorrowedFromOtherBranches() {
        val handler = """
            if (error.code === 14800024) { console.error(error); }
            else if (error.code === 14800011) { await delay(10); await retryOperation(); }
        """.trimIndent()
        assertEquals(2, warnings(handler).size)
        assertEquals(1, warnings("""
            if (error.code === 14800024) {
                if (error.code === 14800011) { await delay(10); await retryOperation(); }
            }
        """).count { it.reason.contains("14800024") })
    }

    @Test
    fun nestedGuardsAndReversedComparisonsAreRecognized() {
        assertTrue(warnings("""
            if ((14800024 === error.code || error?.code == 14800025) && needRetry) {
                if (attempt < maxAttempts) { await delay(10); await retryOperation(); }
            }
        """).isEmpty())
        assertTrue(warnings("if (error.code === 14800011) await recoverDatabase();").isEmpty())
    }

    @Test
    fun stringsCommentsAndFunctionDeclarationsDoNotCountAsActions() {
        val bodies = listOf(
            "// await delay(10); await retryOperation();\nconsole.error(error);",
            "const text = 'delay(10); retryOperation();';",
            "const text = `delay(10); retryOperation();`;",
            "function retryOperation() { delay(10); }",
            "const retryOperation = () => { delay(10); };",
            "return; await delay(10); await retryOperation();",
            "throw error; await delay(10); await retryOperation();"
        )
        for (body in bodies) assertEquals(body, 1, warnings("if (error.code === 14800024) { $body }").size)
    }

    @Test
    fun originalOffsetsSurviveCommentsEscapesAndBracesInStrings() {
        val prefix = "// try { fake(); } catch (error) {}\n/* 中文 { } */\n"
        val input = prefix + source("if (error.code === 14800011) { const text = \"} \\\" {\"; console.error(error); }")
        val result = DatabaseErrorHandlingAnalyzer(input).analyze().single()
        assertEquals(prefix.length, result.start)
        assertEquals(input.length, result.end)
        assertEquals(input.substring(prefix.length), input.substring(result.start, result.end))
    }

    @Test
    fun nonDatabaseHandlersAndLiteralTryCatchAreIgnored() {
        val inputs = listOf(
            "try { await fetch(url); } catch (error) {}",
            "const text = \"try { rdbStore.insert(); } catch (error) {}\";",
            "// try { rdbStore.insert(); } catch (error) {}",
            "try { const query = 'rdbStore'; } catch (error) {}"
        )
        for (input in inputs) assertTrue(input, DatabaseErrorHandlingAnalyzer(input).analyze().isEmpty())
    }

    @Test
    fun typedCatchAndOptionalBindingDoNotCrash() {
        val typed = "try { store.querySql('SELECT 1'); } catch (e: BusinessError) { if (e.code === 14800011) { recoverDatabase(); } }"
        assertTrue(DatabaseErrorHandlingAnalyzer(typed).analyze().isEmpty())
        assertEquals(1, DatabaseErrorHandlingAnalyzer("try { store.insert('t', values); } catch {}").analyze().size)
    }

    @Test
    fun nestedTryCatchIsReportedAtTheInnerHandler() {
        val input = "try { try { store.insert('t', values); } catch (inner) {} } catch (outer) {}"
        val result = DatabaseErrorHandlingAnalyzer(input).analyze()
        assertEquals(2, result.size)
        assertEquals(input.indexOf("try", 1), result.last().start)
    }

    @Test
    fun incompleteCodeAndUnsupportedNegativeGuardsDoNotGetAccepted() {
        val incomplete = listOf("try {", "try { store.query(); } catch (e) {", "try { store.query(); } finally {}")
        for (input in incomplete) assertTrue(input, DatabaseErrorHandlingAnalyzer(input).analyze().isEmpty())
        assertEquals(1, warnings("if (error.code !== 14800024) { delay(10); retryOperation(); }").size)
        assertEquals(1, warnings("if (!(error.code === 14800024)) { delay(10); retryOperation(); }").size)
    }
    @Test
    fun checkedInExamplesAreAssertedAutomatically() {
        val base = Paths.get("src/test/kotlin/com/example/databasecheck/testdata/Rule9")
        val correct = Files.readString(base.resolve("test-rule9-correct.js"))
        val incorrect = Files.readString(base.resolve("test-rule9-violations.js"))
        assertEquals("Positive examples", emptyList<DatabaseErrorHandlingAnalyzer.Violation>(),
            DatabaseErrorHandlingAnalyzer(correct).analyze())
        assertEquals("Negative examples", 9, DatabaseErrorHandlingAnalyzer(incorrect).analyze().size)
    }

    @Test
    fun unusedHandlerFunctionsAndOtherErrorObjectsDoNotSatisfyRule() {
        val handlers = listOf(
            "function unused() { if (error.code === 14800011) { recoverDatabase(); } }",
            "const unused = () => { if (error.code === 14800011) { recoverDatabase(); } };",
            "if (othererror.code === 14800011) { recoverDatabase(); }",
            "if (message.includes('error.code === 14800011')) { recoverDatabase(); }"
        )
        for (handler in handlers) assertEquals(handler, 1, warnings(handler).size)
    }

    @Test
    fun switchDefaultAndAdjacentCasesCannotSupplyMissingActions() {
        val handler = """
            switch (error.code) {
                case 14800024: console.error(error); break;
                case 14800011: await recoverDatabase(); break;
                default: await delay(1); await retryOperation();
            }
        """.trimIndent()
        val result = warnings(handler)
        assertEquals(1, result.size)
        assertTrue(result.single().reason.contains("14800024"))
        assertEquals(1, warnings("""
            switch (other.code) { case 14800011: recoverDatabase(); break; }
        """).size)
    }

    @Test
    fun recognizedCodesAreCheckedIndividuallyWithoutRequiringAllCodes() {
        val result = warnings("""
            if (error.code === 14800024 || error.code === 14800025) { console.error(error); }
        """)
        assertEquals(2, result.size)
        assertTrue(result[0].reason.contains("14800024"))
        assertTrue(result[1].reason.contains("14800025"))
        assertTrue(warnings("if (error.code === 14800011) { recoverDatabase(); }").isEmpty())
        assertEquals(1, warnings("if (error.code === 401) { reportParameterError(); }").size)
    }

    @Test
    fun declarationsAndOperationNamesAloneAreNotDatabaseCalls() {
        val input = "try { function unused() { store.insert('t', values); } } catch (error) {}"
        assertTrue(DatabaseErrorHandlingAnalyzer(input).analyze().isEmpty())
        assertEquals(1, warnings("""
            if (error.code === 14800011) { const restore = 'recoverDatabase'; }
        """).size)
    }

    @Test
    fun sameFileHelpersAreCheckedByTheirBodies() {
        val invalidBusy = source("if (error.code === 14800024) { delay(1); retryOperation(); }") +
            "function retryOperation() { console.error('not a retry'); }"
        val invalidCorrupt = source("if (error.code === 14800011) { recoverDatabase(); }") +
            "function recoverDatabase() {}"
        assertEquals(1, DatabaseErrorHandlingAnalyzer(invalidBusy).analyze().size)
        assertEquals(1, DatabaseErrorHandlingAnalyzer(invalidCorrupt).analyze().size)
        val validBusy = source("if (error.code === 14800024) { repeatInsert(); }") +
            "async function repeatInsert() { await delay(1); await rdbStore.insert('t', values); }"
        val validCorrupt = source("if (error.code === 14800011) { repair(); }") +
            "async function repair() { await rdbStore.restore('backup.db'); }"
        assertTrue(DatabaseErrorHandlingAnalyzer(validBusy).analyze().isEmpty())
        assertTrue(DatabaseErrorHandlingAnalyzer(validCorrupt).analyze().isEmpty())
    }

    @Test
    fun helperCyclesWithoutDatabaseActionsDoNotCountAsRetry() {
        val input = source("if (error.code === 14800024) { delay(1); retryOperation(); }") +
            "function retryOperation() { again(); } function again() { retryOperation(); }"
        assertEquals(1, DatabaseErrorHandlingAnalyzer(input).analyze().size)
    }

}

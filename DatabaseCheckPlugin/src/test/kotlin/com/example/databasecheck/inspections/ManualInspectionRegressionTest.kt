/*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
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

package com.example.databasecheck.inspections

import com.intellij.codeInspection.InspectionManager
import com.intellij.codeInspection.ProblemDescriptor
import com.intellij.codeInspection.ProblemsHolder
import com.intellij.testFramework.fixtures.LightPlatformCodeInsightFixtureTestCase

/** Reproductions supplied in PR !3582, run through the inspection visitor on ETS files. */
class ManualInspectionRegressionTest : LightPlatformCodeInsightFixtureTestCase() {
    fun testTypedSingleOperationTransactionWarns() {
        val problems = inspectFixture(Rule3TransactionUsageInspection(), "rule3-single-operation.ets")
        assertEquals("A single insert must be reported", 1, problems.size)
        assertTrue(problems.single().descriptionTemplate.contains("only one CRUD operation"))
    }

    fun testTypedMultipleOperationTransactionCommitIsRecognized() {
        val problems = inspectFixture(Rule4TransactionCommitInspection(), "rule4-multiple-operations.ets")
        assertTrue("txn.commit() must close the typed transaction: $problems", problems.isEmpty())
    }

    fun testTypedConfigSecurityLevelMismatchWarns() {
        assertConfigMismatch("rule14-security.ets", "securityLevel")
    }

    fun testTypedConfigEncryptMismatchWarns() {
        assertConfigMismatch("rule14-encrypt.ets", "encrypt")
    }

    fun testTypedConfigsInDifferentFunctionsAreCompared() {
        assertConfigMismatch("rule14-functions.ets", "securityLevel")
    }

    fun testMultipleOperationsDoNotTriggerSingleOperationWarning() {
        val problems = inspectFixture(Rule3TransactionUsageInspection(), "rule4-multiple-operations.ets")
        assertTrue("Multiple CRUD operations must be accepted", problems.isEmpty())
    }

    fun testCommitInSiblingTypedFunctionCannotCloseTransaction() {
        val source = """
            async function first(): Promise<void> {
                const txn: relationalStore.Transaction = await rdbStore.createTransaction();
                await txn.insert('t', value);
            }
            async function second(): Promise<void> {
                const txn: relationalStore.Transaction = await rdbStore.createTransaction();
                await txn.insert('t', value);
                txn.commit();
            }
        """.trimIndent()
        val problems = inspectText(Rule4TransactionCommitInspection(), source)
        assertEquals("Only the first transaction is uncommitted", 1, problems.size)
        assertTrue(problems.single().descriptionTemplate.contains("Missing transaction commit"))
    }

    fun testIdenticalTypedConfigurationsDoNotWarn() {
        val source = """
            const config1: relationalStore.StoreConfig = { name: 'db', encrypt: true };
            const config2: relationalStore.StoreConfig = { name: 'db', encrypt: true };
            await relationalStore.getRdbStore(context, config1);
            await relationalStore.getRdbStore(context, config2);
        """.trimIndent()
        assertTrue("Equal configurations must remain accepted",
            inspectText(Rule14DatabaseConfigConsistencyInspection(), source).isEmpty())
    }

    fun testDifferentDatabaseNamesAreNotCompared() {
        val source = """
            await relationalStore.getRdbStore(context, { name: 'first', encrypt: true });
            await relationalStore.getRdbStore(context, { name: 'second', encrypt: false });
        """.trimIndent()
        assertTrue("Different databases can have different configurations",
            inspectText(Rule14DatabaseConfigConsistencyInspection(), source).isEmpty())
    }

    fun testInlineConfigsCompareFirstFieldAndNormalizeNameQuotes() {
        val source = """
            await relationalStore.getRdbStore(context, { name: 'db', encrypt: true });
            await relationalStore.getRdbStore(context, { name: "db", encrypt: false });
        """.trimIndent()
        val problems = inspectText(Rule14DatabaseConfigConsistencyInspection(), source)
        assertEquals("The same literal database name must be grouped", 1, problems.size)
        assertTrue(problems.single().descriptionTemplate.contains("encrypt"))
    }

    fun testUnusedSiblingFunctionConfigDoesNotShadowGlobalConfig() {
        val source = """
            const config: relationalStore.StoreConfig = { name: 'db', encrypt: true };
            await relationalStore.getRdbStore(context, config);
            function unused(): void {
                const config: relationalStore.StoreConfig = { name: 'db', encrypt: false };
            }
            async function open(): Promise<void> {
                await relationalStore.getRdbStore(context, config);
            }
        """.trimIndent()
        assertTrue("A sibling function's local config is not visible at the call",
            inspectText(Rule14DatabaseConfigConsistencyInspection(), source).isEmpty())
    }

    fun testUnusedInnerBlockConfigDoesNotEscapeItsBlock() {
        val source = """
            const config: relationalStore.StoreConfig = { name: 'db', encrypt: true };
            await relationalStore.getRdbStore(context, config);
            if (enabled) {
                const config: relationalStore.StoreConfig = { name: 'db', encrypt: false };
            }
            await relationalStore.getRdbStore(context, config);
        """.trimIndent()
        assertTrue("An inner block's local config must not escape",
            inspectText(Rule14DatabaseConfigConsistencyInspection(), source).isEmpty())
    }

    fun testVarConfigurationRemainsVisibleOutsideItsInnerBlock() {
        val source = """
            function open() {
                if (enabled) { var config = { name: 'db', encrypt: false }; }
                relationalStore.getRdbStore(context, config);
                relationalStore.getRdbStore(context, { name: 'db', encrypt: true });
            }
        """.trimIndent()
        val problems = inspectText(Rule14DatabaseConfigConsistencyInspection(), source, "regression.js")
        assertEquals("JS var must retain function scope", 1, problems.size)
        assertTrue(problems.single().descriptionTemplate.contains("encrypt"))
    }

    fun testConfigWarningOffsetIgnoresCallNamesInComments() {
        val source = """
            // getRdbStore(context, config) is mentioned in documentation.
            await relationalStore.getRdbStore(context, { name: 'db', encrypt: true });
            await relationalStore.getRdbStore(context, { name: 'db', encrypt: false });
        """.trimIndent()
        val problems = inspectText(Rule14DatabaseConfigConsistencyInspection(), source)
        assertEquals("Expected one inline config warning", 1, problems.size)
        assertEquals("The comment must not shift the highlighted call",
            source.lastIndexOf("getRdbStore("), problems.single().textRangeInElement!!.startOffset)
    }

    private fun inspectText(
        rule: BaseDatabaseInspection,
        source: String,
        fileName: String = "regression.ets"
    ): List<ProblemDescriptor> {
        val file = myFixture.configureByText(fileName, source)
        val holder = ProblemsHolder(InspectionManager.getInstance(project), file, false)
        rule.buildVisitor(holder, false).visitFile(file)
        return holder.results
    }

    private fun assertConfigMismatch(fileName: String, parameter: String) {
        val problems = inspectFixture(Rule14DatabaseConfigConsistencyInspection(), fileName)
        assertEquals("Expected one configuration mismatch in $fileName", 1, problems.size)
        assertTrue("The warning must identify $parameter",
            problems.single().descriptionTemplate.contains(parameter))
        val range = problems.single().textRangeInElement!!
        assertEquals("Highlight the second database open call",
            myFixture.file.text.lastIndexOf("getRdbStore("), range.startOffset)
    }

    private fun inspectFixture(rule: BaseDatabaseInspection, fileName: String): List<ProblemDescriptor> {
        // Arrange: load the complete user reproduction, including type annotations and comments.
        myFixture.testDataPath = System.getProperty("user.dir") +
            "/src/test/kotlin/com/example/databasecheck/testdata/ManualRegression"
        val file = myFixture.configureByFile(fileName)
        val holder = ProblemsHolder(InspectionManager.getInstance(project), file, false)

        // Act: keep file filtering and the actual rule adapter in the tested path.
        rule.buildVisitor(holder, false).visitFile(file)
        return holder.results
    }
}

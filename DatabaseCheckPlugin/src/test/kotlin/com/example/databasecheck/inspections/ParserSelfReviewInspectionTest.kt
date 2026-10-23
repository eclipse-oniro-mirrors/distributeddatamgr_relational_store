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
import com.intellij.psi.PsiFile
import com.intellij.testFramework.fixtures.LightPlatformCodeInsightFixtureTestCase

class ParserSelfReviewInspectionTest : LightPlatformCodeInsightFixtureTestCase() {
    fun testRenamedStoreTransactionIsCheckedByRules3And4() {
        val source = """
            async function write(): Promise<void> {
                const txn: relationalStore.Transaction = await store.createTransaction();
                await txn.insert('t', value);
            }
        """.trimIndent()
        assertEquals("Rule3 must recognize store.createTransaction", 1,
            inspect(Rule3TransactionUsageInspection(), source).size)
        assertEquals("Rule4 must recognize store.createTransaction", 1,
            inspect(Rule4TransactionCommitInspection(), source).size)
    }

    fun testRenamedStoreLegacyCallsAreDeprecated() {
        val source = """
            const store = await relationalStore.getRdbStore(context, config);
            store.beginTransaction(); store.commit(); store.rollback();
        """.trimIndent()
        assertEquals("All three legacy store methods must warn", 3,
            inspect(Rule5TransactionNestingInspection(), source).size)
    }

    fun testTransactionObjectNamedRdbStoreIsNotDeprecated() {
        val source = """
            const rdbStore = await store.createTransaction();
            rdbStore.commit(); rdbStore.rollback();
        """.trimIndent()
        assertTrue("Receiver identity must follow its binding, not its spelling",
            inspect(Rule5TransactionNestingInspection(), source).isEmpty())
    }

    fun testTransactionParameterCommitIsNotDeprecated() {
        val source = "function finish(txn: relationalStore.Transaction): void { txn.commit(); }"
        assertTrue(inspect(Rule5TransactionNestingInspection(), source).isEmpty())
    }

    fun testTransactionFallbacksAcceptRenamedStores() {
        val source = "const txn = store.createTransaction(); txn.insert('t', value);"
        assertEquals(1, inspect(Rule3TransactionUsageInspection(), source, "fallbackTransactionAnalysis").size)
        assertEquals(1, inspect(Rule4TransactionCommitInspection(), source, "fallbackTransactionAnalysis").size)
    }

    fun testLegacyFallbackAcceptsRenamedStores() {
        val source = "store.beginTransaction(); store.commit(); store.rollback();"
        assertEquals(3, inspect(Rule5TransactionNestingInspection(), source, "fallbackDeprecatedApiAnalysis").size)
    }

    fun testDeletionFallbackRecognizesRenamedOptionalClose() {
        val source = "store?.close(); relationalStore.deleteRdbStore(context, 'db');"
        assertTrue("Fallback must recognize the same close calls as the main path",
            inspect(Rule6DatabaseDeletionInspection(), source, "fallbackDeletionAnalysis").isEmpty())
    }

    fun testDeletionCannotBorrowCloseFromSiblingFunction() {
        val source = """
            function closeElsewhere(): void { store.close(); }
            function remove(): void { relationalStore.deleteRdbStore(context, 'db'); }
        """.trimIndent()
        assertEquals("A sibling function's close call has not run", 1,
            inspect(Rule6DatabaseDeletionInspection(), source).size)
    }

    fun testTemplateCommentMarkersDoNotHideFollowingViolation() {
        val source = """
            const message = `Don't strip // text or /* text */ }`;
            const txn = store.createTransaction();
            txn.insert('t', value);
        """.trimIndent()
        assertEquals("A template must not swallow the transaction", 1,
            inspect(Rule3TransactionUsageInspection(), source).size)
    }

    fun testTemplateCodeDoesNotCountAsLegacyApiUsage() {
        val source = "const example = `rdbStore.beginTransaction(); rdbStore.commit();`;"
        assertTrue("Documentation in a template is not executed code",
            inspect(Rule5TransactionNestingInspection(), source).isEmpty())
    }

    fun testTemplateBraceDoesNotBreakRollbackAnalysis() {
        val source = """
            async function write(): Promise<void> {
                try {
                    const txn = rdbStore.createTransaction();
                    const message = `} ' // literal`;
                    txn.insert('t', value);
                    txn.commit();
                } catch (error) { console.error(error); }
            }
        """.trimIndent()
        val problems = inspect(Rule4TransactionCommitInspection(), source)
        assertEquals("The try block still requires rollback", 1, problems.size)
        assertTrue(problems.single().descriptionTemplate.contains("rollback"))
    }

    fun testNestedHelperCannotCommitOuterTransaction() {
        val source = """
            async function write(): Promise<void> {
                const txn = rdbStore.createTransaction();
                txn.insert('t', value);
                function neverCalled(): void { txn.commit(); }
            }
        """.trimIndent()
        assertEquals("Declaring a helper does not execute commit", 1,
            inspect(Rule4TransactionCommitInspection(), source).size)
    }

    fun testMultilineTransactionCreationIsBoundToItsVariable() {
        val source = """
            const txn: relationalStore.Transaction = await store.createTransaction(
                { type: getType() }
            );
            txn.insert('t', makeValue());
        """.trimIndent()
        assertEquals(1, inspect(Rule3TransactionUsageInspection(), source).size)
        assertEquals(1, inspect(Rule4TransactionCommitInspection(), source).size)
    }

    fun testReassignedTransactionCannotBorrowPreviousCommit() {
        val source = """
            let txn = store.createTransaction();
            txn.insert('t', value); txn.commit();
            txn = store.createTransaction();
            txn.insert('t', value);
        """.trimIndent()
        val problems = inspect(Rule4TransactionCommitInspection(), source)
        assertEquals("The second transaction still needs commit", 1, problems.size)
    }

    fun testArrowHelperCannotCommitOuterTransaction() {
        val source = """
            const txn = store.createTransaction();
            txn.insert('t', value);
            const later = () => { txn.commit(); };
        """.trimIndent()
        assertEquals(1, inspect(Rule4TransactionCommitInspection(), source).size)
    }

    fun testClassMethodsCannotShareTransactionCommit() {
        val source = """
            class A {
                write(): void { const txn = store.createTransaction(); txn.insert('t', value); }
                finish(): void { const txn = store.createTransaction(); txn.commit(); }
            }
        """.trimIndent()
        assertEquals(1, inspect(Rule4TransactionCommitInspection(), source).size)
    }

    fun testConfigCallWithNestedArgumentsIsCompared() {
        val source = """
            relationalStore.getRdbStore(getContext(), { name: 'db,a', encrypt: true });
            relationalStore.getRdbStore(getContext(), { name: 'db,a', encrypt: false });
        """.trimIndent()
        val problems = inspect(Rule14DatabaseConfigConsistencyInspection(), source)
        assertEquals("Nested context calls and commas in names must not truncate the config", 1, problems.size)
        assertTrue(problems.single().descriptionTemplate.contains("encrypt"))
    }

    fun testConfigCallsInsideStringsAreIgnored() {
        val source = """
            const example = `relationalStore.getRdbStore(context, { name: 'db', encrypt: false })`;
            relationalStore.getRdbStore(context, { name: 'db', encrypt: true });
        """.trimIndent()
        assertTrue(inspect(Rule14DatabaseConfigConsistencyInspection(), source).isEmpty())
    }

    fun testDeletionCannotUseStringCloseOrWrongOrder() {
        val source = """
            const example = 'store.close()';
            relationalStore.deleteRdbStore(context, 'db');
            store.close();
        """.trimIndent()
        assertEquals(1, inspect(Rule6DatabaseDeletionInspection(), source).size)
        assertEquals(1, inspect(Rule6DatabaseDeletionInspection(), source, "fallbackDeletionAnalysis").size)
    }

    fun testTypedStoreParameterIsCheckedButTypedTransactionIsAccepted() {
        assertEquals(1, inspect(Rule5TransactionNestingInspection(),
            "function finish(store: relationalStore.RdbStore): void { store.commit(); }").size)
        assertTrue(inspect(Rule5TransactionNestingInspection(),
            "function finish(rdbStore: relationalStore.Transaction): void { rdbStore.commit(); }").isEmpty())
    }

    fun testStringRollbackDoesNotSatisfyCatchCleanup() {
        val source = """
            try {
                const txn = store.createTransaction(); txn.insert('t', value); txn.commit();
            } catch (error) { console.log('txn.rollback()'); }
        """.trimIndent()
        val problems = inspect(Rule4TransactionCommitInspection(), source)
        assertEquals(1, problems.size)
        assertTrue(problems.single().descriptionTemplate.contains("rollback"))
    }

    fun testNestedCallbackRollbackDoesNotSatisfyCatchCleanup() {
        val source = """
            try {
                const txn = store.createTransaction(); txn.insert('t', value); txn.commit();
            } catch (error) { const later = () => { txn.rollback(); }; }
        """.trimIndent()
        assertEquals(1, inspect(Rule4TransactionCommitInspection(), source).size)
    }

    fun testRealLoopIsNotSingleOperationButTemplateLoopIs() {
        val real = """
            const txn = store.createTransaction();
            for (let i = 0; i < 3; i++) { txn.insert('t', value); }
        """.trimIndent()
        assertTrue("The loop executes more than one insert", inspect(Rule3TransactionUsageInspection(), real).isEmpty())
        val fake = """
            const txn = store.createTransaction();
            const example = `for (let i = 0; i < 3; i++) { }`;
            txn.insert('t', value); txn.commit();
        """.trimIndent()
        assertEquals("Template text is not an actual loop", 1, inspect(Rule3TransactionUsageInspection(), fake).size)
    }

    private fun inspect(
        rule: BaseDatabaseInspection,
        source: String,
        fallback: String? = null
    ): List<ProblemDescriptor> {
        // Arrange: real ETS input and the IDE's problem collector.
        val file = myFixture.configureByText("review.ets", source)
        val holder = ProblemsHolder(InspectionManager.getInstance(project), file, false)
        // Act: test both normal dispatch and the fallback entry points used after analysis failures.
        if (fallback == null) rule.buildVisitor(holder, false).visitFile(file)
        else rule.javaClass.getDeclaredMethod(fallback, PsiFile::class.java, ProblemsHolder::class.java)
            .apply { isAccessible = true }.invoke(rule, file, holder)
        // Assert in each caller using diagnostics, counts and the offending operation.
        return holder.results
    }
}

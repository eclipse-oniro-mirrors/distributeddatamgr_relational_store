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

package com.example.databasecheck.utils

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

/** Reproduce review findings through the production analyzer, without executing the source inputs. */
class GateReviewRegressionTest {
    private fun analyze(body: String): List<DatabaseErrorHandlingAnalyzer.Violation> =
        DatabaseErrorHandlingAnalyzer("try { await store.insert('t', values); } catch (e) { $body }").analyze()

    @Test
    fun typedAndPlainAliasesAcceptCompleteHandling() {
        // Arrange / Act / Assert: casts and aliases retain the caught error's identity.
        for (binding in listOf("const err = e as BusinessError;", "let err = e;",
            "const first = (e as BusinessError); const err = first;")) {
            assertTrue(binding, analyze("$binding if (err.code === 14800024) { retry(); delay(1); }").isEmpty())
        }
    }

    @Test
    fun typedAliasSwitchAcceptsRecovery() {
        assertTrue(analyze("""
            const err = e as BusinessError;
            switch (err.code) { case 14800011: recoverDatabase(); break; }
        """).isEmpty())
    }

    @Test
    fun reassigningAliasCannotHandleCaughtError() {
        assertEquals(1, analyze("""
            let err = e as BusinessError; err = other;
            if (err.code === 14800024) { retry(); delay(1); }
        """).size)
    }

    @Test
    fun aliasRetainsValueBeforeOriginalIsReassigned() {
        assertTrue(analyze("""
            const err = e; e = other;
            if (err.code === 14800024) { retry(); delay(1); }
        """).isEmpty())
    }

    @Test
    fun blockShadowAndSiblingBindingsAreNotErrorAliases() {
        val bodies = listOf(
            "const err = e; { const err = other; if (err.code === 14800024) { retry(); delay(1); } }",
            "{ const err = e; } { if (err.code === 14800024) { retry(); delay(1); } }"
        )
        for (body in bodies) assertEquals(body, 1, analyze(body).size)
    }

    @Test
    fun aliasCycleDoesNotEstablishErrorHandling() {
        assertEquals(1, analyze("let a = b; let b = a; if (a.code === 14800011) { recoverDatabase(); }").size)
    }

    @Test
    fun singleStatementWithoutSemicolonAcceptsExecutedHelper() {
        val input = """
            async function retryWithDelay() { retry(); delay(1); }
            try { await store.insert('t', values); } catch (e) {
                if (e.code === 14800024) await retryWithDelay()
            }
        """.trimIndent()
        assertTrue(DatabaseErrorHandlingAnalyzer(input).analyze().isEmpty())
    }

    @Test
    fun singleStatementCannotBorrowFollowingLineActions() {
        assertEquals(1, analyze("""
            if (e.code === 14800024) console.error(e)
            retry(); delay(1);
        """).size)
    }

    @Test
    fun bracedRecoveryWithoutSemicolonStillPasses() {
        assertTrue(analyze("if (e.code === 14800011) { recoverDatabase() }").isEmpty())
    }

    @Test
    fun regexBracketsCannotHideLoggingOnlyCatch() {
        val input = """
            try { const pattern = /[({]/; await store.insert('t', values); }
            catch (e) { console.error(e); }
        """.trimIndent()
        assertEquals(1, DatabaseErrorHandlingAnalyzer(input).analyze().size)
    }

    @Test
    fun helperCannotBorrowDifferentErrorCodeActions() {
        assertEquals(1, analyze("""
            async function handle() { if (e.code === 14800025) { retry(); delay(1); } }
            if (e.code === 14800024) { handle(); }
        """).size)
    }

    @Test
    fun helperCanUseCapturedErrorAndItsAlias() {
        assertTrue(analyze("""
            const err = e as BusinessError;
            async function handle() { if (err.code === 14800024) { retry(); delay(1); } }
            if (err.code === 14800024) { handle(); }
        """).isEmpty())
    }

    @Test
    fun helperCannotBorrowSwitchCaseOrDefaultActions() {
        assertEquals(1, analyze("""
            function handle() {
                switch (e.code) {
                    case 14800024: console.error(e); break;
                    case 14800025: retry(); delay(1); break;
                    default: retry(); delay(1);
                }
            }
            if (e.code === 14800024) { handle(); }
        """).size)
    }

    @Test
    fun helperParametersRetainErrorIdentity() {
        for (name in listOf("err", "e")) {
            val suffix = """
                function handle($name: BusinessError) {
                    if ($name.code === 14800025) { retry(); delay(1); }
                }
            """.trimIndent()
            val busy = "try { store.insert('t', values); } catch (e) { if (e.code === 14800024) { handle(e); } }"
            assertEquals(1, DatabaseErrorHandlingAnalyzer(busy + suffix).analyze().size)
            val locked = busy.replace("14800024", "14800025")
            assertTrue(name, DatabaseErrorHandlingAnalyzer(locked + suffix).analyze().isEmpty())
            val reassigned = suffix.replace("if (", "$name = other; if (")
            assertEquals(name, 1, DatabaseErrorHandlingAnalyzer(locked + reassigned).analyze().size)
        }
    }

    @Test
    fun helperParameterShadowCannotBorrowCaughtErrorName() {
        assertEquals(1, analyze("""
            function handle(e: BusinessError) { if (e.code === 14800024) { retry(); delay(1); } }
            if (e.code === 14800024) { handle(other); }
        """).size)
    }

    @Test
    fun transitiveHelpersKeepActiveErrorCode() {
        assertEquals(1, analyze("""
            function outer() { inner(); }
            function inner() { if (e.code === 14800025) { retry(); delay(1); } }
            if (e.code === 14800024) { outer(); }
        """).size)
    }

    @Test
    fun caughtErrorShadowsOuterParameterAndAssignment() {
        val body = "try { store.insert('t', values); } catch (e) { if (e.code === 14800011) { recoverDatabase(); } }"
        for (prefix in listOf("function write(e: BusinessError) { ", "const e = other; function write() { ")) {
            assertTrue(DatabaseErrorHandlingAnalyzer(prefix + body + " }").analyze().isEmpty())
        }
    }

    @Test
    fun helperCannotBorrowElseActionsWhenCurrentBranchIsEmpty() {
        for (alternative in listOf("else", "else if (e.code === 14800025)",
            "else if (e.code === 14800025) { console.error(e); } else")) {
            assertEquals(alternative, 1, analyze("""
                function handle() {
                    if (e.code === 14800024) { console.error(e); }
                    $alternative { retry(); delay(1); }
                }
                if (e.code === 14800024) { handle(); }
            """).size)
        }
    }

    @Test
    fun helperCanUseElseOfDifferentErrorCode() {
        assertTrue(analyze("""
            function handle() {
                if (e.code === 14800025) { console.error(e); }
                else { retry(); delay(1); }
            }
            if (e.code === 14800024) { handle(); }
        """).isEmpty())
    }
}

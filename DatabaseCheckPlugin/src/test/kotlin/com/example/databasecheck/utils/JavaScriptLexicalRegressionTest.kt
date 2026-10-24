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

import org.junit.Assert.*
import org.junit.Test

class JavaScriptLexicalRegressionTest {
    @Test
    fun nestedCallsAndTopLevelArgumentsKeepCompleteRanges() {
        val source = "store.execute(makeSql('a,b'), { x: [1, 2] }, done(value));"
        val calls = JavaScriptAnalyzer.extractFunctionCallsFromText(source)
        val outer = calls.single { it.functionName == "store.execute" }
        assertEquals(listOf("makeSql('a,b')", "{ x: [1, 2] }", "done(value)"), outer.arguments)
        assertEquals(source.indexOf(';'), outer.endOffset)
        assertEquals(setOf("store.execute", "makeSql", "done"), calls.map { it.functionName }.toSet())
    }

    @Test
    fun stringsCommentsAndTemplatesAreNotCalls() {
        val source = """
            const a = "store.createTransaction()";
            const b = `store.createTransaction(); // text`;
            /* store.createTransaction(); */
            store.createTransaction();
        """.trimIndent()
        val calls = JavaScriptAnalyzer.extractFunctionCallsFromText(source)
        assertEquals(1, calls.size)
        assertEquals(source.lastIndexOf("store.createTransaction"), calls.single().startOffset)
    }

    @Test
    fun optionalAndSpacedMemberCallsRetainReceiver() {
        val source = "this . store ?. insert(makeValue());"
        val call = JavaScriptAnalyzer.extractFunctionCallsFromText(source).single { it.functionName.endsWith(".insert") }
        assertEquals("this.store", call.objectName)
        assertEquals(listOf("makeValue()"), call.arguments)
        assertEquals(0, call.startOffset)
        assertEquals(source.indexOf(';'), call.endOffset)
    }

    @Test
    fun templateQuotesAndCommentMarkersArePreserved() {
        val source = "const a = `Don't strip // text or /* text */ }`; // real comment\nnext();"
        val cleaned = JavaScriptCode(source).withoutComments()
        assertEquals(source.length, cleaned.length)
        assertEquals(source.substringBefore("; //") + ";", cleaned.substringBefore(';') + ";")
        assertFalse(cleaned.contains("real comment"))
        assertEquals(source.indexOf("next"), cleaned.indexOf("next"))
    }

    @Test
    fun multilineInitializerIsNotTruncatedAtFirstLineOrNestedCall() {
        val source = """
            const txn: relationalStore.Transaction = await store.createTransaction(
                { type: getType() }
            );
            txn.insert('t', value);
        """.trimIndent()
        val assignment = JavaScriptAnalyzer.extractVariableAssignmentsFromText(source).single()
        assertEquals("txn", assignment.variableName)
        assertTrue(assignment.assignmentExpression.endsWith(")"))
        assertEquals(source.indexOf(';'), assignment.endOffset)
    }

    @Test
    fun identicalFunctionNamesStillHaveDistinctScopes() {
        val source = """
            class A { write(): void { store.insert('t', value); } }
            class B { write(): void { store.commit(); } }
        """.trimIndent()
        val (calls, _) = JavaScriptAnalyzer.extractWithFunctionScopes(source)
        assertNotNull(calls[0].functionScope)
        assertNotEquals(calls[0].functionScope, calls[1].functionScope)
    }

    @Test
    fun nestedAndAnonymousFunctionCallsBelongToTheirOwnScopes() {
        val source = """
            function outer() {
                store.insert('t', value);
                function inner() { store.commit(); }
                const a = () => { store.rollback(); };
                const b = function() { store.close(); };
            }
        """.trimIndent()
        val calls = JavaScriptAnalyzer.extractWithFunctionScopes(source).first
        assertEquals(4, calls.size)
        assertEquals(4, calls.map { it.functionScope }.toSet().size)
    }

    @Test
    fun aliasCyclesTerminateWithoutLosingValidDatabaseAliases() {
        val assignments = JavaScriptAnalyzer.extractVariableAssignmentsFromText(
            "const a = b; const b = a; const db = '/data/storage/el1/database/a.db';")
        assertFalse(JavaScriptAnalyzer.containsDatabasePath("a", DatabaseConstants.DATABASE_PATHS, assignments))
        assertTrue(JavaScriptAnalyzer.containsDatabasePath("db", DatabaseConstants.DATABASE_PATHS, assignments))
    }

    @Test
    fun equalityAndCodeInsideStringsAreNotAssignments() {
        val assignments = JavaScriptAnalyzer.extractVariableAssignmentsFromText(
            "if (a === b) {} const example = 'const txn = store.createTransaction();';")
        assertEquals(listOf("example"), assignments.map { it.variableName })
    }

    @Test
    fun semicolonFreeDeclarationsAreSeparated() {
        val assignments = JavaScriptAnalyzer.extractVariableAssignmentsFromText(
            "const first = store.createTransaction()\nconst second = store.createTransaction()\nsecond.commit()")
        assertEquals(listOf("first", "second"), assignments.map { it.variableName })
        assertTrue(assignments.all { it.assignmentExpression == "store.createTransaction()" })
    }

    @Test
    fun regexLiteralsProtectBracketsAndFakeCalls() {
        val source = "const re = /[({]/; if (x) { real(); }"
        val code = JavaScriptCode(source)
        val open = code.tokens.indexOfFirst { it.text == "{" }
        assertEquals(source.lastIndexOf('}'), code.tokens[code.end(open)].start)
        assertEquals(listOf("real"), JavaScriptAnalyzer.extractFunctionCallsFromText(source).map { it.functionName })
        assertTrue(JavaScriptAnalyzer.extractFunctionCallsFromText("const re = /store.insert('t')/g;").isEmpty())
    }

    @Test
    fun regexEscapesClassesAndCommentMarkersRemainOpaque() {
        val source = "const re = /[\\/({]a\\/b/g; const text = /[/][/]/; /* real */ next();"
        val cleaned = JavaScriptCode(source).withoutComments()
        assertEquals(source.length, cleaned.length)
        assertTrue(cleaned.contains("/[/][/]/"))
        assertFalse(cleaned.contains("real"))
        assertEquals(source.indexOf("next"), cleaned.indexOf("next"))
        assertEquals(listOf("next"), JavaScriptAnalyzer.extractFunctionCallsFromText(cleaned).map { it.functionName })
    }

    @Test
    fun divisionDoesNotConsumeFollowingCalls() {
        val source = "const n = total / count / scale; const m = (n + 1) / factor; next(n, m);"
        assertEquals(listOf("next"), JavaScriptAnalyzer.extractFunctionCallsFromText(source).map { it.functionName })
    }

    @Test
    fun regexAfterReturnAndCommaDoesNotBreakArguments() {
        val source = "function pattern() { return /[({]/; } use(/[,)]/, value);"
        val call = JavaScriptAnalyzer.extractFunctionCallsFromText(source).single { it.functionName == "use" }
        assertEquals(listOf("/[,)]/", "value"), call.arguments)
    }

    @Test
    fun incompleteRegexAndDivisionRemainSafe() {
        for (source in listOf("const re = /[", "const re = /unterminated\nnext();", "const x = n / d;")) {
            JavaScriptCode(source).withoutComments()
            assertTrue(JavaScriptAnalyzer.extractFunctionCallsFromText(source)
                .none { it.functionName == "unterminated" })
        }
    }
}

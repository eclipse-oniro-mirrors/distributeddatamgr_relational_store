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

class JavaScriptAnalyzerTest {
    @Test
    fun typedDeclarationRetainsVariableNameAndFullRange() {
        // Arrange: the trailing type name used to be mistaken for the variable.
        val source = "const txn: relationalStore.Transaction = await store.createTransaction();"

        // Act.
        val assignments = JavaScriptAnalyzer.extractVariableAssignmentsFromText(source)

        // Assert: no duplicate assignment for the type name is emitted.
        assertEquals(1, assignments.size)
        assertEquals("txn", assignments.single().variableName)
        assertEquals("await store.createTransaction()", assignments.single().assignmentExpression)
        assertEquals(0, assignments.single().startOffset)
        assertEquals(source.indexOf(';'), assignments.single().endOffset)
    }

    @Test
    fun untypedDeclarationsAndReassignmentsArePreserved() {
        val source = "let txn = store.createTransaction();\ntxn = store.createTransaction();"
        val assignments = JavaScriptAnalyzer.extractVariableAssignmentsFromText(source)
        assertEquals("Both declaration and reassignment must remain visible", 2, assignments.size)
        assertTrue(assignments.all { it.variableName == "txn" })
    }

    @Test
    fun typedFunctionsKeepCallsInSeparateScopes() {
        val source = """
            async function first(context: common.Context): Promise<void> {
                const txn: relationalStore.Transaction = await store.createTransaction();
                txn.insert('t', value);
            }
            function second(): void { txn.commit(); }
        """.trimIndent()
        val (calls, assignments) = JavaScriptAnalyzer.extractWithFunctionScopes(source)
        assertEquals(listOf("first", "second"), JavaScriptAnalyzer.extractFunctionScopes(source).map { it.functionName })
        assertEquals("first@0", calls.single { it.functionName == "txn.insert" }.functionScope)
        assertEquals("second@${source.indexOf("function second")}", calls.single { it.functionName == "txn.commit" }.functionScope)
        assertEquals("first@0", assignments.single().functionScope)
        assertEquals("txn", assignments.single().variableName)
    }

    @Test
    fun untypedFunctionsContinueToProduceScopes() {
        val source = "function open() { store.querySql('SELECT 1'); }"
        val (calls, _) = JavaScriptAnalyzer.extractWithFunctionScopes(source)
        assertEquals("open@0", calls.single { it.functionName == "store.querySql" }.functionScope)
    }
}

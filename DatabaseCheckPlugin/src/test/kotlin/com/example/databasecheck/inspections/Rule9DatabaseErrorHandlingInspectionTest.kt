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

package com.example.databasecheck.inspections

import com.intellij.codeInspection.InspectionManager
import com.intellij.codeInspection.ProblemsHolder
import com.intellij.testFramework.fixtures.LightPlatformCodeInsightFixtureTestCase

class Rule9DatabaseErrorHandlingInspectionTest : LightPlatformCodeInsightFixtureTestCase() {
    fun testBusyLoggingProducesProblemAtOriginalRange() {
        // Arrange: a comment changes the original offset, and the busy handler only logs.
        val prefix = "/* 中文 } */\n"
        val source = prefix + "try { store.insert('t', values); } catch (error) { " +
            "if (error.code === 14800024) { console.error(error); } }"
        val file = myFixture.configureByText("sample.js", source)
        val holder = ProblemsHolder(InspectionManager.getInstance(project), file, false)

        // Act: exercise the registered visitor, including the file-type guard and adapter.
        Rule9DatabaseErrorHandlingInspection().buildVisitor(holder, false).visitFile(file)

        // Assert: an actionable Rule9 diagnostic points at the complete original try/catch.
        assertEquals(1, holder.results.size)
        val problem = holder.results.single()
        assertTrue(problem.descriptionTemplate.contains("Database Rule 9"))
        assertTrue(problem.descriptionTemplate.contains("14800024"))
        assertEquals(prefix.length, problem.textRangeInElement!!.startOffset)
        assertEquals(source.length, problem.textRangeInElement!!.endOffset)
    }

    fun testCorruptionRecoveryDoesNotProduceProblem() {
        val file = myFixture.configureByText("sample.ts", """
            try { store.querySql('SELECT 1'); } catch (error) {
                if (error.code === 14800011) { await store.restore('backup.db'); }
            }
        """.trimIndent())
        val holder = ProblemsHolder(InspectionManager.getInstance(project), file, false)
        Rule9DatabaseErrorHandlingInspection().buildVisitor(holder, false).visitFile(file)
        assertTrue(holder.results.isEmpty())
    }
}

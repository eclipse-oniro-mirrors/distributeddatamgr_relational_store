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
import com.intellij.codeInspection.ProblemsHolder
import com.intellij.psi.PsiFile
import com.intellij.testFramework.fixtures.LightPlatformCodeInsightFixtureTestCase
import java.io.File

/** Verify the deliverable ETS files, including file dispatch and diagnostic content/range. */
class EtsAcceptanceInspectionTest : LightPlatformCodeInsightFixtureTestCase() {
    fun testRule1EtsCases() = verifyRule(1, Rule1NonRdbInterfaceInspection())

    fun testRule2EtsCases() = verifyRule(2, Rule2DatabaseCopyInspection())

    fun testRule3EtsCases() = verifyRule(3, Rule3TransactionUsageInspection())

    fun testRule4EtsCases() = verifyRule(4, Rule4TransactionCommitInspection())

    fun testRule5EtsCases() = verifyRule(5, Rule5TransactionNestingInspection())

    fun testRule6EtsCases() = verifyRule(6, Rule6DatabaseDeletionInspection())

    fun testRule7EtsCases() = verifyRule(7, Rule7DatabasePermissionInspection())

    fun testRule9EtsCases() = verifyRule(9, Rule9DatabaseErrorHandlingInspection())

    fun testRule12EtsCases() = verifyRule(12, Rule12PragmaRestrictionInspection())

    fun testRule14EtsCases() = verifyRule(14, Rule14DatabaseConfigConsistencyInspection())

    private fun verifyRule(number: Int, rule: BaseDatabaseInspection) {
        // Arrange: each source file is an isolated case with an explicit expected result.
        val root = File(System.getProperty("user.dir"),
            "EtsAcceptance/Rule$number")
        val cases = root.listFiles { file -> file.extension == "ets" }?.sortedBy { it.name }.orEmpty()
        assertTrue("No ETS cases found for Rule$number: $root", cases.size >= 2)
        for (input in cases) {
            val source = input.readText()
            val expected = metadata(source, "Expected").toInt()
            val message = metadata(source, "Message")
            val file = myFixture.configureByText(input.name, source)
            val holder = ProblemsHolder(InspectionManager.getInstance(project), file, false)

            // Act: use the same visitor entry as the installed plugin.
            rule.buildVisitor(holder, false).visitFile(file)

            // Assert: check both missing warnings and unwanted warnings for positive cases.
            assertEquals("${input.name}: ${metadata(source, "Point")}", expected, holder.results.size)
            for (problem in holder.results) {
                assertTrue("${input.name}: unexpected message ${problem.descriptionTemplate}",
                    problem.descriptionTemplate.contains(message, ignoreCase = true))
                val range = problem.textRangeInElement
                assertNotNull("${input.name}: diagnostic must have a source range", range)
                assertTrue("${input.name}: invalid diagnostic range $range",
                    range!!.startOffset >= 0 && range.startOffset < range.endOffset && range.endOffset <= source.length)
            }
            assertAllRules(file, source, input.name)
            println("PASS ${input.name}: Rule$number expected=$expected actual=${holder.results.size}")
        }
    }

    private fun assertAllRules(file: PsiFile, source: String, name: String) {
        val encoded = metadata(source, "ExpectedAll")
        val expected = if (encoded == "none") emptyMap() else encoded.split(',').associate {
            val pair = it.split(':')
            pair[0].toInt() to pair[1].toInt()
        }
        val actual = allRules().mapNotNull { rule ->
            val holder = ProblemsHolder(InspectionManager.getInstance(project), file, false)
            rule.buildVisitor(holder, false).visitFile(file)
            if (holder.results.isEmpty()) null else rule.getRuleNumber() to holder.results.size
        }.toMap()
        assertEquals("$name: all enabled rules must match the documented warning distribution", expected, actual)
        println("ALL-RULES $name: expected=$expected actual=$actual")
    }

    private fun allRules(): List<BaseDatabaseInspection> = listOf(
        Rule1NonRdbInterfaceInspection(), Rule2DatabaseCopyInspection(), Rule3TransactionUsageInspection(),
        Rule4TransactionCommitInspection(), Rule5TransactionNestingInspection(), Rule6DatabaseDeletionInspection(),
        Rule7DatabasePermissionInspection(), Rule9DatabaseErrorHandlingInspection(),
        Rule12PragmaRestrictionInspection(), Rule14DatabaseConfigConsistencyInspection()
    )

    private fun metadata(source: String, key: String): String {
        val prefix = "// $key:"
        val line = source.lineSequence().firstOrNull { it.startsWith(prefix) }
        assertNotNull("Missing $key in ETS case", line)
        return line!!.removePrefix(prefix).trim()
    }
}

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

import com.example.databasecheck.utils.JavaScriptAnalyzer
import com.example.databasecheck.utils.JavaScriptCode
import com.intellij.codeInspection.*
import com.intellij.psi.PsiFile

/**
 * Rule 5: Transaction Nesting and Thread Safety Detection
 *
 * This inspection detects violations of HarmonyOS Database Robustness Rule 5, which ensures
 * proper transaction visibility and prevents transaction nesting by detecting old-style API usage.
 *
 * ## Rule Requirements:
 * 1. Detect usage of old-style beginTransaction API which can cause thread safety issues
 * 2. Prevent transaction nesting by identifying deprecated transaction methods
 * 3. Ensure proper transaction visibility between threads
 *
 * ## Detection Methods:
 * ### Method 1: Old-style API Detection
 * - Detect usage of `rdbStore.beginTransaction()` (deprecated)
 * - Detect usage of `rdbStore.commit()` (deprecated) 
 * - Detect usage of `rdbStore.rollback()` (deprecated)
 *
 * ## Violation Types:
 * - **Deprecated beginTransaction**: Using old-style beginTransaction API
 * - **Deprecated commit**: Using old-style commit API
 * - **Deprecated rollback**: Using old-style rollback API
 *
 * @author Database Robustness Checker
 * @since 1.0
 */
class Rule5TransactionNestingInspection : BaseDatabaseInspection() {

    override fun getRuleName(): String = "Transaction Nesting and Thread Safety"
    override fun getRuleNumber(): Int = 5
    override fun getDescription(): String =
        "Prevents transaction nesting and ensures thread safety by detecting deprecated transaction APIs."

    override fun visitJavaScriptFile(file: PsiFile, holder: ProblemsHolder) {
        analyzeJavaScriptFile(file, holder)
    }

    /**
     * Analyze JavaScript file for Rule5 violations.
     */
    private fun analyzeJavaScriptFile(file: PsiFile, holder: ProblemsHolder) {
        try {
            val originalText = file.text
            val codeWithoutComments = removeComments(originalText)
            
            // Find all deprecated transaction API usages
            val deprecatedApiViolations = findDeprecatedTransactionAPIs(codeWithoutComments)
            
            // Report violations
            reportViolations(deprecatedApiViolations, file, holder)
            
        } catch (e: Exception) {
            // Fallback analysis if main analysis fails
            fallbackDeprecatedApiAnalysis(file, holder)
        }
    }

    /**
     * Find deprecated transaction API violations.
     */
    private fun findDeprecatedTransactionAPIs(codeText: String): List<DeprecatedApiViolation> {
        val (calls, assignments) = JavaScriptAnalyzer.extractWithFunctionScopes(codeText)
        val code = JavaScriptCode(codeText)
        return calls.filter { call ->
            call.objectName != null && call.arguments.isEmpty() &&
                (call.functionName.substringAfterLast('.') == "beginTransaction" ||
                    call.functionName.substringAfterLast('.') in setOf("commit", "rollback") &&
                    isStoreReceiver(call, calls, assignments, code))
        }.map { call ->
            val apiName = call.functionName.substringAfterLast('.')
            DeprecatedApiViolation(ProhibitedOperation(call.functionName, call.startOffset, call.endOffset,
                codeText.substring(call.startOffset, call.endOffset), ""),
                when (apiName) {
                    "commit" -> DeprecatedApiType.COMMIT
                    "rollback" -> DeprecatedApiType.ROLLBACK
                    else -> DeprecatedApiType.BEGIN_TRANSACTION
                }, generateViolationReason(apiName))
        }
    }

    private fun isStoreReceiver(
        call: JavaScriptAnalyzer.FunctionCall,
        calls: List<JavaScriptAnalyzer.FunctionCall>,
        assignments: List<JavaScriptAnalyzer.VariableAssignment>,
        code: JavaScriptCode
    ): Boolean {
        val binding = assignments.filter {
            it.variableName == call.objectName && it.startOffset < call.startOffset &&
                code.isVisibleAt(it.startOffset, call.startOffset)
        }.maxByOrNull { it.startOffset }
        if (binding != null) {
            val origins = JavaScriptAnalyzer.extractFunctionCallsFromText(binding.assignmentExpression)
                .map { it.functionName.substringAfterLast('.') }
            if ("createTransaction" in origins) return false
            if ("getRdbStore" in origins) return true
        }
        if (calls.any { it.objectName == call.objectName && it.functionScope == call.functionScope &&
                it.startOffset < call.startOffset && it.functionName.endsWith(".beginTransaction") }) return true
        val scope = JavaScriptAnalyzer.extractFunctionScopes(code.source).filter {
            call.startOffset in it.bodyStartOffset until it.endOffset
        }.minByOrNull { it.endOffset - it.bodyStartOffset }
        val header = scope?.let { code.source.substring(it.startOffset, it.bodyStartOffset) }.orEmpty()
        val typedTransaction = Regex("""\b${Regex.escape(call.objectName.orEmpty())}\s*:\s*(?:[a-zA-Z_$][a-zA-Z0-9_$]*\.)?Transaction\b""")
        if (typedTransaction.containsMatchIn(header)) return false
        val typedStore = Regex("""\b${Regex.escape(call.objectName.orEmpty())}\s*:\s*(?:[a-zA-Z_$][a-zA-Z0-9_$]*\.)?RdbStore\b""")
        return typedStore.containsMatchIn(header) || binding == null && call.objectName == "rdbStore"
    }

    /**
     * Generate violation reason message for deprecated API usage.
     */
    private fun generateViolationReason(apiName: String): String {
        return when (apiName) {
            "beginTransaction" -> 
                "Using deprecated rdbStore.beginTransaction() can cause thread safety issues and transaction nesting. " +
                "Use rdbStore.createTransaction() with proper transaction objects instead."
            "commit" -> 
                "Using deprecated rdbStore.commit() can cause thread safety issues. " +
                "Use transaction.commit() on transaction objects created with createTransaction()."
            "rollback" -> 
                "Using deprecated rdbStore.rollback() can cause thread safety issues. " +
                "Use transaction.rollback() on transaction objects created with createTransaction()."
            else -> 
                "Using deprecated transaction API can cause thread safety issues and transaction nesting."
        }
    }

    /**
     * Report all detected violations.
     */
    private fun reportViolations(violations: List<DeprecatedApiViolation>, file: PsiFile, holder: ProblemsHolder) {
        for (violation in violations) {
            val message = when (violation.apiType) {
                DeprecatedApiType.BEGIN_TRANSACTION -> 
                    "Deprecated beginTransaction API: ${violation.reason}"
                DeprecatedApiType.COMMIT -> 
                    "Deprecated commit API: ${violation.reason}"
                DeprecatedApiType.ROLLBACK -> 
                    "Deprecated rollback API: ${violation.reason}"
            }
            
            reportViolation(violation.operation, file, holder, message)
        }
    }

    /**
     * Fallback analysis for deprecated transaction APIs if main analysis fails.
     */
    private fun fallbackDeprecatedApiAnalysis(file: PsiFile, holder: ProblemsHolder) {
        reportViolations(findDeprecatedTransactionAPIs(removeComments(file.text)), file, holder)
    }

    /**
     * Deprecated API types for Rule5.
     */
    enum class DeprecatedApiType {
        BEGIN_TRANSACTION,  // rdbStore.beginTransaction()
        COMMIT,            // rdbStore.commit()
        ROLLBACK           // rdbStore.rollback()
    }

    /**
     * Deprecated API violation with enhanced type information.
     */
    data class DeprecatedApiViolation(
        val operation: ProhibitedOperation,
        val apiType: DeprecatedApiType,
        val reason: String
    )
}

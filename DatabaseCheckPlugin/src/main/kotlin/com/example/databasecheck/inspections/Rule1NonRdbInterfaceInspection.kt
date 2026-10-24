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

import com.example.databasecheck.utils.DatabaseConstants
import com.example.databasecheck.utils.DatabasePathMatcher
import com.example.databasecheck.utils.JavaScriptAnalyzer
import com.intellij.codeInspection.*
import com.intellij.psi.PsiFile

/**
 * Rule 1: Prohibit non-RDB file operations on database paths.
 * A shared lexical resolver follows visible path/descriptor assignments before each call.
 * This is bounded source analysis, not a full JavaScript type or control-flow analysis.
 */
class Rule1NonRdbInterfaceInspection : BaseDatabaseInspection() {

    override fun getRuleName(): String = "Prohibit Non-RDB Interface Usage"
    override fun getRuleNumber(): Int = 1
    override fun getDescription(): String =
        "Prohibits using non-RDB interfaces (fileIo, fopen, fcntl, etc.) to operate database files. " +
                "Use relationalStore.getRdbStore() for all database operations."

    override fun visitJavaScriptFile(file: PsiFile, holder: ProblemsHolder) {
        analyzeJavaScriptFile(file, holder)
    }

    private fun analyzeJavaScriptFile(file: PsiFile, holder: ProblemsHolder) {
        reportViolations(findPathViolations(removeComments(file.text)), file, holder)
    }

    private fun findPathViolations(code: String): List<DatabaseViolation> {
        val resolver = DatabasePathMatcher.Resolver(code)
        return JavaScriptAnalyzer.extractFunctionCallsFromText(code).filter { call ->
            isProhibitedFileOperation(call.functionName) &&
                (call.arguments.any { resolver.resolves(it, call.startOffset) } ||
                    call.objectName?.let { resolver.resolves(it, call.startOffset) } == true)
        }.map { call ->
            DatabaseViolation(createProhibitedOperation(call), ViolationType.TRACKED_VARIABLE,
                "Targets a database path or a file descriptor resolved at this call")
        }
    }

    /**
     * Report all detected violations.
     */
    private fun reportViolations(violations: List<DatabaseViolation>, file: PsiFile, holder: ProblemsHolder) {
        for (violation in violations) {
            val message = "Prohibited use of '${violation.operation.operationName}' on database path. " +
                    "Direct file operations bypass database integrity checks. " +
                    "Use RDB interfaces instead (relationalStore.getRdbStore()). " +
                    "Reason: ${violation.reason}"
            
            reportViolation(violation.operation, file, holder, message)
        }
    }

    /**
     * Check if a function name is a prohibited file operation.
     */
    private fun isProhibitedFileOperation(functionName: String): Boolean {
        return DatabaseConstants.PROHIBITED_FILE_OPERATIONS.any { op ->
            functionName == op || functionName.endsWith(".$op")
        }
    }

    /**
     * Convert JavaScriptAnalyzer.FunctionCall to ProhibitedOperation.
     */
    private fun createProhibitedOperation(call: JavaScriptAnalyzer.FunctionCall): ProhibitedOperation {
        return ProhibitedOperation(
            operationName = call.functionName,
            startPos = call.startOffset,
            endPos = call.endOffset,
            fullMatch = "${call.functionName}(${call.arguments.joinToString(", ")})",
            parameters = call.arguments.joinToString(", "),
            objectName = call.objectName
        )
    }

    /**
     * Data classes for enhanced violation tracking.
     */
    enum class ViolationType {
        DIRECT_DATABASE_PATH,
        TRACKED_VARIABLE
    }

    data class DatabaseViolation(
        val operation: ProhibitedOperation,
        val violationType: ViolationType,
        val reason: String
    )
}
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
import com.intellij.openapi.util.TextRange
import com.intellij.psi.PsiFile

/**
 * Rule 7: Database Directory and File Permission Validation
 *
 * This inspection detects violations of HarmonyOS Database Robustness Rule 7, which requires
 * that database directory and file permissions (DAC/ACL) are properly configured and inherited
 * to ensure read-write permissions for database files and newly created files.
 *
 * ## Rule Requirements:
 * Prohibit using chmod, fileIo.chmod, fileIo.chmodSync operations on database paths:
 * 1. context.databaseDir
 * 2. /data/storage/el1~el5/database
 * 3. /data/storage/el1~el5/database/<hap-name-xxx>
 * 4. /data/app/el1~el5/<userId>/database/<packagename-xxx>
 * 5. /data/app/el1~el5/<userId>/database/<packagename-xxx>/<hap-name-xxx>
 * 6. /data/service/el1~el4/public/database/<serviceability-xxx>
 * 7. /data/service/el1~el4/<userId>/database/<serviceability-xxx>
 *
 * ## Detected Violations:
 * - Using fileIo.chmod() on database paths
 * - Using fileIo.chmodSync() on database paths
 * - Using chmod() on database paths
 * - Any permission modification operations on HarmonyOS database storage paths
 *
 * ## Positive Example (Safe):
 * ```javascript
 * async function changeMod(context: ExtensionContext, path: string) {
 *     try {
 *         if (path.search('/database/') >= 0) {
 *             return; // Skip database paths
 *         }
 *         await fileIo.chmod(path, 0o771);
 *     } catch (err) {
 *         console.log(`failed, err: ${JSON.stringify(err)}`)
 *     }
 * }
 * ```
 *
 * ## Negative Example (Violation):
 * ```javascript
 * async function changeMod(context: ExtensionContext, path: string) {
 *     await fileIo.chmod(context.databaseDir + path, 0o771);
 *     await fileIo.chmod('/data/storage/el1/database/' + path, 0o771);
 * }
 * ```
 *
 * @author Database Robustness Checker
 * @since 1.0
 */
class Rule7DatabasePermissionInspection : BaseDatabaseInspection() {

    override fun getRuleName(): String = "Database Directory and File Permission Validation"
    override fun getRuleNumber(): Int = 7
    override fun getDescription(): String =
        "Prohibits chmod operations on database paths to maintain proper DAC/ACL permissions."

    override fun visitJavaScriptFile(file: PsiFile, holder: ProblemsHolder) {
        analyzeJavaScriptFile(file, holder)
    }


    /**
     * Main analysis logic for detecting chmod operations on database paths
     */
    private fun analyzeJavaScriptFile(file: PsiFile, holder: ProblemsHolder) {
        val originalText = file.text
        if (originalText.isBlank()) return

        try {
            // Step 0: Remove comments to avoid parsing interference
            val codeText = removeComments(originalText)
            
            // Find all permission operations in the file
            val permissionOperations = findPermissionOperations(codeText)
            
            // Check each permission operation against database paths
            val violations = detectPermissionViolations(permissionOperations, codeText)
            
            // Report all violations
            violations.forEach { violation ->
                reportViolation(violation, file, holder)
            }
            
        } catch (e: Exception) {
            // Fallback analysis if main analysis fails
            fallbackPermissionAnalysis(file, holder)
        }
    }

    /**
     * Find all permission change operations in the code
     */
    private fun findPermissionOperations(codeText: String): List<PermissionOperation> {
        return JavaScriptAnalyzer.extractFunctionCallsFromText(codeText).filter {
            it.functionName in DatabaseConstants.PERMISSION_OPERATIONS && it.arguments.isNotEmpty()
        }.map { PermissionOperation(it.functionName, it.arguments.first(), it.startOffset, it.endOffset) }
    }

    /**
     * Detect violations by checking if permission operations target database paths
     */
    private fun detectPermissionViolations(
        operations: List<PermissionOperation>,
        codeText: String
    ): List<PermissionViolation> {
        val violations = mutableListOf<PermissionViolation>()
        val resolver = DatabasePathMatcher.Resolver(codeText)

        for (operation in operations) {
            val isDatabasePath = resolver.resolves(operation.pathArgument, operation.startOffset)
            
            if (isDatabasePath) {
                violations.add(
                    PermissionViolation(
                        operation = operation,
                        reason = "Permission modification on database path '${operation.pathArgument}'. " +
                                "Database directory permissions should not be modified to maintain " +
                                "proper DAC/ACL inheritance and ensure read-write access.",
                        violationType = PermissionViolationType.DATABASE_PATH_CHMOD
                    )
                )
            }
        }
        
        return violations
    }

    /**
     * Report a permission violation
     */
    private fun reportViolation(violation: PermissionViolation, file: PsiFile, holder: ProblemsHolder) {
        val operation = violation.operation
        val textRange = TextRange(operation.startOffset, operation.endOffset)
        
        if (textRange.startOffset >= 0 && textRange.endOffset <= file.textLength && 
            textRange.startOffset < textRange.endOffset) {
            
            // Use the BaseDatabaseInspection's createProblemDescriptor method
            createProblemDescriptor(
                file, 
                violation.reason,
                holder,
                textRange
            )
        }
    }

    /**
     * Fallback analysis using simpler pattern matching
     */
    private fun fallbackPermissionAnalysis(file: PsiFile, holder: ProblemsHolder) {
        val code = removeComments(file.text)
        detectPermissionViolations(findPermissionOperations(code), code).forEach { reportViolation(it, file, holder) }
    }

    /**
     * Data class representing a permission operation found in the code
     */
    private data class PermissionOperation(
        val operation: String,
        val pathArgument: String,
        val startOffset: Int,
        val endOffset: Int
    )

    /**
     * Data class representing a permission violation
     */
    private data class PermissionViolation(
        val operation: PermissionOperation,
        val reason: String,
        val violationType: PermissionViolationType
    )

    /**
     * Enum for different types of permission violations
     */
    private enum class PermissionViolationType {
        DATABASE_PATH_CHMOD
    }
}
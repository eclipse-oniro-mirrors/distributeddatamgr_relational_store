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
import com.example.databasecheck.utils.JavaScriptAnalyzer
import com.example.databasecheck.utils.JavaScriptCode
import com.intellij.codeInspection.*
import com.intellij.openapi.util.TextRange
import com.intellij.psi.PsiFile

/**
 * Rule 14: Database Configuration Consistency
 *
 * This inspection detects violations of HarmonyOS Database Robustness Rule 14, which requires
 * that database configuration parameters remain consistent across all getRdbStore calls
 * for the same database name within a HAP package.
 *
 * ## Rule Requirements:
 * For the same database name, all getRdbStore calls must use identical configuration parameters:
 * - `name` - Database name (must be identical)
 * - `securityLevel` - Security level setting  
 * - `encrypt` - Encryption setting
 * - `isReadOnly` - Read-only mode setting
 * - `customDir` - Custom directory setting
 *
 * ## Detected Violations:
 * - Different securityLevel values for same database name
 * - Inconsistent encrypt settings (true/false or presence/absence)
 * - Varying isReadOnly settings for same database
 * - Different customDir values for same database name
 * - Mixed configuration approaches (object literals vs variables)
 *
 * ## Positive Example (Consistent):
 * ```javascript
 * const CONFIG = {
 *     "name": STORE_NAME,
 *     securityLevel: relationalStore.SecurityLevel.S3
 * }
 * 
 * async function CreateRdbStore(context) {
 *     return await relationalStore.getRdbStore(context, CONFIG);
 * }
 * 
 * async function GetActiveRdbStore(context) {
 *     return await relationalStore.getRdbStore(context, CONFIG);
 * }
 * ```
 *
 * ## Negative Example (Inconsistent):
 * ```javascript
 * async function CreateRdbStore(context) {
 *     const config = {
 *         "name": STORE_NAME,
 *         securityLevel: relationalStore.SecurityLevel.S3
 *     }
 *     return await relationalStore.getRdbStore(context, config);
 * }
 * 
 * async function GetActiveRdbStore(context) {
 *     const config = {
 *         "name": STORE_NAME,
 *         securityLevel: relationalStore.SecurityLevel.S3,
 *         encrypt: true,        // Added parameter
 *         isReadOnly: true      // Added parameter  
 *     }
 *     return await relationalStore.getRdbStore(context, config);
 * }
 * ```
 *
 * @author Database Robustness Checker
 * @since 1.0
 */
class Rule14DatabaseConfigConsistencyInspection : BaseDatabaseInspection() {

    override fun getRuleName(): String = "Database Configuration Consistency"
    override fun getRuleNumber(): Int = 14
    override fun getDescription(): String =
        "Ensures consistent database configuration parameters across all getRdbStore calls for the same database name."

    override fun visitJavaScriptFile(file: PsiFile, holder: ProblemsHolder) {
        analyzeJavaScriptFile(file, holder)
    }

    /**
     * Main analysis logic for detecting database configuration inconsistencies
     */
    private fun analyzeJavaScriptFile(file: PsiFile, holder: ProblemsHolder) {
        val originalText = file.text
        if (originalText.isBlank()) return

        try {
            // Step 0: Remove comments to avoid parsing interference
            val codeText = removeComments(originalText)
            
            // Find all getRdbStore calls and extract configurations
            val rdbStoreCalls = findGetRdbStoreCalls(codeText)
            
            // Group configurations by database name
            val configsByName = groupConfigurationsByDatabaseName(rdbStoreCalls)
            
            // Detect inconsistencies within each database name group
            val violations = detectConfigurationInconsistencies(configsByName)
            
            // Report all violations
            violations.forEach { violation ->
                reportViolation(violation, file, holder)
            }
            
        } catch (e: Exception) {
            // Fallback analysis if main analysis fails
            fallbackConfigAnalysis(file, holder)
        }
    }

    /**
     * Find all getRdbStore calls and extract their configurations
     */
    private fun findGetRdbStoreCalls(codeText: String): List<RdbStoreCall> {
        return JavaScriptAnalyzer.extractFunctionCallsFromText(codeText).filter {
            it.functionName.substringAfterLast('.') == "getRdbStore" && it.arguments.size >= 2
        }.mapNotNull { call ->
            val method = Regex("""\bgetRdbStore\s*\(""").find(codeText, call.startOffset)
                ?: return@mapNotNull null
            val config = extractConfiguration(call.arguments[1], codeText, method.range.first)
                ?: return@mapNotNull null
            RdbStoreCall(call.arguments[1], config, method.range.first, call.endOffset,
                codeText.substring(method.range.first, call.endOffset))
        }
    }

    /**
     * Extract configuration details from the parameter
     */
    private fun extractConfiguration(configParam: String, codeText: String, callPosition: Int): DatabaseConfiguration? {
        val trimmedParam = configParam.trim()
        
        // Method 1: Direct object literal (starts with {)
        if (trimmedParam.startsWith("{")) {
            return parseObjectLiteral(trimmedParam)
        }
        
        // Method 2: Variable reference (simple identifier)
        if (trimmedParam.matches(Regex("""[a-zA-Z_$][a-zA-Z0-9_$]*"""))) {
            return findVariableConfiguration(trimmedParam, codeText, callPosition)
        }
        
        return null
    }

    /**
     * Parse object literal configuration - extract all parameters
     */
    private fun parseObjectLiteral(objectLiteral: String): DatabaseConfiguration? {
        val config = DatabaseConfiguration()
        
        // Parse all key-value pairs in the object literal
        val parameterPattern = Regex("""\s*(?:"([^"]+)"|'([^']+)'|([a-zA-Z_$][a-zA-Z0-9_$]*))\s*:\s*([\s\S]+)""")
        
        JavaScriptCode(objectLiteral.trim()).arguments(0).forEach { field ->
            val match = parameterPattern.matchEntire(field) ?: return@forEach
            // Get the key name (could be in group 1, 2, or 3 depending on quote type)
            val key = when {
                match.groupValues[1].isNotEmpty() -> match.groupValues[1] // "key"
                match.groupValues[2].isNotEmpty() -> match.groupValues[2] // 'key'
                match.groupValues[3].isNotEmpty() -> match.groupValues[3] // key
                else -> return@forEach
            }
            
            val value = match.groupValues[4].trim()
            
            // Store in allParameters map
            config.allParameters[key] = value
            
            // Also store in specific fields for backward compatibility
            when (key) {
                "name" -> {
                    config.name = if (value.startsWith("\"") || value.startsWith("'")) {
                        unquote(value)
                    } else {
                        value // Keep variable names as-is (e.g., STORE_NAME)
                    }
                }
                "securityLevel" -> config.securityLevel = value
                "encrypt" -> config.encrypt = value
                "isReadOnly" -> config.isReadOnly = value
                "customDir" -> config.customDir = unquote(value)
            }
        }
        
        return if (config.name != null) config else null
    }

    /**
     * Find configuration from variable assignment - improved scope-aware search
     */
    private fun findVariableConfiguration(variableName: String, codeText: String, callPosition: Int): DatabaseConfiguration? {
        val code = JavaScriptCode(codeText)
        val pattern = Regex("""\b(const|let|var)\s+${Regex.escape(variableName)}(?:\s*:\s*[^=;\n]+)?\s*=\s*""")
        val declaration = pattern.findAll(codeText).filter {
            it.range.first < callPosition && declarationIsVisible(it, code, callPosition) &&
                code.tokens.any { token -> token.start == it.range.first && token.text in setOf("const", "let", "var") }
        }.maxByOrNull { it.range.first } ?: return null
        val start = code.tokens.indexOfFirst { it.start > declaration.range.last }
        if (code.text(start) != "{") return null
        val end = code.end(start)
        if (end < start) return null
        return parseObjectLiteral(codeText.substring(code.tokens[start].start, code.tokens[end].end))
    }

    private fun declarationIsVisible(declaration: MatchResult, code: JavaScriptCode, callPosition: Int): Boolean {
        if (declaration.groupValues[1] != "var") return code.isVisibleAt(declaration.range.first, callPosition)
        // Unlike const/let, var declarations remain visible outside an inner block.
        val owner = JavaScriptAnalyzer.extractFunctionScopes(code.source)
            .filter { declaration.range.first in it.bodyStartOffset until it.endOffset }
            .minByOrNull { it.endOffset - it.bodyStartOffset }
        return owner == null || callPosition in owner.bodyStartOffset until owner.endOffset
    }

    private fun unquote(value: String): String = when {
        value.startsWith("\"") && value.endsWith("\"") -> value.removeSurrounding("\"")
        value.startsWith("'") && value.endsWith("'") -> value.removeSurrounding("'")
        else -> value
    }

    /**
     * Group configurations by database name
     */
    private fun groupConfigurationsByDatabaseName(calls: List<RdbStoreCall>): Map<String, List<RdbStoreCall>> {
        return calls
            .filter { it.configuration?.name != null }
            .groupBy { it.configuration!!.name!! }
    }

    /**
     * Detect configuration inconsistencies within each database name group
     */
    private fun detectConfigurationInconsistencies(
        configsByName: Map<String, List<RdbStoreCall>>
    ): List<ConfigurationViolation> {
        val violations = mutableListOf<ConfigurationViolation>()
        
        for ((databaseName, calls) in configsByName) {
            if (calls.size <= 1) continue // No inconsistency possible with single call
            
            // Compare all configurations for this database name
            val referenceConfig = calls[0].configuration!!
            
            for (i in 1 until calls.size) {
                val currentCall = calls[i]
                val currentConfig = currentCall.configuration!!
                
                val inconsistencies = findInconsistencies(referenceConfig, currentConfig)
                
                if (inconsistencies.isNotEmpty()) {
                    violations.add(
                        ConfigurationViolation(
                            databaseName = databaseName,
                            inconsistentCall = currentCall,
                            referenceCall = calls[0],
                            inconsistentParameters = inconsistencies,
                            reason = "Database configuration inconsistency detected for '$databaseName'. " +
                                    "Parameters ${inconsistencies.joinToString(", ")} differ from previous configuration. " +
                                    "All getRdbStore calls for the same database name must use identical configuration parameters."
                        )
                    )
                }
            }
        }
        
        return violations
    }

    /**
     * Find inconsistencies between two configurations by comparing all parameters
     */
    private fun findInconsistencies(reference: DatabaseConfiguration, current: DatabaseConfiguration): List<String> {
        val inconsistencies = mutableListOf<String>()
        
        // Get all parameter names from both configurations
        val allParameters = getAllParameterNames(reference, current)
        
        for (paramName in allParameters) {
            val refValue = getParameterValue(reference, paramName)
            val curValue = getParameterValue(current, paramName)
            
            if (!areParametersEqual(refValue, curValue)) {
                inconsistencies.add(paramName)
            }
        }
        
        return inconsistencies
    }
    
    /**
     * Get all parameter names present in either configuration
     */
    private fun getAllParameterNames(config1: DatabaseConfiguration, config2: DatabaseConfiguration): Set<String> {
        val allParams = mutableSetOf<String>()
        
        // Add parameters from first configuration
        addParametersFromConfig(allParams, config1)
        
        // Add parameters from second configuration  
        addParametersFromConfig(allParams, config2)
        
        // Remove 'name' as it should always be the same for grouped configurations
        allParams.remove("name")
        
        return allParams
    }
    
    /**
     * Add parameter names from a configuration to the set
     */
    private fun addParametersFromConfig(paramSet: MutableSet<String>, config: DatabaseConfiguration) {
        // Add all parameters from the allParameters map
        paramSet.addAll(config.allParameters.keys)
    }
    
    /**
     * Get parameter value from configuration by name
     */
    private fun getParameterValue(config: DatabaseConfiguration, paramName: String): String? {
        // First check the allParameters map, then fall back to specific fields
        return config.allParameters[paramName] ?: when (paramName) {
            "securityLevel" -> config.securityLevel
            "encrypt" -> config.encrypt
            "isReadOnly" -> config.isReadOnly
            "customDir" -> config.customDir
            else -> null
        }
    }

    /**
     * Compare two parameter values, treating null and missing values as different from present values
     */
    private fun areParametersEqual(reference: String?, current: String?): Boolean {
        // Both null/missing - consistent
        if (reference == null && current == null) return true
        
        // One is null, other is not - inconsistent
        if (reference == null || current == null) return false
        
        // Both have values - compare them
        return reference == current
    }

    /**
     * Report a configuration violation
     */
    private fun reportViolation(violation: ConfigurationViolation, file: PsiFile, holder: ProblemsHolder) {
        val call = violation.inconsistentCall
        // Comment removal preserves offsets, so no approximate occurrence mapping is needed.
        if (call.startOffset >= 0 && call.endOffset <= file.textLength && call.startOffset < call.endOffset) {
            createProblemDescriptor(file, violation.reason, holder, TextRange(call.startOffset, call.endOffset))
        }
    }

    /**
     * Fallback analysis using simpler pattern matching
     */
    private fun fallbackConfigAnalysis(file: PsiFile, holder: ProblemsHolder) {
        val codeText = file.text
        
        // Simple pattern to detect potential configuration inconsistencies
        val getRdbStorePattern = Regex("""getRdbStore\s*\([^)]+\)""")
        val matches = getRdbStorePattern.findAll(codeText).toList()
        
        if (matches.size > 1) {
            // If multiple getRdbStore calls found, suggest checking consistency
            matches.forEach { match ->
                val textRange = TextRange(match.range.first, match.range.last + 1)
                if (textRange.startOffset >= 0 && textRange.endOffset <= file.textLength && 
                    textRange.startOffset < textRange.endOffset) {
                    
                    createProblemDescriptor(
                        file,
                        "Multiple getRdbStore calls detected. " +
                        "Ensure that all calls for the same database name use consistent configuration parameters " +
                        "(securityLevel, encrypt, isReadOnly, customDir).",
                        holder,
                        textRange
                    )
                }
            }
        }
    }

    /**
     * Data class representing a getRdbStore call
     */
    private data class RdbStoreCall(
        val configParameter: String,
        val configuration: DatabaseConfiguration?,
        val startOffset: Int,
        val endOffset: Int,
        val fullMatch: String
    )

    /**
     * Data class representing database configuration
     */
    private data class DatabaseConfiguration(
        var name: String? = null,
        var securityLevel: String? = null,
        var encrypt: String? = null,
        var isReadOnly: String? = null,
        var customDir: String? = null,
        // Store all parameters including unknown ones
        val allParameters: MutableMap<String, String> = mutableMapOf()
    )

    /**
     * Data class representing a configuration violation
     */
    private data class ConfigurationViolation(
        val databaseName: String,
        val inconsistentCall: RdbStoreCall,
        val referenceCall: RdbStoreCall,
        val inconsistentParameters: List<String>,
        val reason: String
    )
}

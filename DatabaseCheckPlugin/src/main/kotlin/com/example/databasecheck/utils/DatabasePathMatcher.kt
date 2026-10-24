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
 
package com.example.databasecheck.utils

import com.intellij.psi.*
import com.intellij.psi.util.PsiTreeUtil

/**
 * Utility class for matching database paths in JavaScript code
 */
object DatabasePathMatcher {
    
    /** Resolve local path/descriptor aliases at the position where an operation is executed. */
    internal class Resolver(source: String) {
        private val code = JavaScriptCode(source)
        private val assignments = JavaScriptAnalyzer.extractVariableAssignmentsFromText(source)
        private val scopes = JavaScriptAnalyzer.extractFunctionScopes(source)
        private val identifier = Regex("[a-zA-Z_$][a-zA-Z0-9_$]*")
        private val parameters = scopes.associateWith { JavaScriptAnalyzer.parameterNames(code, it).toSet() }

        fun resolves(expression: String, position: Int): Boolean = resolve(expression, position, emptySet())

        private fun resolve(expression: String, position: Int, visited: Set<Int>): Boolean {
            if (visited.size >= 64) return false
            val view = JavaScriptCode(expression)
            if (view.text(0) in setOf("{", "[")) return false
            val calls = JavaScriptAnalyzer.extractFunctionCallsFromText(expression)
            if (calls.isNotEmpty() && calls.first().functionName.substringAfterLast('.') !in
                setOf("open", "openSync", "fopen", "join")) return false
            if (view.tokens.any { it.isString && DatabaseConstants.DATABASE_PATHS.drop(1)
                    .any { pattern -> pattern.containsMatchIn(it.text) } }) return true
            if (view.tokens.indices.any { !view.tokens[it].isString && view.text(it) == "databaseDir" &&
                    view.text(it - 1) in setOf(".", "?.") }) return true
            val references = view.tokens.indices.filter {
                !view.tokens[it].isString && view.text(it).matches(identifier) &&
                    view.text(it - 1) !in setOf(".", "?.") && view.text(it + 1) != "("
            }.map { view.text(it) }.toSet()
            view.tokens.filter { it.text.startsWith('`') }.forEach { token ->
                Regex("""\$\{\s*([a-zA-Z_$][a-zA-Z0-9_$]*(?:\.[a-zA-Z_$][a-zA-Z0-9_$]*)*)\s*}""")
                    .findAll(token.text).forEach { match ->
                        if (resolve(match.groupValues[1], position, visited)) return true
                    }
            }
            return references.any { variable ->
                val minimum = shadowingOffset(variable, position)
                val binding = assignments.filter {
                    it.variableName == variable && it.startOffset < position && it.startOffset >= minimum &&
                        isVisible(it.startOffset, position)
                }.maxByOrNull { it.startOffset }
                binding != null && binding.startOffset !in visited &&
                    resolve(binding.assignmentExpression, binding.startOffset, visited + binding.startOffset)
            }
        }

        private fun isVisible(declaration: Int, position: Int): Boolean {
            if (code.tokens.none { it.start == declaration && it.text == "var" }) {
                return code.isVisibleAt(declaration, position)
            }
            val owner = scopes.filter { declaration in it.bodyStartOffset until it.endOffset }
                .minByOrNull { it.endOffset - it.bodyStartOffset }
            return owner == null || position in owner.bodyStartOffset until owner.endOffset
        }

        private fun shadowingOffset(name: String, position: Int): Int {
            val parameter = scopes.filter { position in it.bodyStartOffset until it.endOffset &&
                name in parameters.getValue(it) }.maxOfOrNull { it.bodyStartOffset } ?: 0
            return code.tokens.indices.filter {
                code.text(it) in setOf("const", "let", "var") && code.text(it + 1) == name &&
                    code.tokens[it].start < position && isVisible(code.tokens[it].start, position)
            }.maxOfOrNull { code.tokens[it].start }?.coerceAtLeast(parameter) ?: parameter
        }
    }

    /**
     * Checks if a string literal represents a database path
     */
    fun isDatabasePath(pathString: String): Boolean {
        return DatabaseConstants.DATABASE_PATHS.any { pattern ->
            pattern.matches(pathString)
        }
    }
    
    /**
     * Checks if a PSI element contains a database path
     * Handles both string literals and concatenated expressions
     */
    fun containsDatabasePath(element: PsiElement): Boolean {
        return when {
            // Direct string literal
            isStringLiteral(element) -> {
                val stringValue = getStringValue(element)
                stringValue != null && isDatabasePath(stringValue)
            }
            
            // String concatenation or template
            isStringConcatenation(element) -> {
                hasStringConcatenationWithDatabasePath(element)
            }
            
            // Property access (context.databaseDir)
            isPropertyAccess(element) -> {
                isDatabaseDirAccess(element)
            }
            
            else -> false
        }
    }
    
    /**
     * Extracts all database paths from a PSI element
     */
    fun extractDatabasePaths(element: PsiElement): List<String> {
        val paths = mutableListOf<String>()
        
        // Traverse the element tree to find all path strings
        element.accept(object : PsiRecursiveElementVisitor() {
            override fun visitElement(element: PsiElement) {
                if (containsDatabasePath(element)) {
                    getStringValue(element)?.let { paths.add(it) }
                }
                super.visitElement(element)
            }
        })
        
        return paths
    }
    
    private fun isStringLiteral(element: PsiElement): Boolean {
        // For JavaScript: check if it's a string literal
        return element.text.startsWith("'") || element.text.startsWith("\"") || element.text.startsWith("`")
    }
    
    private fun getStringValue(element: PsiElement): String? {
        val text = element.text
        return when {
            text.startsWith("'") && text.endsWith("'") -> text.substring(1, text.length - 1)
            text.startsWith("\"") && text.endsWith("\"") -> text.substring(1, text.length - 1)
            text.startsWith("`") && text.endsWith("`") -> text.substring(1, text.length - 1)
            else -> null
        }
    }
    
    private fun isStringConcatenation(element: PsiElement): Boolean {
        // Look for binary expressions with + operator
        return element.text.contains("+") && 
               (element.text.contains("'") || element.text.contains("\"") || element.text.contains("`"))
    }
    
    private fun hasStringConcatenationWithDatabasePath(element: PsiElement): Boolean {
        val text = element.text
        
        // Check for common database path patterns in concatenations
        return text.contains("/data/storage/el") ||
               text.contains("/database/") ||
               text.contains("context.databaseDir") ||
               text.contains("context.currentHapModuleInfo.name")
    }
    
    private fun isPropertyAccess(element: PsiElement): Boolean {
        return element.text.contains(".")
    }
    
    private fun isDatabaseDirAccess(element: PsiElement): Boolean {
        return element.text.contains("context.databaseDir")
    }
}
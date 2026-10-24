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

import com.intellij.psi.PsiFile

/**
 * JavaScript code analysis utilities using string-based parsing.
 * 
 * This class combines lexical delimiter matching with small declaration patterns
 * to avoid PSI dependency issues while still providing structured analysis.
 * Supports function-scoped analysis to prevent cross-function variable contamination.
 */
object JavaScriptAnalyzer {
    private val IDENTIFIER = Regex("[a-zA-Z_$][a-zA-Z0-9_$]*")
    
    /**
     * Data class representing a variable assignment in JavaScript code.
     */
    data class VariableAssignment(
        val variableName: String,
        val assignmentExpression: String,
        val startOffset: Int,
        val endOffset: Int,
        val functionScope: String? = null // The function this assignment belongs to
    )
    
    /**
     * Data class representing a function call in JavaScript code.
     */
    data class FunctionCall(
        val functionName: String,
        val arguments: List<String>,
        val startOffset: Int,
        val endOffset: Int,
        val objectName: String? = null, // For method calls like obj.method()
        val functionScope: String? = null // The function this call belongs to
    )
    
    /**
     * Data class representing a JavaScript function scope.
     */
    data class FunctionScope(
        val functionName: String,
        val startOffset: Int,      // Function declaration start
        val bodyStartOffset: Int,  // Function body start (after {)
        val endOffset: Int,        // Function end
        val body: String
    )
    
    /**
     * Extract all variable assignments from JavaScript code using lexical delimiter matching.
     * 
     * @param file The JavaScript file to analyze
     * @return List of variable assignments found in the file
     */
    fun extractVariableAssignments(file: PsiFile): List<VariableAssignment> {
        return extractVariableAssignmentsFromText(file.text)
    }

    /**
     * Extract all variable assignments from JavaScript code text using lexical delimiter matching.
     * 
     * @param code The JavaScript code text to analyze
     * @return List of variable assignments found in the code
     */
    fun extractVariableAssignmentsFromText(code: String): List<VariableAssignment> {
        val view = JavaScriptCode(code)
        val declarations = Regex("""\b(?:const|let|var)\s+([a-zA-Z_$][a-zA-Z0-9_$]*)(?:\s*:\s*[^=;\n]+)?\s*=\s*""")
        val assignments = declarations.findAll(code).mapNotNull {
            assignment(it, it.groupValues[1], view)
        }.toMutableList()
        val reassignments = Regex("""(?<![a-zA-Z0-9_$.])([a-zA-Z_$][a-zA-Z0-9_$]*)\s*=\s*""")
        reassignments.findAll(code).forEach { match ->
            if (assignments.none { match.range.first in it.startOffset until it.endOffset }) {
                assignment(match, match.groupValues[1], view)?.let { assignments.add(it) }
            }
        }
        return assignments.sortedBy { it.startOffset }
    }

    private fun assignment(match: MatchResult, name: String, code: JavaScriptCode): VariableAssignment? {
        if (code.tokens.none { it.start == match.range.first }) return null
        val start = code.tokens.indexOfFirst { it.start > match.range.last }
        if (start < 0 || code.text(start - 1) != "=") return null
        val end = code.expressionEnd(start)
        if (end < start) return null
        return VariableAssignment(name,
            code.source.substring(code.tokens[start].start, code.tokens[end].end),
            match.range.first, code.tokens[end].end)
    }

    /**
     * Extract all function calls from JavaScript code using lexical delimiter matching.
     * 
     * @param file The JavaScript file to analyze
     * @return List of function calls found in the file
     */
    fun extractFunctionCalls(file: PsiFile): List<FunctionCall> {
        return extractFunctionCallsFromText(file.text)
    }

    /**
     * Extract all function calls from JavaScript code text using lexical delimiter matching.
     * 
     * @param code The JavaScript code text to analyze
     * @return List of function calls found in the code
     */
    fun extractFunctionCallsFromText(code: String): List<FunctionCall> {
        val view = JavaScriptCode(code)
        return view.tokens.indices.mapNotNull { callAt(view, it) }
    }

    private fun callAt(code: JavaScriptCode, index: Int): FunctionCall? {
        val name = code.text(index)
        if (!name.matches(IDENTIFIER) ||
            name in setOf("if", "while", "for", "switch", "catch", "with", "function") ||
            code.text(index + 1) != "(" || code.text(index - 1) in setOf("function", "new")) return null
        val end = code.end(index + 1)
        if (end < 0) return null
        var start = index
        while (start >= 2 && code.text(start - 1) in setOf(".", "?.") &&
            code.text(start - 2).matches(IDENTIFIER)) start -= 2
        if (start == index && code.text(end + 1) in setOf("{", ":", "=>")) return null
        val receiver = if (start == index) null else code.joined(start until index - 1).replace("?.", ".")
        val fullName = if (receiver == null) name else "$receiver.$name"
        return FunctionCall(fullName, code.arguments(index + 1), code.tokens[start].start,
            code.tokens[end].end, receiver)
    }

    /**
     * Check if a function call matches any of the prohibited operations.
     * 
     * @param call The function call to check
     * @param prohibitedOperations Set of prohibited operation names
     * @return true if the call matches a prohibited operation
     */
    fun isProhibitedOperation(call: FunctionCall, prohibitedOperations: Set<String>): Boolean {
        return prohibitedOperations.contains(call.functionName) ||
               prohibitedOperations.any { op -> call.functionName.endsWith(".$op") }
    }

    fun transactionOperations(
        creation: FunctionCall,
        variable: String,
        calls: List<FunctionCall>,
        assignments: List<VariableAssignment>
    ): List<FunctionCall> {
        val nextBinding = assignments.filter {
            it.variableName == variable && it.startOffset > creation.startOffset
        }.minOfOrNull { it.startOffset } ?: Int.MAX_VALUE
        return calls.filter {
            it.objectName == variable && it.startOffset >= creation.endOffset && it.startOffset < nextBinding
        }
    }
    
    /**
     * Check if an expression contains database path patterns.
     * Enhanced to handle variable references and string concatenation.
     * 
     * @param expression The expression to check
     * @param databasePathPatterns List of regex patterns for database paths
     * @param variableAssignments Optional list of variable assignments for reference resolution
     * @return true if the expression contains database path patterns
     */
    fun containsDatabasePath(
        expression: String, 
        databasePathPatterns: List<Regex>,
        variableAssignments: List<VariableAssignment> = emptyList(),
        visited: Set<String> = emptySet()
    ): Boolean {
        if (visited.size >= 64) return false
        // Direct check: expression contains database path literals
        if (databasePathPatterns.any { pattern -> pattern.containsMatchIn(expression) }) {
            return true
        }
        
        // Variable reference check: resolve variables and check their values
        val variableReferences = extractVariableReferences(expression)
        for (variable in variableReferences) {
            if (variable in visited) continue
            // Find the variable assignment
            val assignment = variableAssignments.find { it.variableName == variable }
            if (assignment != null) {
                // Recursively check the assignment expression
                if (containsDatabasePath(assignment.assignmentExpression, databasePathPatterns, variableAssignments, visited + variable)) {
                    return true
                }
            }
        }
        
        return false
    }
    
    /**
     * Extract variable references from an expression string.
     * 
     * @param expression The expression to analyze
     * @return Set of variable names referenced in the expression
     */
    fun extractVariableReferences(expression: String): Set<String> {
        val variablePattern = Regex("""\b([a-zA-Z_$][a-zA-Z0-9_$]*)\b""")
        return variablePattern.findAll(expression).map { it.value }.toSet()
    }
    
    /**
     * Extract all function scopes from JavaScript code.
     * 
     * @param code The JavaScript code to analyze
     * @return List of function scopes found in the code
     */
    fun extractFunctionScopes(code: String): List<FunctionScope> {
        val view = JavaScriptCode(code)
        val scopes = mutableListOf<FunctionScope>()
        view.tokens.indices.forEach { index ->
            when {
                view.text(index) == "function" -> {
                    val named = view.text(index + 1) != "("
                    val name = if (named) view.text(index + 1) else "<anonymous>"
                    val parameters = index + if (named) 2 else 1
                    val start = if (view.text(index - 1) == "async") index - 1 else index
                    functionScope(view, parameters, start, name)?.let { scopes.add(it) }
                }
                view.text(index) == "=>" -> arrowScope(view, index)?.let { scopes.add(it) }
                view.text(index + 1) == "(" && view.text(index - 1) !in setOf("function", ".", "?.", "new") &&
                    view.text(index).matches(IDENTIFIER) &&
                    view.text(index) !in setOf("if", "while", "for", "switch", "catch", "with") -> {
                    val start = if (view.text(index - 1) == "async") index - 1 else index
                    functionScope(view, index + 1, start, view.text(index))?.let { scopes.add(it) }
                }
            }
        }
        return scopes.distinctBy { it.bodyStartOffset to it.endOffset }
    }

    private fun functionScope(code: JavaScriptCode, parameters: Int, start: Int, name: String): FunctionScope? {
        if (code.text(parameters) != "(") return null
        var body = code.end(parameters) + 1
        if (body <= parameters) return null
        if (code.text(body) == ":") {
            while (body < code.tokens.size && code.text(body) !in setOf("{", ";", "=")) body++
        }
        val end = code.end(body)
        if (code.text(body) != "{" || end < body) return null
        return FunctionScope(name, code.tokens[start].start, code.tokens[body].end,
            code.tokens[end].end, code.source.substring(code.tokens[body].end, code.tokens[end].start))
    }

    private fun arrowScope(code: JavaScriptCode, arrow: Int): FunctionScope? {
        val start = arrow + 1
        if (start >= code.tokens.size) return null
        val block = code.text(start) == "{"
        val end = if (block) code.end(start) else code.expressionEnd(start)
        if (end < start) return null
        val bodyStart = if (block) code.tokens[start].end else code.tokens[start].start
        val bodyEnd = if (block) code.tokens[end].start else code.tokens[end].end
        return FunctionScope("<arrow>", code.tokens[arrow].start, bodyStart,
            code.tokens[end].end, code.source.substring(bodyStart, bodyEnd))
    }

    internal fun parameterNames(code: JavaScriptCode, scope: FunctionScope): List<String> {
        val start = code.tokens.indexOfFirst { it.start == scope.startOffset }
        if (start < 0) return emptyList()
        val open = if (code.text(start) == "=>") {
            if (code.text(start - 1) != ")") return listOf(code.text(start - 1))
            code.tokens.indices.firstOrNull { code.end(it) == start - 1 } ?: return emptyList()
        } else code.tokens.indices.firstOrNull {
            it >= start && code.tokens[it].start < scope.bodyStartOffset && code.text(it) == "("
        } ?: return emptyList()
        return code.arguments(open).mapNotNull {
            Regex("""^\s*(?:\.\.\.)?([a-zA-Z_$][a-zA-Z0-9_$]*)""").find(it)?.groupValues?.get(1)
        }
    }

    /**
     * Extract function calls and assignments with function scope information.
     * 
     * @param code The JavaScript code to analyze
     * @return Pair of (function calls with scope, variable assignments with scope)
     */
    fun extractWithFunctionScopes(code: String): Pair<List<FunctionCall>, List<VariableAssignment>> {
        val scopes = extractFunctionScopes(code)
        fun scopeAt(start: Int, end: Int): String? = scopes.filter {
            start >= it.bodyStartOffset && end <= it.endOffset
        }.minByOrNull { it.endOffset - it.bodyStartOffset }?.let { "${it.functionName}@${it.startOffset}" }
        val calls = extractFunctionCallsFromText(code).map {
            it.copy(functionScope = scopeAt(it.startOffset, it.endOffset))
        }
        val assignments = extractVariableAssignmentsFromText(code).map {
            it.copy(functionScope = scopeAt(it.startOffset, it.endOffset))
        }
        return calls to assignments
    }
}

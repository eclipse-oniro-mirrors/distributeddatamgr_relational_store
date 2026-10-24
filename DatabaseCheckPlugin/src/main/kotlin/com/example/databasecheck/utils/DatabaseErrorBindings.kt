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

/** Resolve catch aliases at their use/assignment position, including helper parameter shadowing. */
internal class DatabaseErrorBindings private constructor(
    private val parsed: Parsed,
    private val root: Root,
    private val parameters: Map<Int, Map<String, Boolean>> = emptyMap()
) {
    private data class Root(val name: String, val position: Int)
    private data class Parsed(
        val code: JavaScriptCode,
        val scopes: List<JavaScriptAnalyzer.FunctionScope>,
        val assignments: List<JavaScriptAnalyzer.VariableAssignment>
    )

    constructor(code: JavaScriptCode, scopes: List<JavaScriptAnalyzer.FunctionScope>, name: String, position: Int) :
        this(Parsed(code, scopes, JavaScriptAnalyzer.extractVariableAssignmentsFromText(code.source)),
            Root(name, position))

    fun namesAt(position: Int): Set<String> = (parsed.assignments.map { it.variableName } + root.name +
        parameters.values.flatMap { it.keys }).filter { resolves(it, position, emptySet()) }.toSet()

    fun forHelper(
        scope: JavaScriptAnalyzer.FunctionScope,
        arguments: List<String>,
        position: Int
    ): DatabaseErrorBindings {
        val values = JavaScriptAnalyzer.parameterNames(parsed.code, scope).mapIndexed { index, name ->
            val value = reference(arguments.getOrNull(index).orEmpty())
            name to (value?.let { resolves(it, position, emptySet()) } == true)
        }.toMap()
        return DatabaseErrorBindings(parsed, root, parameters + (scope.bodyStartOffset to values))
    }

    private fun resolves(name: String, position: Int, visited: Set<Int>): Boolean {
        if (visited.size >= 64) return false
        val code = parsed.code
        val parameter = parsed.scopes.filter {
            position in it.bodyStartOffset until it.endOffset &&
                name in JavaScriptAnalyzer.parameterNames(code, it) &&
                (name != root.name || it.bodyStartOffset > root.position)
        }.maxByOrNull { it.bodyStartOffset }
        val declaration = code.tokens.indices.filter {
            code.text(it) in setOf("const", "let", "var") && code.text(it + 1) == name &&
                code.tokens[it].start < position && code.isVisibleAt(code.tokens[it].start, position)
        }.maxOfOrNull { code.tokens[it].start } ?: 0
        val minimum = maxOf(declaration, parameter?.bodyStartOffset ?: 0,
            if (name == root.name) root.position else 0)
        val binding = parsed.assignments.filter {
            it.variableName == name && it.startOffset < position && it.startOffset >= minimum &&
                code.isVisibleAt(it.startOffset, position)
        }.maxByOrNull { it.startOffset }
        if (binding != null) {
            if (binding.startOffset in visited) return false
            val target = reference(binding.assignmentExpression) ?: return false
            return resolves(target, binding.startOffset, visited + binding.startOffset)
        }
        if (declaration >= (parameter?.bodyStartOffset ?: 0) && declaration > root.position) return false
        if (parameter != null) return parameters[parameter.bodyStartOffset]?.get(name) == true
        return name == root.name && position >= root.position && code.isVisibleAt(root.position, position)
    }

    private fun reference(expression: String): String? {
        val code = JavaScriptCode(expression)
        var start = 0
        var end = code.tokens.lastIndex
        while (code.text(start) == "(" && code.end(start) == end) { start++; end-- }
        val name = code.text(start)
        if (!name.matches(IDENTIFIER)) return null
        if (start == end) return name
        if (code.text(start + 1) != "as" || start + 2 > end) return null
        return name.takeIf { ((start + 2)..end).all { code.text(it) == "." || code.text(it).matches(IDENTIFIER) } }
    }

    companion object {
        private val IDENTIFIER = Regex("[a-zA-Z_$][a-zA-Z0-9_$]*")
    }
}

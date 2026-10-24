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

/** Syntax-based checks, deliberately independent of the IDE so regressions can run in JUnit. */
internal class DatabaseErrorHandlingAnalyzer(source: String) {
    data class Violation(val start: Int, val end: Int, val reason: String)
    private data class Handler(val start: Int, val body: IntRange, val error: String, val operations: Set<String>)
    private data class Branch(val codes: Set<Int>, val body: IntRange, val start: Int)
    private data class ActionContext(val error: Int, val bindings: DatabaseErrorBindings)
    private val code = JavaScriptCode(source)
    private val scopes = JavaScriptAnalyzer.extractFunctionScopes(source)

    fun analyze(): List<Violation> = code.tokens.indices
        .filter { code.text(it) == "try" }
        .mapNotNull { handler(it) }
        .flatMap { checkHandler(it) }

    private fun handler(start: Int): Handler? {
        if (code.text(start + 1) != "{") return null
        val tryEnd = code.end(start + 1)
        val catch = tryEnd + 1
        if (tryEnd < 0 || code.text(catch) != "catch") return null
        val parameters = catch + 1
        val body = if (code.text(parameters) == "(") code.end(parameters) + 1 else parameters
        if (code.text(body) != "{" || code.end(body) < 0) return null
        val operations = calls((start + 2) until tryEnd).filter { it in OPERATIONS }.toSet()
        if (operations.isEmpty()) return null
        val error = if (body != parameters) code.text(parameters + 1) else ""
        val enclosing = scopes.filter {
            code.tokens[start].start in it.startOffset until it.endOffset
        }.minByOrNull { it.endOffset - it.startOffset }?.functionName
        return Handler(start, (body + 1) until code.end(body), error, operations + listOfNotNull(enclosing))
    }

    private fun checkHandler(handler: Handler): List<Violation> {
        val bindings = DatabaseErrorBindings(code, scopes, handler.error, code.tokens[handler.body.first - 1].end)
        val branches = branches(handler.body, bindings).filter { it.codes.isNotEmpty() }
        if (branches.isEmpty()) {
            return listOf(violation(handler, "No verifiable database error-code handling. " +
                "Handle busy/locked (14800024/14800025) with retry and delay, " +
                "and corrupt (14800011) with recovery; logging or rethrowing alone is insufficient."))
        }
        return branches.flatMap { branch ->
            branch.codes.mapNotNull { error ->
                val actions = calls(branch.body, roots = handler.operations, context = ActionContext(error, bindings))
                val missing = missingActions(error, actions, handler.operations)
                if (missing.isEmpty()) null else violation(handler,
                    "Database error $error: missing verifiable ${missing.joinToString(" and ")}. " +
                    "Check this error-code branch; logging or rethrowing alone is insufficient.")
            }
        }
    }

    private fun violation(handler: Handler, reason: String): Violation = Violation(
        code.tokens[handler.start].start,
        code.tokens[handler.body.last + 1].end,
        reason
    )

    private fun branches(range: IntRange, bindings: DatabaseErrorBindings): List<Branch> {
        val result = mutableListOf<Branch>()
        var index = range.first
        while (index <= range.last) {
            if (code.text(index) in setOf("function", "=>")) {
                index = skipFunction(index, range.last)
                continue
            }
            if (code.text(index) == "if") ifBranch(index, bindings)?.let { result.add(it) }
            if (code.text(index) == "switch") result.addAll(switchBranches(index, bindings))
            index++
        }
        return result.filter { it.body.last <= range.last }
    }

    private fun ifBranch(index: Int, bindings: DatabaseErrorBindings): Branch? {
        if (code.text(index + 1) != "(") return null
        val end = code.end(index + 1)
        if (end < 0) return null
        val condition = ((index + 2) until end).joinToString("") {
            if (code.tokens[it].isLiteral) "<literal>" else code.text(it)
        }
        // Negative guards do not establish a positive error-code path.
        if (condition.contains("!(") || condition.contains("=>")) return null
        val property = "(?<![a-zA-Z0-9_$])([a-zA-Z_$][a-zA-Z0-9_$]*)(?:\\?\\.|\\.)code"
        val equality = Regex("(?:$property==(?:=)?([0-9]+)|([0-9]+)==(?:=)?$property)(?![0-9])")
        val matches = equality.findAll(condition).mapNotNull {
            val name = it.groupValues[1].ifEmpty { it.groupValues[4] }
            val number = it.groupValues[2].ifEmpty { it.groupValues[3] }.toIntOrNull()
            number?.takeIf { it in DatabaseConstants.DATABASE_ERROR_CODES }?.let { name to it }
        }.toList()
        if (matches.isEmpty()) return null
        val names = bindings.namesAt(code.tokens[index].start)
        val codes = matches.filter { it.first in names }.map { it.second }.toSet()
        val start = end + 1
        val bodyEnd = code.statementEnd(start)
        if (bodyEnd < start) return null
        val body = if (code.text(start) == "{") (start + 1) until bodyEnd else start..bodyEnd
        return Branch(codes, body, index)
    }

    private fun switchBranches(index: Int, bindings: DatabaseErrorBindings): List<Branch> {
        if (code.text(index + 1) != "(") return emptyList()
        val conditionEnd = code.end(index + 1)
        val property = code.joined((index + 2) until conditionEnd)
        if (!Regex("[a-zA-Z_$][a-zA-Z0-9_$]*(?:\\?\\.|\\.)code").matches(property)) return emptyList()
        val bound = bindings.namesAt(code.tokens[index].start).any { property == "$it.code" || property == "$it?.code" }
        val body = conditionEnd + 1
        val end = code.end(body)
        if (code.text(body) != "{" || end < 0) return emptyList()
        val labels = mutableListOf<Int>()
        var cursor = body + 1
        while (cursor < end) {
            if (code.text(cursor) in setOf("case", "default")) labels.add(cursor)
            val close = code.end(cursor)
            cursor = if (close >= cursor) close + 1 else cursor + 1
        }
        return labels.mapIndexedNotNull { position, label ->
            if (code.text(label) == "default")
                Branch(emptySet(), (label + 2) until (labels.getOrNull(position + 1) ?: end), label)
            else switchBranch(labels, position, label, end)?.let { if (bound) it else it.copy(codes = emptySet()) }
        }
    }

    private fun switchBranch(labels: List<Int>, position: Int, label: Int, end: Int): Branch? {
        val error = code.text(label + 1).toIntOrNull() ?: return null
        if (error !in DatabaseConstants.DATABASE_ERROR_CODES || code.text(label + 2) != ":") return null
        var next = position + 1
        // Empty labels share the following case body, but never a default body.
        while (next < labels.size && labels[next] == labels[next - 1] + 3 &&
            code.text(labels[next]) == "case") next++
        val bodyEnd = labels.getOrNull(next) ?: end
        return Branch(setOf(error), (label + 3) until bodyEnd, label)
    }

    private fun calls(
        range: IntRange,
        roots: Set<String> = emptySet(),
        visiting: Set<String> = emptySet(),
        context: ActionContext? = null
    ): List<String> {
        val skipped = exclusions(range, context)
        val result = mutableListOf<String>()
        var index = range.first
        var last = range.last
        while (index <= last) {
            val nested = skipped.firstOrNull {
                index in it.start..it.body.last && it.start >= range.first && it.body.last <= range.last
            }
            if (nested != null) {
                index = nested.body.last + 1
                continue
            }
            if (code.text(index) == "function" || code.text(index) == "=>") {
                index = skipFunction(index, range.last)
                continue
            }
            val name = code.text(index)
            if (name == "break") break
            if (name in setOf("return", "throw")) {
                if (code.text(index + 1) == ";") break
                last = minOf(last, code.statementEnd(index + 1))
            }
            if (!code.tokens[index].isString && code.text(index + 1) == "(" &&
                name.matches(IDENTIFIER) && code.text(index - 1) != "function") {
                val helper = scopes.firstOrNull { it.functionName == name }
                    ?.takeIf { code.text(index - 1) !in setOf(".", "?.") && name !in roots }
                if (helper == null) result.add(name)
                else if (name !in visiting && visiting.size < 16) {
                    val bindings = context?.bindings?.forHelper(helper,
                        code.arguments(index + 1), code.tokens[index].start)
                    result.addAll(calls(scopeRange(helper), roots = roots, visiting = visiting + name,
                        context = if (bindings == null) null else context.copy(bindings = bindings)))
                }
                if (name in setOf("execute", "executeSql") && checkpointArgument(index)) result.add("checkpoint")
            }
            index++
        }
        return result
    }

    private fun exclusions(range: IntRange, context: ActionContext?): List<Branch> {
        if (context == null) return emptyList()
        val branches = branches(range, context.bindings)
        val result = branches.filter { context.error !in it.codes }.toMutableList()
        for (branch in branches.filter { context.error in it.codes && code.text(it.start) == "if" }) {
            val thenEnd = code.statementEnd(code.end(branch.start + 1) + 1)
            val alternative = thenEnd + 1
            if (code.text(alternative) == "else") {
                val end = conditionalEnd(alternative + 1)
                if (end >= alternative) result.add(Branch(emptySet(), alternative..end, alternative))
            }
        }
        return result
    }

    private fun conditionalEnd(start: Int, depth: Int = 0): Int {
        if (depth >= 16) return -1
        if (code.text(start) != "if") return code.statementEnd(start)
        val condition = code.end(start + 1)
        if (condition < start) return -1
        val end = conditionalEnd(condition + 1, depth + 1)
        return if (end >= 0 && code.text(end + 1) == "else") conditionalEnd(end + 2, depth + 1) else end
    }

    private fun scopeRange(scope: JavaScriptAnalyzer.FunctionScope): IntRange {
        val start = code.tokens.indexOfFirst { it.start >= scope.bodyStartOffset }
        val end = code.tokens.indexOfFirst { it.start >= scope.endOffset - 1 }
        return if (start >= 0 && end >= start) start until end else IntRange.EMPTY
    }

    private fun skipFunction(start: Int, last: Int): Int {
        var cursor = start + 1
        while (cursor <= last && code.text(cursor) != "{") cursor++
        return if (code.end(cursor) >= cursor) code.end(cursor) + 1 else last + 1
    }

    private fun checkpointArgument(index: Int): Boolean {
        val argument = code.tokens.getOrNull(index + 2) ?: return false
        if (!argument.isString || argument.text.startsWith("`")) return false
        return Regex("""(?i)^['"]\s*PRAGMA\s+wal_checkpoint\s*\(\s*TRUNCATE\s*\)\s*;?\s*['"]$""")
            .matches(argument.text)
    }

    private fun missingActions(error: Int, actions: List<String>, operations: Set<String>): List<String> {
        val retry = actions.any { it.startsWith("retry", true) || it in operations }
        val delay = actions.any { it in setOf("sleep", "delay", "backoff", "setTimeout") }
        val recovery = actions.any { it == "restore" || it.startsWith("recover", true) }
        val cleanup = actions.any { it in setOf("cleanupDiskSpace", "cleanDiskSpace", "freeDiskSpace", "clearCache", "unlink", "unlinkSync") }
        val required = when (error) {
            14800024, 14800025 -> listOf("retry" to retry, "delay/backoff" to delay)
            14800011 -> listOf("corruption recovery (restore/recover)" to recovery)
            14800028 -> if (recovery) emptyList() else listOf("I/O retry" to retry, "delay/backoff" to delay)
            14800029 -> listOf("disk-space cleanup" to cleanup, "retry" to retry)
            14800047 -> listOf("ResultSet close" to ("close" in actions), "WAL checkpoint(TRUNCATE)" to ("checkpoint" in actions))
            else -> emptyList()
        }
        return required.filterNot { it.second }.map { it.first }
    }

    companion object {
        private val IDENTIFIER = Regex("[a-zA-Z_$][a-zA-Z0-9_$]*")
        private val OPERATIONS = DatabaseConstants.DATABASE_CRUD_OPERATIONS +
            setOf("getRdbStore", "backup", "restore", "querySql", "insertSync", "updateSync", "deleteSync", "querySync")
    }
}

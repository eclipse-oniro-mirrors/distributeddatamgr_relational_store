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
        val branches = branches(handler)
        if (branches.isEmpty()) {
            return listOf(violation(handler, "No verifiable database error-code handling. " +
                "Handle busy/locked (14800024/14800025) with retry and delay, " +
                "and corrupt (14800011) with recovery; logging or rethrowing alone is insufficient."))
        }
        return branches.flatMap { branch ->
            branch.codes.mapNotNull { error ->
                val actions = calls(branch.body, branches.filter { error !in it.codes }, handler.operations)
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

    private fun branches(handler: Handler): List<Branch> {
        if (handler.error.isEmpty()) return emptyList()
        val result = mutableListOf<Branch>()
        var index = handler.body.first
        while (index <= handler.body.last) {
            if (code.text(index) in setOf("function", "=>")) {
                index = skipFunction(index, handler.body.last)
                continue
            }
            if (code.text(index) == "if") ifBranch(index, handler.error)?.let { result.add(it) }
            if (code.text(index) == "switch") result.addAll(switchBranches(index, handler.error))
            index++
        }
        return result.filter { it.body.last <= handler.body.last }
    }

    private fun ifBranch(index: Int, error: String): Branch? {
        if (code.text(index + 1) != "(") return null
        val end = code.end(index + 1)
        if (end < 0) return null
        val condition = ((index + 2) until end).joinToString("") {
            if (code.tokens[it].isString) "<literal>" else code.text(it)
        }
        // Negative guards do not establish a positive error-code path.
        if (condition.contains("!(") || condition.contains("=>")) return null
        val property = "(?<![a-zA-Z0-9_$])${Regex.escape(error)}(?:\\?\\.|\\.)code"
        val equality = Regex("(?:$property==(?:=)?([0-9]+)|([0-9]+)==(?:=)?$property)(?![0-9])")
        val codes = equality.findAll(condition).mapNotNull {
            (it.groupValues[1].ifEmpty { it.groupValues[2] }).toIntOrNull()
        }.filter { it in DatabaseConstants.DATABASE_ERROR_CODES }.toSet()
        if (codes.isEmpty()) return null
        val start = end + 1
        val bodyEnd = code.statementEnd(start)
        if (bodyEnd < start) return null
        val body = if (code.text(start) == "{") (start + 1) until bodyEnd else start..bodyEnd
        return Branch(codes, body, index)
    }

    private fun switchBranches(index: Int, error: String): List<Branch> {
        if (code.text(index + 1) != "(") return emptyList()
        val conditionEnd = code.end(index + 1)
        val property = code.joined((index + 2) until conditionEnd)
        if (property != "$error.code" && property != "$error?.code") return emptyList()
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
        return labels.mapIndexedNotNull { position, label -> switchBranch(labels, position, label, end) }
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
        excluded: List<Branch> = emptyList(),
        roots: Set<String> = emptySet(),
        visiting: Set<String> = emptySet()
    ): List<String> {
        val result = mutableListOf<String>()
        var index = range.first
        var last = range.last
        while (index <= last) {
            val nested = excluded.firstOrNull {
                index in it.start..it.body.last && it.start >= range.first && it.body.last < range.last
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
                else if (name !in visiting && visiting.size < 16) result.addAll(calls(scopeRange(helper), roots = roots, visiting = visiting + name))
                if (name in setOf("execute", "executeSql") && checkpointArgument(index)) result.add("checkpoint")
            }
            index++
        }
        return result
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

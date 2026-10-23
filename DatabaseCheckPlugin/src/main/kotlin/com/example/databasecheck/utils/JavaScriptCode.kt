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

/** A lexical view shared by database rules; offsets always refer to the original source. */
internal class JavaScriptCode(val source: String) {
    data class Token(val text: String, val start: Int, val end: Int) {
        val isString: Boolean get() = text.firstOrNull() in listOf('\'', '"', '`')
    }

    val tokens: List<Token> = TOKEN_PATTERN.findAll(source)
        .filterNot { it.value.startsWith("//") || it.value.startsWith("/*") }
        .map { Token(it.value, it.range.first, it.range.last + 1) }.toList()
    private val pairs = mutableMapOf<Int, Int>()

    init {
        val stack = mutableListOf<Int>()
        tokens.forEachIndexed { index, token ->
            if (token.text in OPEN) stack.add(index)
            if (token.text in CLOSE && stack.isNotEmpty()) {
                val start = stack.last()
                if (OPEN.indexOf(tokens[start].text) == CLOSE.indexOf(token.text)) {
                    pairs[stack.removeAt(stack.lastIndex)] = index
                }
            }
        }
    }

    fun text(index: Int): String = tokens.getOrNull(index)?.text.orEmpty()
    fun end(index: Int): Int = pairs[index] ?: -1
    fun joined(range: IntRange): String = range.joinToString("") { text(it) }

    fun withoutComments(): String {
        val result = source.toCharArray()
        TOKEN_PATTERN.findAll(source).filter {
            it.value.startsWith("//") || it.value.startsWith("/*")
        }.forEach { comment ->
            for (index in comment.range) if (result[index] !in "\r\n") result[index] = ' '
        }
        return String(result)
    }

    fun withoutStrings(): String {
        val result = source.toCharArray()
        tokens.filter { it.isString }.forEach { token ->
            for (index in token.start until token.end) if (result[index] !in "\r\n") result[index] = ' '
        }
        return String(result)
    }

    fun closingOffsetAt(offset: Int): Int {
        val start = tokens.indexOfFirst { it.start == offset }
        return tokens.getOrNull(end(start))?.start ?: -1
    }

    /** Split only at commas outside nested delimiters and string/template tokens. */
    fun arguments(open: Int): List<String> {
        val close = end(open)
        if (close <= open + 1) return emptyList()
        val result = mutableListOf<String>()
        var first = open + 1
        var cursor = first
        while (cursor < close) {
            if (text(cursor) == ",") {
                result.add(source.substring(tokens[first].start, tokens[cursor].start).trim())
                first = cursor + 1
            }
            val nestedEnd = end(cursor)
            cursor = if (nestedEnd >= cursor) nestedEnd + 1 else cursor + 1
        }
        if (first < close) result.add(source.substring(tokens[first].start, tokens[close].start).trim())
        return result
    }

    /** End of an initializer, including multiline calls/objects and basic semicolon insertion. */
    fun expressionEnd(start: Int): Int {
        var cursor = start
        var last = start - 1
        while (cursor < tokens.size && text(cursor) !in setOf(";", "}", ")", "]", ",")) {
            if (last >= start && source.substring(tokens[last].end, tokens[cursor].start).contains('\n') &&
                text(last) !in CONTINUATIONS && text(cursor) !in CONTINUATIONS) break
            val close = end(cursor)
            last = if (close >= cursor) close else cursor
            cursor = last + 1
        }
        return last
    }

    /** A declaration cannot escape its containing braces into a sibling function or block. */
    fun isVisibleAt(declarationOffset: Int, referenceOffset: Int): Boolean = pairs.all { (open, close) ->
        text(open) != "{" || declarationOffset !in tokens[open].end until tokens[close].start ||
            referenceOffset in tokens[open].end until tokens[close].start
    }

    fun statementEnd(start: Int): Int {
        if (text(start) == "{") return end(start)
        var index = start
        while (index < tokens.size && text(index) !in setOf(";", "}")) {
            val end = end(index)
            index = if (end >= index) end + 1 else index + 1
        }
        return index.coerceAtMost(tokens.lastIndex)
    }

    companion object {
        private val OPEN = listOf("(", "[", "{")
        private val CLOSE = listOf(")", "]", "}")
        private val CONTINUATIONS = setOf("await", "=", ".", "?.", ",", "+", "-", "*", "/", "&&", "||", "?", ":", "=>")
        private val TOKEN_PATTERN = Regex(
            """//[^\r\n]*|/\*[\s\S]*?\*/|"(?:\\.|[^"\\])*"|'(?:\\.|[^'\\])*'|`(?:\\.|[^`\\])*`|[a-zA-Z_$][a-zA-Z0-9_$]*|[0-9]+|===|!==|==|!=|\?\.|&&|\|\||=>|[^\s]"""
        )
    }
}

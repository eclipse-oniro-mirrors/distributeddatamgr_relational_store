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
    data class Token(val text: String, val start: Int, val end: Int, val isRegex: Boolean = false) {
        val isString: Boolean get() = text.firstOrNull() in listOf('\'', '"', '`')
        val isLiteral: Boolean get() = isString || isRegex
    }

    private val lexicalTokens = tokenize(source)
    val tokens: List<Token> = lexicalTokens.filterNot { isComment(it.text) }
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
        lexicalTokens.filter { isComment(it.text) }.forEach { comment ->
            for (index in comment.start until comment.end) if (result[index] !in "\r\n") result[index] = ' '
        }
        return String(result)
    }

    fun withoutStrings(): String {
        val result = source.toCharArray()
        tokens.filter { it.isLiteral }.forEach { token ->
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
        while (index < tokens.size && text(index) !in setOf(";", "}", "else")) {
            if (index > start && source.substring(tokens[index - 1].end, tokens[index].start).contains('\n') &&
                text(index - 1) !in CONTINUATIONS && text(index) !in CONTINUATIONS) break
            val end = end(index)
            index = if (end >= index) end + 1 else index + 1
        }
        return if (text(index) == ";") index else index - 1
    }

    companion object {
        private fun isComment(text: String): Boolean = text.startsWith("//") || text.startsWith("/*")

        private fun tokenize(source: String): List<Token> {
            val result = mutableListOf<Token>()
            var position = 0
            var previous = ""
            while (position < source.length) {
                val match = TOKEN_PATTERN.find(source, position) ?: break
                val regexEnd = if (match.value == "/" && previous in REGEX_PREFIXES)
                    regularExpressionEnd(source, match.range.first) else -1
                val end = if (regexEnd >= 0) regexEnd else match.range.last + 1
                val text = source.substring(match.range.first, end)
                result.add(Token(text, match.range.first, end, regexEnd >= 0))
                if (!isComment(text)) previous = text
                position = end
            }
            return result
        }

        /** Only recognize a regex where an expression may begin; leave division tokens untouched. */
        private fun regularExpressionEnd(source: String, start: Int): Int {
            var cursor = start + 1
            var characterClass = false
            while (cursor < source.length && source[cursor] !in "\r\n") {
                when (source[cursor]) {
                    '\\' -> {
                        if (cursor + 1 >= source.length || source[cursor + 1] in "\r\n") return -1
                        cursor++
                    }
                    '[' -> characterClass = true
                    ']' -> characterClass = false
                    '/' -> if (!characterClass) {
                        cursor++
                        while (cursor < source.length && source[cursor].isLetter()) cursor++
                        return cursor
                    }
                }
                cursor++
            }
            return -1
        }

        private val REGEX_PREFIXES = setOf("", "=", "(", "[", "{", ",", ":", ";", "return", "throw",
            "case", "=>", "!", "~", "?", "&&", "||", "??", "void", "typeof", "yield", "await")
        private val OPEN = listOf("(", "[", "{")
        private val CLOSE = listOf(")", "]", "}")
        private val CONTINUATIONS = setOf("await", "=", ".", "?.", ",", "+", "-", "*", "/", "&&", "||", "?", ":", "=>")
        private val TOKEN_PATTERN = Regex(
            """//[^\r\n]*|/\*[\s\S]*?\*/|"(?:\\.|[^"\\])*"|'(?:\\.|[^'\\])*'|`(?:\\.|[^`\\])*`|[a-zA-Z_$][a-zA-Z0-9_$]*|[0-9]+|===|!==|==|!=|\?\.|&&|\|\||=>|[^\s]"""
        )
    }
}

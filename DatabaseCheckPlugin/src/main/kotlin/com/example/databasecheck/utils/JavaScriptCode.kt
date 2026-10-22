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

/** A small lexical view used by Rule9; offsets always refer to the original source. */
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
        private val TOKEN_PATTERN = Regex(
            """//[^\r\n]*|/\*[\s\S]*?\*/|"(?:\\.|[^"\\])*"|'(?:\\.|[^'\\])*'|`(?:\\.|[^`\\])*`|[a-zA-Z_$][a-zA-Z0-9_$]*|[0-9]+|===|!==|==|!=|\?\.|&&|\|\||=>|[^\s]"""
        )
    }
}

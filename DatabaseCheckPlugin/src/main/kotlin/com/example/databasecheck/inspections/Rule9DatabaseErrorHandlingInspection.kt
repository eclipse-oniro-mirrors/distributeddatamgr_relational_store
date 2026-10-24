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

import com.example.databasecheck.utils.DatabaseErrorHandlingAnalyzer
import com.intellij.codeInspection.ProblemsHolder
import com.intellij.openapi.util.TextRange
import com.intellij.psi.PsiFile

/** Checks error-code branches for retry or recovery, rather than accepting any non-empty catch. */
class Rule9DatabaseErrorHandlingInspection : BaseDatabaseInspection() {
    override fun getRuleName(): String = "Database Error Code Handling and Retry Logic"
    override fun getRuleNumber(): Int = 9
    override fun getDescription(): String =
        "Ensures proper handling of specific database error codes and retry logic for CRUD operations."

    override fun visitJavaScriptFile(file: PsiFile, holder: ProblemsHolder) {
        DatabaseErrorHandlingAnalyzer(file.text).analyze().forEach { violation ->
            createProblemDescriptor(file, violation.reason, holder, TextRange(violation.start, violation.end))
        }
    }
}

/*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
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

import com.intellij.codeInspection.InspectionManager
import com.intellij.codeInspection.ProblemsHolder
import com.intellij.testFramework.fixtures.LightPlatformCodeInsightFixtureTestCase

class PathVariableInspectionTest : LightPlatformCodeInsightFixtureTestCase() {
    fun testTypedPathVariableIsDetected() {
        assertRule1(1, "const path: string = context.databaseDir + '/test.db'; fileIo.openSync(path, 0);")
    }

    fun testStaticPathVariableIsDetected() {
        assertRule1(1, "const path = '/data/storage/el1/database/test.db'; fileIo.openSync(path, 0);")
    }

    fun testAliasAndConcatenationAreDetected() {
        assertRule1(1, "const root = context.databaseDir; const dir = root; const path = dir + '/test.db'; fileIo.open(path, 0);")
    }

    fun testGlobalPathRemainsVisibleInsideFunction() {
        assertRule1(1, "const path = context.databaseDir; function read(): void { fileIo.open(path, 0); }")
    }

    fun testSiblingLocalPathDoesNotContaminateOrdinaryFile() {
        assertRule1(0, """
            function db(): void { const path = context.databaseDir; }
            function ordinary(): void { const path = '/data/storage/el1/files/a.txt'; fileIo.open(path, 0); }
        """.trimIndent())
    }

    fun testLaterAssignmentDoesNotChangeEarlierCall() {
        assertRule1(0, "let path = '/data/storage/el1/files/a.txt'; fileIo.open(path, 0); path = context.databaseDir;")
    }

    fun testOrdinaryReassignmentStopsDatabaseWarning() {
        assertRule1(0, "let path = context.databaseDir; path = '/data/storage/el1/files/a.txt'; fileIo.open(path, 0);")
    }

    fun testDatabaseReassignmentStartsDatabaseWarning() {
        assertRule1(1, "let path = '/data/storage/el1/files/a.txt'; path = context.databaseDir; fileIo.open(path, 0);")
    }

    fun testParameterShadowsGlobalDatabasePath() {
        assertRule1(0, "const path = context.databaseDir; function read(path: string): void { fileIo.open(path, 0); }")
    }

    fun testArrowParameterShadowsGlobalDatabasePath() {
        assertRule1(0, "const path = context.databaseDir; const read = (path: string) => { fileIo.open(path, 0); };")
    }

    fun testUninitializedLocalShadowsGlobalDatabasePath() {
        assertRule1(0, "const path = context.databaseDir; function read(): void { let path: string; fileIo.open(path, 0); }")
    }

    fun testAliasKeepsValueAtItsAssignment() {
        assertRule1(1, "let path = context.databaseDir; const dbPath = path; path = '/files/a.txt'; fileIo.open(dbPath, 0);")
    }

    fun testTemplateWithSimplePathAliasIsDetected() {
        assertRule1(1, "const root = context.databaseDir; const path = `" + "${'$'}{root}/test.db`; fileIo.open(path, 0);")
    }

    fun testFileDescriptorPropagationReportsEachCallOnce() {
        assertRule1(2, "const path = context.databaseDir; const file = fileIo.openSync(path, 0); fileIo.closeSync(file.fd);")
    }

    fun testAliasCycleTerminatesWithoutWarning() {
        assertRule1(0, "const first = second; const second = first; fileIo.open(first, 0);")
    }

    fun testCopyOperationUsesSamePathResolver() {
        assertEquals(1, count(Rule2DatabaseCopyInspection(),
            "const path: string = context.databaseDir + '/test.db'; fileIo.copyFile(path, '/files/copy.db');"))
    }

    fun testCopyDoesNotUseFutureDatabaseValue() {
        assertEquals(0, count(Rule2DatabaseCopyInspection(),
            "let path = '/files/a.txt'; fileIo.copyFile(path, '/files/b.txt'); path = context.databaseDir;"))
    }

    fun testExistingPermissionApiUsesPathVariable() {
        assertEquals(1, count(Rule7DatabasePermissionInspection(),
            "const path: string = context.databaseDir + '/test.db'; fileIo.chmod(path, 0o600);"))
    }

    fun testStandaloneChmodSyncRemainsOutsideApiList() {
        assertEquals(0, count(Rule7DatabasePermissionInspection(),
            "const path = context.databaseDir; chmodSync(path, 0o600);"))
    }

    fun testRdbHandleCloseIsNotAFileIoViolation() {
        assertRule1(0, "const config = { customDir: context.databaseDir }; const store = relationalStore.getRdbStore(context, config); store.close();")
    }

    fun testVarPathRemainsVisibleAfterBlock() {
        assertRule1(1, "function read() { if (ready) { var path = context.databaseDir; } fileIo.open(path, 0); }")
    }

    fun testBlockLocalDatabasePathDoesNotEscapeBlock() {
        assertRule1(0, "let path = '/files/a.txt'; { let path = context.databaseDir; } fileIo.open(path, 0);")
    }

    private fun assertRule1(expected: Int, source: String) {
        assertEquals(source, expected, count(Rule1NonRdbInterfaceInspection(), source))
    }

    private fun count(rule: BaseDatabaseInspection, source: String): Int {
        val file = myFixture.configureByText("paths.ets", source)
        val holder = ProblemsHolder(InspectionManager.getInstance(project), file, false)
        rule.buildVisitor(holder, false).visitFile(file)
        return holder.results.size
    }
}

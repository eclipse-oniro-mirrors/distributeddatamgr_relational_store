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
 
// Rule9 negative examples: one diagnostic for each function below.

async function emptyCatch() {
    try { await rdbStore.insert('users', values); } catch (error) {}
}

async function loggingAndRethrow() {
    try { await rdbStore.insert('users', values); }
    catch (error) { console.error(error); throw error; }
}

async function busyOnlyLogs() {
    try { await rdbStore.insert('users', values); }
    catch (error) {
        if (error.code === 14800024) { console.error(error); }
    }
}

async function lockedWithoutDelay() {
    try { await rdbStore.insert('users', values); }
    catch (error) {
        if (error.code === 14800025) { await retryOperation(); }
    }
}

async function corruptOnlyRetries() {
    try { await relationalStore.getRdbStore(context, config); }
    catch (error) {
        if (error.code === 14800011) { await delay(100); await retryOperation(); }
    }
}

async function ioOnlyLogs() {
    try { await rdbStore.insert('users', values); }
    catch (error) {
        if (error.code === 14800028) { console.error(error); }
    }
}

async function diskFullWithoutCleanup() {
    try { await rdbStore.insert('users', values); }
    catch (error) {
        if (error.code === 14800029) {
            // A cleanup comment is not an action.
            await retryOperation();
        }
    }
}

async function walWithoutCheckpoint() {
    try { await rdbStore.insert('users', values); }
    catch (error) {
        if (error.code === 14800047) { resultSet.close(); }
    }
}

async function wrongBranchRecovery() {
    try { await rdbStore.insert('users', values); }
    catch (error) {
        if (error.code === 14800011) { console.error(error); }
        else { await rdbStore.restore('backup.db'); }
    }
}

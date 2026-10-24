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
 
// Rule9 positive examples: each declared error code has its matching action.
// Helpers below represent application-provided asynchronous policies.

async function busyWithBoundedRetry(attempt = 0) {
    try {
        await rdbStore.insert('users', { name: 'test' });
    } catch (error) {
        if ((error.code === 14800024 || error.code === 14800025) && attempt < 3) {
            await delay(100);
            return await busyWithBoundedRetry(attempt + 1);
        }
        throw error;
    }
}

async function corruptWithRestore() {
    try {
        await relationalStore.getRdbStore(context, config);
    } catch (error) {
        if (error.code === 14800011) {
            await rdbStore.restore('backup.db');
        }
        throw error;
    }
}

async function diskFullWithCleanup() {
    try {
        await rdbStore.insert('users', values);
    } catch (error) {
        if (error.code === 14800029 && !cleaned) {
            await cleanupDiskSpace();
            await retryOperation();
        }
        throw error;
    }
}

async function walWithCloseAndCheckpoint() {
    try {
        await rdbStore.insert('users', values);
    } catch (error) {
        if (error.code === 14800047) {
            resultSet?.close();
            await rdbStore.execute('PRAGMA wal_checkpoint(TRUNCATE)');
        }
        throw error;
    }
}

async function switchWithGroupedCases() {
    try {
        await rdbStore.query(predicates);
    } catch (error) {
        switch (error.code) {
            case 14800024:
            case 14800025:
                await delay(100);
                await retryOperation();
                break;
            case 14800011:
            case 14800028:
                await recoverDatabase();
                break;
            default:
                throw error;
        }
    }
}

async function nestedRetryGuard() {
    try {
        await rdbStore.update(values, predicates);
    } catch (error) {
        if (error.code === 14800024) {
            if (attempt < maxAttempts) {
                await backoff(attempt);
                await retryOperation();
            }
        }
        throw error;
    }
}

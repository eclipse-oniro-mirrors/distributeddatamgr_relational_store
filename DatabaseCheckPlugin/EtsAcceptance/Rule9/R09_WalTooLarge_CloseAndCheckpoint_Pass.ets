/*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the 'License');
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an 'AS IS' BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// Case: R09_WalTooLarge_CloseAndCheckpoint_Pass
// Rule: 9
// Expected: 0
// ExpectedAll: none
// Message:
// Point: WAL 超限分支关闭结果集并执行 TRUNCATE checkpoint

import relationalStore from '@ohos.data.relationalStore';

export async function R09_WalTooLarge_CloseAndCheckpoint_Pass(
  store: relationalStore.RdbStore,
  resultSet: relationalStore.ResultSet
): Promise<void> {
  const values: relationalStore.ValuesBucket = { id: 1, name: 'test' };
  try {
    await store.insert('users', values);
  } catch (error) {
    if (error.code === 14800047) {
      resultSet.close();
      await store.executeSql('PRAGMA wal_checkpoint(TRUNCATE)');
    } else {
      throw error;
    }
  }
}

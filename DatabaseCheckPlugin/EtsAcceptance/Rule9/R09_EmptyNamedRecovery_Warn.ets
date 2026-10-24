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

// Case: R09_EmptyNamedRecovery_Warn
// Rule: 9
// Expected: 1
// ExpectedAll: 9:1
// Message: 14800011
// Point: 同文件空 recover 辅助函数不能只凭名称通过

import relationalStore from '@ohos.data.relationalStore';

function recoverDatabase(): void {
  console.error('not implemented');
}
export async function R09_EmptyNamedRecovery_Warn(store: relationalStore.RdbStore): Promise<void> {
  const values: relationalStore.ValuesBucket = { id: 1, name: 'test' };
  try {
    await store.insert('users', values);
  } catch (error) {
    if (error.code === 14800011) {
      recoverDatabase();
    }
  }
}

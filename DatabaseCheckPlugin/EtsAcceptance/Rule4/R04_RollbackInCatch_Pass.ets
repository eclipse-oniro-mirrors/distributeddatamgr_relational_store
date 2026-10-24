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

// Case: R04_RollbackInCatch_Pass
// Rule: 4
// Expected: 0
// Message:
// Point: catch 中真实回滚对应事务

import relationalStore from '@ohos.data.relationalStore';

export async function R04_RollbackInCatch_Pass(store: relationalStore.RdbStore): Promise<void> {
  let txn: relationalStore.Transaction | undefined = undefined;
  const values: relationalStore.ValuesBucket = { id: 1, name: 'test' };
  try {
    txn = await store.createTransaction();
    await txn.insert('users', values);
    await txn.commit();
  } catch (error) {
    await txn?.rollback();
    throw error;
  }
}

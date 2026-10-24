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

// Case: R03_TemplateLoop_Warn
// Rule: 3
// Expected: 1
// Message: only one CRUD operation
// Point: 模板内伪循环不能取消单操作事务告警

import relationalStore from '@ohos.data.relationalStore';

export async function R03_TemplateLoop_Warn(store: relationalStore.RdbStore): Promise<void> {
  const values: relationalStore.ValuesBucket = { id: 1, name: 'test' };
  const txn: relationalStore.Transaction = await store.createTransaction();
  const text: string = `for (let i = 0; i < 3; i++) { } // literal`;
  console.log(text);
  await txn.insert('users', values);
  await txn.commit();
}

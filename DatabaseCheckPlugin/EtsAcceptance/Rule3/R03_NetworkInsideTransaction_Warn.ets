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

// Case: R03_NetworkInsideTransaction_Warn
// Rule: 3
// Expected: 1
// ExpectedAll: 3:1
// Message: Time-consuming operation
// Point: 多操作事务中插入真实网络请求

import relationalStore from '@ohos.data.relationalStore';
import http from '@ohos.net.http';

export async function R03_NetworkInsideTransaction_Warn(store: relationalStore.RdbStore, url: string): Promise<void> {
  const request: http.HttpRequest = http.createHttp();
  const values: relationalStore.ValuesBucket = { id: 1, name: 'test' };
  const txn: relationalStore.Transaction = await store.createTransaction();
  await txn.insert('users', values);
  await request.request(url);
  await txn.insert('users', { id: 2, name: 'second' });
  await txn.commit();
  request.destroy();
}

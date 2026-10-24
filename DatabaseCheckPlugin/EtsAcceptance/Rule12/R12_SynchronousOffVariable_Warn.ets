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

// Case: R12_SynchronousOffVariable_Warn
// Rule: 12
// Expected: 1
// Message: Prohibited PRAGMA
// Point: SQL 变量中设置 synchronous=OFF

import relationalStore from '@ohos.data.relationalStore';

export async function R12_SynchronousOffVariable_Warn(store: relationalStore.RdbStore): Promise<void> {
  const sql = 'PRAGMA synchronous = OFF';
  await store.executeSql(sql);
}

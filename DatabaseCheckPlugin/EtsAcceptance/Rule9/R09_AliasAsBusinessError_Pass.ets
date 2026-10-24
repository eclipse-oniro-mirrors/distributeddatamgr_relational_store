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

// Case: R09_AliasAsBusinessError_Pass
// Rule: 9
// Expected: 0
// ExpectedAll: none
// Message:
// Point: 异常对象类型断言与别名仍可识别完整重试

import relationalStore from '@ohos.data.relationalStore';
import { BusinessError } from '@ohos.base';

export async function R09_AliasAsBusinessError_Pass(
  store: relationalStore.RdbStore,
  delay: () => Promise<void>,
  other: BusinessError
): Promise<void> {
  const values: relationalStore.ValuesBucket = { id: 1, name: 'test' };
  try {
    await store.insert('users', values);
  } catch (e) {
    const err = e as BusinessError;
    if (err.code === 14800024) {
      await delay();
      await store.insert('users', values);
    } else {
      throw e;
    }
  }
}

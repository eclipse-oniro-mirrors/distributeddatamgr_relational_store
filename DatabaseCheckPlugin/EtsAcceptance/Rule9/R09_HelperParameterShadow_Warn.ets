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

// Case: R09_HelperParameterShadow_Warn
// Rule: 9
// Expected: 1
// ExpectedAll: 9:1
// Message: 14800024
// Point: 辅助函数参数遮蔽捕获异常时不能借用处理动作

import relationalStore from '@ohos.data.relationalStore';
import { BusinessError } from '@ohos.base';

export async function R09_HelperParameterShadow_Warn(
  store: relationalStore.RdbStore,
  delay: () => Promise<void>,
  other: BusinessError
): Promise<void> {
  const values: relationalStore.ValuesBucket = { id: 1, name: 'test' };
  async function handle(error: BusinessError): Promise<void> {
    if (error.code === 14800024) {
      await delay();
      await store.insert('users', values);
    }
  }
  try {
    await store.insert('users', values);
  } catch (error) {
    if (error.code === 14800024) {
      await handle(other);
    }
  }
}

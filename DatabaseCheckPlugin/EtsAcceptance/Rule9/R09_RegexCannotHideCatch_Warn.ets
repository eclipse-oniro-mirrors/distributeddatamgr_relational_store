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

// Case: R09_RegexCannotHideCatch_Warn
// Rule: 9
// Expected: 1
// ExpectedAll: 9:1
// Message: No verifiable
// Point: 正则中的括号不会破坏 try/catch 配对而隐藏日志告警

import relationalStore from '@ohos.data.relationalStore';
import { BusinessError } from '@ohos.base';

export async function R09_RegexCannotHideCatch_Warn(
  store: relationalStore.RdbStore,
  delay: () => Promise<void>,
  other: BusinessError
): Promise<void> {
  const values: relationalStore.ValuesBucket = { id: 1, name: 'test' };
  try {
    const pattern: RegExp = /[({]/;
    console.info(pattern.source);
    await store.insert('users', values);
  } catch (error) {
    console.error(JSON.stringify(error));
  }
}

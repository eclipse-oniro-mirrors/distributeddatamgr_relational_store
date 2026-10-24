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

// Case: R06_RenamedOptionalClose_Pass
// Rule: 6
// Expected: 0
// ExpectedAll: none
// Message:
// Point: 不同变量名且可选访问的 close 位于删除前

import relationalStore from '@ohos.data.relationalStore';
import common from '@ohos.app.ability.common';

export async function R06_RenamedOptionalClose_Pass(
  context: common.Context,
  store: relationalStore.RdbStore
): Promise<void> {
  store?.close();
  await relationalStore.deleteRdbStore(context, 'r06_closed.db');
}

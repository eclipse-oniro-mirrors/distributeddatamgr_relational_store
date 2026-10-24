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

// Case: R06_FutureClose_Warn
// Rule: 6
// Expected: 1
// ExpectedAll: 6:1
// Message: Missing rdbStore.close
// Point: 删除之后的 close 不能抵消告警

import relationalStore from '@ohos.data.relationalStore';
import common from '@ohos.app.ability.common';

export async function R06_FutureClose_Warn(context: common.Context, store: relationalStore.RdbStore): Promise<void> {
  await relationalStore.deleteRdbStore(context, 'r06_order.db');
  store.close();
}

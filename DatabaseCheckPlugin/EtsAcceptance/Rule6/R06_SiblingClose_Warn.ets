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

// Case: R06_SiblingClose_Warn
// Rule: 6
// Expected: 1
// Message: Missing rdbStore.close
// Point: 相邻函数内的 close 不算当前函数已关闭

import relationalStore from '@ohos.data.relationalStore';
import common from '@ohos.app.ability.common';

function unused(store: relationalStore.RdbStore): void {
  store.close();
}
export async function R06_SiblingClose_Warn(context: common.Context, store: relationalStore.RdbStore): Promise<void> {
  await relationalStore.deleteRdbStore(context, 'r06_sibling.db');
}

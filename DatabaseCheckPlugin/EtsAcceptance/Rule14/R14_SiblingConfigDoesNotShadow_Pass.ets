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

// Case: R14_SiblingConfigDoesNotShadow_Pass
// Rule: 14
// Expected: 0
// ExpectedAll: none
// Message:
// Point: 相邻函数局部配置不能覆盖当前可见的全局配置

import relationalStore from '@ohos.data.relationalStore';
import common from '@ohos.app.ability.common';

const config: relationalStore.StoreConfig = { name: 'r14_scope.db', securityLevel: relationalStore.SecurityLevel.S3 };
function unused(): relationalStore.StoreConfig {
  const config: relationalStore.StoreConfig = { name: 'r14_scope.db', securityLevel: relationalStore.SecurityLevel.S1 };
  return config;
}
export async function R14_SiblingConfigDoesNotShadow_Pass(context: common.Context): Promise<void> {
  const first: relationalStore.RdbStore = await relationalStore.getRdbStore(context, config);
  first.close();
  const second: relationalStore.RdbStore = await relationalStore.getRdbStore(context, config);
  second.close();
}

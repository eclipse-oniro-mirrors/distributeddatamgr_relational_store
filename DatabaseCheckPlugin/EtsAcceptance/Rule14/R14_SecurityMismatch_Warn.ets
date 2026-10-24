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

// Case: R14_SecurityMismatch_Warn
// Rule: 14
// Expected: 1
// ExpectedAll: 14:1
// Message: configuration inconsistency
// Point: 同名数据库 securityLevel 不一致

import relationalStore from '@ohos.data.relationalStore';
import common from '@ohos.app.ability.common';

export async function R14_SecurityMismatch_Warn(context: common.Context): Promise<void> {
  const config1: relationalStore.StoreConfig = {
    name: 'r14_security.db',
    securityLevel: relationalStore.SecurityLevel.S1
  };
  const config2: relationalStore.StoreConfig = {
    name: 'r14_security.db',
    securityLevel: relationalStore.SecurityLevel.S3
  };
  const first: relationalStore.RdbStore = await relationalStore.getRdbStore(context, config1);
  first.close();
  const second: relationalStore.RdbStore = await relationalStore.getRdbStore(context, config2);
  second.close();
}

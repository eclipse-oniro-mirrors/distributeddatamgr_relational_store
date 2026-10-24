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

// Case: R14_IdenticalConfig_Pass
// Rule: 14
// Expected: 0
// ExpectedAll: none
// Message:
// Point: 同名数据库配置完全相同

import relationalStore from '@ohos.data.relationalStore';
import common from '@ohos.app.ability.common';

export async function R14_IdenticalConfig_Pass(context: common.Context): Promise<void> {
  const config1: relationalStore.StoreConfig = {
    name: 'r14_equal.db',
    securityLevel: relationalStore.SecurityLevel.S3,
    encrypt: true
  };
  const config2: relationalStore.StoreConfig = {
    name: 'r14_equal.db',
    securityLevel: relationalStore.SecurityLevel.S3,
    encrypt: true
  };
  const first: relationalStore.RdbStore = await relationalStore.getRdbStore(context, config1);
  first.close();
  const second: relationalStore.RdbStore = await relationalStore.getRdbStore(context, config2);
  second.close();
}

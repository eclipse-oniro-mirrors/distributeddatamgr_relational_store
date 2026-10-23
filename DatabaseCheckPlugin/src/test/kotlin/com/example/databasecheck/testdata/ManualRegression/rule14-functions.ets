/*
 * Copyright (c) 2024-2026 Huawei Device Co., Ltd.
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

const STORE_NAME: string = 'TestDB';

// --- violation: 不同函数中配置不一致 ---
async function rule14_violation_funcA(context: common.Context): Promise<void> {
  const config: relationalStore.StoreConfig = {
    name: STORE_NAME,
    securityLevel: relationalStore.SecurityLevel.S1,
  };
  await relationalStore.getRdbStore(context, config);
}

async function rule14_violation_funcB(context: common.Context): Promise<void> {
  const config: relationalStore.StoreConfig = {
    name: STORE_NAME,
    securityLevel: relationalStore.SecurityLevel.S3,
  };
  await relationalStore.getRdbStore(context, config);
}

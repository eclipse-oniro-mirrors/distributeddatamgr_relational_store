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

import { describe, beforeAll, beforeEach, afterEach, afterAll, it, expect } from '@ohos/hypium';
import relationalStore from '@ohos.data.relationalStore';
import AbilityDelegatorRegistry from '@ohos.app.ability.abilityDelegatorRegistry';

const TAG = "[RDB_DUMP_STACK_CONFIG_TEST]";
const delegator = AbilityDelegatorRegistry.getAbilityDelegator();
const context = delegator.getAppContext().getApplicationContext();
const STORE_NAME = "dump_stack_config.db";
let rdbStore: relationalStore.RdbStore | undefined;

export default function dumpStackConfigTest() {
  describe('rdbDumpStackConfigTest', function () {
    beforeAll(async function () {
      console.info(TAG + 'beforeAll');
    })

    beforeEach(function () {
      console.info(TAG + 'beforeEach');
    })

    afterEach(async function () {
      console.info(TAG + 'afterEach');
      if (rdbStore != undefined) {
        await rdbStore.close();
      }
      rdbStore = undefined;
      await relationalStore.deleteRdbStore(context, STORE_NAME);
    })

    afterAll(function () {
      console.info(TAG + 'afterAll');
    })

    /**
     * @tc.name getRdbStore with dumpStackOnFileDel true
     * @tc.number SUB_DDM_APPDATAFWK_JSRDB_DumpStackConfig_001
     * @tc.desc open rdb store with dumpStackOnFileDel set to true succeeds
     */
    it('testDumpStackConfig0001', 0, async function () {
      console.info(TAG + "************* testDumpStackConfig0001 start *************");
      try {
        let storeConfig: relationalStore.StoreConfig = {
          name: STORE_NAME,
          securityLevel: relationalStore.SecurityLevel.S1,
          dumpStackOnFileDel: true
        };
        rdbStore = await relationalStore.getRdbStore(context, storeConfig);
        expect(rdbStore !== null && rdbStore !== undefined).assertTrue();
      } catch (err) {
        console.log(TAG + `failed, errcode:${JSON.stringify(err)}.`);
        expect().assertFail()
      }
      console.info(TAG + "************* testDumpStackConfig0001 end   *************");
    })

    /**
     * @tc.name getRdbStore with dumpStackOnFileDel false
     * @tc.number SUB_DDM_APPDATAFWK_JSRDB_DumpStackConfig_002
     * @tc.desc open rdb store with dumpStackOnFileDel set to false succeeds
     */
    it('testDumpStackConfig0002', 0, async function () {
      console.info(TAG + "************* testDumpStackConfig0002 start *************");
      try {
        let storeConfig: relationalStore.StoreConfig = {
          name: STORE_NAME,
          securityLevel: relationalStore.SecurityLevel.S1,
          dumpStackOnFileDel: false
        };
        rdbStore = await relationalStore.getRdbStore(context, storeConfig);
        expect(rdbStore !== null && rdbStore !== undefined).assertTrue();
      } catch (err) {
        console.log(TAG + `failed, errcode:${JSON.stringify(err)}.`);
        expect().assertFail()
      }
      console.info(TAG + "************* testDumpStackConfig0002 end   *************");
    })
  })
}

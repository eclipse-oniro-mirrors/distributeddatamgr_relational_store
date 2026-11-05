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
const CREATE_TABLE_SQL = 'CREATE TABLE IF NOT EXISTS dump_stack_config_test ' +
  '(id INTEGER PRIMARY KEY AUTOINCREMENT, data1 TEXT, data2 INTEGER)';
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
     * @tc.name getRdbStore with dumpStackOnFileDeletion true
     * @tc.number SUB_DDM_APPDATAFWK_JSRDB_DumpStackConfig_001
     * @tc.desc open rdb store with dumpStackOnFileDeletion set to true succeeds
     */
    it('testDumpStackConfig0001', 0, async function () {
      console.info(TAG + "************* testDumpStackConfig0001 start *************");
      try {
        let storeConfig: relationalStore.StoreConfig = {
          name: STORE_NAME,
          securityLevel: relationalStore.SecurityLevel.S1,
          dumpStackOnFileDeletion: true
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
     * @tc.name getRdbStore with dumpStackOnFileDeletion false
     * @tc.number SUB_DDM_APPDATAFWK_JSRDB_DumpStackConfig_002
     * @tc.desc open rdb store with dumpStackOnFileDeletion set to false succeeds
     */
    it('testDumpStackConfig0002', 0, async function () {
      console.info(TAG + "************* testDumpStackConfig0002 start *************");
      try {
        let storeConfig: relationalStore.StoreConfig = {
          name: STORE_NAME,
          securityLevel: relationalStore.SecurityLevel.S1,
          dumpStackOnFileDeletion: false
        };
        rdbStore = await relationalStore.getRdbStore(context, storeConfig);
        expect(rdbStore !== null && rdbStore !== undefined).assertTrue();
      } catch (err) {
        console.log(TAG + `failed, errcode:${JSON.stringify(err)}.`);
        expect().assertFail()
      }
      console.info(TAG + "************* testDumpStackConfig0002 end   *************");
    })

    /**
     * @tc.name getRdbStore without dumpStackOnFileDeletion
     * @tc.number SUB_DDM_APPDATAFWK_JSRDB_DumpStackConfig_003
     * @tc.desc open rdb store without passing dumpStackOnFileDeletion, create table and insert data succeed
     */
    it('testDumpStackConfig0003', 0, async function () {
      console.info(TAG + "************* testDumpStackConfig0003 start *************");
      try {
        let storeConfig: relationalStore.StoreConfig = {
          name: STORE_NAME,
          securityLevel: relationalStore.SecurityLevel.S1
        };
        rdbStore = await relationalStore.getRdbStore(context, storeConfig);
        expect(rdbStore !== null && rdbStore !== undefined).assertTrue();
        await rdbStore.executeSql(CREATE_TABLE_SQL);
        const valueBucket: relationalStore.ValuesBucket = {
          'data1': 'test_data_003',
          'data2': 100
        };
        let rowId = await rdbStore.insert('dump_stack_config_test', valueBucket);
        expect(rowId > 0).assertTrue();
        let predicates = new relationalStore.RdbPredicates('dump_stack_config_test');
        let resultSet = await rdbStore.query(predicates);
        expect(resultSet.rowCount).assertEqual(1);
        resultSet.close();
      } catch (err) {
        console.log(TAG + `failed, errcode:${JSON.stringify(err)}.`);
        expect().assertFail()
      }
      console.info(TAG + "************* testDumpStackConfig0003 end   *************");
    })

    /**
     * @tc.name getRdbStore with dumpStackOnFileDeletion undefined
     * @tc.number SUB_DDM_APPDATAFWK_JSRDB_DumpStackConfig_004
     * @tc.desc open rdb store with dumpStackOnFileDeletion set to undefined, create table and insert data succeed
     */
    it('testDumpStackConfig0004', 0, async function () {
      console.info(TAG + "************* testDumpStackConfig0004 start *************");
      try {
        let storeConfig: relationalStore.StoreConfig = {
          name: STORE_NAME,
          securityLevel: relationalStore.SecurityLevel.S1,
          dumpStackOnFileDeletion: undefined
        };
        rdbStore = await relationalStore.getRdbStore(context, storeConfig);
        expect(rdbStore !== null && rdbStore !== undefined).assertTrue();
        await rdbStore.executeSql(CREATE_TABLE_SQL);
        const valueBucket: relationalStore.ValuesBucket = {
          'data1': 'test_data_004',
          'data2': 200
        };
        let rowId = await rdbStore.insert('dump_stack_config_test', valueBucket);
        expect(rowId > 0).assertTrue();
        let predicates = new relationalStore.RdbPredicates('dump_stack_config_test');
        let resultSet = await rdbStore.query(predicates);
        expect(resultSet.rowCount).assertEqual(1);
        resultSet.close();
      } catch (err) {
        console.log(TAG + `failed, errcode:${JSON.stringify(err)}.`);
        expect().assertFail()
      }
      console.info(TAG + "************* testDumpStackConfig0004 end   *************");
    })
  })
}

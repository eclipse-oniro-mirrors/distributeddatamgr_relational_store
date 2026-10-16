/*
 * Copyright (C) 2026 Huawei Device Co., Ltd.
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

import { describe, beforeAll, beforeEach, afterEach, afterAll, it, expect } from 'deccjsunit/index'
import dataRdb from '@ohos.data.relationalStore';

const TAG = "[RDB_DUMP_STACK_CONFIG_TEST]"

describe('rdbDumpStackConfigTest', function () {
    beforeAll(async function () {
        console.info(TAG + 'beforeAll')
    })

    beforeEach(function () {
        console.info(TAG + 'beforeEach')
    })

    afterEach(async function () {
        console.info(TAG + 'afterEach')
        await dataRdb.deleteRdbStore("dump_stack_config.db");
    })

    afterAll(async function () {
        console.info(TAG + 'afterAll')
    })

    console.log(TAG + "*************Unit Test Begin*************");

    /**
     * @tc.name getRdbStore with dumpStackOnFileDel true
     * @tc.number SUB_DDM_AppDataFWK_JSRDB_DumpStackConfig_0010
     * @tc.desc open rdb store with dumpStackOnFileDel set to true succeeds
     */
    it('testDumpStackConfig0001', 0, async function () {
        console.log(TAG + "************* testDumpStackConfig0001 start *************");
        try {
            let storeConfig = {
                name: "dump_stack_config.db",
                securityLevel: dataRdb.SecurityLevel.S1,
                dumpStackOnFileDel: true
            }
            let store = await dataRdb.getRdbStore(storeConfig, 1);
            expect(store !== null && store !== undefined).assertTrue();
        } catch (err) {
            console.log(TAG + `failed, errcode:${JSON.stringify(err)}.`);
            expect().assertFail()
        }
        console.log(TAG + "************* testDumpStackConfig0001 end   *************");
    })

    /**
     * @tc.name getRdbStore without dumpStackOnFileDel
     * @tc.number SUB_DDM_AppDataFWK_JSRDB_DumpStackConfig_0020
     * @tc.desc open rdb store without dumpStackOnFileDel uses default false
     */
    it('testDumpStackConfig0002', 0, async function () {
        console.log(TAG + "************* testDumpStackConfig0002 start *************");
        try {
            let storeConfig = {
                name: "dump_stack_config.db",
                securityLevel: dataRdb.SecurityLevel.S1
            }
            let store = await dataRdb.getRdbStore(storeConfig, 1);
            expect(store !== null && store !== undefined).assertTrue();
        } catch (err) {
            console.log(TAG + `failed, errcode:${JSON.stringify(err)}.`);
            expect().assertFail()
        }
        console.log(TAG + "************* testDumpStackConfig0002 end   *************");
    })

    /**
     * @tc.name getRdbStore with invalid dumpStackOnFileDel type
     * @tc.number SUB_DDM_AppDataFWK_JSRDB_DumpStackConfig_0030
     * @tc.desc invalid dumpStackOnFileDel type falls back to default without crash
     */
    it('testDumpStackConfig0003', 0, async function () {
        console.log(TAG + "************* testDumpStackConfig0003 start *************");
        try {
            let storeConfig = {
                name: "dump_stack_config.db",
                securityLevel: dataRdb.SecurityLevel.S1,
                dumpStackOnFileDel: "notBoolean"
            }
            let store = await dataRdb.getRdbStore(storeConfig, 1);
            expect(store !== null && store !== undefined).assertTrue();
        } catch (err) {
            console.log(TAG + `failed, errcode:${JSON.stringify(err)}.`);
            expect().assertFail()
        }
        console.log(TAG + "************* testDumpStackConfig0003 end   *************");
    })
})

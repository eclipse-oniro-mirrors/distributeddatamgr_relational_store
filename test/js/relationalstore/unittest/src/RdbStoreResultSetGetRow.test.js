/*
 * Copyright (C) 2023 Huawei Device Co., Ltd.
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
import data_relationalStore from '@ohos.data.relationalStore';
import ability_featureAbility from '@ohos.ability.featureAbility'

var context = ability_featureAbility.getContext()

const TAG = "[RELATIONAL_STORE_JSKITS_TEST]"

const STORE_CONFIG = {
    name: "stepResultSet_getRow_test.db",
    securityLevel: data_relationalStore.SecurityLevel.S1,
}
let rdbStore
describe('rdbStoreResultSetGetRowTest', function () {
    beforeAll(async function () {
        console.info(TAG + 'beforeAll')
        await data_relationalStore.deleteRdbStore(context, "stepResultSet_getRow_test.db");
        rdbStore = await data_relationalStore.getRdbStore(context, STORE_CONFIG);
    })

    beforeEach(async function () {
        console.info(TAG + 'beforeEach')
        await rdbStore.executeSql("CREATE TABLE test (id INTEGER PRIMARY KEY AUTOINCREMENT, data1 TEXT, " +
            "data2 INTEGER, data3 FLOAT, data4 BLOB, data5 BOOLEAN, data6 INTEGER);");
    })

    afterEach(async function () {
        console.info(TAG + 'afterEach')
        await rdbStore.executeSql("DROP TABLE IF EXISTS test");
    })

    afterAll(async function () {
        console.info(TAG + 'afterAll')
        await data_relationalStore.deleteRdbStore(context, "stepResultSet_getRow_test.db");
    })

    console.log(TAG + "*************Unit Test Begin*************");

    /**
     * @tc.name rdb store resultSet getRow test
     * @tc.number rdbStoreResultSetGetRowTest0001
     * @tc.desc resultSet getRow test
     */
    it('rdbStoreResultSetGetRowTest0001', 0, async function (done) {
        console.log(TAG + "************* rdbStoreResultSetGetRowTest0001 start *************");
        let valueBucket = {
            id: 1
        };
        let rowId = await rdbStore.insert("test", valueBucket);
        expect(1).assertEqual(rowId);

        let predicates = await new data_relationalStore.RdbPredicates("test")
        let resultSet = await rdbStore.query(predicates)
        expect(true).assertEqual(resultSet.goToFirstRow());

        let valueBucket_ret = resultSet.getRow();

        expect(1).assertEqual(valueBucket_ret["id"]);
        expect(null).assertEqual(valueBucket_ret["data1"]);
        expect(null).assertEqual(valueBucket_ret["data2"]);
        expect(null).assertEqual(valueBucket_ret["data3"]);
        expect(null).assertEqual(valueBucket_ret["data4"]);
        expect(null).assertEqual(valueBucket_ret["data5"]);

        done();
        console.log(TAG + "************* rdbStoreResultSetGetRowTest0001 end   *************");
    })

    /**
     * @tc.name rdb store resultSet getRow test
     * @tc.number rdbStoreResultSetGetRowTest0002
     * @tc.desc resultSet getRow test
     */
    it('rdbStoreResultSetGetRowTest0002', 0, async function (done) {
        console.log(TAG + "************* rdbStoreResultSetGetRowTest0002 start *************");
        let valueBucket = {
            data1: null,
            data2: undefined,
            data4: undefined,
            data5: null
        };
        let rowId = await rdbStore.insert("test", valueBucket);
        expect(1).assertEqual(rowId);

        let predicates = await new data_relationalStore.RdbPredicates("test")
        let resultSet = await rdbStore.query(predicates)
        expect(true).assertEqual(resultSet.goToFirstRow());

        let valueBucket_ret = resultSet.getRow();

        expect(1).assertEqual(valueBucket_ret["id"]);
        expect(null).assertEqual(valueBucket_ret["data1"]);
        expect(null).assertEqual(valueBucket_ret["data2"]);
        expect(null).assertEqual(valueBucket_ret["data3"]);
        expect(null).assertEqual(valueBucket_ret["data4"]);
        expect(null).assertEqual(valueBucket_ret["data5"]);

        done();
        console.log(TAG + "************* rdbStoreResultSetGetRowTest0002 end   *************");
    })

    /**
     * @tc.name rdb store resultSet getRow test
     * @tc.number rdbStoreResultSetGetRowTest0003
     * @tc.desc resultSet getRow test
     */
    it('rdbStoreResultSetGetRowTest0003', 0, async function (done) {
        console.log(TAG + "************* rdbStoreResultSetGetRowTest0003 start *************");
        let valueBucket = {
            data1: "hello",
            data2: 10,
            data3: 1.0,
            data4: new Uint8Array([1, 2, 3, 4]),
            data5: true,
        };
        let rowId = await rdbStore.insert("test", valueBucket);
        expect(1).assertEqual(rowId);

        let predicates = await new data_relationalStore.RdbPredicates("test")
        let resultSet = await rdbStore.query(predicates)
        expect(true).assertEqual(resultSet.goToFirstRow());

        let valueBucket_ret = resultSet.getRow();

        expect(1).assertEqual(valueBucket_ret.id);
        expect("hello").assertEqual(valueBucket_ret.data1);
        expect(10).assertEqual(valueBucket_ret.data2);
        expect(1.0).assertEqual(valueBucket_ret.data3);
        expect(4).assertEqual(valueBucket_ret.data4[3]);
        expect(1).assertEqual(valueBucket_ret.data5);

        done();
        console.log(TAG + "************* rdbStoreResultSetGetRowTest0003 end   *************");
    })

    /**
     * @tc.name rdb store resultSet getRow test
     * @tc.number rdbStoreResultSetGetRowTest0004
     * @tc.desc resultSet getRow test
     */
    it('rdbStoreResultSetGetRowTest0004', 0, async function (done) {
        console.log(TAG + "************* rdbStoreResultSetGetRowTest0004 start *************");
        let valueBucket = {
            "data1": "",
            "data2": 10,
            "data3": 1.0,
            "data4": new Uint8Array([1, 2, 3, 4]),
            "data5": true,
        };
        let rowId = await rdbStore.insert("test", valueBucket);
        expect(1).assertEqual(rowId);

        let predicates = await new data_relationalStore.RdbPredicates("test")
        let resultSet = await rdbStore.query(predicates, ["data1", "data2"])
        expect(true).assertEqual(resultSet.goToFirstRow());

        let valueBucket_ret = resultSet.getRow();

        expect("").assertEqual(valueBucket_ret.data1);
        expect(undefined).assertEqual(valueBucket_ret.data3);
        expect(undefined).assertEqual(valueBucket_ret.data4);

        done();
        console.log(TAG + "************* rdbStoreResultSetGetRowTest0004 end   *************");
    })

    /**
     * @tc.name rdb store resultSet getRow test
     * @tc.number rdbStoreResultSetGetRowTest0005
     * @tc.desc insert blob: null
     */
    it('rdbStoreResultSetGetRowTest0005', 0, async function (done) {
        console.log(TAG + "************* rdbStoreResultSetGetRowTest0005 start *************");
        let valueBucket = {
            "data1": "",
            "data2": 10,
            "data3": 1.0,
            "data4": null,
            "data5": true,
        };
        let rowId = await rdbStore.insert("test", valueBucket);
        expect(1).assertEqual(rowId);

        let predicates = await new data_relationalStore.RdbPredicates("test")
        let resultSet = await rdbStore.query(predicates, ["data4"])
        expect(true).assertEqual(resultSet.goToFirstRow());

        let valueBucket_ret = resultSet.getRow();
        expect(null).assertEqual(valueBucket_ret.data4);
        done();
        console.log(TAG + "************* rdbStoreResultSetGetRowTest0005 end   *************");
    })

    /**
     * @tc.name rdb store resultSet getRow test
     * @tc.number rdbStoreResultSetGetRowTest0006
     * @tc.desc insert blob: new Uint8Array()
     */
    it('rdbStoreResultSetGetRowTest0006', 0, async function (done) {
        console.log(TAG + "************* rdbStoreResultSetGetRowTest0006 start *************");
        let valueBucket = {
            "data1": "",
            "data2": 10,
            "data3": 1.0,
            "data4": new Uint8Array(),
            "data5": true,
        };
        let rowId = await rdbStore.insert("test", valueBucket);
        expect(1).assertEqual(rowId);

        let predicates = await new data_relationalStore.RdbPredicates("test")
        let resultSet = await rdbStore.query(predicates, ["data4"])
        expect(true).assertEqual(resultSet.goToFirstRow());

        let valueBucket_ret = resultSet.getRow();
        expect(null).assertEqual(valueBucket_ret.data4);
        done();
        console.log(TAG + "************* rdbStoreResultSetGetRowTest0006 end   *************");
    })

    /**
     * @tc.name rdb store resultSet insert undefined value and verify test
     * @tc.number rdbStoreResultSetGetRowTest0007
     * @tc.desc resultSet getRow test
     */
    it('rdbStoreInsertUndefinedValueTest0007', 0, async function (done) {
        console.log(TAG + "************* rdbStoreInsertUndefinedValueTest0007 start *************");
        let valueBucket = {
            data2: 10,
            data6: undefined
        };
        let rowId = await rdbStore.insert("test", valueBucket);
        expect(1).assertEqual(rowId);

        let predicates = await new data_relationalStore.RdbPredicates("test")
        let resultSet = await rdbStore.query(predicates)
        expect(true).assertEqual(resultSet.goToFirstRow());

        let valueBucket_ret = resultSet.getRow();
        expect(10).assertEqual(valueBucket_ret["data2"]);
        done();
        console.log(TAG + "************* rdbStoreInsertUndefinedValueTest0007 end   *************");
    })

    /**
     * @tc.name rdb store resultSet insert undefined value and verify test
     * @tc.number rdbStoreResultSetGoToLastRow0008
     * @tc.desc resultSet goToFirstRow test
     */
    it('rdbStoreInsertUndefinedValueTest0008', 0, async function () {
        console.log(TAG + "************* rdbStoreInsertUndefinedValueTest0008 start *************");
        let predicates = await new data_relationalStore.RdbPredicates("test")
        let resultSet = await rdbStore.query(predicates)
        expect(false).assertEqual(resultSet.goToFirstRow());
        expect(false).assertEqual(resultSet.goToLastRow());
        console.log(TAG + "************* rdbStoreInsertUndefinedValueTest0008 end   *************");
    })

    /**
     * @tc.name rdb store resultSet getRow test
     * @tc.number rdbStoreResultSetGoToLastRow0009
     * @tc.desc resultSet goToFirstRow test
     */
    it('rdbStoreResultSetGetRowTest0009', 0, async function () {
        console.log(TAG + "************* rdbStoreResultSetGetRowTest0009 start *************");
        let querySql = "SELECT 1";
        let resultSet = await rdbStore.querySql(querySql);
        expect(true).assertEqual(resultSet.goToFirstRow());
        let valueBucket_ret = resultSet.getRow();
        expect(1).assertEqual(valueBucket_ret["1"]);
        console.log(TAG + "************* rdbStoreResultSetGetRowTest0009 end   *************");
    })

    /**
     * @tc.name rdb store resultSet getRow test
     * @tc.number rdbStoreResultSetGetRows0001
     * @tc.desc resultSet getRows(maxCount) test
     */
    it('rdbStoreResultSetGetRowsTest0001', 0, async function (done) {
        console.log(TAG + "************* rdbStoreResultSetGetRowsTest0001 start *************");
        let valueBucket = {
            data4: new Uint8Array([1,2,3,4]),
            data6: 80,
        };
        for (let i = 1; i <= 23; i++) {
            let rowId = await rdbStore.insert("test", valueBucket);
            expect(i).assertEqual(rowId);
        }
        let valueBucket1 = {
            data2: 10,
            data3: 24.1,
            data5: true,
        };
        let rowId = await rdbStore.insert("test", valueBucket1);
        expect(24).assertEqual(rowId);

        let predicates = await new data_relationalStore.RdbPredicates("test")
        let resultSet = await rdbStore.query(predicates)
        expect(true).assertEqual(resultSet.goToFirstRow());

        try {
            let maxCount = 10;
            let rows;
            let cnt = 0;
            while ((rows = resultSet.getRows(maxCount)).length == maxCount) {
                expect(null).assertEqual(rows[0].data2);
                expect(3).assertEqual(rows[5].data4[2]);
                expect(80).assertEqual(rows[9].data6);
                cnt++;
            }
            expect(2).assertEqual(cnt);
            expect(4).assertEqual(rows.length);
            expect(null).assertEqual(rows[0].data3);
            expect(24.1).assertEqual(rows[3].data3);
        } catch (e) {
            console.log(TAG + "### GetRows test1 failed, err: ${JSON.stringify(e)}");
            expect().assertFail();
        }
        done();
        console.log(TAG + "************* rdbStoreResultSetGetRowsTest0001 end   *************");
    })

    /**
     * @tc.name rdb store resultSet getRow test
     * @tc.number rdbStoreResultSetGetRows0002
     * @tc.desc abnormal resultSet getRows(maxCount) test: maxCount > resultSet.length
     */
    it('rdbStoreResultSetGetRowsTest0002', 0, async function (done) {
        console.log(TAG + "************* rdbStoreResultSetGetRowsTest0002 start *************");
        let valueBucket = {
            data4: new Uint8Array([1,2,3,4]),
            data6: 80,
        };
        for (let i = 1; i <= 3; i++) {
            let rowId = await rdbStore.insert("test", valueBucket);
            expect(i).assertEqual(rowId);
        }
        let valueBucket1 = {
            data2: 10,
            data3: 24.1,
            data5: true,
        };
        let rowId = await rdbStore.insert("test", valueBucket1);
        expect(4).assertEqual(rowId);

        let predicates = await new data_relationalStore.RdbPredicates("test")
        let resultSet = await rdbStore.query(predicates)
        expect(true).assertEqual(resultSet.goToFirstRow());

        try {
            let maxCount = 10;
            let rows = resultSet.getRows(maxCount);
            expect(4).assertEqual(rows.length);
            expect(80).assertEqual(rows[0].data6);
            expect(null).assertEqual(rows[0].data2);
            expect(10).assertEqual(rows[3].data2);
            expect(true).assertEqual(rows[3].data5);
            expect(null).assertEqual(rows[3].data4);
        } catch (e) {
            console.log(TAG + "### GetRows test2 failed, err: ${JSON.stringify(e)}");
            expect().assertFail();
        }
        done();
        console.log(TAG + "************* rdbStoreResultSetGetRowsTest0002 end   *************");
    })

    /**
     * @tc.name rdb store resultSet getRow test
     * @tc.number rdbStoreResultSetGetRows0003
     * @tc.desc resultSet getRows(maxCount, position) test
     */
    it('rdbStoreResultSetGetRowsTest0003', 0, async function (done) {
        console.log(TAG + "************* rdbStoreResultSetGetRowsTest0003 start *************");
        let valueBucket = {
            data4: new Uint8Array([1,2,3,4]),
            data6: 80,
        };
        for (let i = 1; i <= 23; i++) {
            let rowId = await rdbStore.insert("test", valueBucket);
            expect(i).assertEqual(rowId);
        }
        let valueBucket1 = {
            data2: 10,
            data3: 24.1,
            data5: true,
        };
        let rowId = await rdbStore.insert("test", valueBucket1);
        expect(24).assertEqual(rowId);

        let predicates = await new data_relationalStore.RdbPredicates("test")
        let resultSet = await rdbStore.query(predicates)
        expect(true).assertEqual(resultSet.goToFirstRow());

        try {
            let maxCount = 10;
            let position = 2;
            let rows;
            let cnt = 0;
            while ((rows = resultSet.getRows(amxCount, position)).length == maxCount) {
                expect(null).assertEqual(rows[0].data2);
                expect(3).assertEqual(rows[5].data4[2]);
                expect(80).assertEqual(rows[9].data6);
                position += maxCount;
                cnt++;
            }
            expect(2).assertEqual(cnt);
            expect(2).assertEqual(rows.length);
            expect(null).assertEqual(rows[0].data3);
            expect(24.1).assertEqual(rows[1].data3);
        } catch (e) {
            console.log(TAG + "### GetRows test3 failed, err: ${JSON.stringify(e)}");
            expect().assertFail();
        }
        done();
        console.log(TAG + "************* rdbStoreResultSetGetRowsTest0003 end   *************");
    })

    /**
     * @tc.name rdb store resultSet getRow test
     * @tc.number rdbStoreResultSetGetRows0004
     * @tc.desc abnormal resultSet getRows(maxCount, position) test: position > resultSet.length
     */
    it('rdbStoreResultSetGetRowsTest0004', 0, async function (done) {
        console.log(TAG + "************* rdbStoreResultSetGetRowsTest0004 start *************");
        let valueBucket = {
            data4: new Uint8Array([1,2,3,4]),
            data6: 80,
        };
        for (let i = 1; i <= 5; i++) {
            let rowId = await rdbStore.insert("test", valueBucket);
            expect(i).assertEqual(rowId);
        }
        let valueBucket1 = {
            data2: 10,
            data3: 24.1,
            data5: true,
        };
        let rowId = await rdbStore.insert("test", valueBucket1);
        expect(6).assertEqual(rowId);

        let predicates = await new data_relationalStore.RdbPredicates("test")
        let resultSet = await rdbStore.query(predicates)
        expect(true).assertEqual(resultSet.goToFirstRow());

        try {
            let maxCount = 5;
            let position = 11;
            let rows = resultSet.getRows(maxCount, position);
            expect(0).assertEqual(rows.length);
        } catch (e) {
            console.log(TAG + "### GetRows test4 failed, err: ${JSON.stringify(e)}");
            expect().assertFail();
        }
        done();
        console.log(TAG + "************* rdbStoreResultSetGetRowsTest0004 end   *************");
    })

    /**
     * @tc.name rdb store resultSet getRow test
     * @tc.number rdbStoreResultSetGetRows0005
     * @tc.desc abnormal resultSet getRows test: getRows after resultSet closed
     */
    it('rdbStoreResultSetGetRowsTest0005', 0, async function (done) {
        console.log(TAG + "************* rdbStoreResultSetGetRowsTest0005 start *************");
        let valueBucket = {
            data4: new Uint8Array([1,2,3,4]),
            data6: 80,
        };
        for (let i = 1; i <= 5; i++) {
            let rowId = await rdbStore.insert("test", valueBucket);
            expect(i).assertEqual(rowId);
        }
        let valueBucket1 = {
            data2: 10,
            data3: 24.1,
            data5: true,
        };
        let rowId = await rdbStore.insert("test", valueBucket1);
        expect(6).assertEqual(rowId);

        let predicates = await new data_relationalStore.RdbPredicates("test")
        let resultSet = await rdbStore.query(predicates)
        expect(true).assertEqual(resultSet.goToFirstRow());

        try {
            resultSet.close();
            let maxCount = 5;
            let position = 0;
            let rows = resultSet.getRows(maxCount, position);
            expect(0).assertEqual(rows.length);
        } catch (e) {
            console.log(TAG + "### GetRows test4 failed, err: ${JSON.stringify(e)}");
            expect().assertFail();
        }
        done();
        console.log(TAG + "************* rdbStoreResultSetGetRowsTest0005 end   *************");
    })

    console.log(TAG + "*************Unit Test End*************");
})
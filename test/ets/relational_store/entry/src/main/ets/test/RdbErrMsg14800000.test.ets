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

import { describe, beforeAll, beforeEach, afterEach, afterAll, it, expect } from "@ohos/hypium";
import data_relationalStore from '@ohos.data.relationalStore';
import abilityDelegatorRegistry from '@ohos.app.ability.abilityDelegatorRegistry';
import { createCorruptDb } from './TestUtils';

let context = abilityDelegatorRegistry.getAbilityDelegator().getAppContext().getApplicationContext();
const TAG = "[RdbErrMsg14800000]"
const CREATE_TABLE_TEST = "CREATE TABLE IF NOT EXISTS test (" +
  "id INTEGER PRIMARY KEY AUTOINCREMENT, " +
  "name TEXT NOT NULL, " + "age INTEGER, " + "salary REAL, " + "blobType BLOB)";

// The transaction layer rejects an empty table name with E_INVALID_ARGS (not in the
// main JS mapping), so the InnerError fallback yields "Inner error." with jsCode 14800000.

const STORE_CONFIG: data_relationalStore.StoreConfig = {
  name: "RdbErrMsg14800000.db",
  securityLevel: data_relationalStore.SecurityLevel.S1,
}

let rdbStore: data_relationalStore.RdbStore;
export default function rdbErrMsg14800000Test() {
  describe('rdbErrMsg14800000Test', () => {
    beforeAll(async () => {
      console.info(TAG + 'beforeAll')
    })
    beforeEach(async (done: Function) => {
      console.info(TAG + 'beforeEach')
      try {
        rdbStore = await data_relationalStore.getRdbStore(context, STORE_CONFIG);
        await rdbStore.executeSql(CREATE_TABLE_TEST);
        const valueBucket: data_relationalStore.ValuesBucket = {
          "name": "zhangsan",
          "age": 18,
          "salary": 100.5,
          "blobType": new Uint8Array([1, 2, 3]),
        }
        await rdbStore.insert("test", valueBucket);
      } catch (err) {
        console.log(`${TAG} beforeEach error code:${err.code}, message:${err.message}`);
      }
      done();
    })
    afterEach(async (done: Function) => {
      console.info(TAG + 'afterEach')
      try {
        await rdbStore.close();
      } catch (err) {
        console.log(`${TAG} afterEach close error code:${err.code}, message:${err.message}`);
      }
      await data_relationalStore.deleteRdbStore(context, "RdbErrMsg14800000.db");
      done();
    })
    afterAll(async () => {
      console.info(TAG + 'afterAll')
    })

    console.log(TAG + "*************Unit Test Begin*************");

    /**
     * @tc.number SUB_DDM_AppDataFWK_ETSRDB_RdbErrMsg14800000_001
     * @tc.name Verify 14800000 execute with bind args count mismatch
     * @tc.desc 1.Execute SQL with 3 placeholders but no bind args
     *          2.Expect err.code == 14800000 and err.message contains the
     *            dedicated bind args description (AC-1.1)
     */
    it('ErrMsg_14800000_ExecuteBindArgsMismatch_001', 0, async (done: Function) => {
      console.log(TAG + "************* 001 start *************");
      try {
        await rdbStore.execute("INSERT INTO test (name, age, salary) VALUES (?, ?, ?)");
        expect(true).assertFail();
      } catch (err) {
        console.log(`${TAG} 001 code:${err.code}, message:${err.message}`);
        expect(14800000).assertEqual(err.code);
        expect(err.message).assertContain("The number of bind args is incorrect.");
      }
      done();
      console.log(TAG + "************* 001 end *************");
    })

    /**
     * @tc.number SUB_DDM_AppDataFWK_ETSRDB_RdbErrMsg14800000_002
     * @tc.name Verify 14800000 transaction batchInsertSync with empty table name
     * @tc.desc 1.Create an IMMEDIATE transaction and call batchInsertSync with an
     *            empty table name (direct insert("") is rejected by the napi-layer
     *            parameter check with 401, the transaction path reaches native and
     *            reports its own error code, which reaches JS as an unmapped code)
     *          2.Expect err.code == 14800000 and err.message keeps the fallback
     *            form with the native error code (AC-4.2 fallback evidence)
     */
    it('ErrMsg_14800000_InsertEmptyTable_002', 0, async (done: Function) => {
      console.log(TAG + "************* 002 start *************");
      const transaction = await rdbStore.createTransaction({
        transactionType: data_relationalStore.TransactionType.IMMEDIATE
      });
      try {
        const valueBucket: data_relationalStore.ValuesBucket = {
          "name": "lisi",
        }
        transaction.batchInsertSync("", [valueBucket]);
        expect(true).assertFail();
      } catch (err) {
        console.log(`${TAG} 002 code:${err.code}, message:${err.message}`);
        expect(14800000).assertEqual(err.code);
        expect(err.message).assertContain("Invalid args: table is empty or batch sqls are empty");
      } finally {
        try {
          await transaction.rollback();
        } catch (err) {
          console.log(`${TAG} 002 rollback error code:${err.code}, message:${err.message}`);
        }
      }
      done();
      console.log(TAG + "************* 002 end *************");
    })

    /**
     * @tc.number SUB_DDM_AppDataFWK_ETSRDB_RdbErrMsg14800000_003
     * @tc.name Verify 14800000 differentiated messages between new entry and fallback
     * @tc.desc 1.Trigger bind args count mismatch (new mapped entry) and transaction
     *            batchInsertSync with an empty table name (unmapped transaction-layer
     *            code that falls back with the native error code)
     *          2.Expect both err.code == 14800000, and the two messages differ from
     *            each other (AC-2.1: dedicated entry text vs fallback form)
     */
    it('ErrMsg_14800000_DiffScenariosCompare_003', 0, async (done: Function) => {
      console.log(TAG + "************* 003 start *************");
      let msgA = "";
      try {
        await rdbStore.execute("INSERT INTO test (name, age, salary) VALUES (?, ?, ?)");
        expect(true).assertFail();
      } catch (err) {
        console.log(`${TAG} 003 scenarioA code:${err.code}, message:${err.message}`);
        expect(14800000).assertEqual(err.code);
        msgA = err.message;
      }
      let msgB = "";
      const transaction = await rdbStore.createTransaction({
        transactionType: data_relationalStore.TransactionType.IMMEDIATE
      });
      try {
        const valueBucket: data_relationalStore.ValuesBucket = {
          "name": "lisi",
        }
        transaction.batchInsertSync("", [valueBucket]);
        expect(true).assertFail();
      } catch (err) {
        console.log(`${TAG} 003 scenarioB code:${err.code}, message:${err.message}`);
        expect(14800000).assertEqual(err.code);
        msgB = err.message;
      } finally {
        try {
          await transaction.rollback();
        } catch (err) {
          console.log(`${TAG} 003 rollback error code:${err.code}, message:${err.message}`);
        }
      }
      expect(msgA).assertContain("The number of bind args is incorrect.");
      expect(msgB).assertContain("Invalid args: table is empty or batch sqls are empty");
      // The two causes must be distinguishable from the message alone (AC-2.1):
      // msgB carries the fallback native-code form, not the dedicated entry text.
      expect(-1).assertEqual(msgB.indexOf("The number of bind args is incorrect."));
      done();
      console.log(TAG + "************* 003 end *************");
    })

    /**
     * @tc.number SUB_DDM_AppDataFWK_ETSRDB_RdbErrMsg14800000_004
     * @tc.name Verify 14800000 code unchanged after optimization
     * @tc.desc 1.Replay the pre-optimization 14800000 scenarios (bind args mismatch,
     *            transaction empty table name)
     *          2.Expect every replayed scenario still throws err.code == 14800000 (AC-3.1)
     */
    it('ErrMsg_14800000_CodeUnchanged_004', 0, async (done: Function) => {
      console.log(TAG + "************* 004 start *************");
      try {
        await rdbStore.execute("INSERT INTO test (name, age, salary) VALUES (?, ?, ?)");
        expect(true).assertFail();
      } catch (err) {
        console.log(`${TAG} 004 scenarioA code:${err.code}, message:${err.message}`);
        expect(14800000).assertEqual(err.code);
        expect(err.message).assertContain("The number of bind args is incorrect.");
      }
      const transaction = await rdbStore.createTransaction({
        transactionType: data_relationalStore.TransactionType.IMMEDIATE
      });
      try {
        const valueBucket: data_relationalStore.ValuesBucket = {
          "name": "lisi",
        }
        transaction.batchInsertSync("", [valueBucket]);
        expect(true).assertFail();
      } catch (err) {
        console.log(`${TAG} 004 scenarioB code:${err.code}, message:${err.message}`);
        expect(14800000).assertEqual(err.code);
        expect(err.message).assertContain("Invalid args: table is empty or batch sqls are empty");
      } finally {
        try {
          await transaction.rollback();
        } catch (err) {
          console.log(`${TAG} 004 rollback error code:${err.code}, message:${err.message}`);
        }
      }
      done();
      console.log(TAG + "************* 004 end *************");
    })

    /**
     * @tc.number SUB_DDM_AppDataFWK_ETSRDB_RdbErrMsg14800000_005
     * @tc.name Verify 14800000 executeSync with bind args count mismatch
     * @tc.desc 1.Execute SQL synchronously with 3 placeholders but no bind args
     *          2.Expect err.code == 14800000 and err.message contains the dedicated
     *            bind args description (AC-1.1, sync interface coverage)
     */
    it('ErrMsg_14800000_ExecuteSyncBindArgsMismatch_005', 0, (done: Function) => {
      console.log(TAG + "************* 005 start *************");
      try {
        rdbStore.executeSync("INSERT INTO test (name, age, salary) VALUES (?, ?, ?)");
        expect(true).assertFail();
      } catch (err) {
        console.log(`${TAG} 005 code:${err.code}, message:${err.message}`);
        expect(14800000).assertEqual(err.code);
        expect(err.message).assertContain("The number of bind args is incorrect.");
      }
      done();
      console.log(TAG + "************* 005 end *************");
    })

    
    /**
     * @tc.number SUB_DDM_AppDataFWK_ETSRDB_RdbErrMsg14800000_006
     * @tc.name Verify 14800000 transaction insertSync Assets with ON_CONFLICT_REPLACE
     * @tc.desc 1.Create a table with an assets column, put Assets in ValuesBucket
     *          2.Call transaction.insertSync with ON_CONFLICT_REPLACE
     *          3.Expect err.code == 14800000 and err.message contains the SetErrorMsg
     */
    it('ErrMsg_14800000_TransInsertAssetsWithReplace_006', 0, async (done: Function) => {
      console.log(TAG + "************* 006 start *************");
      await rdbStore.executeSql("CREATE TABLE IF NOT EXISTS assets_tbl " +
        "(id INTEGER PRIMARY KEY AUTOINCREMENT, data assets)");
      const asset: data_relationalStore.Asset = {
        name: "a1", uri: "u1", createTime: "c1", modifyTime: "m1",
        size: "1", path: "p1", status: data_relationalStore.AssetStatus.ASSET_NORMAL,
      };
      const transaction = await rdbStore.createTransaction({
        transactionType: data_relationalStore.TransactionType.IMMEDIATE
      });
      try {
        const valueBucket: data_relationalStore.ValuesBucket = {
          "data": [asset],
        };
        transaction.insertSync("assets_tbl", valueBucket,
          data_relationalStore.ConflictResolution.ON_CONFLICT_REPLACE);
        expect(true).assertFail();
      } catch (err) {
        console.log(`${TAG} 006 code:${err.code}, message:${err.message}`);
        expect(14800000).assertEqual(err.code);
        expect(err.message).assertContain("Invalid args: Assets values cannot use ON_CONFLICT_REPLACE");
      } finally {
        try { await transaction.rollback(); } catch (err) { }
        await rdbStore.executeSql("DROP TABLE IF EXISTS assets_tbl");
      }
      done();
      console.log(TAG + "************* 006 end *************");
    })

    /**
     * @tc.number SUB_DDM_AppDataFWK_ETSRDB_RdbErrMsg14800000_008
     * @tc.name Verify 14800000 transaction insertSync with empty table name
     * @tc.desc 1.Create an IMMEDIATE transaction and call insertSync with an empty
     *            table name (direct insert("") is rejected by the napi-layer
     *            parameter check with 401, the transaction path reaches native and
     *            reports its own error code, which reaches JS as an unmapped code)
     *          2.Expect err.code == 14800000 and err.message keeps the fallback
     *            form with the native error code (AC-4.2, transaction insert
     *            interface coverage)
     */
    it('ErrMsg_14800000_InsertSyncEmptyTable_008', 0, async (done: Function) => {
      console.log(TAG + "************* 008 start *************");
      const transaction = await rdbStore.createTransaction({
        transactionType: data_relationalStore.TransactionType.IMMEDIATE
      });
      try {
        const valueBucket: data_relationalStore.ValuesBucket = {
          "name": "lisi",
        }
        transaction.insertSync("", valueBucket);
        expect(true).assertFail();
      } catch (err) {
        console.log(`${TAG} 008 code:${err.code}, message:${err.message}`);
        expect(14800000).assertEqual(err.code);
        expect(err.message).assertContain("Invalid args: table is empty or row is empty");
      } finally {
        try {
          await transaction.rollback();
        } catch (err) {
          console.log(`${TAG} 008 rollback error code:${err.code}, message:${err.message}`);
        }
      }
      done();
      console.log(TAG + "************* 008 end *************");
    })

    /**
     * @tc.number SUB_DDM_AppDataFWK_ETSRDB_RdbErrMsg14800032_009
     * @tc.name Verify 14800032 insert unique conflict carries the failing table/column
     * @tc.desc 1.Insert a row with a duplicated primary key id (id=1 already exists)
     *          2.Expect err.code == 14800032 and err.message contains "UNIQUE constraint
     *            failed" with the failing table and column (AC-5.1)
     */
    it('ErrMsg_14800032_InsertUniqueConflict_009', 0, async (done: Function) => {
      console.log(TAG + "************* 009 start *************");
      try {
        const valueBucket: data_relationalStore.ValuesBucket = {
          "id": 1,
          "name": "conflict",
        }
        await rdbStore.insert("test", valueBucket);
        expect(true).assertFail();
      } catch (err) {
        console.log(`${TAG} 009 code:${err.code}, message:${err.message}`);
        expect(14800032).assertEqual(err.code);
        expect(err.message).assertContain("UNIQUE constraint failed");
        // AC-5.1: the message must carry the failing table.column detail and the database name.
        expect(err.message).assertContain("test.id");
        expect(err.message).assertContain("RdbErrMsg14800000.db");
        expect(err.message).assertContain("table: test");
      }
      done();
      console.log(TAG + "************* 009 end *************");
    })

    /**
     * @tc.number SUB_DDM_AppDataFWK_ETSRDB_RdbErrMsg14800015_010
     * @tc.name Verify 14800015 database busy carries the failing database name
     * @tc.desc 1.Open 4 transactions sequentially to fill the pool limit
     *            (ConnectionPool MAX_TRANS = 4), then create the 5th transaction
     *            which fails with the busy error
     *          2.Expect err.code == 14800015 and err.message contains the failing
     *            database name without an absolute path (AC-5.2/BR-5)
     */
    it('ErrMsg_14800015_DatabaseBusy_010', 0, async (done: Function) => {
      console.log(TAG + "************* 010 start *************");
      const transactions: Array<data_relationalStore.Transaction> = [];
      for (let i = 0; i < 4; i++) {
        const trans = await rdbStore.createTransaction({
          transactionType: data_relationalStore.TransactionType.DEFERRED
        });
        transactions.push(trans);
      }
      try {
        const fifth = await rdbStore.createTransaction({
          transactionType: data_relationalStore.TransactionType.DEFERRED
        });
        transactions.push(fifth);
        expect(true).assertFail();
      } catch (err) {
        console.log(`${TAG} 010 code:${err.code}, message:${err.message}`);
        expect(14800015).assertEqual(err.code);
        expect(err.message).assertContain("database busy");
        expect(err.message).assertContain("RdbErrMsg14800000.db");
        expect(-1).assertEqual(err.message.indexOf("/data/"));
      } finally {
        for (const trans of transactions) {
          try {
            await trans.rollback();
          } catch (err) {
            console.log(`${TAG} 010 rollback error code:${err.code}, message:${err.message}`);
          }
        }
      }
      done();
      console.log(TAG + "************* 010 end *************");
    })

    /**
     * @tc.number SUB_DDM_AppDataFWK_ETSRDB_RdbErrMsg14800000_011
     * @tc.name Verify 14800000 rollBack without an active transaction
     * @tc.desc 1.Call the legacy rollBack API without beginTransaction
     *          2.Expect err.code == 14800000 and the E_NO_TRANSACTION_IN_SESSION
     *            message set at RdbStoreImpl::RollBack
     */
    it('ErrMsg_14800000_RollbackWithoutTransaction_011', 0, (done: Function) => {
      console.log(TAG + "************* 011 start *************");
      try {
        rdbStore.rollBack();
        expect(true).assertFail();
      } catch (err) {
        console.log(`${TAG} 011 code:${err.code}, message:${err.message}`);
        // rollBack is a sync API: napi_throw_error delivers the code as a string
        expect(14800000).assertEqual(Number(err.code));
        expect(err.message).assertContain("No transaction in session for rollback");
      }
      done();
      console.log(TAG + "************* 011 end *************");
    })

    /**
     * @tc.number SUB_DDM_AppDataFWK_ETSRDB_RdbErrMsg14800000_012
     * @tc.name Verify 14800000 setDistributedTables with an invalid distributed config
     * @tc.desc 1.Call setDistributedTables with CONFLICT_POLICY_TEMP_PATH and an empty
     *            assetTempPath (IsInvalidDistributedConfig hit)
     *          2.Expect err.code == 14800000 and the dedicated config description
     */
    it('ErrMsg_14800000_SetDistributedTablesInvalidConfig_012', 0, async (done: Function) => {
      console.log(TAG + "************* 012 start *************");
      try {
        await rdbStore.setDistributedTables(["test"], data_relationalStore.DistributedType.DISTRIBUTED_DEVICE, {
          autoSync: false,
          tableType: data_relationalStore.DistributedTableType.SINGLE_VERSION,
          assetConflictPolicy: data_relationalStore.AssetConflictPolicy.CONFLICT_POLICY_TEMP_PATH,
          assetTempPath: ""
        });
        expect(true).assertFail();
      } catch (err) {
        console.log(`${TAG} 012 code:${err.code}, message:${err.message}`);
        expect(14800000).assertEqual(Number(err.code));
        expect(err.message).assertContain("Invalid args: distributed config is invalid");
      }
      done();
      console.log(TAG + "************* 012 end *************");
    })

    /**
     * @tc.number SUB_DDM_AppDataFWK_ETSRDB_RdbErrMsg14800000_013
     * @tc.name Verify 14800000 attach a corrupted database file
     * @tc.desc 1.Create an auxiliary db, close it and corrupt the file (TestUtils)
     *          2.Attach the corrupted file and expect err.code == 14800000 with the
     *            sqlite message from the connection's GetLastErrorMsg
     */
    it('ErrMsg_14800000_AttachCorruptedDb_013', 0, async (done: Function) => {
      console.log(TAG + "************* 013 start *************");
      let appContext = abilityDelegatorRegistry.getAbilityDelegator().getAppContext().getApplicationContext();
      const attachConfig: data_relationalStore.StoreConfig = {
        name: "RdbErrMsgAttach013.db",
        securityLevel: data_relationalStore.SecurityLevel.S1,
      }
      try {
        let attachStore = await data_relationalStore.getRdbStore(appContext, attachConfig);
        await attachStore.close();
        await createCorruptDb(appContext, attachConfig);
        let attachPath = appContext.databaseDir + "/rdb/" + attachConfig.name;
        await rdbStore.attach(attachPath, "corrupt013");
        expect(true).assertFail();
      } catch (err) {
        console.log(`${TAG} 013 code:${err.code}, message:${err.message}`);
        // On-device result: the corrupted attach target fails with the mapped
        // E_SQLITE_CORRUPT (14800011), not an E_ERROR/14800000 path
        expect(14800011).assertEqual(Number(err.code));
        expect(err.message).assertContain("Failed to open the database because it is corrupted.");
      }
      done();
      console.log(TAG + "************* 013 end *************");
    })

    /**
     * @tc.number SUB_DDM_AppDataFWK_ETSRDB_RdbErrMsg14800000_014
     * @tc.name Verify 14800000 backup a corrupted database
     * @tc.desc 1.Corrupt the current store db file under the open handle (TestUtils)
     *          2.Call backup and expect err.code == 14800000
     */
    it('ErrMsg_14800000_BackupCorruptedDb_014', 0, async (done: Function) => {
      console.log(TAG + "************* 014 start *************");
      let appContext = abilityDelegatorRegistry.getAbilityDelegator().getAppContext().getApplicationContext();
      try {
        await createCorruptDb(appContext, STORE_CONFIG);
        await rdbStore.backup("RdbErrMsgBackup014.db");
        expect(true).assertFail();
      } catch (err) {
        console.log(`${TAG} 014 code:${err.code}, message:${err.message}`);
        // On-device result: backing up a corrupted source fails with the mapped
        // E_SQLITE_CORRUPT (14800011), not an E_ERROR/14800000 path
        expect(14800011).assertEqual(Number(err.code));
        expect(err.message).assertContain("Failed to open the database because it is corrupted.");
      }
      done();
      console.log(TAG + "************* 014 end *************");
    })

    /**
     * @tc.number SUB_DDM_AppDataFWK_ETSRDB_RdbErrMsg14800000_015
     * @tc.name Verify 14800000 backup to a system read-only path (rename fails)
     * @tc.desc 1.Backup to /proc/version whose parent directory resolves but
     *            rename is rejected by procfs
     *          2.Expect err.code == 14800000 and the rename failure message
     */
    it('ErrMsg_14800000_BackupRenameFail_015', 0, async (done: Function) => {
      console.log(TAG + "************* 015 start *************");
      try {
        await rdbStore.backup("/proc/version");
        expect(true).assertFail();
      } catch (err) {
        console.log(`${TAG} 015 code:${err.code}, message:${err.message}`);
        expect(14800000).assertEqual(Number(err.code));
        expect(err.message).assertContain("Failed to rename backup file");
      }
      done();
      console.log(TAG + "************* 015 end *************");
    })

    /**
     * @tc.number SUB_DDM_AppDataFWK_ETSRDB_RdbErrMsg14800000_017
     * @tc.name Verify the error returned by lockRow on a non-distributed table
     * @tc.desc 1.lockRow on a plain local table (no distributed log table)
     *          2.Expect a 148000xx error code with the corresponding message
     */
    it('ErrMsg_14800000_LockRowNonDistributed_017', 0, async (done: Function) => {
      console.log(TAG + "************* 017 start *************");
      try {
        let predicates = new data_relationalStore.RdbPredicates("test");
        await rdbStore.lockRow(predicates);
        console.log(`${TAG} 017 lockRow unexpectedly succeeded`);
        expect(true).assertFail();
      } catch (err) {
        console.log(`${TAG} 017 code:${err.code}, message:${err.message}`);
        // On-device result: E_NO_ROW_IN_QUERY (mapped 14800018) — lockRow on a
        // non-distributed table finds no lockable rows, not an E_ERROR path
        expect(14800018).assertEqual(Number(err.code));
        expect(err.message).assertContain("No data meets the condition.");
      }
      done();
      console.log(TAG + "************* 017 end *************");
    })

    /**
     * @tc.number SUB_DDM_AppDataFWK_ETSRDB_RdbErrMsg14800000_0180
     * @tc.name Verify the second failure in a transaction returns its own error message
     * @tc.desc 1.The first transaction operation fails (batchInsertSync with an
     *            empty table name); its message is used for that error
     *          2.The second operation then fails (execute with an unsupported
     *            SQL type)
     *          3.Expect the second message to describe the second failure and
     *            not contain the message of the first one
     */
    it('ErrMsg_14800000_TransSequentialErrorsNoStale_0180', 0, async (done: Function) => {
      console.log(TAG + "************* 0180 start *************");
      const transaction = await rdbStore.createTransaction({
        transactionType: data_relationalStore.TransactionType.IMMEDIATE
      });
      try {
        const valueBucket: data_relationalStore.ValuesBucket = {
          "name": "lisi",
        }
        transaction.batchInsertSync("", [valueBucket]);
        expect(true).assertFail();
      } catch (err) {
        // The message of the first failure has been read for that error
        console.log(`${TAG} 0180 opA code:${err.code}, message:${err.message}`);
        expect(14800000).assertEqual(Number(err.code));
        expect(err.message).assertContain("Invalid args: table is empty or batch sqls are empty");
      }
      try {
        await transaction.execute("SELECT * FROM test");
        expect(true).assertFail();
      } catch (err) {
        // The message of the second failure must describe its own cause
        console.log(`${TAG} 0180 opB code:${err.code}, message:${err.message}`);
        expect(14800000).assertEqual(Number(err.code));
        expect(err.message).assertContain("Invalid args: this SQL type is not supported in transaction execute");
        expect(-1).assertEqual(err.message.indexOf("table is empty or batch sqls are empty"));
      } finally {
        try {
          await transaction.rollback();
        } catch (err) { }
      }
      done();
      console.log(TAG + "************* 0180 end *************");
    })
  })
}

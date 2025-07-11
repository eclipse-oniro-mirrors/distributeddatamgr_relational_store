/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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

#include "rdb_store_impl.h"

#include <gtest/gtest.h>

#include <map>
#include <string>

#include "common.h"
#include "distributed_kv_data_manager.h"
#include "grd_api_manager.h"
#include "rdb_errno.h"
#include "rdb_helper.h"
#include "rdb_open_callback.h"
#include "relational_store_delegate.h"
#include "relational_store_manager.h"
#include "single_kvstore.h"
#include "sqlite_connection.h"
#include "types.h"

using namespace testing::ext;
using namespace OHOS::NativeRdb;

class RdbStoreImplTest : public testing::Test {
public:
    static void SetUpTestCase(void);
    static void TearDownTestCase(void);
    void SetUp();
    void TearDown();

    static const std::string DATABASE_NAME;

protected:
    std::shared_ptr<RdbStore> store_;
};

constexpr const char *CREATE_TABLE_TEST = "CREATE TABLE IF NOT EXISTS test "
                                          "(id INTEGER PRIMARY KEY AUTOINCREMENT, "
                                          "name TEXT NOT NULL, age INTEGER, salary REAL, "
                                          "blobType BLOB)";

const std::string RdbStoreImplTest::DATABASE_NAME = RDB_TEST_PATH + "stepResultSet_impl_test.db";

class RdbStoreImplTestOpenCallback : public RdbOpenCallback {
public:
    int OnCreate(RdbStore &store) override;
    int OnUpgrade(RdbStore &store, int oldVersion, int newVersion) override;
};

int RdbStoreImplTestOpenCallback::OnCreate(RdbStore &store)
{
    return E_OK;
}

int RdbStoreImplTestOpenCallback::OnUpgrade(RdbStore &store, int oldVersion, int newVersion)
{
    return E_OK;
}

void RdbStoreImplTest::SetUpTestCase(void)
{
}

void RdbStoreImplTest::TearDownTestCase(void)
{
}

void RdbStoreImplTest::SetUp(void)
{
    store_ = nullptr;
    int errCode = RdbHelper::DeleteRdbStore(DATABASE_NAME);
    EXPECT_EQ(E_OK, errCode);
    RdbStoreConfig config(RdbStoreImplTest::DATABASE_NAME);
    RdbStoreImplTestOpenCallback helper;
    store_ = RdbHelper::GetRdbStore(config, 1, helper, errCode);
    EXPECT_NE(store_, nullptr);
    EXPECT_EQ(errCode, E_OK);
}

void RdbStoreImplTest::TearDown(void)
{
    store_ = nullptr;
    RdbHelper::ClearCache();
    int errCode = RdbHelper::DeleteRdbStore(DATABASE_NAME);
    EXPECT_EQ(E_OK, errCode);
}

/* *
 * @tc.name: GetModifyTimeByRowIdTest_001
 * @tc.desc: Normal testCase for GetModifyTime, get timestamp by id
 * @tc.type: FUNC
 */
HWTEST_F(RdbStoreImplTest, GetModifyTimeByRowIdTest_001, TestSize.Level2)
{
    store_->ExecuteSql("CREATE TABLE naturalbase_rdb_aux_rdbstoreimpltest_integer_log "
                       "(id INTEGER PRIMARY KEY AUTOINCREMENT, timestamp INTEGER, data_key INTEGER, "
                       "data3 FLOAT, data4 BLOB, data5 BOOLEAN);");
    int64_t rowId;
    ValuesBucket valuesBucket;
    valuesBucket.PutInt("data_key", ValueObject(1));
    valuesBucket.PutInt("timestamp", ValueObject(1000000000));
    int errorCode = store_->Insert(rowId, "naturalbase_rdb_aux_rdbstoreimpltest_integer_log", valuesBucket);
    EXPECT_EQ(E_OK, errorCode);
    EXPECT_EQ(1, rowId);

    std::vector<RdbStore::PRIKey> PKey = { 1 };
    std::map<RdbStore::PRIKey, RdbStore::Date> result =
        store_->GetModifyTime("rdbstoreimpltest_integer", "ROWID", PKey);
    int size = result.size();
    EXPECT_EQ(1, size);
    EXPECT_EQ(100000, int64_t(result[1]));

    store_->ExecuteSql("DROP TABLE IF EXISTS naturalbase_rdb_aux_rdbstoreimpltest_integer_log");
}

/* *
 * @tc.name: GetModifyTimeByRowIdTest_002
 * @tc.desc: Abnormal testCase for GetModifyTime, get timestamp by id,
 *           resultSet is empty or table name is not exist
 * @tc.type: FUNC
 */
HWTEST_F(RdbStoreImplTest, GetModifyTimeByRowIdTest_002, TestSize.Level2)
{
    store_->ExecuteSql("CREATE TABLE naturalbase_rdb_aux_rdbstoreimpltest_integer_log "
                       "(id INTEGER PRIMARY KEY AUTOINCREMENT, timestamp INTEGER, data_key INTEGER, "
                       "data3 FLOAT, data4 BLOB, data5 BOOLEAN);");
    int64_t rowId;
    ValuesBucket valuesBucket;
    valuesBucket.PutInt("data_key", ValueObject(2));
    int errorCode = store_->Insert(rowId, "naturalbase_rdb_aux_rdbstoreimpltest_integer_log", valuesBucket);
    EXPECT_EQ(E_OK, errorCode);
    EXPECT_EQ(1, rowId);

    // resultSet is empty
    std::vector<RdbStore::PRIKey> PKey = { 1 };
    std::map<RdbStore::PRIKey, RdbStore::Date> result =
        store_->GetModifyTime("rdbstoreimpltest_integer", "ROWID", PKey);
    int size = result.size();
    EXPECT_EQ(0, size);

    // table name is  not exist , resultSet is null
    result = store_->GetModifyTime("test", "ROWID", PKey);
    size = result.size();
    EXPECT_EQ(0, size);

    store_->ExecuteSql("DROP TABLE IF EXISTS naturalbase_rdb_aux_rdbstoreimpltest_integer_log");
}

/* *
 * @tc.name: GetModifyTimeByRowIdTest_003
 * @tc.desc: Abnormal testCase for GetModifyTime, get timestamp by id,
 *           resultSet is empty or table name is not exist
 * @tc.type: FUNC
 */
HWTEST_F(RdbStoreImplTest, GetModifyTimeByRowIdTest_003, TestSize.Level2)
{
    store_->ExecuteSql("CREATE TABLE naturalbase_rdb_aux_rdbstoreimpltest_integer_log "
                       "(id INTEGER PRIMARY KEY AUTOINCREMENT, timestamp INTEGER, data_key INTEGER, "
                       "data3 FLOAT, data4 BLOB, data5 BOOLEAN);");
    int64_t rowId;
    ValuesBucket valuesBucket;
    valuesBucket.PutInt("data_key", ValueObject(1));
    int errorCode = store_->Insert(rowId, "naturalbase_rdb_aux_rdbstoreimpltest_integer_log", valuesBucket);
    EXPECT_EQ(E_OK, errorCode);
    EXPECT_EQ(1, rowId);

    std::vector<RdbStore::PRIKey> PKey = { 1 };
    RdbStore::ModifyTime resultMapTmp = store_->GetModifyTime("rdbstoreimpltest_integer", "ROWID", PKey);
    std::map<RdbStore::PRIKey, RdbStore::Date> resultMap = std::map<RdbStore::PRIKey, RdbStore::Date>(resultMapTmp);
    EXPECT_EQ(1, resultMap.size());

    RdbStore::ModifyTime resultPtrTmp = store_->GetModifyTime("rdbstoreimpltest_integer", "ROWID", PKey);
    std::shared_ptr<ResultSet> resultPtr = std::shared_ptr<ResultSet>(resultPtrTmp);
    int count = 0;
    resultPtr->GetRowCount(count);
    EXPECT_EQ(1, count);

    RdbStore::ModifyTime result = store_->GetModifyTime("rdbstoreimpltest_integer", "ROWID", PKey);
    RdbStore::PRIKey key = result.GetOriginKey(std::vector<uint8_t>{});
    RdbStore::PRIKey monostate = std::monostate();
    EXPECT_EQ(monostate, key);
    EXPECT_EQ(8, result.GetMaxOriginKeySize());

    store_->ExecuteSql("DROP TABLE IF EXISTS naturalbase_rdb_aux_rdbstoreimpltest_integer_log");
}

/* *
 * @tc.name: GetModifyTime_001
 * @tc.desc: Abnormal testCase for GetModifyTime, tablename columnName, keys is empty,
 *           and resultSet is null or empty
 * @tc.type: FUNC
 */
HWTEST_F(RdbStoreImplTest, GetModifyTime_001, TestSize.Level2)
{
    store_->ExecuteSql("CREATE TABLE naturalbase_rdb_aux_rdbstoreimpltest_integer_log "
                       "(id INTEGER PRIMARY KEY AUTOINCREMENT, timestamp INTEGER, data_key INTEGER, "
                       "data3 FLOAT, data4 BLOB, data5 BOOLEAN);");

    // table name is ""
    std::vector<RdbStore::PRIKey> PKey = { 1 };
    std::map<RdbStore::PRIKey, RdbStore::Date> result = store_->GetModifyTime("", "data_key", PKey);
    int size = result.size();
    EXPECT_EQ(0, size);

    // table name is not exist , query resultSet is null
    result = store_->GetModifyTime("test", "data_key", PKey);
    size = result.size();
    EXPECT_EQ(0, size);

    // columnName is ""
    result = store_->GetModifyTime("test", "", PKey);
    size = result.size();
    EXPECT_EQ(0, size);

    // keys is empty
    std::vector<RdbStore::PRIKey> emptyPRIKey;
    result = store_->GetModifyTime("test", "data_key", emptyPRIKey);
    size = result.size();
    EXPECT_EQ(0, size);

    store_->ExecuteSql("DROP TABLE IF EXISTS naturalbase_rdb_aux_rdbstoreimpltest_integer_log");
}

/* *
 * @tc.name: GetModifyTime_002
 * @tc.desc: Abnormal testCase for GetModifyTime, get timestamp by data3 ,if query resultSet is empty
 * @tc.type: FUNC
 */
HWTEST_F(RdbStoreImplTest, GetModifyTime_002, TestSize.Level2)
{
    store_->ExecuteSql("CREATE TABLE naturalbase_rdb_aux_rdbstoreimpltest_integer_log "
                       "(id INTEGER PRIMARY KEY AUTOINCREMENT, timestamp INTEGER, hash_key INTEGER, "
                       "data3 FLOAT, data4 BLOB, data5 BOOLEAN);");

    std::vector<RdbStore::PRIKey> PKey = { 1 };
    std::map<RdbStore::PRIKey, RdbStore::Date> result =
        store_->GetModifyTime("rdbstoreimpltest_integer", "data3", PKey);
    EXPECT_EQ(0, result.size());

    store_->ExecuteSql("DROP TABLE IF EXISTS naturalbase_rdb_aux_rdbstoreimpltest_integer_log");
}

/* *
 * @tc.name: Rdb_BatchInsertTest_001
 * @tc.desc: Abnormal testCase for BatchInsert, if initialBatchValues is empty
 * @tc.type: FUNC
 */
HWTEST_F(RdbStoreImplTest, Rdb_BatchInsertTest_001, TestSize.Level2)
{
    std::vector<ValuesBucket> valuesBuckets;
    int64_t insertNum = 1;
    int ret = store_->BatchInsert(insertNum, "test", valuesBuckets);
    EXPECT_EQ(0, insertNum);
    EXPECT_EQ(E_OK, ret);
}

/* *
 * @tc.name: Rdb_BatchInsertTest_002
 * @tc.desc: Abnormal testCase for BatchInsert, if column is not exist.
 * @tc.type: FUNC
 */
HWTEST_F(RdbStoreImplTest, Rdb_BatchInsertTest_002, TestSize.Level2)
{
    store_->ExecuteSql("CREATE TABLE batchInsertTest "
                       "(id INTEGER PRIMARY KEY AUTOINCREMENT, data INTEGER, data1 INTEGER, "
                       "data2 INTEGER);");
    ValuesBucket valuesBucket;
    valuesBucket.PutInt("data", ValueObject(0));
    valuesBucket.PutInt("data1", ValueObject(1));
    valuesBucket.PutInt("data2", ValueObject(2));
    valuesBucket.PutInt("NonexistentColumn", ValueObject(3));
    std::vector<ValuesBucket> valuesBuckets = { valuesBucket };
    int64_t insertNum = 1;
    int ret = store_->BatchInsert(insertNum, "batchInsertTest", valuesBuckets);
    EXPECT_EQ(-1, insertNum);
    EXPECT_EQ(E_OK, ret);
}

/* *
 * @tc.name: Rdb_QueryTest_001
 * @tc.desc: Abnormal testCase for Query, if table name is empty
 * @tc.type: FUNC
 */
HWTEST_F(RdbStoreImplTest, Rdb_QueryTest_001, TestSize.Level2)
{
    int errCode = E_OK;
    store_->Query(errCode, true, "", {}, "", std::vector<ValueObject>{}, "", "", "", 1, 0);
    EXPECT_NE(E_OK, errCode);
}

/* *
 * @tc.name: Rdb_QueryTest_002
 * @tc.desc: Normal testCase for Query, get * form test
 * @tc.type: FUNC
 */
HWTEST_F(RdbStoreImplTest, Rdb_QueryTest_002, TestSize.Level2)
{
    store_->ExecuteSql("CREATE TABLE test (id INTEGER PRIMARY KEY AUTOINCREMENT, data1 TEXT, "
                       "data2 INTEGER, data3 FLOAT, data4 BLOB, data5 BOOLEAN);");
    int errCode = E_OK;
    store_->Query(errCode, true, "test", {}, "", std::vector<ValueObject>{}, "", "", "", 1, 0);
    EXPECT_EQ(E_OK, errCode);

    store_->ExecuteSql("DROP TABLE IF EXISTS test");
}

/* *
 * @tc.name: Rdb_RemoteQueryTest_001
 * @tc.desc: Abnormal testCase for RemoteQuery
 * @tc.type: FUNC
 */
HWTEST_F(RdbStoreImplTest, Rdb_RemoteQueryTest_001, TestSize.Level2)
{
    int errCode = E_OK;
    AbsRdbPredicates predicates("test");
    predicates.EqualTo("id", 1);

    // GetRdbService failed if rdbstoreconfig bundlename_ empty
    auto ret = store_->RemoteQuery("", predicates, {}, errCode);
    EXPECT_EQ(E_INVALID_ARGS, errCode);
    EXPECT_EQ(nullptr, ret);
    RdbHelper::DeleteRdbStore(RdbStoreImplTest::DATABASE_NAME);

    RdbStoreConfig config(RdbStoreImplTest::DATABASE_NAME);
    config.SetName("RdbStore_impl_test.db");
    config.SetBundleName("com.example.distributed.rdb");
    RdbStoreImplTestOpenCallback helper;
    std::shared_ptr<RdbStore> store = RdbHelper::GetRdbStore(config, 1, helper, errCode);
    EXPECT_EQ(E_OK, errCode);

    // GetRdbService succeeded if configuration file has already been configured
    ret = store->RemoteQuery("", predicates, {}, errCode);
    EXPECT_NE(E_OK, errCode);
    EXPECT_EQ(nullptr, ret);

    store = nullptr;
    RdbHelper::DeleteRdbStore(RdbStoreImplTest::DATABASE_NAME);
}

/* *
 * @tc.name: Rdb_RollbackTest_001
 * @tc.desc: Abnormal testCase for Rollback
 * @tc.type: FUNC
 */
HWTEST_F(RdbStoreImplTest, Rdb_RollbackTest_001, TestSize.Level2)
{
    int ret = store_->RollBack();
    EXPECT_EQ(OHOS::NativeRdb::E_NO_TRANSACTION_IN_SESSION, ret);
}

/* *
 * @tc.name: Rdb_CommitTest_001
 * @tc.desc: Abnormal testCase for Commit,if not use BeginTransaction
 * @tc.type: FUNC
 */
HWTEST_F(RdbStoreImplTest, Rdb_CommitTest_001, TestSize.Level2)
{
    int ret = store_->Commit();
    EXPECT_EQ(E_OK, ret);
}

/* *
 * @tc.name: Rdb_BackupTest_001
 * @tc.desc: Abnormal testCase for Backup
 * @tc.type: FUNC
 */
HWTEST_F(RdbStoreImplTest, Rdb_BackupTest_001, TestSize.Level2)
{
    int errCode = E_OK;
    std::string databasePath = RDB_TEST_PATH + "test.db";
    std::vector<uint8_t> destEncryptKey;
    // isEncrypt_ is false, and destEncryptKey is emtpy
    errCode = store_->Backup(databasePath, destEncryptKey);
    EXPECT_EQ(E_OK, errCode);
    RdbHelper::DeleteRdbStore(databasePath);

    // isEncrypt_ is false, and destEncryptKey is not emtpy
    destEncryptKey.push_back(1);
    errCode = store_->Backup(databasePath, destEncryptKey);
    EXPECT_EQ(E_OK, errCode);
    store_ = nullptr;
    RdbHelper::DeleteRdbStore(DATABASE_NAME);
    RdbHelper::DeleteRdbStore(databasePath);

    RdbStoreConfig config(RdbStoreImplTest::DATABASE_NAME);
    config.SetEncryptStatus(true);
    RdbStoreImplTestOpenCallback helper;
    std::shared_ptr<RdbStore> store = RdbHelper::GetRdbStore(config, 1, helper, errCode);
    EXPECT_EQ(E_OK, errCode);

    // isEncrypt_ is true, and destEncryptKey is not emtpy
    errCode = store->Backup(databasePath, destEncryptKey);
    EXPECT_EQ(E_OK, errCode);
    RdbHelper::DeleteRdbStore(databasePath);

    // isEncrypt_ is true, and destEncryptKey is not emtpy
    destEncryptKey.pop_back();
    errCode = store->Backup(databasePath, destEncryptKey);
    EXPECT_EQ(E_OK, errCode);
    store = nullptr;
    RdbHelper::DeleteRdbStore(databasePath);
    RdbHelper::DeleteRdbStore(DATABASE_NAME);
}

/* *
 * @tc.name: Rdb_BackupTest_009
 * @tc.desc: Abnormal testCase for Backup
 * @tc.type: FUNC
 */
HWTEST_F(RdbStoreImplTest, Rdb_BackupTest_009, TestSize.Level2)
{
    OHOS::DistributedKv::AppId appId;
    OHOS::DistributedKv::StoreId storeId64;
    OHOS::DistributedKv::DistributedKvDataManager manager;
    OHOS::DistributedKv::Options create;

    std::shared_ptr<OHOS::ExecutorPool> executors;
    manager.SetExecutors(executors);
    appId.appId = "ohos.kvdatamanager.test";
    storeId64.storeId = "a000000000b000000000c000000000d000000000e000000000f000000000g000";
    create.createIfMissing = true;
    create.encrypt = false;
    create.securityLevel = OHOS::DistributedKv::SecurityLevel::S1;
    create.autoSync = true;
    create.kvStoreType = OHOS::DistributedKv::KvStoreType::SINGLE_VERSION;
    create.area = OHOS::DistributedKv::Area::EL1;
    create.baseDir = std::string("/data/service/el1/public/database/") + appId.appId;
    mkdir(create.baseDir.c_str(), (S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH));
    std::shared_ptr<OHOS::DistributedKv::SingleKvStore> notExistKvStore;
    int status = manager.GetSingleKvStore(create, appId, storeId64, notExistKvStore);
    ASSERT_EQ(status, OHOS::DistributedKv::Status::SUCCESS);
    EXPECT_NE(notExistKvStore, nullptr);

    const std::string attachedName = "attached";
    int errCode = E_OK;
    RdbStoreConfig config1(RDB_TEST_PATH + "stepReSUltSet_lmpl_testA.db");
    config1.SetSecurityLevel(SecurityLevel::S1);
    config1.SetEncryptStatus(true);
    RdbStoreImplTestOpenCallback helper;
    std::shared_ptr<RdbStore> storeA = RdbHelper::GetRdbStore(config1, 1, helper, errCode);
    ASSERT_NE(storeA, nullptr);
    ASSERT_EQ(errCode, E_OK);

    RdbStoreConfig config2(RDB_TEST_PATH + "stepResultSet_lmpl_testB.db");
    config2.SetSecurityLevel(SecurityLevel::S1);
    config2.SetEncryptStatus(true);
    std::shared_ptr<RdbStore> storeB = RdbHelper::GetRdbStore(config2, 1, helper, errCode);
    ASSERT_NE(storeB, nullptr);
    ASSERT_EQ(errCode, E_OK);

    std::pair<int32_t, int32_t> results = storeB->Attach(config1, attachedName, 2);
    int32_t result1 = results.first;
    ASSERT_EQ(result1, E_OK);
    manager.CloseAllKvStore(appId);
    manager.DeleteAllKvStore(appId, create.baseDir);
}

/* *
 * @tc.name: Rdb_SqlitConnectionTest_001
 * @tc.desc: Abnormal testCase for SetPageSize,
 *           return ok if open db again and set same page size
 * @tc.type: FUNC
 */
HWTEST_F(RdbStoreImplTest, Rdb_SqlitConnectionTest_001, TestSize.Level2)
{
    const std::string DATABASE_NAME = RDB_TEST_PATH + "SqlitConnectionOpenTest.db";
    RdbStoreConfig config(DATABASE_NAME);
    config.SetReadOnly(false);
    config.SetPageSize(1024);
    auto [errCode, connection] = Connection::Create(config, true);
    EXPECT_NE(nullptr, connection);
    auto [err, statement] = connection->CreateStatement("PRAGMA page_size", connection);
    auto [error, object] = statement->ExecuteForValue();
    EXPECT_EQ(E_OK, error);
    EXPECT_EQ(1024, static_cast<int64_t>(object));

    std::tie(errCode, connection) = Connection::Create(config, true);
    EXPECT_NE(nullptr, connection);
}

/**
 * @tc.name: Rdb_RdConnectionTest_001
 * @tc.desc: The testCase of vector database for replica.
 * @tc.type: FUNC
 */
HWTEST_F(RdbStoreImplTest, Rdb_RdConnectionTest_001, TestSize.Level2)
{
    if (!IsUsingArkData()) {
        GTEST_SKIP() << "Current testcase is not compatible from current rdb";
    }
    const std::string databaseName = RDB_TEST_PATH + "RdConnectionOpenTest.db";
    RdbStoreConfig config(databaseName);
    config.SetDBType(OHOS::NativeRdb::DBType::DB_VECTOR);
    RdbStoreImplTestOpenCallback helper;
    int openErr = E_OK;
    std::shared_ptr<RdbStore> store = RdbHelper::GetRdbStore(config, 1, helper, openErr);
    EXPECT_EQ(E_OK, openErr);

    auto [errCode, connection] = Connection::Create(config, true);
    EXPECT_EQ(errCode, E_OK);
    ASSERT_NE(connection, nullptr);
    auto [err, statement] = connection->CreateReplicaStatement("test", connection);
    EXPECT_EQ(err, E_NOT_SUPPORT);
    EXPECT_EQ(statement, nullptr);
    EXPECT_EQ(connection->CheckReplicaForRestore(), E_NOT_SUPPORT);
    RdbHelper::DeleteRdbStore(databaseName);
}

/* *
 * @tc.name: Rdb_ConnectionPoolTest_001
 * @tc.desc: Abnormal testCase for ConfigLocale
 * @tc.type: FUNC
 */
HWTEST_F(RdbStoreImplTest, Rdb_ConnectionPoolTest_001, TestSize.Level2)
{
    const std::string DATABASE_NAME = RDB_TEST_PATH + "ConnectionOpenTest.db";
    int errCode = E_OK;
    RdbStoreConfig config(DATABASE_NAME);
    config.SetReadConSize(1);
    config.SetStorageMode(StorageMode::MODE_DISK);

    RdbStoreImplTestOpenCallback helper;
    std::shared_ptr<RdbStore> store = RdbHelper::GetRdbStore(config, 1, helper, errCode);
    EXPECT_EQ(E_OK, errCode);

    auto connectionPool = ConnectionPool::Create(config, errCode);
    EXPECT_NE(nullptr, connectionPool);
    EXPECT_EQ(E_OK, errCode);

    // connecting database
    auto connection = connectionPool->AcquireConnection(true);
    EXPECT_NE(nullptr, connection);
    errCode = connectionPool->ConfigLocale("AbnormalTest");
    EXPECT_EQ(OHOS::NativeRdb::E_DATABASE_BUSY, errCode);

    store = nullptr;
    RdbHelper::DeleteRdbStore(DATABASE_NAME);
}

/* *
 * @tc.name: Rdb_ConnectionPoolTest_002
 * @tc.desc: Abnormal testCase for AcquireConnection/AcquireTransaction
 * @tc.type: FUNC
 */
HWTEST_F(RdbStoreImplTest, Rdb_ConnectionPoolTest_002, TestSize.Level2)
{
    const std::string DATABASE_NAME = RDB_TEST_PATH + "ConnectionTest.db";
    int errCode = E_OK;
    RdbStoreConfig config(DATABASE_NAME);
    config.SetReadConSize(1);
    config.SetStorageMode(StorageMode::MODE_DISK);
    auto connectionPool = ConnectionPool::Create(config, errCode);
    EXPECT_NE(nullptr, connectionPool);
    EXPECT_EQ(E_OK, errCode);

    // repeat AcquireReader without release
    auto connection = connectionPool->AcquireConnection(true);
    EXPECT_NE(nullptr, connection);
    connection = connectionPool->AcquireConnection(true);
    EXPECT_NE(nullptr, connection);
    connection = connectionPool->AcquireConnection(true);
    EXPECT_NE(nullptr, connection);

    // repeat AcquireWriter without release
    connection = connectionPool->AcquireConnection(false);
    EXPECT_NE(nullptr, connection);
    connection = connectionPool->AcquireConnection(false);
    EXPECT_EQ(nullptr, connection);
    connection = connectionPool->AcquireConnection(false);
    EXPECT_NE(nullptr, connection);

    // repeat AcquireTransaction without release
    errCode = connectionPool->AcquireTransaction();
    EXPECT_EQ(E_OK, errCode);
    errCode = connectionPool->AcquireTransaction();
    EXPECT_NE(E_OK, errCode);
    connectionPool->ReleaseTransaction();
}

/* *
 * @tc.name: Rdb_ConnectionPoolTest_003
 * @tc.desc: Abnormal testCase for ChangeDbFileForRestore
 * @tc.type: FUNC
 */
HWTEST_F(RdbStoreImplTest, Rdb_ConnectionPoolTest_0023, TestSize.Level2)
{
    const std::string DATABASE_NAME = RDB_TEST_PATH + "ConnectionTest.db";
    int errCode = E_OK;
    RdbStoreConfig config(DATABASE_NAME);
    config.SetReadConSize(1);
    config.SetStorageMode(StorageMode::MODE_DISK);
    auto connectionPool = ConnectionPool::Create(config, errCode);
    EXPECT_NE(nullptr, connectionPool);
    EXPECT_EQ(E_OK, errCode);

    const std::string newPath = DATABASE_NAME;
    const std::string backupPath = DATABASE_NAME;
    const std::vector<uint8_t> newKey;

    // newPath == currentPath, writeConnectionUsed == true
    auto connection = connectionPool->AcquireConnection(false);
    std::shared_ptr<SlaveStatus> curStatus = std::make_shared<SlaveStatus>(SlaveStatus::UNDEFINED);
    errCode = connectionPool->ChangeDbFileForRestore(newPath, backupPath, newKey, curStatus);
    EXPECT_EQ(E_ERROR, errCode);
    connection = nullptr;
    // newPath == currentPath
    errCode = connectionPool->ChangeDbFileForRestore(newPath, backupPath, newKey, curStatus);
    EXPECT_NE(E_OK, errCode);
    // newPath != currentPath
    const std::string newPath2 = RDB_TEST_PATH + "tmp.db";
    errCode = connectionPool->ChangeDbFileForRestore(newPath2, backupPath, newKey, curStatus);
    EXPECT_EQ(E_ERROR, errCode);
}

/* *
 * @tc.name: Rdb_ConnectionPoolTest_004
 * @tc.desc: Abnormal testCase for ReopenConns
 * @tc.type: FUNC
 */
HWTEST_F(RdbStoreImplTest, Rdb_ConnectionPoolTest_004, TestSize.Level2)
{
    const std::string databaseName = RDB_TEST_PATH + "ConnectionTest004.db";
    int errCode = E_OK;
    RdbStoreConfig config(databaseName);
    config.SetReadConSize(1);
    config.SetStorageMode(StorageMode::MODE_DISK);
    auto connectionPool = ConnectionPool::Create(config, errCode);
    ASSERT_NE(nullptr, connectionPool);
    EXPECT_EQ(E_OK, errCode);

    config.SetReadConSize(65);
    EXPECT_NE(E_OK, connectionPool->ReopenConns());
}

/* *
 * @tc.name: Rdb_ConnectionPoolTest_005
 * @tc.desc: Abnormal repair when pool create fails
 * @tc.type: FUNC
 */
HWTEST_F(RdbStoreImplTest, Rdb_ConnectionPoolTest_005, TestSize.Level2)
{
    const std::string databaseName = RDB_TEST_PATH + "ConnectionTest005.db";
    RdbHelper::DeleteRdbStore(databaseName);
    int errCode = E_OK;
    RdbStoreConfig config(databaseName);
    config.SetReadConSize(1);
    config.SetStorageMode(StorageMode::MODE_DISK);
    config.SetHaMode(HAMode::MAIN_REPLICA);

    RdbStoreImplTestOpenCallback helper;
    std::shared_ptr<RdbStore> store = RdbHelper::GetRdbStore(config, 1, helper, errCode);
    ASSERT_NE(store, nullptr);
    store->ExecuteSql(CREATE_TABLE_TEST);
    store = nullptr;

    auto connectionPool = ConnectionPool::Create(config, errCode);
    ASSERT_NE(nullptr, connectionPool);
    EXPECT_EQ(E_OK, errCode);
    auto connection = connectionPool->AcquireConnection(false);
    ASSERT_NE(nullptr, connection);

    config.SetReadConSize(128);
    EXPECT_EQ(E_OK, connection->Repair(config));
    RdbHelper::DeleteRdbStore(databaseName);
}

HWTEST_F(RdbStoreImplTest, NotifyDataChangeTest_001, TestSize.Level2)
{
    const std::string DATABASE_NAME = RDB_TEST_PATH + "SqlitConnectionOpenTest.db";
    RdbStoreConfig config(DATABASE_NAME);
    config.SetReadOnly(false);
    config.SetPageSize(1024);
    auto [errCode, connection] = SqliteConnection::Create(config, true);
    EXPECT_NE(nullptr, connection);
    RdbStoreImplTestOpenCallback helper;
    std::shared_ptr<RdbStore> store = RdbHelper::GetRdbStore(config, 1, helper, errCode);
    EXPECT_NE(nullptr, store);
}

HWTEST_F(RdbStoreImplTest, NotifyDataChangeTest_002, TestSize.Level2)
{
    int errCode = E_OK;
    RdbStoreConfig config(RdbStoreImplTest::DATABASE_NAME);
    config.SetReadOnly(false);
    config.SetPageSize(1024);
    config.SetBundleName("callback.test2");
    config.SetSearchable(true);
    config.SetStorageMode(StorageMode::MODE_DISK);
    // register callback
    RdbStoreImplTestOpenCallback helper;
    std::shared_ptr<RdbStore> store = RdbHelper::GetRdbStore(config, 1, helper, errCode);
    EXPECT_NE(nullptr, store);
    store->ExecuteSql("DROP TABLE IF EXISTS test_callback_t2;");
    store->ExecuteSql("CREATE TABLE if not exists test_callback_t2 "
                      "(id INTEGER PRIMARY KEY AUTOINCREMENT, timestamp INTEGER, data_key INTEGER, "
                      "data3 FLOAT, data4 BLOB, data5 BOOLEAN);");
    // set TrackerTable
    DistributedDB::TrackerSchema tracker;
    tracker.tableName = "test_callback_t2";
    tracker.extendColNames = { "id" };
    tracker.trackerColNames = { "id", "timestamp" };
    using Delegate = DistributedDB::RelationalStoreDelegate;
    DistributedDB::RelationalStoreManager rStoreManager("test_app", "test_user_id", 0);
    Delegate::Option option;
    Delegate *g_delegate = nullptr;
    EXPECT_EQ(RdbStoreImplTest::DATABASE_NAME, "/data/test/stepResultSet_impl_test.db");
    int status = rStoreManager.OpenStore(RdbStoreImplTest::DATABASE_NAME, "test_callback_t2", option, g_delegate);
    EXPECT_EQ(E_OK, status);
    auto delegatePtr = std::shared_ptr<Delegate>(
        g_delegate, [&rStoreManager](Delegate *delegate) { rStoreManager.CloseStore(delegate); });
    int setStatus = delegatePtr->SetTrackerTable(tracker);
    EXPECT_EQ(E_OK, setStatus);

    int64_t rowId;
    ValuesBucket valuesBucket;
    valuesBucket.PutInt("data_key", ValueObject(1));
    valuesBucket.PutInt("timestamp", ValueObject(1000000000));
    int errorCode = store->Insert(rowId, "test_callback_t2", valuesBucket);
    EXPECT_EQ(E_OK, errorCode);
    EXPECT_EQ(1, rowId);
    store->ExecuteSql("DROP TABLE IF EXISTS test_callback_t2;");
}

HWTEST_F(RdbStoreImplTest, NotifyDataChangeTest_003, TestSize.Level2)
{
    int errCode = E_OK;
    RdbStoreConfig config(RdbStoreImplTest::DATABASE_NAME);
    config.SetReadOnly(false);
    config.SetPageSize(1024);
    config.SetBundleName("callback.test3");
    config.SetSearchable(true);
    config.SetStorageMode(StorageMode::MODE_DISK);

    RdbStoreImplTestOpenCallback helper;
    std::shared_ptr<RdbStore> store = RdbHelper::GetRdbStore(config, 1, helper, errCode);

    store->ExecuteSql("DROP TABLE IF EXISTS test_callback_t3;");

    store->ExecuteSql("CREATE TABLE if not exists test_callback_t3 "
                      "(id INTEGER PRIMARY KEY AUTOINCREMENT, timestamp INTEGER, data_key INTEGER, "
                      "data3 FLOAT, data4 BLOB, data5 BOOLEAN);");
    // set TrackerTable
    DistributedDB::TrackerSchema tracker;
    tracker.tableName = "test_callback_t3";
    tracker.extendColNames = { "id" };
    tracker.trackerColNames = { "id", "timestamp" };
    using Delegate = DistributedDB::RelationalStoreDelegate;
    DistributedDB::RelationalStoreManager rStoreManager("test_app", "test_user_id", 0);
    Delegate::Option option;
    Delegate *g_delegate = nullptr;
    EXPECT_EQ(RdbStoreImplTest::DATABASE_NAME, "/data/test/stepResultSet_impl_test.db");
    int status = rStoreManager.OpenStore(RdbStoreImplTest::DATABASE_NAME, "test_callback_t3", option, g_delegate);
    EXPECT_EQ(E_OK, status);
    auto delegatePtr = std::shared_ptr<Delegate>(
        g_delegate, [&rStoreManager](Delegate *delegate) { rStoreManager.CloseStore(delegate); });
    int setStatus = delegatePtr->SetTrackerTable(tracker);
    EXPECT_EQ(E_OK, setStatus);

    int64_t rowId;
    ValuesBucket valuesBucket;
    valuesBucket.PutInt("data_key", ValueObject(1));
    valuesBucket.PutInt("timestamp", ValueObject(1000000000));
    int errorCode = store->Insert(rowId, "test_callback_t3", valuesBucket);
    EXPECT_EQ(E_OK, errorCode);
    EXPECT_EQ(1, rowId);
    errorCode = store->ExecuteSql("UPDATE test_callback_t3 SET timestamp = 100 WHERE id = 1;");
    EXPECT_EQ(E_OK, errorCode);

    store->ExecuteSql("DROP TABLE IF EXISTS test_callback_t3;");
}

/* *
 * @tc.name: Rdb_QuerySharingResourceTest_001
 * @tc.desc: QuerySharingResource testCase
 * @tc.type: FUNC
 */
HWTEST_F(RdbStoreImplTest, Rdb_QuerySharingResourceTest_001, TestSize.Level2)
{
    RdbHelper::DeleteRdbStore(RdbStoreImplTest::DATABASE_NAME);
    int errCode = E_OK;
    RdbStoreConfig config(RdbStoreImplTest::DATABASE_NAME);
    config.SetName("RdbStore_impl_test.db");
    config.SetBundleName("com.example.distributed.rdb");

    RdbStoreImplTestOpenCallback helper;
    std::shared_ptr<RdbStore> store = RdbHelper::GetRdbStore(config, 1, helper, errCode);
    EXPECT_NE(store, nullptr);
    EXPECT_EQ(errCode, E_OK);
    AbsRdbPredicates predicates("test");
    predicates.EqualTo("id", 1);

    auto ret = store->QuerySharingResource(predicates, {});
    EXPECT_NE(E_OK, ret.first);
    EXPECT_EQ(nullptr, ret.second);
    RdbHelper::DeleteRdbStore(RdbStoreImplTest::DATABASE_NAME);
}

/* *
 * @tc.name: Rdb_QuerySharingResourceTest_002
 * @tc.desc: QuerySharingResource testCase
 * @tc.type: FUNC
 */
HWTEST_F(RdbStoreImplTest, Rdb_QuerySharingResourceTest_002, TestSize.Level2)
{
    RdbHelper::DeleteRdbStore(RdbStoreImplTest::DATABASE_NAME);
    int errCode = E_OK;
    RdbStoreConfig config(RdbStoreImplTest::DATABASE_NAME);
    config.SetName("RdbStore_impl_test.db");
    config.SetBundleName("com.example.distributed.rdb");

    RdbStoreImplTestOpenCallback helper;
    std::shared_ptr<RdbStore> store = RdbHelper::GetRdbStore(config, 1, helper, errCode);
    EXPECT_NE(store, nullptr);
    EXPECT_EQ(errCode, E_OK);
    store->ExecuteSql("CREATE TABLE test_resource "
                      "(id INTEGER PRIMARY KEY AUTOINCREMENT, timestamp INTEGER, data_key INTEGER, "
                      "data3 FLOAT, data4 BLOB, data5 BOOLEAN);");
    int64_t rowId;
    ValuesBucket valuesBucket;
    valuesBucket.PutInt("data_key", ValueObject(1));
    valuesBucket.PutInt("timestamp", ValueObject(1000000000));
    int errorCode = store->Insert(rowId, "test_resource", valuesBucket);
    EXPECT_EQ(E_OK, errorCode);
    EXPECT_EQ(1, rowId);
    AbsRdbPredicates predicates("test_resource");
    predicates.EqualTo("data_key", 1);

    auto [status, resultSet] = store->QuerySharingResource(predicates, { "id", "data_key" });
    EXPECT_NE(E_OK, status);
    ASSERT_EQ(nullptr, resultSet);

    RdbHelper::DeleteRdbStore(RdbStoreImplTest::DATABASE_NAME);
}

/* *
 * @tc.name: CleanDirtyDataTest_001
 * @tc.desc: Abnormal testCase for CleanDirtyData
 * @tc.type: FUNC
 */
HWTEST_F(RdbStoreImplTest, Abnormal_CleanDirtyDataTest_001, TestSize.Level2)
{
    store_->ExecuteSql("CREATE TABLE test (id INTEGER PRIMARY KEY AUTOINCREMENT, data1 TEXT, "
                       "data2 INTEGER, data3 FLOAT, data4 BLOB, data5 BOOLEAN);");
    int errCode = E_OK;

    // tabel is empty
    std::string table = "";
    uint64_t cursor = UINT64_MAX;
    errCode = RdbStoreImplTest::store_->CleanDirtyData(table, cursor);
    EXPECT_EQ(E_INVALID_ARGS, errCode);

    table = "test";
    errCode = RdbStoreImplTest::store_->CleanDirtyData(table, cursor);
    EXPECT_EQ(E_ERROR, errCode);
    store_->ExecuteSql("DROP TABLE IF EXISTS test");
}

/* *
 * @tc.name: ClearCacheTest_001
 * @tc.desc: Normal testCase for ClearCache
 * @tc.type: FUNC
 */
HWTEST_F(RdbStoreImplTest, Normal_ClearCacheTest_001, TestSize.Level2)
{
    RdbHelper::DeleteRdbStore(RdbStoreImplTest::DATABASE_NAME);
    int errCode = E_OK;
    RdbStoreConfig config(RdbStoreImplTest::DATABASE_NAME);
    RdbStoreImplTestOpenCallback helper;
    std::shared_ptr<RdbStore> store = RdbHelper::GetRdbStore(config, 1, helper, errCode);
    EXPECT_NE(nullptr, store);
    EXPECT_EQ(E_OK, errCode);
    store->ExecuteSql("CREATE TABLE test (id INTEGER PRIMARY KEY AUTOINCREMENT, data1 TEXT, "
                       "data2 INTEGER, data3 FLOAT, data4 BLOB, data5 BOOLEAN);");
    int64_t id;
    ValuesBucket valuesBucket;
    for (int i = 0; i < 1000; ++i) {
        valuesBucket.PutString("data1", std::string(1024 * 1024, 'a'));
        valuesBucket.PutInt("data2", 20);
        errCode = store->Insert(id, "test", valuesBucket);
    }
    EXPECT_EQ(errCode, E_OK);
    EXPECT_EQ(1000, id);

    int rowCount;
    std::shared_ptr<ResultSet> resultSet = store->QueryByStep("SELECT * FROM test");
    EXPECT_NE(resultSet, nullptr);
    resultSet->GetRowCount(rowCount);
    EXPECT_EQ(rowCount, 1000);
    int64_t currentMemory = sqlite3_memory_used();
    EXPECT_EQ(E_OK, resultSet->Close());
    EXPECT_LT(sqlite3_memory_used(), currentMemory);
}
 
/* *
* @tc.name: ClearCacheTest_002
* @tc.desc: Normal testCase for ClearCache
* @tc.type: FUNC
*/
HWTEST_F(RdbStoreImplTest, Normal_ClearCacheTest_002, TestSize.Level2)
{
    RdbHelper::DeleteRdbStore(RdbStoreImplTest::DATABASE_NAME);
    int errCode = E_OK;
    RdbStoreConfig config(RdbStoreImplTest::DATABASE_NAME);
    RdbStoreImplTestOpenCallback helper;
    std::shared_ptr<RdbStore> store = RdbHelper::GetRdbStore(config, 1, helper, errCode);
    EXPECT_NE(nullptr, store);
    EXPECT_EQ(E_OK, errCode);
    store->ExecuteSql("CREATE TABLE test1 (id INTEGER PRIMARY KEY AUTOINCREMENT, data1 TEXT, "
                       "data2 INTEGER, data3 FLOAT, data4 BLOB, data5 BOOLEAN);");
    int64_t id;
    ValuesBucket valuesBucket;
    valuesBucket.PutString("data1", std::string(0.5 * 1024 * 1024, 'a'));
    valuesBucket.PutInt("data2", 20);
    errCode = store->Insert(id, "test1", valuesBucket);
    EXPECT_EQ(errCode, E_OK);
    EXPECT_EQ(1, id);
    int64_t currentMemory = sqlite3_memory_used();
    valuesBucket.PutString("data1", std::string(0.5 * 1024 * 1024, 'a'));
    valuesBucket.PutInt("data2", 20);
    errCode = store->Insert(id, "test1", valuesBucket);
    EXPECT_EQ(errCode, E_OK);
    EXPECT_EQ(2, id);
    EXPECT_LT(sqlite3_memory_used(), currentMemory);
}

/* *
 * @tc.name: ClearCacheTest_003
 * @tc.desc: Normal testCase for ClearCache
 * @tc.type: FUNC
 */
HWTEST_F(RdbStoreImplTest, Normal_ClearCacheTest_003, TestSize.Level2)
{
    RdbHelper::DeleteRdbStore(RdbStoreImplTest::DATABASE_NAME);
    int errCode = E_OK;
    int clearMemorySize = 1024 * 1024;
    RdbStoreConfig config(RdbStoreImplTest::DATABASE_NAME);
    config.SetClearMemorySize(1024 * 1024 + 1);
    EXPECT_EQ(clearMemorySize, config.GetClearMemorySize());
    config.SetClearMemorySize(-1);
    EXPECT_EQ(clearMemorySize, config.GetClearMemorySize());
    config.SetClearMemorySize(10240);
    RdbStoreImplTestOpenCallback helper;
    std::shared_ptr<RdbStore> store = RdbHelper::GetRdbStore(config, 1, helper, errCode);
    ASSERT_NE(nullptr, store);
    EXPECT_EQ(E_OK, errCode);
}

/* *
 * @tc.name: LockCloudContainerTest
 * @tc.desc: lock cloudContainer testCase
 * @tc.type: FUNC
 */
HWTEST_F(RdbStoreImplTest, LockCloudContainerTest, TestSize.Level2)
{
    int errCode = E_OK;
    // GetRdbService failed if rdbstoreconfig bundlename_ empty
    auto ret = store_->LockCloudContainer();
    EXPECT_EQ(E_INVALID_ARGS, ret.first);
    EXPECT_EQ(0, ret.second);
    RdbHelper::DeleteRdbStore(RdbStoreImplTest::DATABASE_NAME);

    RdbStoreConfig config(RdbStoreImplTest::DATABASE_NAME);
    config.SetName("RdbStore_impl_test.db");
    config.SetBundleName("com.example.distributed.rdb");
    RdbStoreImplTestOpenCallback helper;
    std::shared_ptr<RdbStore> store = RdbHelper::GetRdbStore(config, 1, helper, errCode);
    EXPECT_EQ(E_OK, errCode);
    // GetRdbService succeeded if configuration file has already been configured
    ret = store->LockCloudContainer();
    EXPECT_NE(E_OK, ret.first);
    store = nullptr;
    RdbHelper::DeleteRdbStore(RdbStoreImplTest::DATABASE_NAME);
}

/* *
 * @tc.name: UnlockCloudContainerTest
 * @tc.desc: unlock cloudContainer testCase
 * @tc.type: FUNC
 */
HWTEST_F(RdbStoreImplTest, UnlockCloudContainerTest, TestSize.Level2)
{
    int errCode = E_OK;
    // GetRdbService failed if rdbstoreconfig bundlename_ empty
    auto result = store_->UnlockCloudContainer();
    EXPECT_EQ(E_INVALID_ARGS, result);
    RdbHelper::DeleteRdbStore(RdbStoreImplTest::DATABASE_NAME);

    RdbStoreConfig config(RdbStoreImplTest::DATABASE_NAME);
    config.SetName("RdbStore_impl_test.db");
    config.SetBundleName("com.example.distributed.rdb");
    RdbStoreImplTestOpenCallback helper;
    std::shared_ptr<RdbStore> store = RdbHelper::GetRdbStore(config, 1, helper, errCode);
    EXPECT_EQ(E_OK, errCode);
    // GetRdbService succeeded if configuration file has already been configured
    result = store->UnlockCloudContainer();
    EXPECT_NE(E_OK, result);
    store = nullptr;
    RdbHelper::DeleteRdbStore(RdbStoreImplTest::DATABASE_NAME);
}

/* *
 * @tc.name: LockCloudContainerTest001
 * @tc.desc: lock cloudContainer testCase
 * @tc.type: FUNC
 */
HWTEST_F(RdbStoreImplTest, LockCloudContainerTest001, TestSize.Level2)
{
    int errCode = E_OK;
    RdbStoreConfig config(RdbStoreImplTest::DATABASE_NAME);
    config.SetName("RdbStore_impl_test.db");
    config.SetBundleName("com.example.distributed.rdb");
    RdbStoreImplTestOpenCallback helper;
    std::shared_ptr<RdbStore> store = RdbHelper::GetRdbStore(config, 1, helper, errCode);
    ASSERT_NE(store, nullptr);
    EXPECT_EQ(E_OK, errCode);
    // GetRdbService succeeded if configuration file has already been configured
    auto ret = store->RdbStore::LockCloudContainer();
    EXPECT_EQ(E_OK, ret.first);
    EXPECT_EQ(0, ret.second);
    store = nullptr;
    RdbHelper::DeleteRdbStore(RdbStoreImplTest::DATABASE_NAME);
}

/* *
 * @tc.name: UnlockCloudContainerTest001
 * @tc.desc: unlock cloudContainer testCase
 * @tc.type: FUNC
 */
HWTEST_F(RdbStoreImplTest, UnlockCloudContainerTest001, TestSize.Level2)
{
    int errCode = E_OK;
    RdbStoreConfig config(RdbStoreImplTest::DATABASE_NAME);
    config.SetName("RdbStore_impl_test.db");
    config.SetBundleName("com.example.distributed.rdb");
    RdbStoreImplTestOpenCallback helper;
    std::shared_ptr<RdbStore> store = RdbHelper::GetRdbStore(config, 1, helper, errCode);
    ASSERT_NE(store, nullptr);
    EXPECT_EQ(E_OK, errCode);
    // GetRdbService succeeded if configuration file has already been configured
    auto result = store->RdbStore::UnlockCloudContainer();
    EXPECT_EQ(E_OK, result);
    store = nullptr;
    RdbHelper::DeleteRdbStore(RdbStoreImplTest::DATABASE_NAME);
}

/* *
 * @tc.name: SetSearchableTest
 * @tc.desc: SetSearchable testCase
 * @tc.type: FUNC
 */
HWTEST_F(RdbStoreImplTest, SetSearchableTest, TestSize.Level2)
{
    int errCode = E_OK;
    RdbStoreConfig config(RdbStoreImplTest::DATABASE_NAME);
    config.SetBundleName("");
    RdbStoreImplTestOpenCallback helper;
    std::shared_ptr<RdbStore> store = RdbHelper::GetRdbStore(config, 1, helper, errCode);
    EXPECT_EQ(E_OK, errCode);

    int result = store->SetSearchable(true);
    EXPECT_EQ(E_INVALID_ARGS, result);
    RdbHelper::DeleteRdbStore(RdbStoreImplTest::DATABASE_NAME);

    config.SetBundleName("com.example.distributed.rdb");
    EXPECT_EQ(E_OK, errCode);
    store = RdbHelper::GetRdbStore(config, 1, helper, errCode);
    EXPECT_EQ(E_OK, errCode);
    result = store->SetSearchable(true);
    EXPECT_EQ(E_OK, result);
}

/* *
 * @tc.name: CreateTransaction_001
 * @tc.desc: create the DEFERRED, IMMEDIATE, EXCLUSIVE transaction.
 * @tc.type: FUNC
 */
HWTEST_F(RdbStoreImplTest, CreateTransaction_001, TestSize.Level1)
{
    auto [errCode, trans] = store_->CreateTransaction(Transaction::DEFERRED);
    ASSERT_EQ(errCode, E_OK);
    ASSERT_NE(trans, nullptr);
    trans = nullptr;
    std::tie(errCode, trans) = store_->CreateTransaction(Transaction::IMMEDIATE);
    ASSERT_EQ(errCode, E_OK);
    ASSERT_NE(trans, nullptr);
    trans = nullptr;
    std::tie(errCode, trans) = store_->CreateTransaction(Transaction::EXCLUSIVE);
    ASSERT_EQ(errCode, E_OK);
    ASSERT_NE(trans, nullptr);
}

/* *
 * @tc.name: CreateTransaction_002
 * @tc.desc: create the invalid type transaction.
 * @tc.type: FUNC
 */
HWTEST_F(RdbStoreImplTest, CreateTransaction_002, TestSize.Level1)
{
    auto [errCode, trans] = store_->CreateTransaction(-1);
    ASSERT_EQ(errCode, E_INVALID_ARGS);
    ASSERT_EQ(trans, nullptr);
    std::tie(errCode, trans) = store_->CreateTransaction(Transaction::TRANS_BUTT);
    ASSERT_EQ(errCode, E_INVALID_ARGS);
    ASSERT_EQ(trans, nullptr);
    std::tie(errCode, trans) = store_->CreateTransaction(100);
    ASSERT_EQ(errCode, E_INVALID_ARGS);
    ASSERT_EQ(trans, nullptr);
}

/* *
 * @tc.name: CreateTransaction_003
 * @tc.desc: create the over the max trans.
 * @tc.type: FUNC
 */
HWTEST_F(RdbStoreImplTest, CreateTransaction_003, TestSize.Level1)
{
    constexpr size_t MAX_TRANS = 4;
    std::vector<std::shared_ptr<Transaction>> entries;
    int32_t errCode = E_OK;
    std::shared_ptr<Transaction> trans = nullptr;
    for (int i = 0; i < 100; ++i) {
        std::tie(errCode, trans) = store_->CreateTransaction(Transaction::DEFERRED);
        if (trans == nullptr) {
            break;
        }
        entries.push_back(std::move(trans));
    }
    ASSERT_EQ(errCode, E_DATABASE_BUSY);
    ASSERT_EQ(entries.size(), MAX_TRANS);
}

/* *
 * @tc.name: CreateTransaction_004
 * @tc.desc: create the auto release trans.
 * @tc.type: FUNC
 */
HWTEST_F(RdbStoreImplTest, CreateTransaction_004, TestSize.Level1)
{
    int i = 0;
    for (i = 0; i < 20; ++i) {
        auto [errCode, trans] = store_->CreateTransaction(Transaction::EXCLUSIVE);
        ASSERT_EQ(errCode, E_OK);
        ASSERT_NE(trans, nullptr);
    }
    ASSERT_EQ(i, 20);
}

/* *
 * @tc.name: BatchInsert_001
 * @tc.desc: BatchInsert when violation the unique constraint.
 * @tc.type: FUNC
 */
HWTEST_F(RdbStoreImplTest, BatchInsert_001, TestSize.Level1)
{
    std::string tableName = "BatchInsertTest";
    store_->Execute("DROP TABLE IF EXISTS " + tableName);
    auto res =
        store_->Execute("CREATE TABLE " + tableName + " (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT NOT NULL)");
    ASSERT_EQ(res.first, E_OK);

    ValuesBuckets rows;
    for (int i = 0; i < 5; i++) {
        ValuesBucket row;
        row.Put("id", i);
        row.Put("name", "Jim");
        rows.Put(row);
    }
    Transaction::Row row;
    row.Put("id", 2);
    row.Put("name", "Jim");
    auto result = store_->Insert(tableName, row);
    ASSERT_EQ(result.first, E_OK);
    ASSERT_EQ(result.second, 2);
    result = store_->BatchInsert(tableName, rows, ConflictResolution::ON_CONFLICT_NONE);
    ASSERT_EQ(result.first, E_SQLITE_CONSTRAINT);
    ASSERT_EQ(result.second, 0);

    result = store_->BatchInsert(tableName, rows, ConflictResolution::ON_CONFLICT_ROLLBACK);
    ASSERT_EQ(result.first, E_SQLITE_CONSTRAINT);
    ASSERT_EQ(result.second, 0);

    result = store_->BatchInsert(tableName, rows, ConflictResolution::ON_CONFLICT_ABORT);
    ASSERT_EQ(result.first, E_SQLITE_CONSTRAINT);
    ASSERT_EQ(result.second, 0);

    result = store_->BatchInsert(tableName, rows, ConflictResolution::ON_CONFLICT_FAIL);
    ASSERT_EQ(result.first, E_SQLITE_CONSTRAINT);
    ASSERT_EQ(result.second, 2);

    result = store_->BatchInsert(tableName, rows, ConflictResolution::ON_CONFLICT_IGNORE);
    ASSERT_EQ(result.first, E_OK);
    ASSERT_EQ(result.second, 2);

    result = store_->BatchInsert(tableName, rows, ConflictResolution::ON_CONFLICT_REPLACE);
    ASSERT_EQ(result.first, E_OK);
    ASSERT_EQ(result.second, 5);

    std::shared_ptr<ResultSet> resultSet = store_->QueryByStep("SELECT * FROM " + tableName);
    ASSERT_NE(resultSet, nullptr);
    int rowCount;
    ASSERT_EQ(resultSet->GetRowCount(rowCount), E_OK);
    ASSERT_EQ(rowCount, 5);
}

/* *
 * @tc.name: BatchInsert_002
 * @tc.desc: BatchInsert when violation the not null constraint.
 * @tc.type: FUNC
 */
HWTEST_F(RdbStoreImplTest, BatchInsert_002, TestSize.Level1)
{
    std::string tableName = "BatchInsertTest";
    store_->Execute("DROP TABLE IF EXISTS " + tableName);
    auto res =
        store_->Execute("CREATE TABLE " + tableName + " (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT NOT NULL)");
    ASSERT_EQ(res.first, E_OK);

    ValuesBuckets rows;
    for (int i = 0; i < 5; i++) {
        ValuesBucket row;
        row.Put("id", i);
        row.Put("name", i == 2 ? ValueObject() : "Jim");
        rows.Put(row);
    }
    auto result = store_->BatchInsert(tableName, rows, ConflictResolution::ON_CONFLICT_NONE);
    ASSERT_EQ(result.first, E_SQLITE_CONSTRAINT);
    ASSERT_EQ(result.second, 0);

    result = store_->BatchInsert(tableName, rows, ConflictResolution::ON_CONFLICT_ROLLBACK);
    ASSERT_EQ(result.first, E_SQLITE_CONSTRAINT);
    ASSERT_EQ(result.second, 0);

    result = store_->BatchInsert(tableName, rows, ConflictResolution::ON_CONFLICT_ABORT);
    ASSERT_EQ(result.first, E_SQLITE_CONSTRAINT);
    ASSERT_EQ(result.second, 0);

    result = store_->BatchInsert(tableName, rows, ConflictResolution::ON_CONFLICT_FAIL);
    ASSERT_EQ(result.first, E_SQLITE_CONSTRAINT);
    ASSERT_EQ(result.second, 2);

    result = store_->BatchInsert(tableName, rows, ConflictResolution::ON_CONFLICT_IGNORE);
    ASSERT_EQ(result.first, E_OK);
    ASSERT_EQ(result.second, 2);

    result = store_->BatchInsert(tableName, rows, ConflictResolution::ON_CONFLICT_REPLACE);
    ASSERT_EQ(result.first, E_SQLITE_CONSTRAINT);
    ASSERT_EQ(result.second, 0);

    std::shared_ptr<ResultSet> resultSet = store_->QueryByStep("SELECT * FROM " + tableName);
    ASSERT_NE(resultSet, nullptr);
    int rowCount;
    ASSERT_EQ(resultSet->GetRowCount(rowCount), E_OK);
    ASSERT_EQ(rowCount, 4);
}

/* *
 * @tc.name: BatchInsert_003
 * @tc.desc: BatchInsert when violation the PRIMARY constraint.
 * @tc.type: FUNC
 */
HWTEST_F(RdbStoreImplTest, BatchInsert_003, TestSize.Level1)
{
    std::string tableName = "BatchInsertTest";
    store_->Execute("DROP TABLE IF EXISTS " + tableName);
    auto res =
        store_->Execute("CREATE TABLE " + tableName + " (id TEXT PRIMARY KEY, name TEXT NOT NULL)");
    ASSERT_EQ(res.first, E_OK);

    ValuesBuckets rows;
    for (int i = 0; i < 5; i++) {
        ValuesBucket row;
        row.Put("id", std::to_string(i));
        row.Put("name", "Jim");
        rows.Put(row);
    }
    Transaction::Row row;
    row.Put("id", "2");
    row.Put("name", "Jim");
    auto result = store_->Insert(tableName, row);
    ASSERT_EQ(result.first, E_OK);
    ASSERT_EQ(result.second, 1);
    result = store_->BatchInsert(tableName, rows, ConflictResolution::ON_CONFLICT_NONE);
    ASSERT_EQ(result.first, E_SQLITE_CONSTRAINT);
    ASSERT_EQ(result.second, 0);

    result = store_->BatchInsert(tableName, rows, ConflictResolution::ON_CONFLICT_ROLLBACK);
    ASSERT_EQ(result.first, E_SQLITE_CONSTRAINT);
    ASSERT_EQ(result.second, 0);

    result = store_->BatchInsert(tableName, rows, ConflictResolution::ON_CONFLICT_ABORT);
    ASSERT_EQ(result.first, E_SQLITE_CONSTRAINT);
    ASSERT_EQ(result.second, 0);

    result = store_->BatchInsert(tableName, rows, ConflictResolution::ON_CONFLICT_FAIL);
    ASSERT_EQ(result.first, E_SQLITE_CONSTRAINT);
    ASSERT_EQ(result.second, 2);

    result = store_->BatchInsert(tableName, rows, ConflictResolution::ON_CONFLICT_IGNORE);
    ASSERT_EQ(result.first, E_OK);
    ASSERT_EQ(result.second, 2);

    result = store_->BatchInsert(tableName, rows, ConflictResolution::ON_CONFLICT_REPLACE);
    ASSERT_EQ(result.first, E_OK);
    ASSERT_EQ(result.second, 5);

    std::shared_ptr<ResultSet> resultSet = store_->QueryByStep("SELECT * FROM " + tableName);
    ASSERT_NE(resultSet, nullptr);
    int rowCount;
    ASSERT_EQ(resultSet->GetRowCount(rowCount), E_OK);
    ASSERT_EQ(rowCount, 5);
}

/* *
 * @tc.name: BatchInsert_004
 * @tc.desc: BatchInsert when violation the check constraint.
 * @tc.type: FUNC
 */
HWTEST_F(RdbStoreImplTest, BatchInsert_004, TestSize.Level1)
{
    std::string tableName = "BatchInsertTest";
    store_->Execute("DROP TABLE IF EXISTS " + tableName);
    auto res = store_->Execute(
        "CREATE TABLE " + tableName + " (id INTEGER PRIMARY KEY CHECK (id >= 3 OR id <= 1), name TEXT NOT NULL)");
    ASSERT_EQ(res.first, E_OK);

    ValuesBuckets rows;
    for (int i = 0; i < 5; i++) {
        ValuesBucket row;
        row.Put("id", i);
        row.Put("name", "Jim");
        rows.Put(row);
    }
    auto result = store_->BatchInsert(tableName, rows, ConflictResolution::ON_CONFLICT_NONE);
    ASSERT_EQ(result.first, E_SQLITE_CONSTRAINT);
    ASSERT_EQ(result.second, 0);

    result = store_->BatchInsert(tableName, rows, ConflictResolution::ON_CONFLICT_ROLLBACK);
    ASSERT_EQ(result.first, E_SQLITE_CONSTRAINT);
    ASSERT_EQ(result.second, 0);

    result = store_->BatchInsert(tableName, rows, ConflictResolution::ON_CONFLICT_ABORT);
    ASSERT_EQ(result.first, E_SQLITE_CONSTRAINT);
    ASSERT_EQ(result.second, 0);

    result = store_->BatchInsert(tableName, rows, ConflictResolution::ON_CONFLICT_FAIL);
    ASSERT_EQ(result.first, E_SQLITE_CONSTRAINT);
    ASSERT_EQ(result.second, 2);

    result = store_->BatchInsert(tableName, rows, ConflictResolution::ON_CONFLICT_IGNORE);
    ASSERT_EQ(result.first, E_OK);
    ASSERT_EQ(result.second, 2);

    result = store_->BatchInsert(tableName, rows, ConflictResolution::ON_CONFLICT_REPLACE);
    ASSERT_EQ(result.first, E_SQLITE_CONSTRAINT);
    ASSERT_EQ(result.second, 0);

    std::shared_ptr<ResultSet> resultSet = store_->QueryByStep("SELECT * FROM " + tableName);
    ASSERT_NE(resultSet, nullptr);
    int rowCount;
    ASSERT_EQ(resultSet->GetRowCount(rowCount), E_OK);
    ASSERT_EQ(rowCount, 4);
}

/* *
 * @tc.name: BatchInsert_005
 * @tc.desc: BatchInsert when busy.
 * @tc.type: FUNC
 */
HWTEST_F(RdbStoreImplTest, BatchInsert_005, TestSize.Level1)
{
    std::string tableName = "BatchInsertTest";
    store_->Execute("DROP TABLE IF EXISTS " + tableName);
    auto res = store_->Execute(
        "CREATE TABLE " + tableName + " (id INTEGER PRIMARY KEY CHECK (id >= 3 OR id <= 1), name TEXT NOT NULL)");
    ASSERT_EQ(res.first, E_OK);

    auto [code, transaction] = store_->CreateTransaction(Transaction::IMMEDIATE);
    ASSERT_EQ(code, E_OK);
    ASSERT_NE(transaction, nullptr);

    ValuesBuckets rows;
    for (int i = 0; i < 5; i++) {
        ValuesBucket row;
        row.Put("id", i);
        row.Put("name", "Jim");
        rows.Put(row);
    }
    auto result = store_->BatchInsert(tableName, rows, ConflictResolution::ON_CONFLICT_NONE);
    ASSERT_EQ(result.first, E_SQLITE_BUSY);
    ASSERT_EQ(result.second, -1);

    result = store_->BatchInsert(tableName, rows, ConflictResolution::ON_CONFLICT_ROLLBACK);
    ASSERT_EQ(result.first, E_SQLITE_BUSY);
    ASSERT_EQ(result.second, -1);

    result = store_->BatchInsert(tableName, rows, ConflictResolution::ON_CONFLICT_ABORT);
    ASSERT_EQ(result.first, E_SQLITE_BUSY);
    ASSERT_EQ(result.second, -1);

    result = store_->BatchInsert(tableName, rows, ConflictResolution::ON_CONFLICT_FAIL);
    ASSERT_EQ(result.first, E_SQLITE_BUSY);
    ASSERT_EQ(result.second, -1);

    result = store_->BatchInsert(tableName, rows, ConflictResolution::ON_CONFLICT_IGNORE);
    ASSERT_EQ(result.first, E_SQLITE_BUSY);
    ASSERT_EQ(result.second, -1);

    result = store_->BatchInsert(tableName, rows, ConflictResolution::ON_CONFLICT_REPLACE);
    ASSERT_EQ(result.first, E_SQLITE_BUSY);
    ASSERT_EQ(result.second, -1);

    std::shared_ptr<ResultSet> resultSet = store_->QueryByStep("SELECT * FROM " + tableName);
    ASSERT_NE(resultSet, nullptr);
    int rowCount;
    ASSERT_EQ(resultSet->GetRowCount(rowCount), E_OK);
    ASSERT_EQ(rowCount, 0);
}

/* *
 * @tc.name: BatchInsert_006
 * @tc.desc: Normal BatchInsert.
 * @tc.type: FUNC
 */
HWTEST_F(RdbStoreImplTest, BatchInsert_006, TestSize.Level1)
{
    std::string tableName = "BatchInsertTest";
    store_->Execute("DROP TABLE IF EXISTS " + tableName);
    auto res =
        store_->Execute("CREATE TABLE " + tableName + " (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT NOT NULL)");
    ASSERT_EQ(res.first, E_OK);

    ValuesBuckets rows;

    auto result = store_->BatchInsert(tableName, rows, ConflictResolution::ON_CONFLICT_NONE);
    ASSERT_EQ(result.first, E_OK);
    ASSERT_EQ(result.second, 0);

    result = store_->BatchInsert(tableName, rows, ConflictResolution::ON_CONFLICT_ROLLBACK);
    ASSERT_EQ(result.first, E_OK);
    ASSERT_EQ(result.second, 0);

    result = store_->BatchInsert(tableName, rows, ConflictResolution::ON_CONFLICT_ABORT);
    ASSERT_EQ(result.first, E_OK);
    ASSERT_EQ(result.second, 0);
    for (int i = 0; i < 5; i++) {
        ValuesBucket row;
        row.Put("name", "Jim");
        rows.Put(row);
    }

    result = store_->BatchInsert(tableName, rows, ConflictResolution::ON_CONFLICT_FAIL);
    ASSERT_EQ(result.first, E_OK);
    ASSERT_EQ(result.second, 5);

    result = store_->BatchInsert(tableName, rows, ConflictResolution::ON_CONFLICT_IGNORE);
    ASSERT_EQ(result.first, E_OK);
    ASSERT_EQ(result.second, 5);

    result = store_->BatchInsert(tableName, rows, ConflictResolution::ON_CONFLICT_REPLACE);
    ASSERT_EQ(result.first, E_OK);
    ASSERT_EQ(result.second, 5);

    std::shared_ptr<ResultSet> resultSet = store_->QueryByStep("SELECT * FROM " + tableName);
    ASSERT_NE(resultSet, nullptr);
    int rowCount;
    ASSERT_EQ(resultSet->GetRowCount(rowCount), E_OK);
    ASSERT_EQ(rowCount, 15);
}

/* *
 * @tc.name: BatchInsert_007
 * @tc.desc: over limit params BatchInsert.
 * @tc.type: FUNC
 */
HWTEST_F(RdbStoreImplTest, BatchInsert_007, TestSize.Level1)
{
    std::string tableName = "BatchInsertTest";
    store_->Execute("DROP TABLE IF EXISTS " + tableName);
    auto res =
        store_->Execute("CREATE TABLE " + tableName + " (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT NOT NULL)");
    ASSERT_EQ(res.first, E_OK);
    //sqlite default max param number
    int32_t maxNumber = 32766;
    int32_t maxRows = maxNumber / 2 + 1;
    ValuesBuckets rows;
    for (int32_t i = 0; i < maxRows; i++) {
        ValuesBucket row;
        row.Put("id", i);
        row.Put("name", "Jim");
        rows.Put(row);
    }
    auto result = store_->BatchInsert(tableName, rows, ConflictResolution::ON_CONFLICT_NONE);
    ASSERT_EQ(result.first, E_INVALID_ARGS);
    ASSERT_EQ(result.second, -1);

    result = store_->BatchInsert(tableName, rows, ConflictResolution::ON_CONFLICT_ROLLBACK);
    ASSERT_EQ(result.first, E_INVALID_ARGS);
    ASSERT_EQ(result.second, -1);

    result = store_->BatchInsert(tableName, rows, ConflictResolution::ON_CONFLICT_ABORT);
    ASSERT_EQ(result.first, E_INVALID_ARGS);
    ASSERT_EQ(result.second, -1);

    result = store_->BatchInsert(tableName, rows, ConflictResolution::ON_CONFLICT_FAIL);
    ASSERT_EQ(result.first, E_INVALID_ARGS);
    ASSERT_EQ(result.second, -1);

    result = store_->BatchInsert(tableName, rows, ConflictResolution::ON_CONFLICT_IGNORE);
    ASSERT_EQ(result.first, E_INVALID_ARGS);
    ASSERT_EQ(result.second, -1);

    result = store_->BatchInsert(tableName, rows, ConflictResolution::ON_CONFLICT_REPLACE);
    ASSERT_EQ(result.first, E_INVALID_ARGS);
    ASSERT_EQ(result.second, -1);

    std::shared_ptr<ResultSet> resultSet = store_->QueryByStep("SELECT * FROM " + tableName);
    ASSERT_NE(resultSet, nullptr);
    int rowCount;
    ASSERT_EQ(resultSet->GetRowCount(rowCount), E_OK);
    ASSERT_EQ(rowCount, 0);
}

/**
 * @tc.name: RdbStore_Execute_001
 * @tc.desc: test RdbStore Execute
 * @tc.type: FUNC
 */
HWTEST_F(RdbStoreImplTest, RdbStore_Execute_001, TestSize.Level1)
{
    const std::string dbPath = RDB_TEST_PATH + "GetDatabase1.db";
    RdbStoreConfig config(dbPath);
    config.SetName("RdbStoreConfig_test.db");
    std::string bundleName = "com.ohos.config.TestSubUser";
    config.SetBundleName(bundleName);
    config.SetSubUser(100);
    auto subUser = config.GetSubUser();
    EXPECT_EQ(subUser, 100);
    int errCode = E_OK;

    RdbStoreImplTestOpenCallback helper;
    std::shared_ptr<RdbStore> rdbStore = RdbHelper::GetRdbStore(config, 1, helper, errCode);
    EXPECT_EQ(errCode, E_OK);
    ASSERT_NE(rdbStore, nullptr);
    rdbStore->ExecuteSql(CREATE_TABLE_TEST);

    int64_t id;
    ValuesBucket values;

    values.PutInt("id", 1);
    values.PutString("name", std::string("zhangsan"));
    values.PutInt("age", 18);
    values.PutDouble("salary", 100.5);
    values.PutBlob("blobType", std::vector<uint8_t>{ 1, 2, 3 });
    int ret = rdbStore->Insert(id, "test", values);
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(1, id);

    values.Clear();
    values.PutInt("id", 2);
    values.PutString("name", std::string("lisi"));
    values.PutInt("age", 19);
    values.PutDouble("salary", 200.5);
    values.PutBlob("blobType", std::vector<uint8_t>{ 4, 5, 6 });
    ret = rdbStore->Insert(id, "test", values);
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(2, id);

    int64_t count;
    ret = rdbStore->ExecuteAndGetLong(count, "SELECT COUNT(*) FROM test");
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(count, 2);

    ret = rdbStore->ExecuteSql("DELETE FROM test WHERE age = 18");
    EXPECT_EQ(ret, E_OK);

    ret = rdbStore->ExecuteAndGetLong(count, "SELECT COUNT(*) FROM test where age = 19");
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(count, 1);

    ret = rdbStore->ExecuteSql("DELETE FROM test WHERE age = 19");
    EXPECT_EQ(ret, E_OK);

    ret = RdbHelper::DeleteRdbStore(config);
    EXPECT_EQ(ret, E_OK);
}

/**
 * @tc.name: RdbStore_Crypt_001
 * @tc.desc: test RdbStore Crypt
 * @tc.type: FUNC
 */
HWTEST_F(RdbStoreImplTest, RdbStore_Crypt_001, TestSize.Level1)
{
    const std::string dbPath = RDB_TEST_PATH + "GetDatabase1.db";
    RdbStoreConfig::CryptoParam cryptoParam;
    cryptoParam.encryptKey_ = std::vector<uint8_t>{ 1, 2, 3, 4, 5, 6 };
    RdbStoreConfig config(dbPath);
    config.SetName("RdbStoreConfig_test.db");
    std::string bundleName = "com.ohos.config.TestSubUser";
    config.SetBundleName(bundleName);
    config.SetCryptoParam(cryptoParam);
    int errCode = E_OK;

    RdbStoreImplTestOpenCallback helper;
    std::shared_ptr<RdbStore> rdbStore = RdbHelper::GetRdbStore(config, 1, helper, errCode);
    EXPECT_EQ(errCode, E_OK);
    ASSERT_NE(rdbStore, nullptr);
    rdbStore->ExecuteSql(CREATE_TABLE_TEST);
    rdbStore = nullptr;
    cryptoParam.encryptKey_ = std::vector<uint8_t>{ 6, 5, 4, 3, 2, 1 };
    config.SetCryptoParam(cryptoParam);

    rdbStore = RdbHelper::GetRdbStore(config, 1, helper, errCode);
    EXPECT_EQ(errCode, E_SQLITE_CORRUPT);
}

/**
 * @tc.name: RdbStore_ClearDirtyLog_001
 * @tc.desc: test RdbStore ClearDirtyLog
 * @tc.type: FUNC
 */
HWTEST_F(RdbStoreImplTest, RdbStore_ClearDirtyLog_001, TestSize.Level1)
{
    int errCode = E_OK;
    RdbStoreConfig config(RdbStoreImplTest::DATABASE_NAME);
    config.SetBundleName("");
    config.SetEnableSemanticIndex(true);
    RdbStoreImplTestOpenCallback helper;
    std::shared_ptr<RdbStore> store = RdbHelper::GetRdbStore(config, 1, helper, errCode);
    EXPECT_EQ(E_OK, errCode);
    ASSERT_NE(store, nullptr);
    errCode = store->ExecuteSql(CREATE_TABLE_TEST);
    EXPECT_EQ(errCode, E_OK);
    errCode = store->CleanDirtyLog("test", 0);
    EXPECT_EQ(errCode, E_OK);
    errCode = RdbHelper::DeleteRdbStore(config);
    EXPECT_EQ(errCode, E_OK);
}

/**
 * @tc.name: RdbStore_InitKnowledgeSchema_001
 * @tc.desc: test RdbStore InitKnowledgeSchema
 * @tc.type: FUNC
 */
HWTEST_F(RdbStoreImplTest, RdbStore_InitKnowledgeSchema_001, TestSize.Level1)
{
    int errCode = E_OK;
    RdbStoreConfig config(RdbStoreImplTest::DATABASE_NAME);
    config.SetBundleName("");
    config.SetEnableSemanticIndex(true);
    OHOS::DistributedRdb::RdbKnowledgeSchema schema;
    RdbStoreImplTestOpenCallback helper;
    std::shared_ptr<RdbStore> store = RdbHelper::GetRdbStore(config, 1, helper, errCode);
    EXPECT_EQ(E_OK, errCode);
    ASSERT_NE(store, nullptr);

    errCode = store->InitKnowledgeSchema(schema);
    EXPECT_EQ(errCode, E_OK);
}

/**
 * @tc.name: RdbStore_InitKnowledgeSchema_002
 * @tc.desc: test RdbStore SetKnowledgeSchema when create table
 * @tc.type: FUNC
 */
HWTEST_F(RdbStoreImplTest, RdbStore_InitKnowledgeSchema_002, TestSize.Level1)
{
    store_ = nullptr;
    int errCode = RdbHelper::DeleteRdbStore(DATABASE_NAME);
    EXPECT_EQ(E_OK, errCode);

    RdbStoreConfig config(RdbStoreImplTest::DATABASE_NAME);
    config.SetBundleName("");
    config.SetEnableSemanticIndex(true);
    OHOS::DistributedRdb::RdbKnowledgeSchema schema;
    RdbStoreImplTestOpenCallback helper;
    std::shared_ptr<RdbStore> store = RdbHelper::GetRdbStore(config, 1, helper, errCode);
    EXPECT_EQ(E_OK, errCode);
    ASSERT_NE(store, nullptr);

    std::pair<int32_t, ValueObject> ret = store->Execute(std::string(CREATE_TABLE_TEST));
    ASSERT_EQ(ret.first, E_OK);

    ret = store->Execute(std::string(CREATE_TABLE_TEST));
    ASSERT_EQ(ret.first, E_OK);
}

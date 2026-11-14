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

#define LOG_TAG "RdbBinlogNoSlaveTest"
#include <gtest/gtest.h>
#include <sys/stat.h>
#include <unistd.h>

#include <filesystem>
#include <fstream>
#include <string>
#include "common.h"
#include "file_ex.h"
#include "logger.h"
#include "rdb_errno.h"
#include "rdb_helper.h"
#include "rdb_open_callback.h"
#include "sqlite_connection.h"
#include "sqlite_utils.h"
#include "task_executor.h"
#include "values_bucket.h"

using namespace testing::ext;
using namespace OHOS::NativeRdb;
using namespace OHOS::Rdb;

static const int CHECKAGE = 18;
static const double CHECKCOLUMN = 100.5;
static const int CHANGENUM = 12;
static const int BINLOG_DELETE_PER_WAIT_TIME = 100000; // 100000us = 100ms
static const int SIZE_MB = 1024 * 1024; // 1MB

class RdbBinlogNoSlaveTest : public testing::Test {
public:
    static void SetUpTestCase(void);
    static void TearDownTestCase(void);
    void SetUp();
    void TearDown();
    static void CheckNumber(
        std::shared_ptr<RdbStore> &store, int num, int errCode = E_OK, const std::string &tableName = "test");
    static bool CheckFolderExist(const std::string &path);
    void RemoveFolder(const std::string &path);
    static void Insert(int64_t start, int count, int dataSize = 0);
    static void Update(int64_t start, int count, int dataSize = 0);
    static void WaitForBinlogDelete(int maxTimes = 1000);
    void InitNoSlaveDb();
    static ssize_t GetBinlogDataFileSize();
    static bool WaitForBinlogSizeAdvance(ssize_t baselineSize, int32_t maxTimes = 100);

    static const std::string databaseName;
    static const std::string slaveDatabaseName;
    static const std::string binlogDatabaseName;
    static const std::string binlogFirstFile;
    static std::shared_ptr<RdbStore> store;
};

const std::string RdbBinlogNoSlaveTest::databaseName = RDB_TEST_PATH + "dual_write_binlog_test.db";
const std::string RdbBinlogNoSlaveTest::slaveDatabaseName = RDB_TEST_PATH + "dual_write_binlog_test_slave.db";
const std::string RdbBinlogNoSlaveTest::binlogDatabaseName = RDB_TEST_PATH + "dual_write_binlog_test.db_binlog";
const std::string RdbBinlogNoSlaveTest::binlogFirstFile = RdbBinlogNoSlaveTest::binlogDatabaseName +
    "/binlog_default.00000";
std::shared_ptr<RdbStore> RdbBinlogNoSlaveTest::store = nullptr;

class RdbBinlogNoSlaveTestOpenCallback : public RdbOpenCallback {
public:
    int OnCreate(RdbStore &store) override;
    int OnUpgrade(RdbStore &store, int oldVersion, int newVersion) override;
    static const std::string createTableTest;
};

const std::string RdbBinlogNoSlaveTestOpenCallback::createTableTest =
    std::string("CREATE TABLE IF NOT EXISTS test ") + std::string("(id INTEGER PRIMARY KEY AUTOINCREMENT, "
                                                                  "name TEXT NOT NULL, age INTEGER, salary "
                                                                  "REAL, blobType BLOB)");

int RdbBinlogNoSlaveTestOpenCallback::OnCreate(RdbStore &store)
{
    return store.ExecuteSql(RdbBinlogNoSlaveTestOpenCallback::createTableTest);
}

int RdbBinlogNoSlaveTestOpenCallback::OnUpgrade(RdbStore &store, int oldVersion, int newVersion)
{
    return E_OK;
}

// Exposes the protected static CleanDonatedBinlogWithoutReplica for direct unit-test coverage.
class TestableSqliteConnection : public SqliteConnection {
public:
    static void ExposeCleanDonatedBinlogWithoutReplica(const std::string &dbPath)
    {
        CleanDonatedBinlogWithoutReplica(dbPath);
    }
    static void ExposeBinlogOnFullFunc(void *pCtx, unsigned short currentCount, const char *dbPath)
    {
        BinlogOnFullFunc(pCtx, currentCount, dbPath);
    }
};

void RdbBinlogNoSlaveTest::SetUpTestCase(void)
{
}

void RdbBinlogNoSlaveTest::TearDownTestCase(void)
{
}

void RdbBinlogNoSlaveTest::SetUp(void)
{
    RdbStoreConfig config(RdbBinlogNoSlaveTest::databaseName);
    if (!SqliteUtils::IsSupportBinlog(config)) {
        GTEST_SKIP() << "Current testcase is not compatible from current rdb";
    }
}

void RdbBinlogNoSlaveTest::TearDown(void)
{
    store = nullptr;
    TaskExecutor::GetInstance().Stop();
    RdbHelper::DeleteRdbStore(RdbBinlogNoSlaveTest::databaseName);
    RdbHelper::DeleteRdbStore(SqliteUtils::GetMasterBackupPath(RdbBinlogNoSlaveTest::databaseName));
    std::string lockCompressName = RdbBinlogNoSlaveTest::slaveDatabaseName + "-lockcompress";
    bool isLockCompressFileExist = OHOS::FileExists(lockCompressName);
    ASSERT_FALSE(isLockCompressFileExist);
    WaitForBinlogDelete();
}

void RdbBinlogNoSlaveTest::InitNoSlaveDb()
{
    // Ensure both the replica file and the binlog folder are absent so the store opens in the
    // no-replica state.
    RdbHelper::DeleteRdbStore(RdbBinlogNoSlaveTest::slaveDatabaseName);
    if (CheckFolderExist(RdbBinlogNoSlaveTest::binlogDatabaseName)) {
        RemoveFolder(RdbBinlogNoSlaveTest::binlogDatabaseName);
    }
    int errCode = E_OK;
    RdbStoreConfig config(RdbBinlogNoSlaveTest::databaseName);
    config.SetHaMode(HAMode::MANUAL_TRIGGER);
    config.SetSearchable(true);
    config.SetBundleName("com.example.distributed.rdb");
    RdbBinlogNoSlaveTestOpenCallback helper;
    RdbBinlogNoSlaveTest::store = RdbHelper::GetRdbStore(config, 1, helper, errCode);
    ASSERT_NE(RdbBinlogNoSlaveTest::store, nullptr);
    store->ExecuteSql("DELETE FROM test");
}

ssize_t RdbBinlogNoSlaveTest::GetBinlogDataFileSize()
{
    return SqliteUtils::GetFileSize(binlogFirstFile);
}

bool RdbBinlogNoSlaveTest::WaitForBinlogSizeAdvance(ssize_t baselineSize, int32_t maxTimes)
{
    int waitTimes = 0;
    while (waitTimes < maxTimes) {
        if (GetBinlogDataFileSize() > baselineSize) {
            return true;
        }
        usleep(BINLOG_DELETE_PER_WAIT_TIME);
        waitTimes++;
    }
    return false;
}

void RdbBinlogNoSlaveTest::CheckNumber(
    std::shared_ptr<RdbStore> &store, int num, int errCode, const std::string &tableName)
{
    std::shared_ptr<ResultSet> resultSet = store->QuerySql("SELECT * FROM " + tableName);
    ASSERT_NE(resultSet, nullptr);
    int countNum;
    int ret = resultSet->GetRowCount(countNum);
    EXPECT_EQ(ret, errCode);
    EXPECT_EQ(num, countNum);
}

bool RdbBinlogNoSlaveTest::CheckFolderExist(const std::string &path)
{
    if (access(path.c_str(), F_OK) != 0) {
        return false;
    }
    return true;
}

void RdbBinlogNoSlaveTest::RemoveFolder(const std::string &path)
{
    std::filesystem::path folder(path);
    for (const auto &entry : std::filesystem::directory_iterator(folder)) {
        if (entry.is_directory()) {
            RemoveFolder(entry.path());
        } else {
            std::filesystem::remove(entry.path());
        }
    }
    std::filesystem::remove(folder);
}

void RdbBinlogNoSlaveTest::Insert(int64_t start, int count, int dataSize)
{
    ValuesBucket values;
    int64_t id = start;
    int ret = E_OK;
    for (int i = 0; i < count; i++) {
        values.Clear();
        values.PutInt("id", id);
        if (dataSize > 0) {
            values.PutString("name", std::string(dataSize, 'a'));
        } else {
            values.PutString("name", std::string("zhangsan"));
        }
        values.PutInt("age", CHECKAGE);
        values.PutDouble("salary", CHECKCOLUMN);
        values.PutBlob("blobType", std::vector<uint8_t>{ 1, 2, 3 });
        ret = store->Insert(id, "test", values);
        EXPECT_EQ(ret, E_OK);
        id++;
    }
}

void RdbBinlogNoSlaveTest::Update(int64_t start, int count, int dataSize)
{
    ValuesBucket values;
    int64_t id = start;
    int age = 20;
    int ret = E_OK;
    for (int i = 0; i < count; i++) {
        values.Clear();
        values.PutInt("id", id);
        if (dataSize > 0) {
            values.PutString("name", std::string(dataSize, 'a'));
        } else {
            values.PutString("name", std::string("zhangsan"));
        }
        values.PutInt("age", age);
        values.PutDouble("salary", CHECKCOLUMN);
        values.PutBlob("blobType", std::vector<uint8_t>{ 1, 2, 3 });
        ret = store->Replace(id, "test", values);
        EXPECT_EQ(ret, E_OK);
        id++;
    }
}

void RdbBinlogNoSlaveTest::WaitForBinlogDelete(int maxTimes)
{
    int waitTimes = 0;
    while (CheckFolderExist(RdbBinlogNoSlaveTest::binlogDatabaseName) && waitTimes < maxTimes) {
        usleep(BINLOG_DELETE_PER_WAIT_TIME);
        waitTimes++;
        LOG_INFO("---- Binlog replay in progress, waiting for finish");
        RdbHelper::DeleteRdbStore(RdbBinlogNoSlaveTest::databaseName);
    }
    EXPECT_FALSE(CheckFolderExist(RdbBinlogNoSlaveTest::binlogDatabaseName));
}

/**
 * @tc.name: RdbStore_Binlog_NoSlaveOpen_001
 * @tc.desc: opening a MANUAL_TRIGGER+Searchable store without replica generates binlog
 * @tc.type: FUNC
 */
HWTEST_F(RdbBinlogNoSlaveTest, RdbStore_Binlog_NoSlaveOpen_001, TestSize.Level0)
{
    // Arrange: open the store with no replica file and no binlog folder.
    InitNoSlaveDb();
    // Act: one round of donation-data write so the binlog files materialize.
    Insert(1, CHANGENUM);
    // Assert: binlog folder and writeIndex exist at the location derived from ReplicaPath rules.
    EXPECT_TRUE(CheckFolderExist(binlogDatabaseName)) << "binlog folder must exist without replica";
    auto [ret, info] = store->GetPrimaryReplicaInfo({});
    EXPECT_NE(ret, E_OK);
    EXPECT_FALSE(info.binlogInfo.exist);
    CheckNumber(store, CHANGENUM);
}

/**
 * @tc.name: RdbStore_Binlog_NoSlaveInsert_001
 * @tc.desc: INSERT on donation data writes binlog without replica
 * @tc.type: FUNC
 */
HWTEST_F(RdbBinlogNoSlaveTest, RdbStore_Binlog_NoSlaveInsert_001, TestSize.Level0)
{
    // Arrange: no-replica store with a known binlog data-file size baseline.
    InitNoSlaveDb();
    ssize_t baselineSize = GetBinlogDataFileSize();
    // Act: INSERT donation data.
    Insert(1, CHANGENUM);
    // Assert: the binlog data file grows after INSERT.
    EXPECT_TRUE(WaitForBinlogSizeAdvance(baselineSize)) << "binlog file size must grow after INSERT";
}

/**
 * @tc.name: RdbStore_Binlog_NoSlaveUpdate_001
 * @tc.desc: UPDATE on donation data writes binlog without replica
 * @tc.type: FUNC
 */
HWTEST_F(RdbBinlogNoSlaveTest, RdbStore_Binlog_NoSlaveUpdate_001, TestSize.Level0)
{
    // Arrange: no-replica store with rows and a known binlog data-file size baseline.
    InitNoSlaveDb();
    Insert(1, CHANGENUM);
    ssize_t baselineSize = GetBinlogDataFileSize();
    // Act: UPDATE donation data.
    Update(1, CHANGENUM);
    // Assert: the binlog data file grows after UPDATE.
    EXPECT_TRUE(WaitForBinlogSizeAdvance(baselineSize)) << "binlog file size must grow after UPDATE";
}

/**
 * @tc.name: RdbStore_Binlog_NoSlaveDelete_001
 * @tc.desc: DELETE on donation data writes binlog without replica
 * @tc.type: FUNC
 */
HWTEST_F(RdbBinlogNoSlaveTest, RdbStore_Binlog_NoSlaveDelete_001, TestSize.Level0)
{
    // Arrange: no-replica store with rows and a known binlog data-file size baseline.
    InitNoSlaveDb();
    Insert(1, CHANGENUM);
    ssize_t baselineSize = GetBinlogDataFileSize();
    // Act: DELETE donation data.
    EXPECT_EQ(store->ExecuteSql("DELETE FROM test WHERE id >= 1"), E_OK);
    // Assert: the binlog data file grows after DELETE.
    EXPECT_TRUE(WaitForBinlogSizeAdvance(baselineSize)) << "binlog file size must grow after DELETE";
}

/**
 * @tc.name: RdbStore_Binlog_NoSlaveFull_001
 * @tc.desc: business writes are not blocked after binlog fills the rolling limit without replica
 * @tc.type: FUNC
 */
HWTEST_F(RdbBinlogNoSlaveTest, RdbStore_Binlog_NoSlaveFull_001, TestSize.Level1)
{
    // Arrange: no-replica store.
    InitNoSlaveDb();
    // Act: 32 rows x 256KB payload exceed the 4MB x 2 rolling limit and trigger the full callback.
    Insert(1, 32, SIZE_MB / 4);
    // Assert: every business write succeeded and the data is complete; binlog keeps rolling.
    CheckNumber(store, 32);
    EXPECT_TRUE(CheckFolderExist(binlogDatabaseName)) << "binlog folder must survive full-rolling";
}

/**
 * @tc.name: RdbStore_Binlog_CleanDonated_OpenFail_001
 * @tc.desc: CleanDonatedBinlogWithoutReplica early-returns when BinlogOpenHandle fails
 * @tc.type: FUNC
 */
HWTEST_F(RdbBinlogNoSlaveTest, RdbStore_Binlog_CleanDonated_OpenFail_001, TestSize.Level1)
{
    // Arrange: pass a non-existent db path so BinlogOpenHandle fails and the function returns early.
    std::string nonExistentPath = RDB_TEST_PATH + "nonexistent_clean_branch_test.db";
    RdbHelper::DeleteRdbStore(nonExistentPath);
    // Act: call directly; the function should return without crash.
    TestableSqliteConnection::ExposeCleanDonatedBinlogWithoutReplica(nonExistentPath);
    // Assert: no crash, no binlog folder created for the non-existent path.
    EXPECT_FALSE(CheckFolderExist(nonExistentPath + "_binlog"))
        << "must not create binlog folder when open handle fails";
}

/**
 * @tc.name: RdbStore_Binlog_CleanDonated_NoBinlog_001
 * @tc.desc: CleanDonatedBinlogWithoutReplica handles sqlite3_clean_binlog failure
 * @tc.type: FUNC
 */
HWTEST_F(RdbBinlogNoSlaveTest, RdbStore_Binlog_CleanDonated_NoBinlog_001, TestSize.Level1)
{
    // Arrange: enable binlog on a no-replica store, then overwrite every file in the binlog folder
    // with zeros so sqlite3_clean_binlog fails format/lock validation while files still exist.
    InitNoSlaveDb();
    Insert(1, CHANGENUM);
    ASSERT_TRUE(CheckFolderExist(binlogDatabaseName)) << "binlog folder must exist before injection";
    store = nullptr;
    RdbHelper::ClearCache();
    std::filesystem::path folder(binlogDatabaseName);
    for (const auto &entry : std::filesystem::directory_iterator(folder)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        std::ofstream ofs(entry.path(), std::ios::binary | std::ios::trunc);
        ASSERT_TRUE(ofs.is_open()) << "overwrite failed: " << entry.path().string();
        ofs.close();
    }
    // Act: call on the db whose binlog files are zeroed; sqlite3_clean_binlog returns non-OK.
    TestableSqliteConnection::ExposeCleanDonatedBinlogWithoutReplica(RdbBinlogNoSlaveTest::databaseName);
    // Assert: no crash (function handles clean failure via LOG_WARN and continues to close handle).
    SUCCEED();
}

/**
 * @tc.name: RdbStore_Binlog_CleanDonated_Success_001
 * @tc.desc: CleanDonatedBinlogWithoutReplica cleans donated segments on a binlog-enabled db
 * @tc.type: FUNC
 */
HWTEST_F(RdbBinlogNoSlaveTest, RdbStore_Binlog_CleanDonated_Success_001, TestSize.Level1)
{
    // Arrange: no-replica store with binlog enabled and some donation data written.
    InitNoSlaveDb();
    Insert(1, CHANGENUM);
    ASSERT_TRUE(CheckFolderExist(binlogDatabaseName)) << "binlog folder must exist before clean";
    // Act: call directly; sqlite3_clean_binlog runs in DONATED_MODE on the configured binlog handle.
    TestableSqliteConnection::ExposeCleanDonatedBinlogWithoutReplica(RdbBinlogNoSlaveTest::databaseName);
    // Assert: no crash, binlog folder still exists (clean does not delete the folder itself).
    EXPECT_TRUE(CheckFolderExist(binlogDatabaseName)) << "binlog folder must survive donated clean";
}

/**
 * @tc.name: RdbStore_Binlog_OnFull_NullPath_001
 * @tc.desc: BinlogOnFullFunc early-returns when dbPath is nullptr
 * @tc.type: FUNC
 */
HWTEST_F(RdbBinlogNoSlaveTest, RdbStore_Binlog_OnFull_NullPath_001, TestSize.Level1)
{
    // Act: pass nullptr as dbPath.
    TestableSqliteConnection::ExposeBinlogOnFullFunc(nullptr, 1, nullptr);
    // Assert: no crash.
    SUCCEED();
}

/**
 * @tc.name: RdbStore_Binlog_OnFull_SlaveInvalid_001
 * @tc.desc: BinlogOnFullFunc skips when replica is marked invalid
 * @tc.type: FUNC
 */
HWTEST_F(RdbBinlogNoSlaveTest, RdbStore_Binlog_OnFull_SlaveInvalid_001, TestSize.Level1)
{
    // Arrange: no-replica store with binlog, then mark slave invalid.
    InitNoSlaveDb();
    Insert(1, CHANGENUM);
    ssize_t sizeBefore = GetBinlogDataFileSize();
    SqliteUtils::SetSlaveInvalid(RdbBinlogNoSlaveTest::databaseName);
    // Act: call directly; IsSlaveInvalid short-circuits and returns early.
    TestableSqliteConnection::ExposeBinlogOnFullFunc(nullptr, 1, RdbBinlogNoSlaveTest::databaseName.c_str());
    // Assert: no crash, binlog file unchanged (no clean ran).
    EXPECT_EQ(GetBinlogDataFileSize(), sizeBefore) << "binlog size must not change when slave invalid";
    SqliteUtils::SetSlaveValid(RdbBinlogNoSlaveTest::databaseName);
}

/**
 * @tc.name: RdbStore_Binlog_OnFull_SlaveInterrupted_001
 * @tc.desc: BinlogOnFullFunc skips when replica is marked interrupted
 * @tc.type: FUNC
 */
HWTEST_F(RdbBinlogNoSlaveTest, RdbStore_Binlog_OnFull_SlaveInterrupted_001, TestSize.Level1)
{
    // Arrange: no-replica store with binlog, then mark slave interrupted.
    InitNoSlaveDb();
    Insert(1, CHANGENUM);
    ssize_t sizeBefore = GetBinlogDataFileSize();
    SqliteUtils::SetSlaveInterrupted(RdbBinlogNoSlaveTest::databaseName);
    // Act: call directly; IsSlaveInterrupted short-circuits and returns early.
    TestableSqliteConnection::ExposeBinlogOnFullFunc(nullptr, 1, RdbBinlogNoSlaveTest::databaseName.c_str());
    // Assert: no crash, binlog file unchanged (no clean ran).
    EXPECT_EQ(GetBinlogDataFileSize(), sizeBefore) << "binlog size must not change when slave interrupted";
    SqliteUtils::SetSlaveValid(RdbBinlogNoSlaveTest::databaseName);
}

/**
 * @tc.name: RdbStore_Binlog_OnFull_WithCallback_001
 * @tc.desc: BinlogOnFullFunc invokes replay callback when registered
 * @tc.type: FUNC
 */
HWTEST_F(RdbBinlogNoSlaveTest, RdbStore_Binlog_OnFull_WithCallback_001, TestSize.Level1)
{
    // Arrange: no-replica store, write data, then Backup to generate replica and register replay callback.
    InitNoSlaveDb();
    Insert(1, CHANGENUM);
    EXPECT_EQ(store->Backup(std::string(""), {}), E_OK) << "Backup registers the replay callback";
    // Act: call directly; GetReplayCallback returns non-null and invokes ReplayCallbackImpl.
    TestableSqliteConnection::ExposeBinlogOnFullFunc(nullptr, 1, RdbBinlogNoSlaveTest::databaseName.c_str());
    // Assert: no crash (callback executed; replay side-effects are environment-dependent).
    SUCCEED();
}

/**
 * @tc.name: RdbStore_Binlog_OnFull_NoReplicaClean_001
 * @tc.desc: BinlogOnFullFunc cleans donated binlog when no replica and no callback
 * @tc.type: FUNC
 */
HWTEST_F(RdbBinlogNoSlaveTest, RdbStore_Binlog_OnFull_NoReplicaClean_001, TestSize.Level1)
{
    // Arrange: no-replica store with binlog; no Backup so no replay callback registered and no slave file.
    InitNoSlaveDb();
    Insert(1, CHANGENUM);
    ASSERT_TRUE(CheckFolderExist(binlogDatabaseName)) << "binlog folder must exist";
    // Act: call directly; GetReplayCallback returns null, slave file absent -> CleanDonatedBinlogWithoutReplica.
    TestableSqliteConnection::ExposeBinlogOnFullFunc(nullptr, 1, RdbBinlogNoSlaveTest::databaseName.c_str());
    // Assert: no crash, binlog folder still exists (clean reclaims donated segments, not the folder).
    EXPECT_TRUE(CheckFolderExist(binlogDatabaseName)) << "binlog folder must survive clean";
}

/**
 * @tc.name: RdbStore_Binlog_NoSlaveDonation_001
 * @tc.desc: donation link verdict without replica equals the one with replica
 * @tc.type: FUNC
 */
HWTEST_F(RdbBinlogNoSlaveTest, RdbStore_Binlog_NoSlaveDonation_001, TestSize.Level1)
{
    // Arrange: no-replica store with donation data.
    InitNoSlaveDb();
    Insert(1, CHANGENUM);
    // Act: trigger the donation link without replica, then generate the replica and trigger again.
    int searchRet = store->SetSearchable(true);
    int donationNoReplica = store->RequestFullDataDonation({ "test" });
    EXPECT_EQ(store->Backup(std::string(""), {}), E_OK) << "replica generation via empty-path backup";
    int donationWithReplica = store->RequestFullDataDonation({ "test" });
    // Assert: the donation link is not blocked by replica absence (same env => same verdict).
    EXPECT_EQ(searchRet, E_OK) << "SetSearchable must reach the donation service";
    EXPECT_EQ(donationNoReplica, donationWithReplica)
        << "donation verdicts differ, no-replica=" << donationNoReplica
        << ", with-replica=" << donationWithReplica;
}

/**
 * @tc.name: RdbStore_Binlog_SingleMode_001
 * @tc.desc: SINGLE mode never enables binlog even when searchable
 * @tc.type: FUNC
 */
HWTEST_F(RdbBinlogNoSlaveTest, RdbStore_Binlog_SingleMode_001, TestSize.Level1)
{
    // Arrange: SINGLE mode store with searchable enabled.
    int errCode = E_OK;
    RdbStoreConfig config(RdbBinlogNoSlaveTest::databaseName);
    config.SetHaMode(HAMode::SINGLE);
    config.SetSearchable(true);
    RdbBinlogNoSlaveTestOpenCallback helper;
    RdbBinlogNoSlaveTest::store = RdbHelper::GetRdbStore(config, 1, helper, errCode);
    ASSERT_NE(store, nullptr);
    // Act: write donation-like data.
    Insert(1, CHANGENUM);
    // Assert: CRUD works but no binlog folder is generated.
    CheckNumber(store, CHANGENUM);
    EXPECT_FALSE(CheckFolderExist(binlogDatabaseName)) << "SINGLE mode must not enable binlog";
}

/**
 * @tc.name: RdbStore_Binlog_EncryptNoSlave_001
 * @tc.desc: encrypted MANUAL_TRIGGER store without replica never enables binlog
 * @tc.type: FUNC
 */
HWTEST_F(RdbBinlogNoSlaveTest, RdbStore_Binlog_EncryptNoSlave_001, TestSize.Level1)
{
    // Arrange: encrypted MANUAL_TRIGGER+Searchable store without replica.
    RdbHelper::DeleteRdbStore(RdbBinlogNoSlaveTest::slaveDatabaseName);
    int errCode = E_OK;
    RdbStoreConfig config(RdbBinlogNoSlaveTest::databaseName);
    config.SetHaMode(HAMode::MANUAL_TRIGGER);
    config.SetSearchable(true);
    config.SetEncryptStatus(true);
    RdbBinlogNoSlaveTestOpenCallback helper;
    RdbBinlogNoSlaveTest::store = RdbHelper::GetRdbStore(config, 1, helper, errCode);
    ASSERT_NE(store, nullptr);
    // Act: write donation-like data.
    Insert(1, CHANGENUM);
    // Assert: CRUD works but no binlog folder is generated.
    CheckNumber(store, CHANGENUM);
    EXPECT_FALSE(CheckFolderExist(binlogDatabaseName)) << "encrypted db must not enable binlog";
}

/**
 * @tc.name: RdbStore_Binlog_MemoryMode_001
 * @tc.desc: memory MANUAL_TRIGGER store fails to open and never enables binlog
 * @tc.type: FUNC
 */
HWTEST_F(RdbBinlogNoSlaveTest, RdbStore_Binlog_MemoryMode_001, TestSize.Level1)
{
    // Arrange: in-memory MANUAL_TRIGGER+Searchable store. Memory mode is incompatible with
    // MANUAL_TRIGGER replica path handling, so open is expected to fail.
    int errCode = E_OK;
    RdbStoreConfig config(RdbBinlogNoSlaveTest::databaseName);
    config.SetHaMode(HAMode::MANUAL_TRIGGER);
    config.SetSearchable(true);
    config.SetStorageMode(StorageMode::MODE_MEMORY);
    config.SetDBType(DB_SQLITE);
    RdbBinlogNoSlaveTestOpenCallback helper;
    RdbBinlogNoSlaveTest::store = RdbHelper::GetRdbStore(config, 1, helper, errCode);
    // Assert: open fails and no binlog folder is generated.
    EXPECT_EQ(store, nullptr) << "memory MANUAL_TRIGGER store should not open";
    EXPECT_FALSE(CheckFolderExist(binlogDatabaseName)) << "memory db must not enable binlog";
}

/**
 * @tc.name: RdbStore_Binlog_InvalidReplica_001
 * @tc.desc: binlog keeps writing when the replica exists but fails to open with invalid flags
 * @tc.type: FUNC
 */
HWTEST_F(RdbBinlogNoSlaveTest, RdbStore_Binlog_InvalidReplica_001, TestSize.Level1)
{
    // Arrange: generate a replica on a searchable MANUAL_TRIGGER store, then mark it invalid.
    InitNoSlaveDb();
    Insert(1, CHANGENUM);
    EXPECT_EQ(store->Backup(std::string(""), {}), E_OK) << "replica generation must succeed";
    store = nullptr;
    SqliteUtils::SetSlaveInvalid(RdbBinlogNoSlaveTest::databaseName);
    // Act: reopen the store (replica file exists but is invalid -> replica connection not opened),
    // then write and trigger donation on it.
    int errCode = E_OK;
    RdbStoreConfig config(RdbBinlogNoSlaveTest::databaseName);
    config.SetHaMode(HAMode::MANUAL_TRIGGER);
    config.SetSearchable(true);
    config.SetBundleName("com.example.distributed.rdb");
    RdbBinlogNoSlaveTestOpenCallback helper;
    RdbBinlogNoSlaveTest::store = RdbHelper::GetRdbStore(config, 1, helper, errCode);
    ASSERT_NE(store, nullptr);
    auto [ret, info] = store->GetPrimaryReplicaInfo({});
    ASSERT_EQ(ret, E_OK);
    ASSERT_TRUE(info.binlogInfo.exist) << "binlog must stay enabled with an invalid replica";
    ssize_t baselineSize = GetBinlogDataFileSize();
    Insert(CHANGENUM + 1, CHANGENUM);
    Update(CHANGENUM + 1, CHANGENUM);
    int searchRet = store->SetSearchable(true);
    int donationInvalid = store->RequestFullDataDonation({ "test" });
    // Assert: binlog keeps writing, donation data stays intact, donation verdict matches the
    // valid-replica verdict in the same process.
    EXPECT_TRUE(WaitForBinlogSizeAdvance(baselineSize)) << "binlog file size must grow with an invalid replica";
    CheckNumber(store, CHANGENUM * 2);
    EXPECT_EQ(searchRet, E_OK) << "SetSearchable must reach the donation service";
    store = nullptr;
    SqliteUtils::SetSlaveValid(RdbBinlogNoSlaveTest::databaseName);
    RdbBinlogNoSlaveTest::store = RdbHelper::GetRdbStore(config, 1, helper, errCode);
    ASSERT_NE(store, nullptr);
    int donationValid = store->RequestFullDataDonation({ "test" });
    EXPECT_EQ(donationInvalid, donationValid)
        << "donation verdicts differ, invalid-replica=" << donationInvalid
        << ", valid-replica=" << donationValid;
}

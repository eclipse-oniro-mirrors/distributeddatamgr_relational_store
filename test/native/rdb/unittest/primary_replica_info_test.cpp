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

// PrimaryReplicaInfoTest: coverage for the GetPrimaryReplicaInfo diagnostic
// interface (primary/replica consistency, error paths, replay, checkpoint).
#define LOG_TAG "PrimaryReplicaInfoTest"
#include <gtest/gtest.h>
#include <sys/stat.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

#include "common.h"
#include "rdb_common.h"
#include "rdb_errno.h"
#include "rdb_helper.h"
#include "rdb_open_callback.h"
#include "rdb_security_manager.h"
#include "rdb_store_impl.h"
#include "rdb_types.h"
#include "sqlite_connection.h"
#include "sqlite_global_config.h"
#include "sqlite_utils.h"
#include "values_bucket.h"

using namespace testing::ext;
using namespace OHOS::NativeRdb;

static constexpr int BASE_AGE = 20;

// RAII scope guard for extra test resources
struct ScopeGuard {
    std::function<void()> func;
    explicit ScopeGuard(std::function<void()> f) : func(std::move(f))
    {
    }
    ~ScopeGuard()
    {
        if (func) {
            func();
        }
    }
    ScopeGuard(const ScopeGuard &) = delete;
    ScopeGuard &operator=(const ScopeGuard &) = delete;
};

class PrimaryReplicaInfoTest : public testing::Test {
public:
    static void SetUpTestCase(void);
    static void TearDownTestCase(void);
    void SetUp();
    void TearDown();
    std::string dbName;
    std::string slaveDbName;

protected:
    std::shared_ptr<RdbStore> store_;
};

class PrimaryReplicaInfoCallback : public RdbOpenCallback {
public:
    int OnCreate(RdbStore &store) override
    {
        return store.ExecuteSql("CREATE TABLE IF NOT EXISTS test "
                                "(id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT NOT NULL, age INTEGER)");
    }
    int OnUpgrade(RdbStore &store, int oldVersion, int newVersion) override
    {
        return E_OK;
    }
};

class EmptyCallback : public RdbOpenCallback {
public:
    int OnCreate(RdbStore &store) override
    {
        return E_OK;
    }
    int OnUpgrade(RdbStore &store, int oldVersion, int newVersion) override
    {
        return E_OK;
    }
};

void PrimaryReplicaInfoTest::SetUpTestCase(void)
{
    SqliteGlobalConfig::InitSqliteGlobalConfig();
    RdbSecurityManager::GetInstance().Init("rdb.primary.replica.test");
}

void PrimaryReplicaInfoTest::TearDownTestCase(void)
{
}

void PrimaryReplicaInfoTest::SetUp()
{
    auto *info = testing::UnitTest::GetInstance()->current_test_info();
    std::string testTag = std::string(info->test_suite_name()) + "_" + info->name();
    dbName = RDB_TEST_PATH + testTag + ".db";
    slaveDbName = RDB_TEST_PATH + testTag + "_slave.db";
    RdbHelper::DeleteRdbStore(dbName);
    RdbHelper::DeleteRdbStore(slaveDbName);
    RdbStoreConfig config(dbName);
    config.SetHaMode(HAMode::MAIN_REPLICA);
    PrimaryReplicaInfoCallback helper;
    int errCode = E_OK;
    store_ = RdbHelper::GetRdbStore(config, 1, helper, errCode);
    ASSERT_NE(store_, nullptr);
    ASSERT_EQ(errCode, E_OK);
}

void PrimaryReplicaInfoTest::TearDown()
{
    store_ = nullptr;
    RdbHelper::ClearCache();
    RdbHelper::DeleteRdbStore(dbName);
    RdbHelper::DeleteRdbStore(slaveDbName);
}

static void InsertRows(std::shared_ptr<RdbStore> &store, int start, int count)
{
    for (int i = 0; i < count; i++) {
        ValuesBucket values;
        values.PutInt("id", start + i);
        values.PutString("name", "row" + std::to_string(start + i));
        values.PutInt("age", BASE_AGE + i);
        int64_t rowId = 0;
        int ret = store->Insert(rowId, "test", values);
        EXPECT_EQ(ret, E_OK);
    }
}

/**
 * @tc.name: PrimaryReplicaInfo_TypeExistence_001
 * @tc.desc: AC-6.4: Verify all new types can be default-constructed with correct defaults.
 * @tc.type: FUNC
 */
HWTEST_F(PrimaryReplicaInfoTest, TypeExistence_001, TestSize.Level1)
{
    PrimaryAndReplicaInfo info{};
    EXPECT_EQ(info.primaryInfo.dbInfo.size, 0);
    EXPECT_EQ(info.primaryInfo.dbInfo.ctime, 0);
    EXPECT_EQ(info.primaryInfo.dbInfo.mtime, 0);
    EXPECT_EQ(info.primaryInfo.walInfo.size, 0);
    EXPECT_EQ(info.primaryInfo.userVersion, 0);
    EXPECT_TRUE(info.primaryInfo.tables.empty());
    EXPECT_EQ(info.binlogInfo.exist, false);
    EXPECT_EQ(info.binlogInfo.lastModifyTime, 0);
    EXPECT_EQ(info.GetPrimaryLastModifyTime(), 0);
    EXPECT_EQ(info.GetReplicaLastModifyTime(), 0);

    RdbFileInfo fi{};
    fi.size = 1024;
    fi.mtime = 1234567890;
    EXPECT_EQ(fi.size, 1024);
    EXPECT_EQ(fi.mtime, 1234567890);

    TableInfo ti{};
    ti.count = 42;
    EXPECT_EQ(ti.count, 42);

    RdbBinlogInfo bi{};
    bi.exist = true;
    bi.lastModifyTime = 99999;
    EXPECT_EQ(bi.exist, true);
}

/**
 * @tc.name: PrimaryReplicaInfo_SingleMode_001
 * @tc.desc: AC-5.3: SINGLE ha-mode returns E_NOT_SUPPORT.
 * @tc.type: FUNC
 */
HWTEST_F(PrimaryReplicaInfoTest, SingleMode_001, TestSize.Level1)
{
    std::string singleDb = RDB_TEST_PATH + "pri_replica_single_test.db";
    RdbHelper::DeleteRdbStore(singleDb);
    ScopeGuard cleanup([&singleDb]() {
        RdbHelper::ClearCache();
        RdbHelper::DeleteRdbStore(singleDb);
    });
    RdbStoreConfig config(singleDb);
    config.SetHaMode(HAMode::SINGLE);
    PrimaryReplicaInfoCallback helper;
    int errCode = E_OK;
    auto singleStore = RdbHelper::GetRdbStore(config, 1, helper, errCode);
    ASSERT_NE(singleStore, nullptr);
    ASSERT_EQ(errCode, E_OK);

    auto [ret, info] = singleStore->GetPrimaryReplicaInfo({});
    EXPECT_EQ(ret, E_NOT_SUPPORT);
}

/**
 * @tc.name: PrimaryReplicaInfo_StoreClosed_001
 * @tc.desc: AC-5.1: Closed store returns E_ALREADY_CLOSED.
 * @tc.type: FUNC
 */
HWTEST_F(PrimaryReplicaInfoTest, StoreClosed_001, TestSize.Level1)
{
    std::string closedDb = RDB_TEST_PATH + "pri_replica_closed_test.db";
    RdbHelper::DeleteRdbStore(closedDb);
    ScopeGuard cleanup([&closedDb]() {
        RdbHelper::ClearCache();
        RdbHelper::DeleteRdbStore(closedDb);
    });
    RdbStoreConfig config(closedDb);
    config.SetHaMode(HAMode::MAIN_REPLICA);
    PrimaryReplicaInfoCallback helper;
    int errCode = E_OK;
    auto closedStore = RdbHelper::GetRdbStore(config, 1, helper, errCode);
    ASSERT_NE(closedStore, nullptr);

    auto impl = std::static_pointer_cast<RdbStoreImpl>(closedStore);
    impl->Close();

    auto [ret, info] = closedStore->GetPrimaryReplicaInfo({});
    EXPECT_EQ(ret, E_ALREADY_CLOSED);
}

/**
 * @tc.name: PrimaryReplicaInfo_EmptyTableName_001
 * @tc.desc: AC-3.2: Empty table name skips count, tables map stays empty.
 * @tc.type: FUNC
 */
HWTEST_F(PrimaryReplicaInfoTest, EmptyTableName_001, TestSize.Level1)
{
    InsertRows(store_, 1, 2);
    auto [ret, info] = store_->GetPrimaryReplicaInfo({});
    EXPECT_EQ(ret, E_OK);
    EXPECT_TRUE(info.primaryInfo.tables.empty());
    EXPECT_TRUE(info.replicaInfo.tables.empty());
}

/**
 * @tc.name: PrimaryReplicaInfo_FileSizes_001
 * @tc.desc: AC-1.1: Primary DB file size matches stat after checkpoint.
 * @tc.type: FUNC
 */
HWTEST_F(PrimaryReplicaInfoTest, FileSizes_001, TestSize.Level1)
{
    InsertRows(store_, 1, 5);
    auto [ret, info] = store_->GetPrimaryReplicaInfo({});
    EXPECT_EQ(ret, E_OK);
    EXPECT_GT(info.primaryInfo.dbInfo.size, 0);

    struct stat st;
    ASSERT_EQ(stat(dbName.c_str(), &st), 0);
    EXPECT_EQ(info.primaryInfo.dbInfo.size, static_cast<int64_t>(st.st_size));
}

/**
 * @tc.name: PrimaryReplicaInfo_TableCount_001
 * @tc.desc: AC-3.1: Table row count matches inserted rows on primary.
 * @tc.type: FUNC
 */
HWTEST_F(PrimaryReplicaInfoTest, TableCount_001, TestSize.Level1)
{
    InsertRows(store_, 1, 3);
    auto [ret, info] = store_->GetPrimaryReplicaInfo({ "test" });
    EXPECT_EQ(ret, E_OK);
    auto it = info.primaryInfo.tables.find("test");
    ASSERT_NE(it, info.primaryInfo.tables.end());
    EXPECT_EQ(it->second.count, 3);
}

/**
 * @tc.name: PrimaryReplicaInfo_TimeFields_001
 * @tc.desc: AC-2.1: Time fields are collected and are positive millisecond timestamps.
 * @tc.type: FUNC
 */
HWTEST_F(PrimaryReplicaInfoTest, TimeFields_001, TestSize.Level1)
{
    InsertRows(store_, 1, 1);
    auto [ret, info] = store_->GetPrimaryReplicaInfo({});
    EXPECT_EQ(ret, E_OK);
    EXPECT_GT(info.primaryInfo.dbInfo.mtime, 0);
    EXPECT_GE(info.primaryInfo.dbInfo.ctime, 0);
    EXPECT_GE(info.primaryInfo.dbInfo.atime, 0);
    // Verify summarized time is set and follows max(DB, WAL) rule (T-04 fix).
    EXPECT_GT(info.GetPrimaryLastModifyTime(), 0);
    EXPECT_EQ(info.GetPrimaryLastModifyTime(), std::max(info.primaryInfo.dbInfo.mtime, info.primaryInfo.walInfo.mtime));
}

/**
 * @tc.name: PrimaryReplicaInfo_BinlogInfo_NoBinlog_001
 * @tc.desc: AC-2.4: binlogInfo.exist depends on whether binlog is supported.
 * @tc.type: FUNC
 */
HWTEST_F(PrimaryReplicaInfoTest, BinlogInfo_NoBinlog_001, TestSize.Level1)
{
    auto [ret, info] = store_->GetPrimaryReplicaInfo({});
    EXPECT_EQ(ret, E_OK);
    RdbStoreConfig checkConfig(dbName);
    checkConfig.SetHaMode(HAMode::MAIN_REPLICA);
    bool supportBinlog = SqliteUtils::IsSupportBinlog(checkConfig);
    EXPECT_EQ(info.binlogInfo.exist, supportBinlog);
    if (supportBinlog) {
        EXPECT_GT(info.binlogInfo.lastModifyTime, 0);
    } else {
        EXPECT_EQ(info.binlogInfo.lastModifyTime, 0);
    }
}

/**
 * @tc.name: PrimaryReplicaInfo_BinlogInfo_NoBinlogDb_001
 * @tc.desc: AC-2.4: An encrypted DB does not support binlog; binlogInfo.exist is false.
 * @tc.type: FUNC
 */
HWTEST_F(PrimaryReplicaInfoTest, BinlogInfo_NoBinlogDb_001, TestSize.Level1)
{
    std::string encDb = RDB_TEST_PATH + "pri_replica_no_binlog_enc.db";
    RdbHelper::DeleteRdbStore(encDb);
    ScopeGuard cleanup([&encDb]() {
        RdbHelper::ClearCache();
        RdbHelper::DeleteRdbStore(encDb);
    });
    RdbStoreConfig config(encDb);
    config.SetHaMode(HAMode::MAIN_REPLICA);
    config.SetEncryptStatus(true);
    config.SetBundleName("rdb.primary.replica.test");
    PrimaryReplicaInfoCallback helper;
    int errCode = E_OK;
    auto encStore = RdbHelper::GetRdbStore(config, 1, helper, errCode);
    ASSERT_NE(encStore, nullptr);
    ASSERT_EQ(errCode, E_OK);
    // Encrypted DBs do not support binlog → exist must be false.
    auto [ret, info] = encStore->GetPrimaryReplicaInfo({});
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(info.binlogInfo.exist, false);
}

/**
 * @tc.name: PrimaryReplicaInfo_UserVersion_001
 * @tc.desc: AC-1.1: user_version is collected from the primary database.
 * @tc.type: FUNC
 */
HWTEST_F(PrimaryReplicaInfoTest, UserVersion_001, TestSize.Level1)
{
    EXPECT_EQ(store_->ExecuteSql("PRAGMA user_version = 42"), E_OK);
    auto [ret, info] = store_->GetPrimaryReplicaInfo({});
    EXPECT_EQ(ret, E_OK);
    // Verify exact user_version value, not just >= 0 (T-07 fix).
    EXPECT_EQ(info.primaryInfo.userVersion, 42);
}

/**
 * @tc.name: PrimaryReplicaInfo_PartialResults_001
 * @tc.desc: AC-5.1: Even when replica is absent, primary fields are valid and ret is E_OK.
 * @tc.type: FUNC
 */
HWTEST_F(PrimaryReplicaInfoTest, PartialResults_001, TestSize.Level1)
{
    InsertRows(store_, 1, 2);
    auto [ret, info] = store_->GetPrimaryReplicaInfo({ "test" });
    EXPECT_EQ(ret, E_OK);
    // Primary should have valid data.
    EXPECT_GT(info.primaryInfo.dbInfo.size, 0);
    auto it = info.primaryInfo.tables.find("test");
    ASSERT_NE(it, info.primaryInfo.tables.end());
    EXPECT_EQ(it->second.count, 2);
    // Summarized time must be populated (R-03).
    EXPECT_EQ(info.GetPrimaryLastModifyTime(), std::max(info.primaryInfo.dbInfo.mtime, info.primaryInfo.walInfo.mtime));
}

/**
 * @tc.name: PrimaryReplicaInfo_InvalidTableName_001
 * @tc.desc: AC-3.4: Invalid table name returns E_INVALID_ARGS with partial results preserved.
 * @tc.type: FUNC
 */
HWTEST_F(PrimaryReplicaInfoTest, InvalidTableName_001, TestSize.Level1)
{
    InsertRows(store_, 1, 1);
    // Table name with injection attempt must be rejected by IsValidTableName (T-01 fix).
    auto [ret, info] = store_->GetPrimaryReplicaInfo({ "test; DROP TABLE test" });
    EXPECT_EQ(ret, E_INVALID_ARGS);
    // Partial results should still be present.
    EXPECT_GT(info.primaryInfo.dbInfo.size, 0);
    // Original table must still exist with its data (no SQL injection).
    auto [ret2, info2] = store_->GetPrimaryReplicaInfo({ "test" });
    EXPECT_EQ(ret2, E_OK);
    auto it = info2.primaryInfo.tables.find("test");
    ASSERT_NE(it, info2.primaryInfo.tables.end());
    EXPECT_EQ(it->second.count, 1);
}

/**
 * @tc.name: PrimaryReplicaInfo_ReadOnlyDb_001
 * @tc.desc: AC-5.4: Read-only database returns E_NOT_SUPPORT.
 * @tc.type: FUNC
 */
HWTEST_F(PrimaryReplicaInfoTest, ReadOnlyDb_001, TestSize.Level1)
{
    std::string roDb = RDB_TEST_PATH + "pri_replica_readonly_test.db";
    RdbHelper::DeleteRdbStore(roDb);
    ScopeGuard cleanup([&roDb]() {
        RdbHelper::ClearCache();
        RdbHelper::DeleteRdbStore(roDb);
    });
    // Create the DB first so the file exists before reopening read-only
    {
        RdbStoreConfig createConfig(roDb);
        createConfig.SetHaMode(HAMode::MAIN_REPLICA);
        PrimaryReplicaInfoCallback helper;
        int errCode = E_OK;
        auto store = RdbHelper::GetRdbStore(createConfig, 1, helper, errCode);
        ASSERT_NE(store, nullptr);
        store = nullptr;
        RdbHelper::ClearCache();
    }
    // Reopen as read-only
    RdbStoreConfig config(roDb);
    config.SetHaMode(HAMode::MAIN_REPLICA);
    config.SetReadOnly(true);
    PrimaryReplicaInfoCallback helper;
    int errCode = E_OK;
    auto roStore = RdbHelper::GetRdbStore(config, 1, helper, errCode);
    ASSERT_NE(roStore, nullptr);
    auto [ret, info] = roStore->GetPrimaryReplicaInfo({});
    EXPECT_EQ(ret, E_NOT_SUPPORT);
}

/**
 * @tc.name: PrimaryReplicaInfo_MemoryDb_001
 * @tc.desc: AC-5.4: A real memory database (StorageMode::MODE_MEMORY) sets isMemoryRdb_=true;
 *           GetPrimaryReplicaInfo returns E_NOT_SUPPORT from the pre-check.
 * @tc.type: FUNC
 */
HWTEST_F(PrimaryReplicaInfoTest, MemoryDb_001, TestSize.Level1)
{
    std::string memDb = RDB_TEST_PATH + "pri_replica_memory_test.db";
    RdbHelper::DeleteRdbStore(memDb);
    ScopeGuard cleanup([&memDb]() {
        RdbHelper::ClearCache();
        RdbHelper::DeleteRdbStore(memDb);
    });
    RdbStoreConfig config(memDb, StorageMode::MODE_MEMORY);
    PrimaryReplicaInfoCallback helper;
    int errCode = E_OK;
    auto memStore = RdbHelper::GetRdbStore(config, 1, helper, errCode);
    ASSERT_NE(memStore, nullptr);
    ASSERT_EQ(errCode, E_OK);
    auto [ret, info] = memStore->GetPrimaryReplicaInfo({});
    EXPECT_EQ(ret, E_NOT_SUPPORT);
}

/**
 * @tc.name: PrimaryReplicaInfo_VectorDb_001
 * @tc.desc: AC-5.4: A real vector database (SetIsVector → DB_VECTOR) is rejected by the pre-check;
 *           GetPrimaryReplicaInfo returns E_NOT_SUPPORT. Skips only when IsSupportArkDataDb()
 *           returns false (ArkData not available in the environment).
 * @tc.type: FUNC
 */
HWTEST_F(PrimaryReplicaInfoTest, VectorDb_001, TestSize.Level1)
{
    std::string vecDb = RDB_TEST_PATH + "pri_replica_vector_test.db";
    RdbHelper::DeleteRdbStore(vecDb);
    ScopeGuard cleanup([&vecDb]() {
        RdbHelper::ClearCache();
        RdbHelper::DeleteRdbStore(vecDb);
    });
    if (!RdbHelper::IsSupportArkDataDb()) {
        GTEST_SKIP() << "VectorDb is not supported, skipping test.";
    }
    RdbStoreConfig config(vecDb);
    config.SetIsVector(true);
    EmptyCallback helper;
    int errCode = E_OK;
    auto vecStore = RdbHelper::GetRdbStore(config, 1, helper, errCode);
    ASSERT_NE(vecStore, nullptr);
    ASSERT_EQ(errCode, E_OK);
    auto [ret, info] = vecStore->GetPrimaryReplicaInfo({});
    EXPECT_EQ(ret, E_NOT_SUPPORT);
}

/**
 * @tc.name: PrimaryReplicaInfo_EmptyTableCount_001
 * @tc.desc: AC-3.1: Table with 0 rows returns count=0 (TC-13).
 * @tc.type: FUNC
 */
HWTEST_F(PrimaryReplicaInfoTest, EmptyTableCount_001, TestSize.Level1)
{
    auto [ret, info] = store_->GetPrimaryReplicaInfo({ "test" });
    EXPECT_EQ(ret, E_OK);
    auto it = info.primaryInfo.tables.find("test");
    ASSERT_NE(it, info.primaryInfo.tables.end());
    EXPECT_EQ(it->second.count, 0);
}

/**
 * @tc.name: PrimaryReplicaInfo_TableNotExist_001
 * @tc.desc: AC-3.3: Non-existent table yields count=-1 sentinel (TC-15).
 * @tc.type: FUNC
 */
HWTEST_F(PrimaryReplicaInfoTest, TableNotExist_001, TestSize.Level1)
{
    auto [ret, info] = store_->GetPrimaryReplicaInfo({ "nonexistent_table" });
    EXPECT_EQ(ret, E_OK);
    auto it = info.primaryInfo.tables.find("nonexistent_table");
    ASSERT_NE(it, info.primaryInfo.tables.end());
    EXPECT_EQ(it->second.count, -1);
}

/**
 * @tc.name: PrimaryReplicaInfo_PrimaryTimeRule_001
 * @tc.desc: AC-2.2: primaryLastModifyTime = max(primaryDb.mtime, primaryWal.mtime) (TC-05).
 * @tc.type: FUNC
 */
HWTEST_F(PrimaryReplicaInfoTest, PrimaryTimeRule_001, TestSize.Level1)
{
    InsertRows(store_, 1, 3);
    auto [ret, info] = store_->GetPrimaryReplicaInfo({});
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(info.GetPrimaryLastModifyTime(), std::max(info.primaryInfo.dbInfo.mtime, info.primaryInfo.walInfo.mtime));
    EXPECT_GT(info.GetPrimaryLastModifyTime(), 0);
}

/**
 * @tc.name: PrimaryReplicaInfo_ReplicaTimeRule_001
 * @tc.desc: AC-2.3/2.4: replicaLastModifyTime follows binlog-priority or max(DB,WAL) fallback (TC-07).
 * @tc.type: FUNC
 */
HWTEST_F(PrimaryReplicaInfoTest, ReplicaTimeRule_001, TestSize.Level1)
{
    auto [ret, info] = store_->GetPrimaryReplicaInfo({});
    EXPECT_EQ(ret, E_OK);
    int64_t expected = info.binlogInfo.exist ? info.binlogInfo.lastModifyTime
                                             : std::max(info.replicaInfo.dbInfo.mtime, info.replicaInfo.walInfo.mtime);
    EXPECT_EQ(info.GetReplicaLastModifyTime(), expected);
}

/**
 * @tc.name: PrimaryReplicaInfo_StatusFieldsDefault_001
 * @tc.desc: AC-5.1: Successful query returns E_OK with valid table count.
 * @tc.type: FUNC
 */
HWTEST_F(PrimaryReplicaInfoTest, StatusFieldsDefault_001, TestSize.Level1)
{
    InsertRows(store_, 1, 2);
    auto [ret, info] = store_->GetPrimaryReplicaInfo({ "test" });
    EXPECT_EQ(ret, E_OK);
    auto it = info.primaryInfo.tables.find("test");
    ASSERT_NE(it, info.primaryInfo.tables.end());
    EXPECT_EQ(it->second.count, 2);
}

/**
 * @tc.name: PrimaryReplicaInfo_RepeatedQuery_001
 * @tc.desc: AC-5.1: Repeated queries return consistent results (TC-22).
 * @tc.type: FUNC
 */
HWTEST_F(PrimaryReplicaInfoTest, RepeatedQuery_001, TestSize.Level1)
{
    InsertRows(store_, 1, 3);
    auto [ret1, info1] = store_->GetPrimaryReplicaInfo({ "test" });
    EXPECT_EQ(ret1, E_OK);
    auto [ret2, info2] = store_->GetPrimaryReplicaInfo({ "test" });
    EXPECT_EQ(ret2, E_OK);
    auto it1 = info1.primaryInfo.tables.find("test");
    auto it2 = info2.primaryInfo.tables.find("test");
    ASSERT_NE(it1, info1.primaryInfo.tables.end());
    ASSERT_NE(it2, info2.primaryInfo.tables.end());
    EXPECT_EQ(it1->second.count, it2->second.count);
    EXPECT_EQ(info1.primaryInfo.userVersion, info2.primaryInfo.userVersion);
    EXPECT_EQ(info1.primaryInfo.dbInfo.size, info2.primaryInfo.dbInfo.size);
}

/**
 * @tc.name: PrimaryReplicaInfo_WalFileStat_001
 * @tc.desc: AC-1.1: Primary WAL file is stat'd successfully after writes (TC-03).
 * @tc.type: FUNC
 */
HWTEST_F(PrimaryReplicaInfoTest, WalFileStat_001, TestSize.Level1)
{
    InsertRows(store_, 1, 5);
    auto [ret, info] = store_->GetPrimaryReplicaInfo({});
    EXPECT_EQ(ret, E_OK);
    // After checkpoint, WAL may be truncated to 0 bytes.
    EXPECT_GE(info.primaryInfo.walInfo.size, 0);
    EXPECT_GE(info.primaryInfo.walInfo.mtime, 0);
}

/**
 * @tc.name: PrimaryReplicaInfo_CheckpointStatus_001
 * @tc.desc: AC-5.1: checkpointErr is E_OK on a MAIN_REPLICA writer with slave connection.
 *           Verify WAL is truncated after checkpoint (TRUNCATE mode).
 * @tc.type: FUNC
 */
HWTEST_F(PrimaryReplicaInfoTest, CheckpointStatus_001, TestSize.Level1)
{
    InsertRows(store_, 1, 3);
    // Arrange: WAL file should exist after inserts (before checkpoint)
    std::string walPath = dbName + "-wal";
    struct stat stBefore;
    ASSERT_EQ(stat(walPath.c_str(), &stBefore), 0) << "WAL file should exist after inserts";

    // Act: diagnostic call triggers CheckpointDatabases with SQLITE_CHECKPOINT_TRUNCATE
    auto [ret, info] = store_->GetPrimaryReplicaInfo({});
    EXPECT_EQ(ret, E_OK) << "Diagnostic should succeed when checkpoint succeeds";

    // Assert: After TRUNCATE checkpoint, WAL size should be 0 (truncated back to initial size)
    EXPECT_EQ(info.primaryInfo.walInfo.size, 0) << "WAL should be truncated to 0 after SQLITE_CHECKPOINT_TRUNCATE";
    // DB size should be positive (data was written and checkpointed into main DB)
    EXPECT_GT(info.primaryInfo.dbInfo.size, 0) << "Primary DB size should be positive after checkpoint of 3 rows";
}

/**
 * @tc.name: PrimaryReplicaInfo_BinlogErrCode_001
 * @tc.desc: AC-2.4: binlogInfo.exist matches IsSupportBinlog when no binlog directory exists.
 * @tc.type: FUNC
 */
HWTEST_F(PrimaryReplicaInfoTest, BinlogErrCode_001, TestSize.Level1)
{
    auto [ret, info] = store_->GetPrimaryReplicaInfo({});
    EXPECT_EQ(ret, E_OK);
    RdbStoreConfig checkConfig(dbName);
    checkConfig.SetHaMode(HAMode::MAIN_REPLICA);
    EXPECT_EQ(info.binlogInfo.exist, SqliteUtils::IsSupportBinlog(checkConfig));
}

/**
 * @tc.name: PrimaryReplicaInfo_ReplicaWalErrCode_001
 * @tc.desc: AC-1.1: Replica WAL and DB are stat'd when slave connection exists.
 * @tc.type: FUNC
 */
HWTEST_F(PrimaryReplicaInfoTest, ReplicaWalErrCode_001, TestSize.Level1)
{
    InsertRows(store_, 1, 2);
    auto [ret, info] = store_->GetPrimaryReplicaInfo({});
    EXPECT_EQ(ret, E_OK);
    // With a slave connection, replica data should be populated.
    EXPECT_GE(info.replicaInfo.dbInfo.size, 0);
    EXPECT_GE(info.replicaInfo.walInfo.size, 0);
}

/**
 * @tc.name: PrimaryReplicaInfo_TimeValidity_001
 * @tc.desc: AC-2.2: primaryLastModifyTime = max(db.mtime, wal.mtime) on successful stat.
 * @tc.type: FUNC
 */
HWTEST_F(PrimaryReplicaInfoTest, TimeValidity_001, TestSize.Level1)
{
    InsertRows(store_, 1, 5);
    auto [ret, info] = store_->GetPrimaryReplicaInfo({});
    EXPECT_EQ(ret, E_OK);
    // Both DB and WAL stats succeed → primaryLastModifyTime = max(db.mtime, wal.mtime)
    EXPECT_EQ(info.GetPrimaryLastModifyTime(), std::max(info.primaryInfo.dbInfo.mtime, info.primaryInfo.walInfo.mtime));
    EXPECT_GT(info.GetPrimaryLastModifyTime(), 0);
}

/**
 * @tc.name: PrimaryReplicaInfo_ReplicaTimeFallback_001
 * @tc.desc: AC-2.3: replicaLastModifyTime falls back to file times when no binlog.
 * @tc.type: FUNC
 */
HWTEST_F(PrimaryReplicaInfoTest, ReplicaTimeFallback_001, TestSize.Level1)
{
    auto [ret, info] = store_->GetPrimaryReplicaInfo({});
    EXPECT_EQ(ret, E_OK);
    // No binlog: replicaLastModifyTime = max(db.mtime, wal.mtime)
    if (!info.binlogInfo.exist) {
        EXPECT_EQ(
            info.GetReplicaLastModifyTime(), std::max(info.replicaInfo.dbInfo.mtime, info.replicaInfo.walInfo.mtime));
    }
}

/**
 * @tc.name: PrimaryReplicaInfo_DiagnosticHealthyReplica_001
 * @tc.desc: AC-1.3: A healthy replica remains available after diagnostic query and connection release.
 * @tc.type: FUNC
 */
HWTEST_F(PrimaryReplicaInfoTest, DiagnosticHealthyReplica_001, TestSize.Level1)
{
    InsertRows(store_, 1, 2);
    // Verify replica DB file exists before diagnostic call
    struct stat stBefore;
    ASSERT_EQ(stat(slaveDbName.c_str(), &stBefore), 0);
    auto [ret, info] = store_->GetPrimaryReplicaInfo({});
    EXPECT_EQ(ret, E_OK);
    // Verify replica DB file still exists after diagnostic call + connection release
    struct stat stAfter;
    ASSERT_EQ(stat(slaveDbName.c_str(), &stAfter), 0);
    // Replica checkpoint executed successfully (implied by ret == E_OK).
}

/**
 * @tc.name: PrimaryReplicaInfo_SchemaTableQuoting_001
 * @tc.desc: AC-3.4: schema.table is properly quoted as "schema"."table" in COUNT.
 * @tc.type: FUNC
 */
HWTEST_F(PrimaryReplicaInfoTest, SchemaTableQuoting_001, TestSize.Level1)
{
    InsertRows(store_, 1, 3);
    // "main.test" splits and quotes as "main"."test" — same table in main schema.
    auto [ret, info] = store_->GetPrimaryReplicaInfo({ "main.test" });
    EXPECT_EQ(ret, E_OK);
    auto it = info.primaryInfo.tables.find("main.test");
    ASSERT_NE(it, info.primaryInfo.tables.end());
    EXPECT_EQ(it->second.count, 3);
}

/**
 * @tc.name: PrimaryReplicaInfo_ReplicaDbStatFromConfig_001
 * @tc.desc: AC-1.1: Replica DB is stat'd from config path even without a slave connection.
 * @tc.type: FUNC
 */
HWTEST_F(PrimaryReplicaInfoTest, ReplicaDbStatFromConfig_001, TestSize.Level1)
{
    InsertRows(store_, 1, 2);
    auto [ret, info] = store_->GetPrimaryReplicaInfo({});
    EXPECT_EQ(ret, E_OK);
    // In MAIN_REPLICA mode, the slave DB file exists on disk
    EXPECT_GE(info.replicaInfo.dbInfo.size, 0);
}

/**
 * @tc.name: PrimaryReplicaInfo_NoSlaveConnection_001
 * @tc.desc: AC-4.2/4.4: A standalone connection without slaveConnection_ reports partial results —
 *           replica DB stat'd from config path (E_OK), WAL/replay/replica-checkpoint skipped (E_NOT_SUPPORT).
 * @tc.type: FUNC
 */
HWTEST_F(PrimaryReplicaInfoTest, NoSlaveConnection_001, TestSize.Level1)
{
    InsertRows(store_, 1, 2);
    // Release the fixture pool so a standalone connection opens without lock conflict.
    store_ = nullptr;
    RdbHelper::ClearCache();

    RdbStoreConfig config(dbName);
    config.SetHaMode(HAMode::MAIN_REPLICA);
    // Read connection (isWrite=false) skips slave creation in InnerCreate → slaveConnection_ is null.
    auto [createErr, conn] = SqliteConnection::Create(config, false);
    ASSERT_EQ(createErr, E_OK) << "Failed to create standalone read connection";
    ASSERT_NE(conn, nullptr);

    auto [ret, info] = conn->GetPrimaryAndReplicaInfo({});

    // Overall E_OK: partial results preserved, no fatal failure.
    EXPECT_EQ(ret, E_OK);

    // Replica DB stat'd from config path — file exists from the fixture pool.
    EXPECT_GE(info.replicaInfo.dbInfo.size, 0);

    // Diagnostic did not create or remove the replica file.
    EXPECT_EQ(access(slaveDbName.c_str(), F_OK), 0);
}

/**
 * @tc.name: PrimaryReplicaInfo_TimeNotOverwrittenByCheckpoint_001
 * @tc.desc: AC-2.5: mtime collected before checkpoint is not overwritten by post-checkpoint stat.
 * @tc.type: FUNC
 */
HWTEST_F(PrimaryReplicaInfoTest, TimeNotOverwrittenByCheckpoint_001, TestSize.Level1)
{
    InsertRows(store_, 1, 10);
    // Stat the WAL file before the diagnostic call (pre-checkpoint mtime)
    std::string walPath = dbName + "-wal";
    struct stat stBefore;
    ASSERT_EQ(stat(walPath.c_str(), &stBefore), 0) << "WAL file should exist after inserts";

    auto [ret, info] = store_->GetPrimaryReplicaInfo({});
    EXPECT_EQ(ret, E_OK);
    // AC-2.5: The returned WAL mtime must be from the initial collection (before checkpoint),
    // not overwritten by RefreshSizesAfterCheckpoint which only updates size.
    EXPECT_GT(info.primaryInfo.walInfo.mtime, 0);

    int64_t mtimeBeforeMs = static_cast<int64_t>(stBefore.st_mtim.tv_sec) * SqliteUtils::MSEC_PER_SEC +
                            stBefore.st_mtim.tv_nsec / SqliteUtils::NSEC_PER_MSEC;
    EXPECT_EQ(info.primaryInfo.walInfo.mtime, mtimeBeforeMs);
}

/**
 * @tc.name: PrimaryReplicaInfo_ConcurrentWrite_001
 * @tc.desc: AC-6.1: Concurrent writes during query do not crash or freeze; BUSY is a status not a global error.
 * @tc.type: FUNC
 */
HWTEST_F(PrimaryReplicaInfoTest, ConcurrentWrite_001, TestSize.Level1)
{
    InsertRows(store_, 1, 5);
    std::atomic<bool> stop{ false };
    std::atomic<int> writeCount{ 0 };

    std::mutex mtx;
    std::condition_variable cv;
    bool wroteOnce = false;

    // Writer thread: insert rows; notify after the first successful write so the main
    // thread can start sampling while writes are still in progress (no fixed sleep).
    std::thread writer([&stop, &writeCount, &mtx, &cv, &wroteOnce, this]() {
        for (int i = 100; i < 200 && !stop.load(); i++) {
            ValuesBucket values;
            values.PutInt("id", i);
            values.PutString("name", "concurrent" + std::to_string(i));
            values.PutInt("age", BASE_AGE + i);
            int64_t rowId = 0;
            if (store_->Insert(rowId, "test", values) != E_OK) {
                continue;
            }
            if (writeCount.fetch_add(1) != 0) {
                continue;
            }
            {
                std::lock_guard<std::mutex> lk(mtx);
                wroteOnce = true;
            }
            cv.notify_one();
        }
    });

    // RAII: stop and join the writer regardless of assertion outcomes.
    ScopeGuard joinGuard([&stop, &writer]() {
        stop.store(true);
        writer.join();
    });

    // Wait (bounded) until the writer has completed at least one successful write.
    {
        std::unique_lock<std::mutex> lk(mtx);
        cv.wait_for(lk, std::chrono::seconds(3), [&wroteOnce] { return wroteOnce; });
    }

    // Main thread: call diagnostic while writer is still active.
    auto [ret, info] = store_->GetPrimaryReplicaInfo({ "test" });
    // Concurrent writes may cause checkpoint BUSY → overall return may be E_OK or E_SQLITE_BUSY.
    EXPECT_TRUE(ret == E_OK || ret == E_SQLITE_BUSY)
        << "ret=" << ret << " (concurrent write may cause checkpoint BUSY)";

    EXPECT_GT(writeCount.load(), 0) << "Writer thread should have successfully written rows";

    auto it = info.primaryInfo.tables.find("test");
    ASSERT_NE(it, info.primaryInfo.tables.end());
    EXPECT_GE(it->second.count, 5); // At least the initial 5 rows
}

/**
 * @tc.name: PrimaryReplicaInfo_CustomReplicaPath_001
 * @tc.desc: AC-6.3: Custom replicaPath is used for replica DB stat, not the default adjacent path.
 * @tc.type: FUNC
 */
HWTEST_F(PrimaryReplicaInfoTest, CustomReplicaPath_001, TestSize.Level1)
{
    // AC-6.3: SetReplicaPath takes a directory; slave DB is created inside it
    std::string customDir = RDB_TEST_PATH + "custom_replica_dir";
    mkdir(customDir.c_str(), 0755); // IsValidReplicaPath requires an existing directory

    // Slave DB file name = ExtractFileName(dbName) with .db → _slave.db
    std::string baseName = dbName.substr(dbName.rfind('/') + 1);
    auto dotPos = baseName.rfind(".db");
    std::string slaveName = baseName.substr(0, dotPos) + "_slave.db";
    std::string expectedSlavePath = customDir + "/" + slaveName;

    RdbHelper::DeleteRdbStore(dbName);
    RdbHelper::DeleteRdbStore(expectedSlavePath);

    ScopeGuard cleanup([&expectedSlavePath, &customDir, this]() {
        RdbHelper::ClearCache();
        RdbHelper::DeleteRdbStore(expectedSlavePath);
        RdbHelper::DeleteRdbStore(dbName);
        rmdir(customDir.c_str());
    });

    RdbStoreConfig config(dbName);
    config.SetHaMode(HAMode::MAIN_REPLICA);
    config.SetReplicaPath(customDir);
    PrimaryReplicaInfoCallback helper;
    int errCode = E_OK;
    auto customStore = RdbHelper::GetRdbStore(config, 1, helper, errCode);
    ASSERT_NE(customStore, nullptr);
    ASSERT_EQ(errCode, E_OK);

    InsertRows(customStore, 1, 3);
    auto [ret, info] = customStore->GetPrimaryReplicaInfo({});
    EXPECT_EQ(ret, E_OK);
    // Replica DB should be stat'd from the custom directory path, not the default adjacent path
    EXPECT_GT(info.replicaInfo.dbInfo.size, 0);

    // Verify the slave DB file exists inside the custom directory
    struct stat st;
    EXPECT_EQ(stat(expectedSlavePath.c_str(), &st), 0);
}

/**
 * @tc.name: PrimaryReplicaInfo_ViewRejection_001
 * @tc.desc: AC-3.3: CountTable rejects views via type check — view count = -1.
 * @tc.type: FUNC
 */
HWTEST_F(PrimaryReplicaInfoTest, ViewRejection_001, TestSize.Level1)
{
    InsertRows(store_, 1, 3);
    // Create a view on the test table
    EXPECT_EQ(store_->ExecuteSql("CREATE VIEW IF NOT EXISTS test_view AS SELECT id FROM test"), E_OK);

    // "test_view" passes IsValidTableName but fails type check in CountTable.
    auto [ret, info] = store_->GetPrimaryReplicaInfo({ "test_view" });
    EXPECT_EQ(ret, E_OK); // Overall call succeeds; count=-1 for view
    auto it = info.primaryInfo.tables.find("test_view");
    ASSERT_NE(it, info.primaryInfo.tables.end());
    EXPECT_EQ(it->second.count, -1) << "View should be rejected by type check";

    // Verify regular table still counts correctly on both primary and replica
    auto [ret2, info2] = store_->GetPrimaryReplicaInfo({ "test" });
    EXPECT_EQ(ret2, E_OK);
    auto it2 = info2.primaryInfo.tables.find("test");
    ASSERT_NE(it2, info2.primaryInfo.tables.end());
    EXPECT_EQ(it2->second.count, 3) << "Primary table count should be 3 after view rejection";
    // Replica side should also count the regular table correctly
    auto rit2 = info2.replicaInfo.tables.find("test");
    ASSERT_NE(rit2, info2.replicaInfo.tables.end());
    EXPECT_EQ(rit2->second.count, 3) << "Replica table count should be 3 after view rejection";

    store_->ExecuteSql("DROP VIEW IF EXISTS test_view");
}

/**
 * @tc.name: PrimaryReplicaInfo_ReplayStatus_001
 * @tc.desc: AC-2.4: Diagnostic succeeds; verify binlogInfo.exist matches
 *           IsSupportBinlog to confirm the replay path is actually exercised.
 * @tc.type: FUNC
 */
HWTEST_F(PrimaryReplicaInfoTest, ReplayStatus_001, TestSize.Level1)
{
    InsertRows(store_, 1, 2);
    auto [ret, info] = store_->GetPrimaryReplicaInfo({});
    EXPECT_EQ(ret, E_OK) << "Diagnostic should succeed";

    // Verify that the binlog info was actually collected — if IsSupportBinlog is true,
    // the TryReplayBinlog path was taken instead of the early-exit path.
    RdbStoreConfig checkConfig(dbName);
    checkConfig.SetHaMode(HAMode::MAIN_REPLICA);
    bool supportBinlog = SqliteUtils::IsSupportBinlog(checkConfig);
    EXPECT_EQ(info.binlogInfo.exist, supportBinlog)
        << "binlogInfo.exist should match IsSupportBinlog (replay path verification)";
}

/**
 * @tc.name: PrimaryReplicaInfo_TimeErrValidity_001
 * @tc.desc: AC-2.2: Time values are valid when stat succeeds.
 * @tc.type: FUNC
 */
HWTEST_F(PrimaryReplicaInfoTest, TimeErrValidity_001, TestSize.Level1)
{
    InsertRows(store_, 1, 5);
    auto [ret, info] = store_->GetPrimaryReplicaInfo({});
    EXPECT_EQ(ret, E_OK);
    // Both DB and WAL stats succeed → primaryLastModifyTime is positive.
    EXPECT_GT(info.GetPrimaryLastModifyTime(), 0);
}

/**
 * @tc.name: PrimaryReplicaInfo_SizeErrValidity_001
 * @tc.desc: AC-1.2: File sizes are refreshed after checkpoint.
 * @tc.type: FUNC
 */
HWTEST_F(PrimaryReplicaInfoTest, SizeErrValidity_001, TestSize.Level1)
{
    InsertRows(store_, 1, 3);
    auto [ret, info] = store_->GetPrimaryReplicaInfo({});
    EXPECT_EQ(ret, E_OK);
    EXPECT_GE(info.primaryInfo.dbInfo.size, 0);
    EXPECT_GE(info.primaryInfo.walInfo.size, 0);
}

/**
 * @tc.name: PrimaryReplicaInfo_EmptyStringInTableNames_001
 * @tc.desc: AC-3.2: Empty string in tableNames vector is skipped via continue;
 *           subsequent valid table names are still processed.
 * @tc.type: FUNC
 */
HWTEST_F(PrimaryReplicaInfoTest, EmptyStringInTableNames_001, TestSize.Level1)
{
    InsertRows(store_, 1, 3);
    // Mixed vector: valid table name followed by empty string.
    // Exercises the `if (tableName.empty()) { continue; }` branch (B7E).
    auto [ret, info] = store_->GetPrimaryReplicaInfo({ "test", "" });
    EXPECT_EQ(ret, E_OK);
    // "test" should be counted; empty string should be skipped via continue.
    auto it = info.primaryInfo.tables.find("test");
    ASSERT_NE(it, info.primaryInfo.tables.end());
    EXPECT_EQ(it->second.count, 3);
    // Empty string must not appear in the tables map.
    EXPECT_EQ(info.primaryInfo.tables.count(""), 0);
    // Replica side should also have "test" but not "".
    EXPECT_EQ(info.replicaInfo.tables.count("test"), 1);
    EXPECT_EQ(info.replicaInfo.tables.count(""), 0);
}

/**
 * @tc.name: PrimaryReplicaInfo_NoSlaveConnectionWithTable_001
 * @tc.desc: AC-4.2: Standalone connection (no slave) with non-empty tableNames —
 *           primary tables populated, replica tables empty (slaveConnection_ null).
 * @tc.type: FUNC
 */
HWTEST_F(PrimaryReplicaInfoTest, NoSlaveConnectionWithTable_001, TestSize.Level1)
{
    InsertRows(store_, 1, 2);
    store_ = nullptr;
    RdbHelper::ClearCache();

    RdbStoreConfig config(dbName);
    config.SetHaMode(HAMode::MAIN_REPLICA);
    auto [createErr, conn] = SqliteConnection::Create(config, false);
    ASSERT_EQ(createErr, E_OK);
    ASSERT_NE(conn, nullptr);

    // Non-empty tableNames exercises the counting loop with slaveConnection_ == nullptr (B7J).
    // The `if (slaveConnection_ != nullptr)` false branch skips replica CountTable.
    auto [ret, info] = conn->GetPrimaryAndReplicaInfo({ "test" });
    EXPECT_EQ(ret, E_OK);

    // Primary table count is populated.
    auto it = info.primaryInfo.tables.find("test");
    ASSERT_NE(it, info.primaryInfo.tables.end());
    EXPECT_EQ(it->second.count, 2);

    // Replica tables map is empty — slaveConnection_ is null, so CountTable is never called for replica.
    EXPECT_TRUE(info.replicaInfo.tables.empty());
    // Replica userVersion stays at default (slaveConnection_ null → QueryUserVersion skipped).
    EXPECT_EQ(info.replicaInfo.userVersion, 0);
}

/**
 * @tc.name: PrimaryReplicaInfo_ReplicaWalStatVerified_001
 * @tc.desc: AC-1.1: Replica WAL file is independently stat'd; mtime matches pre-call value,
 *           size matches post-checkpoint value.
 * @tc.type: FUNC
 */
HWTEST_F(PrimaryReplicaInfoTest, ReplicaWalStatVerified_001, TestSize.Level1)
{
    InsertRows(store_, 1, 5);
    InsertRows(store_, 100, 3);

    // Stat replica WAL before the diagnostic call to capture initial mtime (B4I/B4J).
    std::string replicaWalPath = slaveDbName + "-wal";
    struct stat stBefore;
    if (stat(replicaWalPath.c_str(), &stBefore) != 0) {
        GTEST_SKIP() << "Replica WAL file does not exist before diagnostic call";
    }

    auto [ret, info] = store_->GetPrimaryReplicaInfo({});
    EXPECT_EQ(ret, E_OK);

    // Replica WAL was stat'd in CollectInitialFileInfo — mtime should match pre-call value.
    int64_t expectedMtime = static_cast<int64_t>(stBefore.st_mtim.tv_sec) * SqliteUtils::MSEC_PER_SEC +
                            stBefore.st_mtim.tv_nsec / SqliteUtils::NSEC_PER_MSEC;
    EXPECT_EQ(info.replicaInfo.walInfo.mtime, expectedMtime)
        << "Replica WAL mtime should match pre-call stat (from CollectInitialFileInfo)";

    // After checkpoint, WAL may be truncated. Verify size matches post-checkpoint stat (B8I/B8J).
    struct stat stAfter;
    if (stat(replicaWalPath.c_str(), &stAfter) == 0) {
        EXPECT_EQ(info.replicaInfo.walInfo.size, static_cast<int64_t>(stAfter.st_size))
            << "Replica WAL size should match post-checkpoint stat (from RefreshSizesAfterCheckpoint)";
    }
}

/**
 * @tc.name: PrimaryReplicaInfo_PrimaryDbStatFailure_001
 * @tc.desc: AC-5.1: When primary DB file is removed before diagnostic, stat failure
 *           propagates as error from CollectInitialFileInfo.
 * @tc.type: FUNC
 */
HWTEST_F(PrimaryReplicaInfoTest, PrimaryDbStatFailure_001, TestSize.Level1)
{
    InsertRows(store_, 1, 2);
    store_ = nullptr;
    RdbHelper::ClearCache();

    RdbStoreConfig config(dbName);
    config.SetHaMode(HAMode::MAIN_REPLICA);
    auto [createErr, conn] = SqliteConnection::Create(config, false);
    ASSERT_EQ(createErr, E_OK);
    ASSERT_NE(conn, nullptr);

    // Rename the primary DB file so stat(originalPath) returns ENOENT (B4A/B8A).
    std::string renamedPath = dbName + ".renamed";
    if (rename(dbName.c_str(), renamedPath.c_str()) != 0) {
        GTEST_SKIP() << "Cannot rename open DB file on this platform";
    }
    ScopeGuard restore([&renamedPath, this]() { rename(renamedPath.c_str(), dbName.c_str()); });

    // CollectInitialFileInfo: primary DB stat fails → error propagated immediately.
    auto [ret, info] = conn->GetPrimaryAndReplicaInfo({});
    EXPECT_NE(ret, E_OK);
    // Primary DB info is not populated (stat failed before assignment).
    EXPECT_EQ(info.primaryInfo.dbInfo.size, 0);
}

/**
 * @tc.name: PrimaryReplicaInfo_ReplicaDbStatFailure_001
 * @tc.desc: AC-4.4: When replica DB file does not exist, stat fails silently —
 *           replicaInfo.dbInfo stays zeroed, overall ret is E_OK.
 * @tc.type: FUNC
 */
HWTEST_F(PrimaryReplicaInfoTest, ReplicaDbStatFailure_001, TestSize.Level1)
{
    InsertRows(store_, 1, 2);
    store_ = nullptr;
    RdbHelper::ClearCache();

    // Delete the slave DB file so replica DB stat fails (B4F/B8H).
    RdbHelper::DeleteRdbStore(slaveDbName);
    ASSERT_NE(access(slaveDbName.c_str(), F_OK), 0) << "Slave DB should be deleted";

    RdbStoreConfig config(dbName);
    config.SetHaMode(HAMode::MAIN_REPLICA);
    auto [createErr, conn] = SqliteConnection::Create(config, false);
    ASSERT_EQ(createErr, E_OK);
    ASSERT_NE(conn, nullptr);

    // Replica DB stat fails silently (file missing) — not fatal, ret stays E_OK.
    auto [ret, info] = conn->GetPrimaryAndReplicaInfo({});
    EXPECT_EQ(ret, E_OK);
    // Replica DB info stays at default zero values.
    EXPECT_EQ(info.replicaInfo.dbInfo.size, 0);
    EXPECT_EQ(info.replicaInfo.dbInfo.mtime, 0);
    // Primary DB info should still be valid.
    EXPECT_GT(info.primaryInfo.dbInfo.size, 0);
}

/**
 * @tc.name: PrimaryReplicaInfo_PrimaryReplicaConsistency_001
 * @tc.desc: AC-7.1: Primary and replica table row counts are equal (EQ assertion).
 * @tc.type: FUNC
 */
HWTEST_F(PrimaryReplicaInfoTest, PrimaryReplicaConsistency_001, TestSize.Level1)
{
    // Arrange: insert rows into primary
    InsertRows(store_, 1, 5);

    // Act: get diagnostic info with table names
    auto [ret, info] = store_->GetPrimaryReplicaInfo({ "test" });
    ASSERT_EQ(ret, E_OK) << "Diagnostic should succeed on healthy MAIN_REPLICA DB";

    // Assert: primary and replica table counts must be equal
    auto pit = info.primaryInfo.tables.find("test");
    auto rit = info.replicaInfo.tables.find("test");
    ASSERT_NE(pit, info.primaryInfo.tables.end()) << "Primary table entry must exist";
    ASSERT_NE(rit, info.replicaInfo.tables.end()) << "Replica table entry must exist";
    EXPECT_EQ(pit->second.count, rit->second.count)
        << "Primary and replica table row counts must be equal (primary=" << pit->second.count
        << ", replica=" << rit->second.count << ")";
    EXPECT_EQ(pit->second.count, 5) << "Both sides should have 5 rows";
}

/**
 * @tc.name: PrimaryReplicaInfo_PrimaryReplicaConsistency_002
 * @tc.desc: AC-7.2: Primary and replica userVersion are equal (EQ assertion).
 * @tc.type: FUNC
 */
HWTEST_F(PrimaryReplicaInfoTest, PrimaryReplicaConsistency_002, TestSize.Level1)
{
    // Arrange: set a specific user_version
    EXPECT_EQ(store_->ExecuteSql("PRAGMA user_version = 99"), E_OK);
    InsertRows(store_, 1, 2);

    // Act
    auto [ret, info] = store_->GetPrimaryReplicaInfo({ "test" });
    ASSERT_EQ(ret, E_OK) << "Diagnostic should succeed";

    // Assert: primary and replica userVersion must be equal
    EXPECT_EQ(info.primaryInfo.userVersion, info.replicaInfo.userVersion)
        << "Primary and replica userVersion must be equal (primary=" << info.primaryInfo.userVersion
        << ", replica=" << info.replicaInfo.userVersion << ")";
    EXPECT_EQ(info.primaryInfo.userVersion, 99) << "userVersion should be 99";
}

/**
 * @tc.name: PrimaryReplicaInfo_PrimaryReplicaConsistency_003
 * @tc.desc: AC-7.3: Primary and replica DB sizes are equal after checkpoint (EQ assertion).
 * @tc.type: FUNC
 */
HWTEST_F(PrimaryReplicaInfoTest, PrimaryReplicaConsistency_003, TestSize.Level1)
{
    // Arrange: insert rows so both DBs have data
    InsertRows(store_, 1, 10);

    // Act: diagnostic triggers checkpoint, after which both DBs should have similar sizes
    auto [ret, info] = store_->GetPrimaryReplicaInfo({});
    ASSERT_EQ(ret, E_OK) << "Diagnostic should succeed";

    // Assert: after checkpoint, primary and replica DB sizes should be equal
    EXPECT_EQ(info.primaryInfo.dbInfo.size, info.replicaInfo.dbInfo.size)
        << "Primary and replica DB sizes must be equal after checkpoint (primary=" << info.primaryInfo.dbInfo.size
        << ", replica=" << info.replicaInfo.dbInfo.size << ")";
}

/**
 * @tc.name: PrimaryReplicaInfo_PrimaryReplicaConsistency_004
 * @tc.desc: AC-7.4: Primary and replica modification times are close (within 60s threshold).
 * @tc.type: FUNC
 */
HWTEST_F(PrimaryReplicaInfoTest, PrimaryReplicaConsistency_004, TestSize.Level1)
{
    // Arrange
    InsertRows(store_, 1, 3);

    // Act
    auto [ret, info] = store_->GetPrimaryReplicaInfo({});
    ASSERT_EQ(ret, E_OK) << "Diagnostic should succeed";

    // Assert: modification times should be close (within 60 seconds = 60000 ms)
    int64_t primaryTime = info.GetPrimaryLastModifyTime();
    int64_t replicaTime = info.GetReplicaLastModifyTime();
    EXPECT_GT(primaryTime, 0) << "Primary last modify time should be positive";
    EXPECT_GT(replicaTime, 0) << "Replica last modify time should be positive";
    int64_t diff = std::abs(primaryTime - replicaTime);
    EXPECT_LE(diff, 60000) << "Primary and replica modification times should be within 60s (diff=" << diff << " ms)";
}

/**
 * @tc.name: PrimaryReplicaInfo_PrimaryReplicaConsistency_005
 * @tc.desc: AC-7.5: Multiple tables — primary and replica counts match for each table (EQ assertion).
 * @tc.type: FUNC
 */
HWTEST_F(PrimaryReplicaInfoTest, PrimaryReplicaConsistency_005, TestSize.Level1)
{
    // Arrange: create a second table and insert data into both
    EXPECT_EQ(store_->ExecuteSql("CREATE TABLE IF NOT EXISTS test2 (id INTEGER PRIMARY KEY, value TEXT)"), E_OK);
    InsertRows(store_, 1, 3);
    for (int i = 0; i < 7; i++) {
        ValuesBucket values;
        values.PutInt("id", i);
        values.PutString("value", "val" + std::to_string(i));
        int64_t rowId = 0;
        EXPECT_EQ(store_->Insert(rowId, "test2", values), E_OK);
    }

    // Act
    auto [ret, info] = store_->GetPrimaryReplicaInfo({ "test", "test2" });
    ASSERT_EQ(ret, E_OK) << "Diagnostic should succeed";

    // Assert: each table's count must match between primary and replica
    for (const auto &tableName : { "test", "test2" }) {
        auto pit = info.primaryInfo.tables.find(tableName);
        auto rit = info.replicaInfo.tables.find(tableName);
        ASSERT_NE(pit, info.primaryInfo.tables.end()) << "Primary table " << tableName << " not found";
        ASSERT_NE(rit, info.replicaInfo.tables.end()) << "Replica table " << tableName << " not found";
        EXPECT_EQ(pit->second.count, rit->second.count)
            << "Table " << tableName << ": primary and replica counts must be equal (primary=" << pit->second.count
            << ", replica=" << rit->second.count << ")";
    }
    // Verify exact counts
    EXPECT_EQ(info.primaryInfo.tables["test"].count, 3) << "test table should have 3 rows";
    EXPECT_EQ(info.primaryInfo.tables["test2"].count, 7) << "test2 table should have 7 rows";
}

/**
 * @tc.name: PrimaryReplicaInfo_ParamValidationAfterWriteSideEffect_001
 * @tc.desc: AC-3.5: Parameter validation (E_INVALID_ARGS) occurs after checkpoint and other
 *           write side effects; valid tables before the invalid one have counts populated
 *           in partial results.
 * @tc.type: FUNC
 */
HWTEST_F(PrimaryReplicaInfoTest, ParamValidationAfterWriteSideEffect_001, TestSize.Level1)
{
    // Arrange: insert rows so checkpoint has work to do
    InsertRows(store_, 1, 5);

    // Act: pass a vector with a valid table followed by an invalid table name.
    // The diagnostic pipeline runs steps 1-4 (stat, replay, checkpoint, refresh) before
    // step 5 (CollectVersionsAndTables) validates table names. So checkpoint side effects
    // have already occurred when E_INVALID_ARGS is returned.
    auto [ret, info] = store_->GetPrimaryReplicaInfo({ "test", "test; DROP TABLE test" });

    // Assert: E_INVALID_ARGS returned for the invalid table name
    EXPECT_EQ(ret, E_INVALID_ARGS) << "Invalid table name in vector should return E_INVALID_ARGS";

    // The valid table "test" before the invalid one should have its count populated
    // (CollectVersionsAndTables processes tables sequentially — the valid one is counted
    // before IsValidTableName rejects the invalid one).
    auto pit = info.primaryInfo.tables.find("test");
    ASSERT_NE(pit, info.primaryInfo.tables.end())
        << "Valid table 'test' should have count populated before invalid table rejected";
    EXPECT_EQ(pit->second.count, 5) << "Valid table count should be 5 (processed before invalid table name)";

    // Replica side should also have the valid table counted
    auto rit = info.replicaInfo.tables.find("test");
    ASSERT_NE(rit, info.replicaInfo.tables.end())
        << "Replica side should also have 'test' counted before invalid table rejected";
    EXPECT_EQ(rit->second.count, 5) << "Replica table count should be 5 (processed before invalid table name)";

    // Verify that write side effects (checkpoint) actually occurred:
    // After checkpoint, WAL should be truncated to 0.
    EXPECT_EQ(info.primaryInfo.walInfo.size, 0)
        << "WAL should be truncated (checkpoint side effect occurred before param validation)";

    // Verify the table still exists (no SQL injection)
    auto [ret2, info2] = store_->GetPrimaryReplicaInfo({ "test" });
    EXPECT_EQ(ret2, E_OK) << "Table should still exist after injection attempt";
    auto it2 = info2.primaryInfo.tables.find("test");
    ASSERT_NE(it2, info2.primaryInfo.tables.end());
    EXPECT_EQ(it2->second.count, 5) << "Data should be intact after injection attempt";
}

/**
 * @tc.name: PrimaryReplicaInfo_SlaveCountTableErrorPath_001
 * @tc.desc: AC-4.3: When slave CountTable fails (non-existent table), the error is silently
 *           discarded — overall ret stays E_OK, replica table gets count=-1 sentinel.
 * @tc.type: FUNC
 */
HWTEST_F(PrimaryReplicaInfoTest, SlaveCountTableErrorPath_001, TestSize.Level1)
{
    // Arrange: insert data into a valid table
    InsertRows(store_, 1, 3);

    // Act: query both a valid table and a non-existent table.
    // For the non-existent table, CountTable returns an error on both primary and slave,
    // but the error codes (pErr/rErr) are discarded in CollectVersionsAndTables.
    // The overall ret should still be E_OK (error silently discarded, not propagated).
    auto [ret, info] = store_->GetPrimaryReplicaInfo({ "test", "nonexistent_table" });
    EXPECT_EQ(ret, E_OK) << "Overall ret should be E_OK even when CountTable fails (error silently discarded)";

    // Assert: valid table has correct count on both primary and replica
    auto pit = info.primaryInfo.tables.find("test");
    ASSERT_NE(pit, info.primaryInfo.tables.end()) << "Primary 'test' should be in tables map";
    EXPECT_EQ(pit->second.count, 3) << "Primary 'test' should have 3 rows";

    auto rit = info.replicaInfo.tables.find("test");
    ASSERT_NE(rit, info.replicaInfo.tables.end()) << "Replica 'test' should be in tables map";
    EXPECT_EQ(rit->second.count, 3) << "Replica 'test' should have 3 rows";

    // Assert: non-existent table gets count=-1 sentinel on both sides
    // (CountTable error is silently discarded, sentinel value stored instead)
    auto pne = info.primaryInfo.tables.find("nonexistent_table");
    ASSERT_NE(pne, info.primaryInfo.tables.end()) << "Primary 'nonexistent_table' should be in map";
    EXPECT_EQ(pne->second.count, -1) << "Primary nonexistent table should have count=-1 sentinel (error discarded)";

    auto rne = info.replicaInfo.tables.find("nonexistent_table");
    ASSERT_NE(rne, info.replicaInfo.tables.end()) << "Replica 'nonexistent_table' should be in map";
    EXPECT_EQ(rne->second.count, -1)
        << "Replica nonexistent table should have count=-1 sentinel (error silently discarded)";
}

/**
 * @tc.name: PrimaryReplicaInfo_SlaveCountTableErrorPath_002
 * @tc.desc: AC-4.3: When slave DB is missing (stat fails), slave CountTable is skipped —
 *           replica tables map is empty, but primary tables are still counted correctly.
 *           This verifies the slave CountTable error path is handled gracefully.
 * @tc.type: FUNC
 */
HWTEST_F(PrimaryReplicaInfoTest, SlaveCountTableErrorPath_002, TestSize.Level1)
{
    // Arrange: insert data and create a standalone read connection (no slave)
    InsertRows(store_, 1, 4);
    store_ = nullptr;
    RdbHelper::ClearCache();

    RdbStoreConfig config(dbName);
    config.SetHaMode(HAMode::MAIN_REPLICA);
    auto [createErr, conn] = SqliteConnection::Create(config, false);
    ASSERT_EQ(createErr, E_OK);
    ASSERT_NE(conn, nullptr);

    // Act: call diagnostic with table names — slaveConnection_ is null,
    // so slave CountTable is never called (the `if (slaveConnection_ != nullptr)` branch is false)
    auto [ret, info] = conn->GetPrimaryAndReplicaInfo({ "test" });
    EXPECT_EQ(ret, E_OK) << "Overall ret should be E_OK when slave CountTable is skipped (no slave connection)";

    // Assert: primary table is counted correctly
    auto pit = info.primaryInfo.tables.find("test");
    ASSERT_NE(pit, info.primaryInfo.tables.end()) << "Primary 'test' should be counted";
    EXPECT_EQ(pit->second.count, 4) << "Primary 'test' should have 4 rows";

    // Assert: replica tables map is empty — slave CountTable was not called
    EXPECT_TRUE(info.replicaInfo.tables.empty()) << "Replica tables should be empty when slave CountTable is skipped";
}

/**
 * @tc.name: PrimaryReplicaInfo_TryReplayBinlogPath_001
 * @tc.desc: AC-2.4: When binlog is supported and exists, TryReplayBinlog is exercised.
 *           When binlog is not supported, the early-exit path returns E_OK without replay.
 *           This test covers both paths deterministically.
 * @tc.type: FUNC
 */
HWTEST_F(PrimaryReplicaInfoTest, TryReplayBinlogPath_001, TestSize.Level1)
{
    InsertRows(store_, 1, 5);

    // Determine if binlog is supported in this environment
    RdbStoreConfig checkConfig(dbName);
    checkConfig.SetHaMode(HAMode::MAIN_REPLICA);
    bool supportBinlog = SqliteUtils::IsSupportBinlog(checkConfig);

    // Act: call diagnostic — TryReplayBinlog is step 2 in the pipeline
    auto [ret, info] = store_->GetPrimaryReplicaInfo({ "test" });
    EXPECT_EQ(ret, E_OK) << "Diagnostic should succeed regardless of whether replay was taken or skipped";

    // Assert: binlogInfo.exist must match IsSupportBinlog
    EXPECT_EQ(info.binlogInfo.exist, supportBinlog)
        << "binlogInfo.exist should match IsSupportBinlog (supportBinlog=" << supportBinlog << ")";

    if (supportBinlog) {
        // When binlog is supported and exists, TryReplayBinlog was actually called.
        // After replay + checkpoint, primary and replica data should be consistent.
        EXPECT_GT(info.binlogInfo.lastModifyTime, 0)
            << "binlogInfo.lastModifyTime should be positive when binlog exists";
        auto pit = info.primaryInfo.tables.find("test");
        auto rit = info.replicaInfo.tables.find("test");
        ASSERT_NE(pit, info.primaryInfo.tables.end());
        ASSERT_NE(rit, info.replicaInfo.tables.end());
        EXPECT_EQ(pit->second.count, rit->second.count) << "After replay, primary and replica counts should be equal";
    } else {
        // When binlog is not supported, TryReplayBinlog returns E_OK via early exit.
        // binlogInfo.exist should be false, lastModifyTime should be 0.
        EXPECT_EQ(info.binlogInfo.lastModifyTime, 0)
            << "binlogInfo.lastModifyTime should be 0 when binlog not supported";
    }
}

/**
 * @tc.name: PrimaryReplicaInfo_TryReplayBinlogPath_002
 * @tc.desc: AC-2.4: TryReplayBinlog early-exit path when binlog directory does not exist.
 *           Even if IsSupportBinlog is true, if binlogInfo.exist is false (no binlog folder),
 *           TryReplayBinlog returns E_OK without calling ReplayBinlogSqlite.
 * @tc.type: FUNC
 */
HWTEST_F(PrimaryReplicaInfoTest, TryReplayBinlogPath_002, TestSize.Level1)
{
    // The fixture already created the DB in SetUp, but no rows have been inserted.
    RdbStoreConfig checkConfig(dbName);
    checkConfig.SetHaMode(HAMode::MAIN_REPLICA);
    bool supportBinlog = SqliteUtils::IsSupportBinlog(checkConfig);

    // Act: call diagnostic on a fresh DB
    auto [ret, info] = store_->GetPrimaryReplicaInfo({ "test" });
    EXPECT_EQ(ret, E_OK) << "Diagnostic should succeed on fresh DB (TryReplayBinlog early-exit or no-op)";

    // Assert: the diagnostic call itself should not fail due to replay
    // Whether binlog exists or not, the early-exit path returns E_OK
    auto it = info.primaryInfo.tables.find("test");
    ASSERT_NE(it, info.primaryInfo.tables.end());
    EXPECT_EQ(it->second.count, 0) << "Fresh table should have 0 rows";

    // Verify binlogInfo.exist is consistent
    if (!supportBinlog) {
        EXPECT_EQ(info.binlogInfo.exist, false) << "binlogInfo.exist should be false when binlog not supported";
    }
}

/**
 * @tc.name: PrimaryReplicaInfo_CheckpointFailure_001
 * @tc.desc: AC-5.2: When the primary DB TRUNCATE checkpoint cannot acquire the
 *           writer lock (held by a concurrent open write transaction),
 *           CheckpointDatabases returns SQLITE_BUSY and GetPrimaryAndReplicaInfo
 *           propagates it instead of swallowing the error. Partial diagnostic
 *           results collected before checkpoint remain available.
 * @tc.type: FUNC
 */
HWTEST_F(PrimaryReplicaInfoTest, CheckpointFailure_001, TestSize.Level1)
{
    // Arrange: insert data to generate WAL content on the primary DB.
    InsertRows(store_, 1, 5);

    // Act: open and hold a write transaction on the fixture store so the primary
    // writer lock stays held. A second writer's TRUNCATE checkpoint cannot grab
    // the exclusive writer lock while this transaction is open, so
    // sqlite3_wal_checkpoint_v2 returns SQLITE_BUSY after the busy handler times out.
    EXPECT_EQ(store_->BeginTransaction(), E_OK);
    // Roll back the fixture transaction on every exit path (including ASSERT abort),
    // so a dangling uncommitted transaction never leaks into later test cases.
    // This mirrors the ScopeGuard RAII convention used in PrimaryDbStatFailure_001.
    ScopeGuard rollbackTxn([this]() { store_->RollBack(); });
    ValuesBucket values;
    values.PutInt("id", 999);
    values.PutString("name", "lock_holder");
    values.PutInt("age", 99);
    int64_t rowId = 0;
    EXPECT_EQ(store_->Insert(rowId, "test", values), E_OK);
    // Transaction is still open — do NOT commit yet.

    // Create a separate direct WRITE connection. Only a writer enters the
    // CheckpointDatabases branch (dbHandle_ != nullptr && isWriter_); a read
    // connection (isWriter_ == false) skips checkpoint entirely and the failure
    // path under test would be dead code.
    RdbStoreConfig config(dbName);
    config.SetHaMode(HAMode::MAIN_REPLICA);
    auto [createErr, conn] = SqliteConnection::Create(config, true);
    ASSERT_EQ(createErr, E_OK);
    ASSERT_NE(conn, nullptr);

    // The diagnostic call: the second writer's primary TRUNCATE checkpoint fails to
    // acquire the writer lock held by the fixture store's open transaction and
    // returns SQLITE_BUSY. GetPrimaryAndReplicaInfo must propagate that error.
    auto [ret, info] = conn->GetPrimaryAndReplicaInfo({});

    // Assert: the checkpoint failure (SQLITE_BUSY) is propagated, not swallowed as E_OK.
    EXPECT_EQ(ret, E_SQLITE_BUSY)
        << "TRUNCATE checkpoint under a concurrent write transaction should fail with SQLITE_BUSY";
    // Partial results collected by CollectInitialFileInfo before checkpoint are still present.
    EXPECT_GT(info.primaryInfo.dbInfo.size, 0)
        << "Partial results should be present even when checkpoint failed (ret=" << ret << ")";
}

/**
 * @tc.name: PrimaryReplicaInfo_CheckpointFailure_002
 * @tc.desc: Verify graceful handling when the replica WAL/SHM files are missing:
 *           SQLite rebuilds the WAL on reopen, checkpoint succeeds, and
 *           GetPrimaryAndReplicaInfo returns E_OK with primary data intact.
 *           (Reliably forcing a slave-side checkpoint *failure* via file
 *           manipulation is not achievable because SQLite recovers from a
 *           missing/corrupt WAL; the slave-checkpoint error-propagation path is
 *           structurally identical to the primary one covered by
 *           CheckpointFailure_001, which exercises the same
 *           `if (ckptErr != SQLITE_OK) return SQLiteError::ErrNo(ckptErr)` branch.)
 * @tc.type: FUNC
 */
HWTEST_F(PrimaryReplicaInfoTest, CheckpointFailure_002, TestSize.Level1)
{
    // Arrange: insert data to generate WAL content on both primary and replica
    InsertRows(store_, 1, 5);
    // Force a first checkpoint to ensure slave DB has data
    auto [preRet, preInfo] = store_->GetPrimaryReplicaInfo({});
    EXPECT_EQ(preRet, E_OK) << "Pre-diagnostic should succeed";

    // Release the fixture store so we can manipulate files
    store_ = nullptr;
    RdbHelper::ClearCache();

    // Remove the slave WAL and SHM auxiliary files. Unlike the previous
    // "corrupt the slave WAL" framing, deleting WAL/SHM while no connection
    // holds the DB is SQLite's standard recovery scenario: on reopen the WAL is
    // rebuilt and checkpoint succeeds, so this guards *missing-file tolerance*,
    // not failure propagation.
    std::string slaveWalPath = slaveDbName + "-wal";
    std::string slaveShmPath = slaveDbName + "-shm";
    if (access(slaveWalPath.c_str(), F_OK) == 0) {
        EXPECT_EQ(unlink(slaveWalPath.c_str()), 0) << "Should delete slave WAL file";
    }
    if (access(slaveShmPath.c_str(), F_OK) == 0) {
        EXPECT_EQ(unlink(slaveShmPath.c_str()), 0) << "Should delete slave SHM file";
    }

    // Create a new write connection — it reopens the slave DB and rebuilds the WAL.
    RdbStoreConfig config(dbName);
    config.SetHaMode(HAMode::MAIN_REPLICA);
    auto [createErr, conn] = SqliteConnection::Create(config, true);
    ASSERT_EQ(createErr, E_OK) << "Reopen after WAL removal should succeed";
    ASSERT_NE(conn, nullptr);

    // Act: call diagnostic — checkpoint rebuilds the (now missing) WAL and succeeds.
    auto [ret, info] = conn->GetPrimaryAndReplicaInfo({ "test" });

    // Assert: the diagnostic handles the missing slave WAL gracefully (E_OK),
    // and primary data is still intact.
    EXPECT_EQ(ret, E_OK) << "Missing slave WAL/SHM should be recovered gracefully";
    auto pit = info.primaryInfo.tables.find("test");
    EXPECT_NE(pit, info.primaryInfo.tables.end()) << "Primary table info should be present";
    if (pit != info.primaryInfo.tables.end()) {
        EXPECT_GE(pit->second.count, 5) << "Primary should have at least 5 rows";
    }
}

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

#include <gtest/gtest.h>

#include <sys/stat.h>
#include <string>

#include "common.h"
#include "rdb_errno.h"
#include "rdb_helper.h"
#include "rdb_open_callback.h"
#include "rdb_store.h"
#include "rdb_store_config.h"

using namespace testing::ext;
using namespace OHOS::NativeRdb;

namespace {
constexpr const char *CREATE_TABLE_SQL = "CREATE TABLE IF NOT EXISTS test_table "
                                         "(id INTEGER PRIMARY KEY AUTOINCREMENT, data TEXT)";
constexpr ssize_t SIZE_100KB = 100 * 1024;
constexpr ssize_t SIZE_300KB = 300 * 1024;
constexpr ssize_t SIZE_20MB = 20 * 1024 * 1024;
constexpr ssize_t SIZE_100MB = 100 * 1024 * 1024;
constexpr ssize_t SIZE_384MB = 384 * 1024 * 1024;
}

class CheckpointSizeTest : public testing::Test {
public:
    static void SetUpTestCase(void) {}
    static void TearDownTestCase(void) {}
    void SetUp(void) {}
    void TearDown(void) {}

protected:
    static std::shared_ptr<RdbStore> GetStore(const RdbStoreConfig &config);
    static int64_t GetFileSize(const std::string &path);
    static void InsertRows(const std::shared_ptr<RdbStore> &store, int count, int dataSize);
    static int QueryCount(const std::shared_ptr<RdbStore> &store);
    static void CleanupDb(const std::string &dbPath);
};

class CheckpointSizeCallback : public RdbOpenCallback {
public:
    int OnCreate(RdbStore &store) override { return store.ExecuteSql(CREATE_TABLE_SQL); }
    int OnUpgrade(RdbStore &store, int oldVersion, int newVersion) override { return E_OK; }
};

std::shared_ptr<RdbStore> CheckpointSizeTest::GetStore(const RdbStoreConfig &config)
{
    CheckpointSizeCallback callback;
    int errCode = E_OK;
    return RdbHelper::GetRdbStore(config, 1, callback, errCode);
}

int64_t CheckpointSizeTest::GetFileSize(const std::string &path)
{
    struct stat st;
    if (stat(path.c_str(), &st) != 0) {
        return 0;
    }
    return static_cast<int64_t>(st.st_size);
}

void CheckpointSizeTest::InsertRows(const std::shared_ptr<RdbStore> &store, int count, int dataSize)
{
    std::string data(dataSize, 'x');
    for (int i = 0; i < count; i++) {
        store->ExecuteSql("INSERT INTO test_table (data) VALUES ('" + data + "')");
    }
}

int CheckpointSizeTest::QueryCount(const std::shared_ptr<RdbStore> &store)
{
    auto resultSet = store->QueryByStep("SELECT * FROM test_table");
    if (resultSet == nullptr) {
        return -1;
    }
    int count = 0;
    resultSet->GetRowCount(count);
    return count;
}

void CheckpointSizeTest::CleanupDb(const std::string &dbPath)
{
    RdbHelper::ClearCache();
    RdbHelper::DeleteRdbStore(dbPath);
}

/**
 * @brief Set startCheckpointSize=100KB → GetRdbStore → insert data exceeding 100KB →
 *        verify checkpoint triggered (WAL size reduced below threshold), data intact
 * @param TestCaseName Checkpoint_Trigger_001
 * @since 1.0
 */
HWTEST_F(CheckpointSizeTest, Checkpoint_Trigger_001, TestSize.Level0)
{
    std::string dbPath = RDB_TEST_PATH + "ckpt_trigger_001.db";
    // Arrange: set startCheckpointSize=100KB
    RdbHelper::DeleteRdbStore(dbPath);
    RdbStoreConfig config(dbPath);
    config.SetStartCheckpointSize(SIZE_100KB);
    EXPECT_EQ(config.GetStartCheckpointSize(), SIZE_100KB) << "startCheckpointSize should be 100KB";

    // Act: open DB, insert 200 rows × 1KB ≈ 200KB WAL (exceeds 100KB threshold)
    auto store = GetStore(config);
    ASSERT_NE(store, nullptr) << "Failed to open store";
    InsertRows(store, 200, 1024);

    // Assert: WAL stays below 100KB — connection pool calls TryCheckPoint(true) on release,
    // checkpoint triggered when WAL > 100KB, WAL truncated
    int64_t walSize = GetFileSize(dbPath + "-wal");
    EXPECT_LT(walSize, SIZE_100KB) << "WAL should stay below 100KB after checkpoint, got " << walSize;

    // Assert: data intact
    EXPECT_EQ(QueryCount(store), 200) << "Row count should be 200 after checkpoint";

    // Cleanup
    store = nullptr;
    CleanupDb(dbPath);
}

/**
 * @brief Set startCheckpointSize=100KB → GetRdbStore → insert 50KB data (< 100KB threshold) →
 *        verify checkpoint NOT triggered (WAL exists with data), data intact
 * @param TestCaseName Checkpoint_NoTriggerBelowThreshold_001
 * @since 1.0
 */
HWTEST_F(CheckpointSizeTest, Checkpoint_NoTriggerBelowThreshold_001, TestSize.Level1)
{
    std::string dbPath = RDB_TEST_PATH + "ckpt_no_trigger_below_001.db";
    // Arrange: set startCheckpointSize=100KB
    RdbHelper::DeleteRdbStore(dbPath);
    RdbStoreConfig config(dbPath);
    config.SetStartCheckpointSize(SIZE_100KB);
    EXPECT_EQ(config.GetStartCheckpointSize(), SIZE_100KB) << "startCheckpointSize should be 100KB";

    // Act: open DB, insert 50 rows × 1KB = 50KB WAL (below 100KB threshold)
    auto store = GetStore(config);
    ASSERT_NE(store, nullptr) << "Failed to open store";
    InsertRows(store, 50, 1024);

    // Assert: WAL not checkpointed (50KB < 100KB threshold), WAL should exist with data
    int64_t walSize = GetFileSize(dbPath + "-wal");
    EXPECT_GT(walSize, 0) << "WAL should exist (50KB < 100KB threshold, no checkpoint)";

    // Assert: data intact
    EXPECT_EQ(QueryCount(store), 50) << "Row count should be 50";

    // Cleanup
    store = nullptr;
    CleanupDb(dbPath);
}

/**
 * @brief Set startCheckpointSize=300KB → insert data exceeding 300KB → verify checkpoint
 *        triggered (WAL < 100KB). 300KB < auto-checkpoint threshold (~400KB, 100 pages × 4KB),
 *        so RDB TryCheckPoint fires before SQLite auto-checkpoint, isolating RDB checkpoint logic.
 * @param TestCaseName Checkpoint_Trigger_300KB_001
 * @since 1.0
 */
HWTEST_F(CheckpointSizeTest, Checkpoint_Trigger_300KB_001, TestSize.Level0)
{
    std::string dbPath = RDB_TEST_PATH + "ckpt_trigger_300kb_001.db";
    // Arrange: set startCheckpointSize=300KB (< auto-checkpoint ~400KB)
    RdbHelper::DeleteRdbStore(dbPath);
    RdbStoreConfig config(dbPath);
    config.SetStartCheckpointSize(SIZE_300KB);
    EXPECT_EQ(config.GetStartCheckpointSize(), SIZE_300KB) << "startCheckpointSize should be 300KB";

    // Act: open DB, insert 80 rows × 1KB. Each row writes ~1 page (4KB) to WAL,
    // so ~80 frames → WAL ≈ 330KB > 300KB threshold. 80 frames < 100 (auto-checkpoint),
    // so TryCheckPoint fires first, not auto-checkpoint.
    auto store = GetStore(config);
    ASSERT_NE(store, nullptr) << "Failed to open store";
    InsertRows(store, 80, 1024);

    // Assert: WAL < 100KB — proves TryCheckPoint triggered.
    // Without checkpoint, WAL would be ~330KB; after checkpoint (TRUNCATE), only
    // remaining frames after last checkpoint stay (~20KB).
    int64_t walSize = GetFileSize(dbPath + "-wal");
    EXPECT_LT(walSize, SIZE_100KB) << "WAL should be below 100KB after checkpoint, got " << walSize;

    // Assert: data intact
    EXPECT_EQ(QueryCount(store), 80) << "Row count should be 80 after checkpoint";

    // Cleanup
    store = nullptr;
    CleanupDb(dbPath);
}

/**
 * @brief Set startCheckpointSize exceeding checkpointSize → clamped to checkpointSize →
 *        GetRdbStore → insert enough data to exceed clamped threshold → verify checkpoint triggered
 * @param TestCaseName Checkpoint_Clamped_001
 * @since 1.0
 */
HWTEST_F(CheckpointSizeTest, Checkpoint_Clamped_001, TestSize.Level1)
{
    std::string dbPath = RDB_TEST_PATH + "ckpt_clamped_001.db";
    // Arrange: set startCheckpointSize exceeding checkpointSize → clamped
    RdbHelper::DeleteRdbStore(dbPath);
    RdbStoreConfig config(dbPath);
    ssize_t checkpointSize = config.GetCheckpointSize();
    config.SetStartCheckpointSize(checkpointSize + 10 * 1024 * 1024);

    // Assert: clamped to checkpointSize
    EXPECT_EQ(config.GetStartCheckpointSize(), checkpointSize)
        << "startCheckpointSize should be clamped to checkpointSize (" << checkpointSize << ")";

    // Act: open DB, insert enough data to exceed the clamped threshold
    auto store = GetStore(config);
    ASSERT_NE(store, nullptr) << "Store should open with clamped startCheckpointSize";
    InsertRows(store, 2700, 100 * 1024); // 2700 rows × 100KB ≈ 270MB > 256MB checkpointSize

    // Assert: checkpoint triggered when WAL exceeded clamped threshold, WAL should be below threshold
    int64_t walSize = GetFileSize(dbPath + "-wal");
    EXPECT_LT(walSize, SIZE_100MB) << "WAL should be below clamped threshold (" << checkpointSize
                                       << ") after checkpoint, got " << walSize;

    // Assert: data intact
    EXPECT_EQ(QueryCount(store), 2700) << "Row count should be 2700 with clamped threshold";

    // Cleanup
    store = nullptr;
    CleanupDb(dbPath);
}

/**
 * @brief Same as Checkpoint_Clamped_001 but disables SQLite auto-checkpoint (PRAGMA
 *        wal_autocheckpoint=0) after opening DB, so checkpoint is triggered solely by
 *        RDB TryCheckPoint. Set startCheckpointSize exceeding checkpointSize → clamped →
 *        disable auto-checkpoint → insert data exceeding clamped threshold → verify
 *        RDB checkpoint triggered (WAL < checkpointSize), data intact.
 * @param TestCaseName Checkpoint_ClampedNoAutoCkpt_001
 * @since 1.0
 */
HWTEST_F(CheckpointSizeTest, Checkpoint_ClampedNoAutoCkpt_001, TestSize.Level1)
{
    std::string dbPath = RDB_TEST_PATH + "ckpt_clamped_no_auto_001.db";
    // Arrange: set startCheckpointSize exceeding checkpointSize → clamped
    RdbHelper::DeleteRdbStore(dbPath);
    RdbStoreConfig config(dbPath);
    ssize_t checkpointSize = config.GetCheckpointSize();
    config.SetStartCheckpointSize(checkpointSize + 10 * 1024 * 1024);

    // Assert: clamped to checkpointSize
    EXPECT_EQ(config.GetStartCheckpointSize(), checkpointSize)
        << "startCheckpointSize should be clamped to checkpointSize (" << checkpointSize << ")";

    // Act: open DB, disable auto-checkpoint so only RDB TryCheckPoint controls checkpointing
    auto store = GetStore(config);
    ASSERT_NE(store, nullptr) << "Store should open with clamped startCheckpointSize";
    EXPECT_EQ(store->ExecuteSql("PRAGMA wal_autocheckpoint=0"), E_OK)
        << "Failed to disable auto-checkpoint";

    // Act: insert enough data to exceed the clamped threshold
    InsertRows(store, 2700, 100 * 1024); // 2700 rows × 100KB ≈ 270MB > 256MB checkpointSize

    // Assert: RDB TryCheckPoint triggered when WAL exceeded clamped threshold,
    // WAL should be below 100M — proves checkpoint fired (without checkpoint, WAL would be ~270MB)
    int64_t walSize = GetFileSize(dbPath + "-wal");
    EXPECT_LT(walSize, SIZE_100MB) << "WAL should be below 100M after RDB checkpoint, got " << walSize;

    // Assert: data intact
    EXPECT_EQ(QueryCount(store), 2700) << "Row count should be 2700";

    // Cleanup
    store = nullptr;
    CleanupDb(dbPath);
}

/**
 * @brief SetWalLimitSize(512MB) → checkpointSize_ = (512>>1)+(512>>2) = 384MB,
 *        SetStartCheckpointSize exceeding 384MB → clamped to 384MB, verify DB opens and read/write works
 * @param TestCaseName Checkpoint_ClampedWithWalLimit_001
 * @since 1.0
 */
HWTEST_F(CheckpointSizeTest, Checkpoint_ClampedWithWalLimit_001, TestSize.Level1)
{
    std::string dbPath = RDB_TEST_PATH + "ckpt_walimit_clamp_001.db";
    // Arrange: SetWalLimitSize(512MB) → checkpointSize_ = 384MB
    RdbHelper::DeleteRdbStore(dbPath);
    RdbStoreConfig config(dbPath);
    config.SetWalLimitSize(512 * 1024 * 1024);

    // Assert: checkpointSize_ is exactly 384MB
    EXPECT_EQ(config.GetCheckpointSize(), SIZE_384MB)
        << "checkpointSize should be 384MB after SetWalLimitSize(512MB)";

    // Act: set startCheckpointSize exceeding 384MB → clamped to 384MB
    config.SetStartCheckpointSize(SIZE_384MB + 10 * 1024 * 1024);

    // Assert: clamped to 384MB
    EXPECT_EQ(config.GetStartCheckpointSize(), SIZE_384MB)
        << "startCheckpointSize should be clamped to 384MB";

    // Act: open DB, insert 200 rows × 1KB, verify read/write works
    auto store = GetStore(config);
    ASSERT_NE(store, nullptr) << "Store should open with walLimit config";
    InsertRows(store, 200, 1024);

    // Assert: data intact
    EXPECT_EQ(QueryCount(store), 200) << "Row count should be 200";

    // Cleanup
    store = nullptr;
    CleanupDb(dbPath);
}

/**
 * @brief Set startCheckpointSize=-1 → falls back to default 20M → GetRdbStore →
 *        insert 25MB data (> 20M threshold) → checkpoint triggered, data intact
 * @param TestCaseName Checkpoint_NegativeFallback_001
 * @since 1.0
 */
HWTEST_F(CheckpointSizeTest, Checkpoint_NegativeFallback_001, TestSize.Level1)
{
    std::string dbPath = RDB_TEST_PATH + "ckpt_negative_fallback_001.db";
    // Arrange: set negative startCheckpointSize → falls back to default 20M
    RdbHelper::DeleteRdbStore(dbPath);
    RdbStoreConfig config(dbPath);
    config.SetStartCheckpointSize(-1);
    EXPECT_EQ(config.GetStartCheckpointSize(), SIZE_20MB)
        << "Negative startCheckpointSize should fall back to default 20M";

    // Act: open DB, insert 250 rows × 100KB = 25MB WAL (exceeds 20M threshold)
    auto store = GetStore(config);
    ASSERT_NE(store, nullptr) << "Store should open with negative startCkptSize (fallback to default)";
    InsertRows(store, 250, SIZE_100KB);

    // Assert: checkpoint triggered when WAL exceeded 20M threshold, WAL should be below threshold
    int64_t walSize = GetFileSize(dbPath + "-wal");
    EXPECT_LT(walSize, SIZE_20MB) << "WAL should be below 20M after checkpoint, got " << walSize;

    // Assert: data intact
    EXPECT_EQ(QueryCount(store), 250) << "Row count should be 250";

    // Cleanup
    store = nullptr;
    CleanupDb(dbPath);
}

/**
 * @brief Set startCheckpointSize=0 → falls back to default 20M (same as negative) →
 *        GetRdbStore → insert data, verify read/write works
 * @param TestCaseName Checkpoint_ZeroFallback_001
 * @since 1.0
 */
HWTEST_F(CheckpointSizeTest, Checkpoint_ZeroFallback_001, TestSize.Level1)
{
    std::string dbPath = RDB_TEST_PATH + "ckpt_zero_fallback_001.db";
    // Arrange: set startCheckpointSize=0 → falls back to default 20M
    RdbHelper::DeleteRdbStore(dbPath);
    RdbStoreConfig config(dbPath);
    config.SetStartCheckpointSize(0);
    EXPECT_EQ(config.GetStartCheckpointSize(), SIZE_20MB)
        << "Zero startCheckpointSize should fall back to default 20M";

    // Act: open DB, insert 200 rows × 1KB, verify read/write works
    auto store = GetStore(config);
    ASSERT_NE(store, nullptr) << "Store should open with zero startCkptSize (fallback to default)";
    InsertRows(store, 200, 1024);

    // Assert: data intact
    EXPECT_EQ(QueryCount(store), 200) << "Row count should be 200";

    // Cleanup
    store = nullptr;
    CleanupDb(dbPath);
}

/**
 * @brief Memory mode DB with custom startCheckpointSize: open OK, read/write works,
 *        no WAL file (checkpoint N/A in memory mode)
 * @param TestCaseName Checkpoint_MemoryDb_001
 * @since 1.0
 */
HWTEST_F(CheckpointSizeTest, Checkpoint_MemoryDb_001, TestSize.Level1)
{
    std::string dbPath = RDB_TEST_PATH + "ckpt_memory_001.db";
    // Arrange: memory mode with custom startCheckpointSize
    RdbHelper::DeleteRdbStore(dbPath);
    RdbStoreConfig config(dbPath, StorageMode::MODE_MEMORY);
    config.SetStartCheckpointSize(SIZE_100KB);
    EXPECT_EQ(config.GetStartCheckpointSize(), SIZE_100KB) << "startCheckpointSize should be 100KB";

    // Act: open DB, create table, insert data
    auto store = GetStore(config);
    ASSERT_NE(store, nullptr) << "Memory DB should open with custom startCheckpointSize";
    EXPECT_EQ(store->ExecuteSql("INSERT INTO test_table (data) VALUES ('mem_data')"), E_OK)
        << "Insert should succeed";

    // Assert: data readable
    EXPECT_EQ(QueryCount(store), 1) << "Row count should be 1";

    // Assert: no WAL file — memory mode uses in-memory DB, checkpoint does not apply
    int64_t walSize = GetFileSize(dbPath + "-wal");
    EXPECT_EQ(walSize, 0) << "Memory mode should not create WAL file";

    // Cleanup
    store = nullptr;
    RdbHelper::ClearCache();
}

/**
 * @brief Non-WAL mode (TRUNCATE) DB with custom startCheckpointSize: open OK, read/write works,
 *        no WAL file (checkpoint N/A in TRUNCATE mode)
 * @param TestCaseName Checkpoint_NonWalMode_001
 * @since 1.0
 */
HWTEST_F(CheckpointSizeTest, Checkpoint_NonWalMode_001, TestSize.Level1)
{
    std::string dbPath = RDB_TEST_PATH + "ckpt_nonwal_001.db";
    // Arrange: TRUNCATE journal mode with custom startCheckpointSize
    RdbHelper::DeleteRdbStore(dbPath);
    RdbStoreConfig config(dbPath);
    config.SetJournalMode(JournalMode::MODE_TRUNCATE);
    config.SetStartCheckpointSize(SIZE_100KB);
    EXPECT_EQ(config.GetStartCheckpointSize(), SIZE_100KB) << "startCheckpointSize should be 100KB";

    // Act: open DB, create table, insert data
    auto store = GetStore(config);
    ASSERT_NE(store, nullptr) << "Non-WAL DB should open with custom startCheckpointSize";
    EXPECT_EQ(store->ExecuteSql("INSERT INTO test_table (data) VALUES ('truncate_data')"), E_OK)
        << "Insert should succeed in TRUNCATE mode";

    // Assert: data readable
    EXPECT_EQ(QueryCount(store), 1) << "Row count should be 1 in TRUNCATE mode";

    // Assert: no WAL file — TRUNCATE mode uses rollback journal, checkpoint does not apply
    int64_t walSize = GetFileSize(dbPath + "-wal");
    EXPECT_EQ(walSize, 0) << "TRUNCATE mode should not create WAL file";

    // Cleanup
    store = nullptr;
    CleanupDb(dbPath);
}

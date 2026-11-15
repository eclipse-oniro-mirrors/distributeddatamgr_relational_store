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

#define LOG_TAG "RdbStoreFaultTest"
#include <fcntl.h>
#include <gtest/gtest.h>
#include <sys/stat.h>
#include <unistd.h>

#include <fstream>
#include <memory>
#include <string>

#include "common.h"
#include "rdb_errno.h"
#include "rdb_fault_hiview_reporter.h"
#include "rdb_helper.h"
#include "rdb_open_callback.h"
#include "rdb_store_config.h"
#include "sqlite_utils.h"
#include "transaction.h"
#include "transaction_impl.h"
#include "values_bucket.h"
#include "values_buckets.h"

using namespace testing::ext;
using namespace OHOS::NativeRdb;

namespace {
constexpr mode_t TEST_DIR_MODE = 0770;
constexpr int TEST_ROW_COUNT = 5;
} // namespace

class FaultTestOpenCallback : public RdbOpenCallback {
public:
    int OnCreate(RdbStore &store) override
    {
        return store.ExecuteSql(
            "CREATE TABLE IF NOT EXISTS test (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT NOT NULL)");
    }
    int OnUpgrade(RdbStore &store, int oldVersion, int newVersion) override
    {
        return E_OK;
    }
};

class RdbStoreFaultTest : public testing::Test {
public:
    static void SetUpTestCase()
    {
        mkdir(RDB_TEST_PATH.c_str(), TEST_DIR_MODE);
        Transaction::RegisterCreator(
            [](int32_t type, std::shared_ptr<Connection> conn, const std::string &path, bool auditEnabled)
                -> std::pair<int32_t, std::shared_ptr<Transaction>> {
                return TransactionImpl::Create(type, std::move(conn), path, auditEnabled);
            });
    }
    static void TearDownTestCase() {}
    void SetUp() override
    {
        auto *info = testing::UnitTest::GetInstance()->current_test_info();
        testTag_ = std::string(info->test_suite_name()) + "_" + info->name();
        dbName_ = RDB_TEST_PATH + testTag_ + ".db";
        RdbHelper::DeleteRdbStore(dbName_);
        RdbFaultHiViewReporter::memCorruptReportedFlg_ = false;
    }
    void TearDown() override
    {
        store_ = nullptr;
        RdbHelper::ClearCache();
        RdbHelper::DeleteRdbStore(dbName_);
        RdbHelper::DeleteRdbStore(dbName_ + "_slave.db");
        unlink((dbName_ + "-slaveFailure").c_str());
        unlink((dbName_ + "-syncInterrupt").c_str());
        unlink((dbName_ + ".corruptedflg").c_str());
        unlink((dbName_ + ".bothcorruptedflg").c_str());
        unlink((dbName_ + "_slave.db.corruptedflg").c_str());
        RdbFaultHiViewReporter::memCorruptReportedFlg_ = false;
    }

protected:
    std::shared_ptr<RdbStore> store_;
    std::string testTag_;
    std::string dbName_;
};

/**
 * @brief Test that ReportSlaveInconsistent deduplicates when slave is already
 *        marked invalid via -slaveFailure flag under MAIN_REPLICA mode.
 * @param TestCaseName Rdb_StoreFault_SlaveFailureFlag_001
 * @since 1.0
 */
HWTEST_F(RdbStoreFaultTest, Rdb_StoreFault_SlaveFailureFlag_001, TestSize.Level1)
{
    // 1. Setup: create database with MAIN_REPLICA ha-mode
    RdbStoreConfig config(dbName_);
    config.SetHaMode(HAMode::MAIN_REPLICA);
    FaultTestOpenCallback callback;
    int errCode = E_ERROR;
    store_ = RdbHelper::GetRdbStore(config, 1, callback, errCode);
    ASSERT_NE(store_, nullptr) << "GetRdbStore failed, errCode=" << errCode;
    ASSERT_EQ(errCode, E_OK);

    // 2. Execute: create -slaveFailure flag file to simulate slave invalidation
    std::string flagPath = dbName_ + "-slaveFailure";
    {
        std::ofstream f(flagPath);
        ASSERT_TRUE(f.is_open()) << "cannot create slaveFailure flag: " << flagPath;
        f << "2026-10-20\nverify_failed";
        f.close();
    }
    ASSERT_TRUE(access(flagPath.c_str(), F_OK) == 0) << "slaveFailure flag not created";
    EXPECT_TRUE(SqliteUtils::IsSlaveInvalid(dbName_)) << "IsSlaveInvalid should be true when flag exists";

    // 3. Verify: ReportSlaveInconsistent should take dedup path without crash or duplicate report
    EXPECT_NO_FATAL_FAILURE(
        RdbFaultHiViewReporter::ReportSlaveInconsistent(config, "test slave failure dedup"));
    EXPECT_NE(access((dbName_ + ".corruptedflg").c_str(), F_OK), 0)
        << "corruptedflg should not be created in dedup path";
}

/**
 * @brief Test that BatchInsert returns E_SQLITE_BUSY when an IMMEDIATE transaction
 *        holds the write lock.
 * @param TestCaseName Rdb_StoreFault_SqliteBusy_001
 * @since 1.0
 */
HWTEST_F(RdbStoreFaultTest, Rdb_StoreFault_SqliteBusy_001, TestSize.Level1)
{
    // 1. Setup: create database and table
    RdbStoreConfig config(dbName_);
    FaultTestOpenCallback callback;
    int errCode = E_ERROR;
    store_ = RdbHelper::GetRdbStore(config, 1, callback, errCode);
    ASSERT_NE(store_, nullptr) << "GetRdbStore failed, errCode=" << errCode;
    ASSERT_EQ(errCode, E_OK);

    std::string tableName = "BusyTest";
    auto res = store_->Execute(
        "CREATE TABLE " + tableName + " (id INTEGER PRIMARY KEY, name TEXT NOT NULL)");
    ASSERT_EQ(res.first, E_OK);

    // 2. Execute: hold write lock via IMMEDIATE transaction, then BatchInsert to trigger SQLITE_BUSY
    auto [code, transaction] = store_->CreateTransaction(Transaction::IMMEDIATE);
    ASSERT_EQ(code, E_OK) << "CreateTransaction failed, code=" << code;
    ASSERT_NE(transaction, nullptr);

    ValuesBuckets rows;
    for (int i = 0; i < TEST_ROW_COUNT; i++) {
        ValuesBucket row;
        row.Put("id", i);
        row.Put("name", "Jim");
        rows.Put(row);
    }
    auto result = store_->BatchInsert(tableName, rows, ConflictResolution::ON_CONFLICT_NONE);

    // 3. Verify: BatchInsert should return E_SQLITE_BUSY with no rows inserted
    EXPECT_EQ(result.first, E_SQLITE_BUSY) << "BatchInsert should return E_SQLITE_BUSY, actual=" << result.first;
    EXPECT_EQ(result.second, -1) << "changed count should be -1 on BUSY, actual=" << result.second;

    auto resultSet = store_->QueryByStep("SELECT COUNT(*) FROM " + tableName);
    ASSERT_NE(resultSet, nullptr) << "QueryByStep returned null";
    ASSERT_EQ(resultSet->GoToNextRow(), E_OK);
    int64_t count = 0;
    resultSet->GetLong(0, count);
    EXPECT_EQ(count, 0) << "no rows should be inserted under BUSY, actual=" << count;
}

/**
 * @brief Test that ReportSlaveCorruptWithMasterCheck creates .bothcorruptedflg
 *        marker file for dedup, and second call does not duplicate the event.
 * @param TestCaseName Rdb_StoreFault_BothCorruptDedup_001
 * @since 1.0
 */
HWTEST_F(RdbStoreFaultTest, Rdb_StoreFault_BothCorruptDedup_001, TestSize.Level1)
{
    // 1. Setup: create database with MAIN_REPLICA mode, then close to operate on flag files
    RdbStoreConfig config(dbName_);
    config.SetHaMode(HAMode::MAIN_REPLICA);
    FaultTestOpenCallback callback;
    int errCode = E_ERROR;
    store_ = RdbHelper::GetRdbStore(config, 1, callback, errCode);
    ASSERT_NE(store_, nullptr) << "GetRdbStore failed, errCode=" << errCode;
    ASSERT_EQ(errCode, E_OK);
    store_ = nullptr;
    RdbHelper::ClearCache();

    // 2. Simulate master corruption: create .corruptedflg marker file
    std::string corruptedFlag = dbName_ + ".corruptedflg";
    {
        std::ofstream f(corruptedFlag);
        ASSERT_TRUE(f.is_open()) << "cannot create corruptedflg: " << corruptedFlag;
        f.close();
    }
    ASSERT_TRUE(RdbFaultHiViewReporter::IsCorruptedFlagExist(dbName_));

    // 3. Build slave corrupt event and call ReportSlaveCorruptWithMasterCheck
    std::string slavePath = SqliteUtils::GetSlavePath(config);
    RdbStoreConfig slaveCfg(slavePath);
    auto slaveEvent = RdbFaultHiViewReporter::Create(slaveCfg, E_DFX_SLAVE_CORRUPT,
        "slave corrupt on open", false, true);
    slaveEvent.subCaseDesc = RdbSubCase::BOTH_FAIL;

    EXPECT_FALSE(RdbFaultHiViewReporter::IsBothCorruptFlagExist(dbName_));
    EXPECT_NO_FATAL_FAILURE(RdbFaultHiViewReporter::ReportSlaveCorruptWithMasterCheck(
        slaveEvent, config, "master and slave both corrupt"));
    EXPECT_TRUE(RdbFaultHiViewReporter::IsBothCorruptFlagExist(dbName_));

    // 4. Second call: .bothcorruptedflg already exists, dedup path should not crash or duplicate
    EXPECT_NO_FATAL_FAILURE(RdbFaultHiViewReporter::ReportSlaveCorruptWithMasterCheck(
        slaveEvent, config, "master and slave both corrupt on restore"));
    EXPECT_TRUE(RdbFaultHiViewReporter::IsBothCorruptFlagExist(dbName_));
}

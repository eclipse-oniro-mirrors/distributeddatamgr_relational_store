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

// Unit tests for the RDB fault-injection mock layer. Two levels are covered:
// the injector itself (config parsing, budgets, probability, per-DB
// targeting, open modes) and the interface level (CRUD, execute, transaction
// and result-set reads returning E_SQLITE_CORRUPT through the sink hooks in
// RdbStoreImpl/TransDB/TransactionImpl and the result-set decorators).

#define LOG_TAG "RdbMockInject"
#include <gtest/gtest.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

#include "logger.h"
#include "rdb_errno.h"
#include "rdb_fault_injector.h"
#include "rdb_helper.h"
#include "rdb_open_callback.h"
#include "rdb_predicates.h"
#include "rdb_store.h"
#include "rdb_store_config.h"
#include "rdb_test_common.h"
#include "transaction.h"
#include "values_bucket.h"

using namespace testing::ext;
using namespace OHOS::NativeRdb;

namespace {
constexpr const char *FAULT_CONFIG_NAME = "rdb_fault_inject_config.json";
const std::string TEST_DB_A = RDB_TEST_PATH + "mock_inject_a.db";
const std::string TEST_DB_B = RDB_TEST_PATH + "mock_inject_b.db";
const std::string CONFIG_PATH = RDB_TEST_PATH + FAULT_CONFIG_NAME;

constexpr const char *MOCK_STORE_DB = "mock_inject_store.db";

class MockInjectOpenCallback : public RdbOpenCallback {
public:
    int OnCreate(RdbStore &store) override
    {
        return store.ExecuteSql(createMockTable);
    }
    int OnUpgrade(RdbStore &store, int oldVersion, int newVersion) override
    {
        return E_OK;
    }
    static constexpr const char *createMockTable =
        "CREATE TABLE IF NOT EXISTS mock_test (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT NOT NULL)";
};

} // namespace

class RdbMockInjectTest : public testing::Test {
public:
    static void SetUpTestCase()
    {
    }
    static void TearDownTestCase()
    {
    }

    void SetUp() override
    {
        RemoveConfig();
    }

    void TearDown() override
    {
        RemoveConfig();
    }

protected:
    // Sample rounds and expected hits for the concurrent-load cases (the
    // config writes an INSERT budget of CONCURRENT_BUDGET).
    static constexpr int CONCURRENT_ROUNDS = 100;
    static constexpr int CONCURRENT_BUDGET = 16;

    void RemoveConfig()
    {
        // Absence is not an error (the config may never have been written);
        // any other removal failure is surfaced so it is not silently lost.
        if (remove(CONFIG_PATH.c_str()) != 0 && errno != ENOENT) {
            GTEST_LOG_(WARNING) << "remove config failed, errno: " << errno;
        }
    }

    void WriteConfig(const std::string &content)
    {
        std::ofstream fout(CONFIG_PATH, std::ios::trunc);
        ASSERT_TRUE(fout.is_open());
        fout << content << std::endl;
    }

    static int CountHits(RdbFaultInjector &injector, RdbFaultOp op, int rounds)
    {
        int hits = 0;
        for (int i = 0; i < rounds; ++i) {
            if (injector.Check(op)) {
                ++hits;
            }
        }
        return hits;
    }

    // Thread body for the concurrent-consumption case: counts INSERT hits
    // in rounds and adds them to the shared counter atomically; all threads
    // share one injector instance so the budget is consumed race-free.
    static void CountInsertWorker(RdbFaultInjector *injector, std::atomic<int> *hits, int rounds)
    {
        for (int i = 0; i < rounds; ++i) {
            if (injector->Check(RdbFaultOp::INSERT)) {
                hits->fetch_add(1);
            }
        }
    }

    // Thread body for the concurrent-load case: constructs its own injector
    // (reads the shared config file) and verifies its own budget.
    static void LoadAndVerifyWorker()
    {
        RdbFaultInjector concurrent(TEST_DB_A);
        EXPECT_EQ(CountHits(concurrent, RdbFaultOp::INSERT, CONCURRENT_ROUNDS), CONCURRENT_BUDGET);
    }

    std::shared_ptr<RdbStore> OpenMockStore(int &errCode)
    {
        RdbHelper::ClearCache();
        RdbHelper::DeleteRdbStore(RDB_TEST_PATH + MOCK_STORE_DB);
        RdbStoreConfig config(RDB_TEST_PATH + MOCK_STORE_DB);
        MockInjectOpenCallback callback;
        errCode = E_ERROR;
        return RdbHelper::GetRdbStore(config, 1, callback, errCode);
    }

    // Two-phase open for configs that cover the executeSql family: the
    // injector is created before the base Init, so a fresh open would hit
    // OnCreate's table DDL and fail the open. The table is created by a
    // clean first open, then the cache is dropped (the file stays), the
    // config is written and the reopen skips the open callbacks (version
    // match), leaving the budget intact for the assertions.
    std::shared_ptr<RdbStore> ReopenMockStoreWithConfig(const std::string &content, int &errCode)
    {
        RemoveConfig();
        RdbHelper::ClearCache();
        RdbHelper::DeleteRdbStore(RDB_TEST_PATH + MOCK_STORE_DB);
        RdbStoreConfig config(RDB_TEST_PATH + MOCK_STORE_DB);
        MockInjectOpenCallback callback;
        errCode = E_ERROR;
        if (RdbHelper::GetRdbStore(config, 1, callback, errCode) == nullptr) {
            return nullptr;
        }
        RdbHelper::ClearCache();
        WriteConfig(content);
        errCode = E_ERROR;
        return RdbHelper::GetRdbStore(config, 1, callback, errCode);
    }

    int QueryRowCount(const std::shared_ptr<RdbStore> &store)
    {
        auto resultSet = store->QuerySql("SELECT COUNT(*) AS cnt FROM mock_test");
        if (resultSet == nullptr) {
            return -1;
        }
        if (resultSet->GoToNextRow() != E_OK) {
            resultSet->Close();
            return -1;
        }
        int count = -1;
        int err = resultSet->GetInt(0, count);
        resultSet->Close();
        return err == E_OK ? count : -1;
    }
};

// ===== Injector semantics =====

/**
 * @tc.name: RdbFaultInject_NoConfig_001
 * @tc.desc: Verify that with no config file present, every verdict is a miss.
 * @tc.type: FUNC
 */
HWTEST_F(RdbMockInjectTest, RdbFaultInject_NoConfig_001, TestSize.Level1)
{
    RdbFaultInjector injectorA(TEST_DB_A);
    EXPECT_FALSE(injectorA.Check(RdbFaultOp::INSERT));
    EXPECT_FALSE(injectorA.CheckMethod("Backup"));
    EXPECT_FALSE(injectorA.ConsumeOpen());
}

/**
 * @tc.name: RdbFaultInject_DefaultParams_002
 * @tc.desc: Verify the default entry parameters: probability 100 and an
 *           unlimited count; unconfigured operations stay unaffected.
 * @tc.type: FUNC
 */
HWTEST_F(RdbMockInjectTest, RdbFaultInject_DefaultParams_002, TestSize.Level1)
{
    WriteConfig(R"({"databases": {"mock_inject_a.db": {"operations": {"insert": {}}}}})");
    RdbFaultInjector injectorA(TEST_DB_A);
    EXPECT_EQ(CountHits(injectorA, RdbFaultOp::INSERT, 100), 100);
    EXPECT_EQ(CountHits(injectorA, RdbFaultOp::UPDATE, 100), 0) << "unconfigured op must never hit";
}

/**
 * @tc.name: RdbFaultInject_CountExhaust_003
 * @tc.desc: Verify that a count of N injects exactly N times and then
 *           recovers naturally.
 * @tc.type: FUNC
 */
HWTEST_F(RdbMockInjectTest, RdbFaultInject_CountExhaust_003, TestSize.Level1)
{
    WriteConfig(R"({"databases": {"mock_inject_a.db": {"operations": {"insert": {"count": 2}}}}})");
    RdbFaultInjector injectorA(TEST_DB_A);
    EXPECT_TRUE(injectorA.Check(RdbFaultOp::INSERT));
    EXPECT_TRUE(injectorA.Check(RdbFaultOp::INSERT));
    EXPECT_FALSE(injectorA.Check(RdbFaultOp::INSERT));
    EXPECT_FALSE(injectorA.Check(RdbFaultOp::INSERT));
}

/**
 * @tc.name: RdbFaultInject_ProbabilityInvalid_004
 * @tc.desc: Verify that probability 0 and count 0 never inject, an
 *           out-of-range probability is kept raw (always hits), and
 *           sibling entries still work.
 * @tc.type: FUNC
 */
HWTEST_F(RdbMockInjectTest, RdbFaultInject_ProbabilityInvalid_004, TestSize.Level1)
{
    WriteConfig(R"({"databases": {"mock_inject_a.db": {"operations": {
        "insert": {"probability": 0},
        "update": {"count": 0},
        "delete": {"probability": 150},
        "query": {"count": 3}
    }}}})");
    RdbFaultInjector injectorA(TEST_DB_A);
    EXPECT_EQ(CountHits(injectorA, RdbFaultOp::INSERT, 50), 0);
    EXPECT_EQ(CountHits(injectorA, RdbFaultOp::UPDATE, 50), 0);
    EXPECT_EQ(CountHits(injectorA, RdbFaultOp::DELETE, 50), 50) << "out-of-range probability is kept raw";
    EXPECT_EQ(CountHits(injectorA, RdbFaultOp::QUERY, 50), 3) << "valid sibling entry must still work";
}

/**
 * @tc.name: RdbFaultInject_ProbabilityNoConsume_005
 * @tc.desc: Verify that a missed probability sample does not consume budget
 *           and that concurrent consumers never exceed the configured count.
 * @tc.type: FUNC
 */
HWTEST_F(RdbMockInjectTest, RdbFaultInject_ProbabilityNoConsume_005, TestSize.Level2)
{
    WriteConfig(R"({"databases": {"mock_inject_a.db": {"operations": {"insert": {"count": 2, "probability": 50}}}}})");
    RdbFaultInjector injectorA(TEST_DB_A);
    // After 1000 samples the hit count must equal the count exactly: if a
    // missed probability also consumed budget, the hits would be fewer.
    EXPECT_EQ(CountHits(injectorA, RdbFaultOp::INSERT, 1000), 2);

    // Concurrent stress: 4 threads consume in parallel from one shared
    // instance; the total number of hits equals the count exactly
    // (atomic consumption, no over-issue).
    WriteConfig(R"({"databases": {"mock_inject_a.db": {"operations": {"insert": {"count": 8, "probability": 50}}}}})");
    RdbFaultInjector injectorShared(TEST_DB_A);
    std::atomic<int> hits(0);
    std::vector<std::thread> workers;
    for (int t = 0; t < 4; ++t) {
        workers.emplace_back(&RdbMockInjectTest::CountInsertWorker, &injectorShared, &hits, 500);
    }
    for (auto &worker : workers) {
        worker.join();
    }
    EXPECT_EQ(hits.load(), 8) << "concurrent consumption must not exceed the count";
}

/**
 * @tc.name: RdbFaultInject_ReopenRefresh_006
 * @tc.desc: Verify that a config modified at runtime is picked up by a
 *           freshly constructed injector (each store/injector reads the
 *           file once at construction).
 * @tc.type: FUNC
 */
HWTEST_F(RdbMockInjectTest, RdbFaultInject_ReopenRefresh_006, TestSize.Level1)
{
    WriteConfig(R"({"databases": {"mock_inject_a.db": {"operations": {"insert": {"count": 1}}}}})");
    RdbFaultInjector injectorA(TEST_DB_A);
    EXPECT_TRUE(injectorA.Check(RdbFaultOp::INSERT));
    EXPECT_FALSE(injectorA.Check(RdbFaultOp::INSERT)) << "budget exhausted";

    // Modify the config at runtime (count widened to 5); a new injector
    // (in production: a newly constructed store) picks the new budget up.
    WriteConfig(R"({"databases": {"mock_inject_a.db": {"operations": {"insert": {"count": 5}}}}})");
    RdbFaultInjector injectorReloaded(TEST_DB_A);
    EXPECT_EQ(CountHits(injectorReloaded, RdbFaultOp::INSERT, 10), 5);

    // Delete the config and construct again: fully recovered.
    RemoveConfig();
    RdbFaultInjector injectorClean(TEST_DB_A);
    EXPECT_EQ(CountHits(injectorClean, RdbFaultOp::INSERT, 10), 0);
}

/**
 * @tc.name: RdbFaultInject_BadConfigFailSafe_007
 * @tc.desc: Verify that a malformed config file is treated as "not
 *           configured" (fail-safe).
 * @tc.type: FUNC
 */
HWTEST_F(RdbMockInjectTest, RdbFaultInject_BadConfigFailSafe_007, TestSize.Level1)
{
    WriteConfig(R"({"databases": {"mock_inject_a.db": {"operations": {"insert": )");
    RdbFaultInjector injectorA(TEST_DB_A);
    EXPECT_EQ(CountHits(injectorA, RdbFaultOp::INSERT, 10), 0);
}

/**
 * @tc.name: RdbFaultInject_UnknownFieldTolerated_008
 * @tc.desc: Verify that unknown top-level fields are tolerated (forward
 *           compatibility) while configured entries keep working.
 * @tc.type: FUNC
 */
HWTEST_F(RdbMockInjectTest, RdbFaultInject_UnknownFieldTolerated_008, TestSize.Level1)
{
    WriteConfig(R"({"futureField": 1, "databases": {"mock_inject_a.db": {"operations": {"insert": {"count": 1}}}}})");
    RdbFaultInjector injectorA(TEST_DB_A);
    EXPECT_TRUE(injectorA.Check(RdbFaultOp::INSERT));
    EXPECT_FALSE(injectorA.Check(RdbFaultOp::INSERT));
}

/**
 * @tc.name: RdbFaultInject_MethodLevelEntry_009
 * @tc.desc: Verify that a per-method entry hits only the named method and
 *           leaves other methods and operation families unaffected.
 * @tc.type: FUNC
 */
HWTEST_F(RdbMockInjectTest, RdbFaultInject_MethodLevelEntry_009, TestSize.Level1)
{
    WriteConfig(R"({"databases": {"mock_inject_a.db": {"methods": {"Backup": {"count": 1}}}}})");
    RdbFaultInjector injectorA(TEST_DB_A);
    EXPECT_TRUE(injectorA.CheckMethod("Backup"));
    EXPECT_FALSE(injectorA.CheckMethod("Backup")) << "count exhausted";
    EXPECT_FALSE(injectorA.CheckMethod("Restore"));
    EXPECT_FALSE(injectorA.Check(RdbFaultOp::INSERT)) << "family not configured";
}

/**
 * @tc.name: RdbFaultInject_DbNameTargeting_010
 * @tc.desc: Verify per-DB-name targeting: only the listed database in the
 *           shared directory is affected.
 * @tc.type: FUNC
 */
HWTEST_F(RdbMockInjectTest, RdbFaultInject_DbNameTargeting_010, TestSize.Level1)
{
    WriteConfig(R"({"databases": {"mock_inject_a.db": {"operations": {"insert": {}}}}})");
    RdbFaultInjector injectorA(TEST_DB_A);
    RdbFaultInjector injectorB(TEST_DB_B); // same directory, not listed
    EXPECT_EQ(CountHits(injectorA, RdbFaultOp::INSERT, 3), 3);
    EXPECT_EQ(CountHits(injectorB, RdbFaultOp::INSERT, 3), 0) << "unlisted db must be unaffected";
}

/**
 * @tc.name: RdbFaultInject_OpenEntryParse_011
 * @tc.desc: Verify open-entry parsing: a configured open entry injects
 *           (the open path returns the corruption error) and consumes its
 *           budget; a database not listed never injects.
 * @tc.type: FUNC
 */
HWTEST_F(RdbMockInjectTest, RdbFaultInject_OpenEntryParse_011, TestSize.Level1)
{
    WriteConfig(R"({"databases": {"mock_inject_a.db": {"open": {"count": 1}}}})");
    RdbFaultInjector injectorA(TEST_DB_A);
    RdbFaultInjector injectorB(TEST_DB_B); // same directory, not listed
    EXPECT_TRUE(injectorA.ConsumeOpen());
    EXPECT_FALSE(injectorA.ConsumeOpen()) << "count exhausted";
    EXPECT_FALSE(injectorB.ConsumeOpen()) << "not listed";
}

/**
 * @tc.name: RdbFaultInject_ConcurrentLoad_012
 * @tc.desc: Verify that concurrent injector constructions from one config
 *           file neither crash nor affect each other's budget.
 * @tc.type: FUNC
 */
HWTEST_F(RdbMockInjectTest, RdbFaultInject_ConcurrentLoad_012, TestSize.Level2)
{
    WriteConfig(R"({"databases": {"mock_inject_a.db": {"operations": {"insert": {"count": 16}}}}})");
    // Each thread constructs its own injector (reads the shared config
    // file) and verifies its own budget independently.
    std::vector<std::thread> loaders;
    for (int t = 0; t < 8; ++t) {
        loaders.emplace_back(&RdbMockInjectTest::LoadAndVerifyWorker);
    }
    for (auto &loader : loaders) {
        loader.join();
    }
    RdbFaultInjector injectorA(TEST_DB_A);
    EXPECT_EQ(CountHits(injectorA, RdbFaultOp::INSERT, CONCURRENT_ROUNDS), CONCURRENT_BUDGET);
}

/**
 * @tc.name: RdbFaultInject_OpTypeCoverage_014
 * @tc.desc: Verify that all seven operation keys parse and consume, plus
 *           the open entry.
 * @tc.type: FUNC
 */
HWTEST_F(RdbMockInjectTest, RdbFaultInject_OpTypeCoverage_014, TestSize.Level1)
{
    WriteConfig(R"({"databases": {"mock_inject_a.db": {
        "open": {"count": 1},
        "operations": {
            "insert": {"count": 1}, "update": {"count": 1}, "delete": {"count": 1},
            "query": {"count": 1}, "executeSql": {"count": 1}, "transaction": {"count": 1},
            "resultSetRead": {"count": 1}
        }
    }}})");
    RdbFaultInjector injectorA(TEST_DB_A);
    EXPECT_TRUE(injectorA.Check(RdbFaultOp::INSERT));
    EXPECT_TRUE(injectorA.Check(RdbFaultOp::UPDATE));
    EXPECT_TRUE(injectorA.Check(RdbFaultOp::DELETE));
    EXPECT_TRUE(injectorA.Check(RdbFaultOp::QUERY));
    EXPECT_TRUE(injectorA.Check(RdbFaultOp::EXECUTE_SQL));
    EXPECT_TRUE(injectorA.Check(RdbFaultOp::TRANSACTION));
    EXPECT_TRUE(injectorA.Check(RdbFaultOp::RESULT_SET_READ));
    EXPECT_TRUE(injectorA.ConsumeOpen());
}

// ===== Interface-level behavior =====


/**
 * @tc.name: RdbFaultInject_InsertInterface_015
 * @tc.desc: Verify insert injection at the operation-family level: the hit
 *           returns E_SQLITE_CORRUPT with no side effect and the store
 *           recovers once the budget is exhausted.
 * @tc.type: FUNC
 */
HWTEST_F(RdbMockInjectTest, RdbFaultInject_InsertInterface_015, TestSize.Level1)
{
    WriteConfig(R"({"databases": {"mock_inject_store.db": {"operations": {"insert": {"count": 1}}}}})");
    int errCode = E_ERROR;
    auto store = OpenMockStore(errCode);
    ASSERT_NE(store, nullptr) << "open must succeed with only an insert entry";
    EXPECT_EQ(errCode, E_OK);

    ValuesBucket bucket;
    bucket.PutString("name", "alice");
    int64_t outRowId = 0;
    EXPECT_EQ(store->Insert(outRowId, "mock_test", bucket), E_SQLITE_CORRUPT) << "injected insert";
    EXPECT_EQ(store->Insert(outRowId, "mock_test", bucket), E_OK) << "budget exhausted, real insert";
    EXPECT_GT(outRowId, 0);
    EXPECT_EQ(QueryRowCount(store), 1) << "the injected insert must not reach the database";
}

/**
 * @tc.name: RdbFaultInject_UpdateDeleteInterface_016
 * @tc.desc: Verify update and delete injection: hits return E_SQLITE_CORRUPT
 *           with no side effect, and real operations run after exhaustion.
 * @tc.type: FUNC
 */
HWTEST_F(RdbMockInjectTest, RdbFaultInject_UpdateDeleteInterface_016, TestSize.Level1)
{
    WriteConfig(R"({"databases": {"mock_inject_store.db": {"operations": {
        "update": {"count": 1}, "delete": {"count": 1}}}}})");
    int errCode = E_ERROR;
    auto store = OpenMockStore(errCode);
    ASSERT_NE(store, nullptr);
    ValuesBucket bucket;
    bucket.PutString("name", "alice");
    ValuesBucket bucket2;
    bucket2.PutString("name", "carol");
    int64_t outRowId = 0;
    ASSERT_EQ(store->Insert(outRowId, "mock_test", bucket), E_OK);
    ASSERT_EQ(store->Insert(outRowId, "mock_test", bucket2), E_OK);

    RdbPredicates updatePredicates("mock_test");
    updatePredicates.EqualTo("name", "alice");
    ValuesBucket updateBucket;
    updateBucket.PutString("name", "bob");
    int changedRows = 0;
    int deletedRows = 0;
    EXPECT_EQ(store->Update(changedRows, updateBucket, updatePredicates), E_SQLITE_CORRUPT) << "update injected";
    EXPECT_EQ(store->Update(changedRows, updateBucket, updatePredicates), E_OK) << "real update (alice to bob)";
    RdbPredicates deletePredicates("mock_test");
    deletePredicates.EqualTo("name", "bob");
    EXPECT_EQ(store->Delete(deletedRows, deletePredicates), E_SQLITE_CORRUPT) << "delete injected";
    EXPECT_EQ(store->Delete(deletedRows, deletePredicates), E_OK) << "real delete of the updated row";
    EXPECT_EQ(QueryRowCount(store), 1) << "one injected delete and one real delete leave one row";
}

/**
 * @tc.name: RdbFaultInject_QueryExecuteInterface_017
 * @tc.desc: Verify query and executeSql injection: a query hit returns a
 *           null result set and an executeSql hit returns the corruption
 *           error without executing the SQL.
 * @tc.type: FUNC
 */
HWTEST_F(RdbMockInjectTest, RdbFaultInject_QueryExecuteInterface_017, TestSize.Level1)
{
    int errCode = E_ERROR;
    auto store = ReopenMockStoreWithConfig(R"({"databases": {"mock_inject_store.db": {"operations": {
        "query": {"count": 1}, "executeSql": {"count": 1}}}}})", errCode);
    ASSERT_NE(store, nullptr);
    ValuesBucket bucket;
    bucket.PutString("name", "alice");
    int64_t outRowId = 0;
    ASSERT_EQ(store->Insert(outRowId, "mock_test", bucket), E_OK);

    EXPECT_EQ(store->QuerySql("SELECT COUNT(*) AS cnt FROM mock_test"), nullptr) << "query injected";
    EXPECT_NE(store->QuerySql("SELECT COUNT(*) AS cnt FROM mock_test"), nullptr) << "budget exhausted";
    EXPECT_EQ(store->ExecuteSql("INSERT INTO mock_test (name) VALUES ('carol')"), E_SQLITE_CORRUPT);
    EXPECT_EQ(QueryRowCount(store), 1) << "the injected executeSql must not run";
    EXPECT_EQ(store->ExecuteSql("INSERT INTO mock_test (name) VALUES ('carol')"), E_OK);
    EXPECT_EQ(QueryRowCount(store), 2);
}

/**
 * @tc.name: RdbFaultInject_ResultSetReadInterface_018
 * @tc.desc: Verify resultSetRead injection: the cursor does not advance on a
 *           hit, recovers after exhaustion, and Close stays available.
 * @tc.type: FUNC
 */
HWTEST_F(RdbMockInjectTest, RdbFaultInject_ResultSetReadInterface_018, TestSize.Level1)
{
    WriteConfig(R"({"databases": {"mock_inject_store.db": {"operations": {"resultSetRead": {"count": 1}}}}})");
    int errCode = E_ERROR;
    auto store = OpenMockStore(errCode);
    ASSERT_NE(store, nullptr);
    ValuesBucket bucket;
    bucket.PutString("name", "alice");
    int64_t outRowId = 0;
    ASSERT_EQ(store->Insert(outRowId, "mock_test", bucket), E_OK);
    ASSERT_EQ(store->Insert(outRowId, "mock_test", bucket), E_OK);

    auto resultSet = store->QuerySql("SELECT id FROM mock_test");
    ASSERT_NE(resultSet, nullptr);
    EXPECT_EQ(resultSet->GoToNextRow(), E_SQLITE_CORRUPT) << "injected read, cursor stays";
    EXPECT_EQ(resultSet->GoToNextRow(), E_OK) << "budget exhausted, cursor advances to row 1";
    int id = 0;
    EXPECT_EQ(resultSet->GetInt(0, id), E_OK);
    EXPECT_EQ(id, 1);
    EXPECT_EQ(resultSet->Close(), E_OK) << "Close is excluded from the read family";
}

/**
 * @tc.name: RdbFaultInject_TransactionInterface_019
 * @tc.desc: Verify transaction-family injection at the store entry point:
 *           CreateTransaction reports the corruption error on a hit; after
 *           exhaustion the transaction is created and commits for real, so
 *           the writes become visible.
 * @tc.type: FUNC
 */
HWTEST_F(RdbMockInjectTest, RdbFaultInject_TransactionInterface_019, TestSize.Level1)
{
    WriteConfig(R"({"databases": {"mock_inject_store.db": {"operations": {"transaction": {"count": 1}}}}})");
    int errCode = E_ERROR;
    auto store = OpenMockStore(errCode);
    ASSERT_NE(store, nullptr);

    auto [firstRet, firstTrans] = store->CreateTransaction(Transaction::IMMEDIATE);
    EXPECT_EQ(firstRet, E_SQLITE_CORRUPT) << "CreateTransaction hit by the transaction family";
    EXPECT_EQ(firstTrans, nullptr);
    firstTrans = nullptr;

    auto [ret, transaction] = store->CreateTransaction(Transaction::IMMEDIATE);
    EXPECT_EQ(ret, E_OK) << "budget exhausted";
    ASSERT_NE(transaction, nullptr);
    Transaction::Row row;
    row.PutString("name", "dave");
    auto [insErr, insRowId] = transaction->Insert("mock_test", row);
    EXPECT_EQ(insErr, E_OK) << "insert family not configured, real execution";
    EXPECT_EQ(transaction->Commit(), E_OK) << "real commit after exhaustion";
    EXPECT_EQ(QueryRowCount(store), 1) << "committed writes are visible";
}

/**
 * @tc.name: RdbFaultInject_MethodLevelInterface_020
 * @tc.desc: Verify per-method injection at the interface level: naming
 *           QuerySql injects only that method while other families stay
 *           unaffected.
 * @tc.type: FUNC
 */
HWTEST_F(RdbMockInjectTest, RdbFaultInject_MethodLevelInterface_020, TestSize.Level1)
{
    WriteConfig(R"({"databases": {"mock_inject_store.db": {"methods": {"QuerySql": {"count": 1}}}}})");
    int errCode = E_ERROR;
    auto store = OpenMockStore(errCode);
    ASSERT_NE(store, nullptr);
    EXPECT_EQ(store->QuerySql("SELECT COUNT(*) AS cnt FROM mock_test"), nullptr) << "named method hit";
    EXPECT_NE(store->QuerySql("SELECT COUNT(*) AS cnt FROM mock_test"), nullptr) << "budget exhausted";
    ValuesBucket bucket;
    bucket.PutString("name", "alice");
    int64_t outRowId = 0;
    EXPECT_EQ(store->Insert(outRowId, "mock_test", bucket), E_OK) << "unnamed method unaffected";
}

/**
 * @tc.name: RdbFaultInject_MethodOverridesCategory_021
 * @tc.desc: Verify that the per-method entry takes precedence over the
 *           operation family entry.
 * @tc.type: FUNC
 */
HWTEST_F(RdbMockInjectTest, RdbFaultInject_MethodOverridesCategory_021, TestSize.Level1)
{
    WriteConfig(R"({"databases": {"mock_inject_store.db": {"operations": {"insert": {"count": 0}},
        "methods": {"Insert": {"count": 1}}}}})");
    int errCode = E_ERROR;
    auto store = OpenMockStore(errCode);
    ASSERT_NE(store, nullptr);
    ValuesBucket bucket;
    bucket.PutString("name", "alice");
    int64_t outRowId = 0;
    EXPECT_EQ(store->Insert(outRowId, "mock_test", bucket), E_SQLITE_CORRUPT) << "method-level hit";
    EXPECT_EQ(store->Insert(outRowId, "mock_test", bucket), E_OK) << "method exhausted, family count 0 never injects";
    EXPECT_EQ(QueryRowCount(store), 1);
}

/**
 * @tc.name: RdbFaultInject_ErrorCodeValue_022
 * @tc.desc: Verify the native error code value: an injected call returns
 *           exactly E_SQLITE_CORRUPT (27394104).
 * @tc.type: FUNC
 */
HWTEST_F(RdbMockInjectTest, RdbFaultInject_ErrorCodeValue_022, TestSize.Level1)
{
    WriteConfig(R"({"databases": {"mock_inject_store.db": {"operations": {"insert": {"count": 1}}}}})");
    int errCode = E_ERROR;
    auto store = OpenMockStore(errCode);
    ASSERT_NE(store, nullptr);
    ValuesBucket bucket;
    bucket.PutString("name", "alice");
    int64_t outRowId = 0;
    EXPECT_EQ(store->Insert(outRowId, "mock_test", bucket), 27394104);
}

/**
 * @tc.name: RdbFaultInject_HitLog_023
 * @tc.desc: Verify the injected call path executes (the distinguishable
 *           rate-limited hit log with TAG RDB_FAULT_INJECT goes to the
 *           process hilog stream, evidenced by the test session console).
 * @tc.type: FUNC
 */
HWTEST_F(RdbMockInjectTest, RdbFaultInject_HitLog_023, TestSize.Level1)
{
    WriteConfig(R"({"databases": {"mock_inject_store.db": {"operations": {"insert": {"count": 1}}}}})");
    int errCode = E_ERROR;
    auto store = OpenMockStore(errCode);
    ASSERT_NE(store, nullptr);
    ValuesBucket bucket;
    bucket.PutString("name", "alice");
    int64_t outRowId = 0;
    EXPECT_EQ(store->Insert(outRowId, "mock_test", bucket), E_SQLITE_CORRUPT);
}

// ===== Open-path modes =====

/**
 * @tc.name: RdbFaultInject_OpenErrorMode_024
 * @tc.desc: Verify the open error mode (default): the open fails with the
 *           corruption error, no handle is returned and no database file is
 *           created.
 * @tc.type: FUNC
 */
HWTEST_F(RdbMockInjectTest, RdbFaultInject_OpenErrorMode_024, TestSize.Level1)
{
    WriteConfig(R"({"databases": {"mock_inject_store.db": {"open": {"count": 1}}}})");
    int errCode = E_ERROR;
    auto store = OpenMockStore(errCode);
    EXPECT_EQ(errCode, E_SQLITE_CORRUPT) << "open fails with the corruption error";
    EXPECT_EQ(store, nullptr) << "no handle";
    EXPECT_EQ(access((RDB_TEST_PATH + MOCK_STORE_DB).c_str(), F_OK) != 0, true) << "no db file produced";
}

/**
 * @tc.name: RdbFaultInject_OpenErrorExplicit_025
 * @tc.desc: Verify the explicit "error" open mode behaves the same as the
 *           default.
 * @tc.type: FUNC
 */
HWTEST_F(RdbMockInjectTest, RdbFaultInject_OpenErrorExplicit_025, TestSize.Level1)
{
    WriteConfig(R"({"databases": {"mock_inject_store.db": {"open": {"mode": "error", "count": 1}}}})");
    int errCode = E_ERROR;
    auto store = OpenMockStore(errCode);
    EXPECT_EQ(errCode, E_SQLITE_CORRUPT);
    EXPECT_EQ(store, nullptr);
}

/**
 * @tc.name: RdbFaultInject_OpenErrorRecoverPaths_029
 * @tc.desc: Verify the two recovery paths of the open entry: (a) count 0
 *           never injects; (b) an injected open recovers after the config
 *           is removed and the store reopens.
 * @tc.type: FUNC
 */
HWTEST_F(RdbMockInjectTest, RdbFaultInject_OpenErrorRecoverPaths_029, TestSize.Level1)
{
    // (a) count 0 guard branch
    WriteConfig(R"({"databases": {"mock_inject_store.db": {"open": {"count": 0}}}})");
    int errCode = E_ERROR;
    auto store = OpenMockStore(errCode);
    ASSERT_NE(store, nullptr);
    EXPECT_EQ(errCode, E_OK) << "count 0 never injects";

    // (b) count 1 injects, then the config is removed and the reopen recovers
    WriteConfig(R"({"databases": {"mock_inject_store.db": {"open": {"count": 1}}}})");
    errCode = E_ERROR;
    store = OpenMockStore(errCode);
    EXPECT_EQ(errCode, E_SQLITE_CORRUPT);
    EXPECT_EQ(store, nullptr);
    RemoveConfig();
    errCode = E_ERROR;
    store = OpenMockStore(errCode);
    ASSERT_NE(store, nullptr);
    EXPECT_EQ(errCode, E_OK);
    ValuesBucket bucket;
    bucket.PutString("name", "alice");
    int64_t outRowId = 0;
    EXPECT_EQ(store->Insert(outRowId, "mock_test", bucket), E_OK);
}

/**
 * @tc.name: RdbFaultInject_OpenInvalidParamsIgnored_030
 * @tc.desc: Verify that an open entry with out-of-range parameters is
 *           ignored as a whole: the open proceeds normally.
 * @tc.type: FUNC
 */
HWTEST_F(RdbMockInjectTest, RdbFaultInject_OpenInvalidParamsIgnored_030, TestSize.Level1)
{
    WriteConfig(R"({"databases": {"mock_inject_a.db": {"open": {"probability": 150}}}})");
    RdbFaultInjector injectorA(TEST_DB_A);
    EXPECT_FALSE(injectorA.Check(RdbFaultOp::INSERT));


    WriteConfig(R"({"databases": {"mock_inject_store.db": {"open": {"probability": 150}}}})");
    int errCode = E_ERROR;
    auto store = OpenMockStore(errCode);
    ASSERT_NE(store, nullptr);
    EXPECT_EQ(errCode, E_OK) << "invalid entry ignored, normal open";
    ValuesBucket bucket;
    bucket.PutString("name", "alice");
    int64_t outRowId = 0;
    EXPECT_EQ(store->Insert(outRowId, "mock_test", bucket), E_OK);
}

// ===== Config parsing edge branches =====

/**
 * @tc.name: RdbFaultInject_MissingDatabasesKey_031
 * @tc.desc: Verify that a config without the "databases" key or with an
 *           empty object is treated as "not configured".
 * @tc.type: FUNC
 */
HWTEST_F(RdbMockInjectTest, RdbFaultInject_MissingDatabasesKey_031, TestSize.Level1)
{
    WriteConfig(R"({"futureField": 1})");
    RdbFaultInjector injectorA(TEST_DB_A);
    EXPECT_FALSE(injectorA.Check(RdbFaultOp::INSERT));

    WriteConfig("{}");
    RdbFaultInjector injectorEmpty(TEST_DB_A);
    EXPECT_FALSE(injectorEmpty.Check(RdbFaultOp::INSERT));
}

/**
 * @tc.name: RdbFaultInject_DbNodeNotObject_032
 * @tc.desc: Verify that a non-object database node (an integer) is treated
 *           as "not configured".
 * @tc.type: FUNC
 */
HWTEST_F(RdbMockInjectTest, RdbFaultInject_DbNodeNotObject_032, TestSize.Level1)
{
    WriteConfig(R"({"databases": {"mock_inject_a.db": 123}})");
    RdbFaultInjector injectorA(TEST_DB_A);
    EXPECT_FALSE(injectorA.Check(RdbFaultOp::INSERT));
}

/**
 * @tc.name: RdbFaultInject_MemoryDbGetRdbInject_033
 * @tc.desc: Verify injection on an in-memory database (MODE_MEMORY, file:
 *           URI) reached through RdbHelper::GetRdb: the insert hook hits
 *           with the config resolved relative to the CWD, and the GetRdb
 *           open-error branch returns nullptr.
 * @tc.type: FUNC
 */
HWTEST_F(RdbMockInjectTest, RdbFaultInject_MemoryDbGetRdbInject_033, TestSize.Level1)
{
    RemoveConfig();
    // Cleanup of the CWD-relative config (file: URIs); absence is fine.
    if (remove("rdb_fault_inject_config.json") != 0 && errno != ENOENT) {
        GTEST_LOG_(WARNING) << "remove config failed, errno: " << errno;
    }
    std::ofstream fout("rdb_fault_inject_config.json", std::ios::trunc);
    ASSERT_TRUE(fout.is_open());
    fout << R"({"databases": {"mem_mock.db": {"operations": {"insert": {"count": 1}}}}})" << std::endl;

    RdbStoreConfig config(RDB_TEST_PATH);
    config.SetName("mem_mock.db");
    config.SetStorageMode(StorageMode::MODE_MEMORY);
    MockInjectOpenCallback callback;
    int errCode = E_ERROR;
    // GetRdb only returns an already-cached, initialized store: create it
    // through GetRdbStore first, then exercise the GetRdb path.
    auto created = RdbHelper::GetRdbStore(config, 1, callback, errCode);
    ASSERT_NE(created, nullptr);
    EXPECT_EQ(errCode, E_OK);
    auto store = RdbHelper::GetRdb(config);
    ASSERT_NE(store, nullptr);
    ValuesBucket bucket;
    bucket.PutString("name", "alice");
    int64_t outRowId = 0;
    EXPECT_EQ(store->Insert(outRowId, "mock_test", bucket), E_SQLITE_CORRUPT) << "memory db injection hit";
    EXPECT_EQ(store->Insert(outRowId, "mock_test", bucket), E_OK) << "budget exhausted";

    RdbHelper::DeleteRdbStore(config);

    // GetRdb open-error branch: rewrite the config with an open entry and
    // GetRdb returns nullptr directly.
    std::ofstream fout2("rdb_fault_inject_config.json", std::ios::trunc);
    ASSERT_TRUE(fout2.is_open());
    fout2 << R"({"databases": {"mem_mock.db": {"open": {"count": 1}}}})" << std::endl;
    EXPECT_EQ(RdbHelper::GetRdb(config), nullptr) << "GetRdb with the open entry consumed";
    EXPECT_EQ(RdbHelper::GetRdb(config), nullptr) << "store not created (first open failed)";

    EXPECT_EQ(remove("rdb_fault_inject_config.json"), 0);
    RdbStoreConfig recreateConfig(RDB_TEST_PATH);
    recreateConfig.SetName("mem_mock.db");
    recreateConfig.SetStorageMode(StorageMode::MODE_MEMORY);
    RdbHelper::DeleteRdbStore(recreateConfig);
}

/**
 * @tc.name: RdbFaultInject_RealQueryErrorTransparent_034
 * @tc.desc: Verify that a real query error surfaces unchanged through the
 *           wrapped result set: the error is not the injected corruption
 *           code, so the two are distinguishable.
 * @tc.type: FUNC
 */
HWTEST_F(RdbMockInjectTest, RdbFaultInject_RealQueryErrorTransparent_034, TestSize.Level1)
{
    WriteConfig(R"({"databases": {"mock_inject_store.db": {"operations": {"insert": {}}}}})");
    int errCode = E_ERROR;
    auto store = OpenMockStore(errCode); // entry present, result sets wrapped
    ASSERT_NE(store, nullptr);
    auto resultSet = store->QuerySql("SELECT * FROM nonexistent_table");
    ASSERT_NE(resultSet, nullptr) << "inner returns an error result set, not nullptr";
    int stepErr = resultSet->GoToNextRow();
    EXPECT_NE(stepErr, E_OK) << "the real SQL error surfaces as-is";
    EXPECT_NE(stepErr, E_SQLITE_CORRUPT) << "must not be confused with the injected code";
    EXPECT_EQ(resultSet->Close(), E_OK);
}

/**
 * @tc.name: RdbFaultInject_CommitMethodLevel_035
 * @tc.desc: Verify per-method Commit injection on a transaction: only the
 *           named method is injected, the data operations run for real, and
 *           the transaction converges through a real rollback.
 * @tc.type: FUNC
 */
HWTEST_F(RdbMockInjectTest, RdbFaultInject_CommitMethodLevel_035, TestSize.Level1)
{
    WriteConfig(R"({"databases": {"mock_inject_store.db": {"methods": {"Commit": {"count": 1}}}}})");
    int errCode = E_ERROR;
    auto store = OpenMockStore(errCode);
    ASSERT_NE(store, nullptr);
    auto [ret, transaction] = store->CreateTransaction(Transaction::IMMEDIATE);
    EXPECT_EQ(ret, E_OK);
    ASSERT_NE(transaction, nullptr);
    Transaction::Row row;
    row.PutString("name", "dave");
    auto [insErr, insRowId] = transaction->Insert("mock_test", row);
    EXPECT_EQ(insErr, E_OK) << "only Commit is named, data ops run for real";
    EXPECT_EQ(transaction->Commit(), E_SQLITE_CORRUPT) << "method-level hit";
    EXPECT_EQ(QueryRowCount(store), 0) << "transaction not committed";
    EXPECT_EQ(transaction->Rollback(), E_OK) << "budget exhausted, real rollback converges";
}

/**
 * @tc.name: RdbFaultInject_ParseEdgeBranches_036
 * @tc.desc: Verify LoadEntry parsing edges: unknown operation keys,
 *           non-integer counts and a non-string mode are each ignored while
 *           valid sibling entries keep working.
 * @tc.type: FUNC
 */
HWTEST_F(RdbMockInjectTest, RdbFaultInject_ParseEdgeBranches_036, TestSize.Level1)
{
    WriteConfig(R"({"databases": {"mock_inject_a.db": {"operations": {
        "unknownOperation": {"count": 1},
        "insert": {"count": "abc"},
        "query": {"count": 1}
    }, "open": {"mode": 123, "count": 1}}}})");
    RdbFaultInjector injectorA(TEST_DB_A);
    EXPECT_TRUE(injectorA.Check(RdbFaultOp::INSERT)) << "non-integer count keeps the default";
    EXPECT_TRUE(injectorA.Check(RdbFaultOp::QUERY)) << "valid entry works";
    // open: the unknown "mode" field is tolerated; count 1 is consumable
    EXPECT_TRUE(injectorA.ConsumeOpen());
}

/**
 * @tc.name: RdbFaultInject_ResultSetMethodLevel_037
 * @tc.desc: Verify per-method injection on a wrapped result set: naming
 *           GoToNextRow injects only that method.
 * @tc.type: FUNC
 */
HWTEST_F(RdbMockInjectTest, RdbFaultInject_ResultSetMethodLevel_037, TestSize.Level1)
{
    WriteConfig(R"({"databases": {"mock_inject_store.db": {"methods": {"GoToNextRow": {"count": 1}}}}})");
    int errCode = E_ERROR;
    auto store = OpenMockStore(errCode);
    ASSERT_NE(store, nullptr);
    ValuesBucket bucket;
    bucket.PutString("name", "alice");
    int64_t outRowId = 0;
    ASSERT_EQ(store->Insert(outRowId, "mock_test", bucket), E_OK);
    ASSERT_EQ(store->Insert(outRowId, "mock_test", bucket), E_OK);

    auto resultSet = store->QuerySql("SELECT id FROM mock_test");
    ASSERT_NE(resultSet, nullptr);
    EXPECT_EQ(resultSet->GoToNextRow(), E_SQLITE_CORRUPT) << "method-level hit";
    EXPECT_EQ(resultSet->GoToNextRow(), E_OK) << "budget exhausted, family not configured";
    EXPECT_EQ(resultSet->Close(), E_OK);
}

/**
 * @tc.name: RdbFaultInject_ResultSetCloseMethodLevel_038
 * @tc.desc: Verify that Close is only injectable per method (excluded from
 *           the read family) and that entries with invalid parameters are
 *           ignored.
 * @tc.type: FUNC
 */
HWTEST_F(RdbMockInjectTest, RdbFaultInject_ResultSetCloseMethodLevel_038, TestSize.Level1)
{
    WriteConfig(R"({"databases": {"mock_inject_store.db": {"methods": {
        "Close": {"count": 1}, "GoToFirstRow": {"count": "abc"}}}}})");
    int errCode = E_ERROR;
    auto store = OpenMockStore(errCode);
    ASSERT_NE(store, nullptr);
    ValuesBucket bucket;
    bucket.PutString("name", "alice");
    int64_t outRowId = 0;
    ASSERT_EQ(store->Insert(outRowId, "mock_test", bucket), E_OK);

    auto resultSet = store->QuerySql("SELECT id FROM mock_test");
    ASSERT_NE(resultSet, nullptr);
    EXPECT_EQ(resultSet->GoToFirstRow(), E_SQLITE_CORRUPT) << "non-integer count keeps the default";
    EXPECT_EQ(resultSet->Close(), E_SQLITE_CORRUPT) << "Close per-method hit";
    EXPECT_EQ(resultSet->Close(), E_OK) << "budget exhausted, real close";
}

/**
 * @tc.name: RdbFaultInject_ParseNegativeParams_039
 * @tc.desc: Verify ParseParams edges: an out-of-range integer is kept raw
 *           (negative count = unlimited, negative probability = never)
 *           while null keeps the defaults.
 * @tc.type: FUNC
 */
HWTEST_F(RdbMockInjectTest, RdbFaultInject_ParseNegativeParams_039, TestSize.Level1)
{
    // Negative count: kept raw, acts as unlimited
    WriteConfig(R"({"databases": {"mock_inject_a.db": {"operations": {"insert": {"count": -5}, "query": {}}}}})");
    RdbFaultInjector injectorNegCount(TEST_DB_A);
    EXPECT_TRUE(injectorNegCount.Check(RdbFaultOp::INSERT)) << "negative count acts as unlimited";
    EXPECT_TRUE(injectorNegCount.Check(RdbFaultOp::QUERY)) << "sibling entry works";

    // Negative probability: kept raw, never hits
    WriteConfig(R"({"databases": {"mock_inject_a.db": {"operations": {"insert": {"probability": -1}}}}})");
    RdbFaultInjector injectorNegProb(TEST_DB_A);
    EXPECT_FALSE(injectorNegProb.Check(RdbFaultOp::INSERT));

    // Non-integer probability: keeps the default (always inject)
    WriteConfig(R"({"databases": {"mock_inject_a.db": {"operations": {"insert": {"probability": "abc"}}}}})");
    RdbFaultInjector injectorNonInt(TEST_DB_A);
    EXPECT_TRUE(injectorNonInt.Check(RdbFaultOp::INSERT));

    // Null count: takes the default branch (entry valid, unlimited)
    WriteConfig(R"({"databases": {"mock_inject_a.db": {"operations": {"insert": {"count": null}}}}})");
    RdbFaultInjector injectorNullCount(TEST_DB_A);
    EXPECT_TRUE(injectorNullCount.Check(RdbFaultOp::INSERT)) << "null means default";
}

/**
 * @tc.name: RdbFaultInject_NonObjectEntries_040
 * @tc.desc: Verify that non-object nodes (databases, open, operations,
 *           methods entries) are each ignored while sibling entries work.
 * @tc.type: FUNC
 */
HWTEST_F(RdbMockInjectTest, RdbFaultInject_NonObjectEntries_040, TestSize.Level1)
{
    // databases node not an object
    WriteConfig(R"({"databases": 123})");
    RdbFaultInjector injectorBadDatabases(TEST_DB_A);
    EXPECT_FALSE(injectorBadDatabases.Check(RdbFaultOp::INSERT));

    // open entry not an object
    WriteConfig(R"({"databases": {"mock_inject_a.db": {"open": 123}}})");
    RdbFaultInjector injectorBadOpen(TEST_DB_A);
    EXPECT_FALSE(injectorBadOpen.ConsumeOpen());

    // operations entry not an object: keeps the defaults
    WriteConfig(R"({"databases": {"mock_inject_a.db": {"operations": {"insert": 123, "query": {}}}}})");
    RdbFaultInjector injectorBadOperations(TEST_DB_A);
    EXPECT_TRUE(injectorBadOperations.Check(RdbFaultOp::INSERT)) << "non-object entry keeps the defaults";
    EXPECT_TRUE(injectorBadOperations.Check(RdbFaultOp::QUERY));

    // methods entry not an object: keeps the defaults
    WriteConfig(R"({"databases": {"mock_inject_a.db": {"methods": {"Backup": 123, "Restore": {}}}}})");
    RdbFaultInjector injectorBadMethods(TEST_DB_A);
    EXPECT_TRUE(injectorBadMethods.CheckMethod("Backup")) << "non-object entry keeps the defaults";
    EXPECT_TRUE(injectorBadMethods.CheckMethod("Restore"));
}

/**
 * @tc.name: RdbFaultInject_FileUriNoQuery_041
 * @tc.desc: Verify GetDbName with a file: URI that has no query string: the
 *           whole name after the prefix is the database name and the config
 *           is resolved relative to the CWD.
 * @tc.type: FUNC
 */
HWTEST_F(RdbMockInjectTest, RdbFaultInject_FileUriNoQuery_041, TestSize.Level1)
{
    // Cleanup of the CWD-relative config (file: has no directory
    // semantics); absence is fine.
    if (remove("rdb_fault_inject_config.json") != 0 && errno != ENOENT) {
        GTEST_LOG_(WARNING) << "remove config failed, errno: " << errno;
    }
    std::ofstream fout("rdb_fault_inject_config.json", std::ios::trunc);
    ASSERT_TRUE(fout.is_open());
    fout << R"({"databases": {"uri_plain.db": {"operations": {"insert": {}}}}})" << std::endl;
    // file: without '?' - the whole name after the prefix is the db name
    RdbFaultInjector injectorUri("file:uri_plain.db");
    EXPECT_TRUE(injectorUri.Check(RdbFaultOp::INSERT));
    EXPECT_EQ(remove("rdb_fault_inject_config.json"), 0);
}

/**
 * @tc.name: RdbFaultInject_ConsumeOpenOpsOnly_042
 * @tc.desc: Verify that ConsumeOpen returns false for a database configured
 *           with operation entries only (no open entry).
 * @tc.type: FUNC
 */
HWTEST_F(RdbMockInjectTest, RdbFaultInject_ConsumeOpenOpsOnly_042, TestSize.Level1)
{
    WriteConfig(R"({"databases": {"mock_inject_a.db": {"operations": {"insert": {}}}}})");
    RdbFaultInjector injectorA(TEST_DB_A);
    EXPECT_FALSE(injectorA.ConsumeOpen()) << "no openConfig";
}

/**
 * @tc.name: RdbFaultInject_NeverLoadedPath_043
 * @tc.desc: Verify that a path never loaded takes the not-found branch of
 *           every verdict.
 * @tc.type: FUNC
 */
HWTEST_F(RdbMockInjectTest, RdbFaultInject_NeverLoadedPath_043, TestSize.Level1)
{
    const std::string neverLoaded = RDB_TEST_PATH + "never_loaded_zz.db";
    RemoveConfig(); // make sure no file exists
    RdbFaultInjector neverInjected(neverLoaded);
    EXPECT_FALSE(neverInjected.Check(RdbFaultOp::INSERT));
    EXPECT_FALSE(neverInjected.CheckMethod("Backup"));
    EXPECT_FALSE(neverInjected.ConsumeOpen());
    EXPECT_FALSE(neverInjected.HasEntry());
}

// ===== RdbStoreManager branch coverage =====

/**
 * @tc.name: RdbFaultInject_ManagerCacheExpiredDecorator_044
 * @tc.desc: Verify the weak-ref-expired cache path creates a decorator:
 *           after the store is released (cache entry expires), a reopen
 *           yields a store whose CRUD can be injected.
 * @tc.type: FUNC
 */
HWTEST_F(RdbMockInjectTest, RdbFaultInject_ManagerCacheExpiredDecorator_044, TestSize.Level1)
{
    // Open a store normally, then release it so the cache weak-ref expires.
    RemoveConfig();
    int errCode = E_ERROR;
    auto store = OpenMockStore(errCode);
    ASSERT_NE(store, nullptr);
    EXPECT_EQ(errCode, E_OK);
    store = nullptr; // release the shared_ptr so the weak-ref expires

    // Reopen: the cache entry is stale (lock fails) → a new decorator is
    // created on the expired path. Configure injection and verify the
    // reopened store is a decorator (CRUD returns the corruption error).
    WriteConfig(R"({"databases": {"mock_inject_store.db": {"operations": {"insert": {"count": 1}}}}})");
    errCode = E_ERROR;
    store = OpenMockStore(errCode);
    ASSERT_NE(store, nullptr);
    EXPECT_EQ(errCode, E_OK);
    ValuesBucket bucket;
    bucket.PutString("name", "alice");
    int64_t outRowId = 0;
    EXPECT_EQ(store->Insert(outRowId, "mock_test", bucket), E_SQLITE_CORRUPT)
        << "reopened store is a decorator (expired cache path)";
    EXPECT_EQ(store->Insert(outRowId, "mock_test", bucket), E_OK) << "budget exhausted";
}

/**
 * @tc.name: RdbFaultInject_ManagerConfigChangedDecorator_045
 * @tc.desc: Verify the config-changed cache path creates a decorator:
 *           when the same path is opened with a different config, the
 *           recreated store can be injected.
 * @tc.type: FUNC
 */
HWTEST_F(RdbMockInjectTest, RdbFaultInject_ManagerConfigChangedDecorator_045, TestSize.Level2)
{
    RemoveConfig();
    RdbHelper::ClearCache();
    RdbHelper::DeleteRdbStore(RDB_TEST_PATH + MOCK_STORE_DB);

    // Open with the base config.
    RdbStoreConfig baseConfig(RDB_TEST_PATH + MOCK_STORE_DB);
    MockInjectOpenCallback callback;
    int errCode = E_ERROR;
    auto store = RdbHelper::GetRdbStore(baseConfig, 1, callback, errCode);
    ASSERT_NE(store, nullptr);
    EXPECT_EQ(errCode, E_OK);
    store = nullptr; // release so the reopen goes through the cache

    // Write the injection config BEFORE the reopen: the decorator reads
    // the config file once at construction.
    WriteConfig(R"({"databases": {"mock_inject_store.db": {"operations": {"insert": {"count": 1}}}}})");

    // Reopen with a changed config that still allows the recreation path
    // (version < INVALID_CONFIG_CHANGE_NOT_ALLOWED).
    RdbStoreConfig changedConfig(RDB_TEST_PATH + MOCK_STORE_DB);
    changedConfig.SetJournalMode(JournalMode::MODE_WAL); // differs from the base default
    errCode = E_ERROR;
    store = RdbHelper::GetRdbStore(changedConfig, 1, callback, errCode);
    ASSERT_NE(store, nullptr);
    EXPECT_EQ(errCode, E_OK);

    // The config-changed path recreated a decorator with the injection
    // entry already loaded: CRUD returns the corruption error.
    ValuesBucket bucket;
    bucket.PutString("name", "alice");
    int64_t outRowId = 0;
    EXPECT_EQ(store->Insert(outRowId, "mock_test", bucket), E_SQLITE_CORRUPT)
        << "recreated store is a decorator (config-changed path)";
    EXPECT_EQ(store->Insert(outRowId, "mock_test", bucket), E_OK) << "budget exhausted";
}

/**
 * @tc.name: RdbFaultInject_ManagerGetRdbOpenError_046
 * @tc.desc: Verify the GetRdb open-error branch: when the open entry is
 *           configured, GetRdb returns nullptr without creating a store.
 * @tc.type: FUNC
 */
HWTEST_F(RdbMockInjectTest, RdbFaultInject_ManagerGetRdbOpenError_046, TestSize.Level1)
{
    RemoveConfig();
    RdbHelper::ClearCache();
    RdbHelper::DeleteRdbStore(RDB_TEST_PATH + MOCK_STORE_DB);

    // Normal open through GetRdb succeeds.
    RdbStoreConfig config(RDB_TEST_PATH + MOCK_STORE_DB);
    MockInjectOpenCallback callback;
    int errCode = E_ERROR;
    auto store = RdbHelper::GetRdbStore(config, 1, callback, errCode);
    ASSERT_NE(store, nullptr);
    EXPECT_EQ(errCode, E_OK);
    RdbHelper::DeleteRdbStore(config);

    // Configure an open entry and verify GetRdb returns nullptr.
    WriteConfig(R"({"databases": {"mock_inject_store.db": {"open": {"count": 1}}}})");
    EXPECT_EQ(RdbHelper::GetRdb(config), nullptr) << "GetRdb open entry consumed";
    RemoveConfig();
}

/**
 * @tc.name: RdbFaultInject_TransactionCreateBeginFail_047
 * @tc.desc: Verify the Begin-failure branch of the decorated transaction
 *           factory (TransactionImplDecorator::Create): an invalid
 *           transaction type fails Begin with E_INVALID_ARGS and the error
 *           is surfaced with a null transaction; the store stays usable
 *           afterwards.
 * @tc.type: FUNC
 */
HWTEST_F(RdbMockInjectTest, RdbFaultInject_TransactionCreateBeginFail_047, TestSize.Level1)
{
    // No entry configured: the Begin-failure branch is injection-independent.
    RemoveConfig();
    int errCode = E_ERROR;
    auto store = OpenMockStore(errCode);
    ASSERT_NE(store, nullptr);

    auto [buttRet, buttTrans] = store->CreateTransaction(Transaction::TRANS_BUTT);
    EXPECT_EQ(buttRet, E_INVALID_ARGS) << "TRANS_BUTT fails Begin inside the factory";
    EXPECT_EQ(buttTrans, nullptr);
    auto [negRet, negTrans] = store->CreateTransaction(-1);
    EXPECT_EQ(negRet, E_INVALID_ARGS) << "negative type also rejected";
    EXPECT_EQ(negTrans, nullptr);

    // The failed creates leave the store usable.
    auto [ret, transaction] = store->CreateTransaction(Transaction::IMMEDIATE);
    EXPECT_EQ(ret, E_OK);
    ASSERT_NE(transaction, nullptr);
    EXPECT_EQ(transaction->Rollback(), E_OK);
}

/**
 * @tc.name: RdbFaultInject_TransactionQueryPassthrough_048
 * @tc.desc: Verify the passthrough branch of the transaction query wrap
 *           (TransactionImplDecorator::WrapIfConfigured): without any
 *           configured entry the result set of a transaction query is
 *           returned unwrapped and reads succeed.
 * @tc.type: FUNC
 */
HWTEST_F(RdbMockInjectTest, RdbFaultInject_TransactionQueryPassthrough_048, TestSize.Level1)
{
    RemoveConfig();
    int errCode = E_ERROR;
    auto store = OpenMockStore(errCode);
    ASSERT_NE(store, nullptr);
    ValuesBucket bucket;
    bucket.PutString("name", "alice");
    int64_t outRowId = 0;
    ASSERT_EQ(store->Insert(outRowId, "mock_test", bucket), E_OK);

    auto [ret, transaction] = store->CreateTransaction(Transaction::IMMEDIATE);
    EXPECT_EQ(ret, E_OK);
    ASSERT_NE(transaction, nullptr);
    auto resultSet = transaction->QueryByStep("SELECT id FROM mock_test");
    ASSERT_NE(resultSet, nullptr);
    int rowCount = 0;
    EXPECT_EQ(resultSet->GetRowCount(rowCount), E_OK) << "unwrapped read succeeds";
    EXPECT_EQ(rowCount, 1);
    EXPECT_EQ(resultSet->Close(), E_OK);
    EXPECT_EQ(transaction->Rollback(), E_OK);
}

/**
 * @tc.name: RdbFaultInject_StoreQueryNoEntryPassthrough_049
 * @tc.desc: Verify the no-entry passthrough branches of the store query
 *           sinks (RdbStoreDecorator::QuerySql/QueryByStep): with no
 *           configured entry (HasEntry false) the parent result sets are
 *           returned unwrapped and reads succeed end to end.
 * @tc.type: FUNC
 */
HWTEST_F(RdbMockInjectTest, RdbFaultInject_StoreQueryNoEntryPassthrough_049, TestSize.Level1)
{
    RemoveConfig();
    int errCode = E_ERROR;
    auto store = OpenMockStore(errCode);
    ASSERT_NE(store, nullptr);
    ValuesBucket bucket;
    bucket.PutString("name", "alice");
    int64_t outRowId = 0;
    ASSERT_EQ(store->Insert(outRowId, "mock_test", bucket), E_OK);

    auto sharedRs = store->QuerySql("SELECT id FROM mock_test");
    ASSERT_NE(sharedRs, nullptr);
    EXPECT_EQ(sharedRs->GoToNextRow(), E_OK) << "unwrapped shared read succeeds";
    EXPECT_EQ(sharedRs->Close(), E_OK);

    auto stepRs = store->QueryByStep("SELECT id FROM mock_test");
    ASSERT_NE(stepRs, nullptr);
    int rowCount = 0;
    EXPECT_EQ(stepRs->GetRowCount(rowCount), E_OK) << "unwrapped step read succeeds";
    EXPECT_EQ(rowCount, 1);
    EXPECT_EQ(stepRs->Close(), E_OK);
}

/**
 * @tc.name: RdbFaultInject_BackupRestoreMethodLevel_050
 * @tc.desc: Verify method-name-only injection on the file-level sinks:
 *           Backup/Restore/ForceRestore return E_SQLITE_CORRUPT without
 *           touching the database files, and converge after exhaustion.
 * @tc.type: FUNC
 */
HWTEST_F(RdbMockInjectTest, RdbFaultInject_BackupRestoreMethodLevel_050, TestSize.Level1)
{
    WriteConfig(R"({"databases": {"mock_inject_store.db": {"methods": {
        "Backup": {"count": 1}, "Restore": {"count": 1}, "ForceRestore": {"count": 1}}}}})");
    int errCode = E_ERROR;
    auto store = OpenMockStore(errCode);
    ASSERT_NE(store, nullptr);
    ValuesBucket bucket;
    bucket.PutString("name", "alice");
    int64_t outRowId = 0;
    ASSERT_EQ(store->Insert(outRowId, "mock_test", bucket), E_OK);

    const std::string backupPath = RDB_TEST_PATH + "mock_inject_backup.db";
    EXPECT_EQ(store->Backup(backupPath), E_SQLITE_CORRUPT) << "Backup hit";
    EXPECT_NE(access(backupPath.c_str(), F_OK), 0) << "the injected backup must not touch files";
    EXPECT_EQ(store->Restore(backupPath), E_SQLITE_CORRUPT) << "Restore hit";
    EXPECT_EQ(store->ForceRestore(backupPath), E_SQLITE_CORRUPT) << "ForceRestore hit";

    // Budget exhausted: the backup runs for real and produces the file.
    EXPECT_EQ(store->Backup(backupPath), E_OK) << "converges after exhaustion";
    EXPECT_EQ(access(backupPath.c_str(), F_OK), 0) << "real backup produced";
    EXPECT_EQ(remove(backupPath.c_str()), 0);
}

/**
 * @tc.name: RdbFaultInject_PerDbBudgetIsolation_051
 * @tc.desc: Verify that different database paths never share an injector:
 *           with both DBs listed in one config, direct instances keep
 *           independent budgets, and so do the per-store instances handed
 *           out by the creation helper.
 * @tc.type: FUNC
 */
HWTEST_F(RdbMockInjectTest, RdbFaultInject_PerDbBudgetIsolation_051, TestSize.Level1)
{
    WriteConfig(R"({"databases": {
        "mock_inject_a.db": {"operations": {"insert": {"count": 2}}},
        "mock_inject_b.db": {"operations": {"insert": {"count": 1}}}}})");
    RdbFaultInjector injectorA(TEST_DB_A);
    RdbFaultInjector injectorB(TEST_DB_B);
    EXPECT_EQ(CountHits(injectorA, RdbFaultOp::INSERT, 3), 2) << "A's own budget";
    EXPECT_EQ(CountHits(injectorB, RdbFaultOp::INSERT, 3), 1) << "A's consumption never bleeds into B";

    // Store level: the per-store creation helper (RdbFaultInjector::CreateOnce)
    // hands each path its own instance as well - exhausting one store's
    // budget leaves the other's untouched.
    RdbStoreConfig configA(TEST_DB_A);
    RdbStoreConfig configB(TEST_DB_B);
    MockInjectOpenCallback callback;
    int errCode = E_ERROR;
    auto storeA = RdbHelper::GetRdbStore(configA, 1, callback, errCode);
    ASSERT_NE(storeA, nullptr);
    EXPECT_EQ(errCode, E_OK);
    auto storeB = RdbHelper::GetRdbStore(configB, 1, callback, errCode);
    ASSERT_NE(storeB, nullptr);
    EXPECT_EQ(errCode, E_OK);
    ValuesBucket bucket;
    bucket.PutString("name", "alice");
    int64_t outRowId = 0;
    EXPECT_EQ(storeA->Insert(outRowId, "mock_test", bucket), E_SQLITE_CORRUPT);
    EXPECT_EQ(storeA->Insert(outRowId, "mock_test", bucket), E_SQLITE_CORRUPT);
    EXPECT_EQ(storeA->Insert(outRowId, "mock_test", bucket), E_OK) << "A exhausted";

    EXPECT_EQ(storeB->Insert(outRowId, "mock_test", bucket), E_SQLITE_CORRUPT) << "B's budget intact";
    EXPECT_EQ(storeB->Insert(outRowId, "mock_test", bucket), E_OK) << "B exhausts on its own count";

    RdbHelper::DeleteRdbStore(TEST_DB_A);
    RdbHelper::DeleteRdbStore(TEST_DB_B);
}


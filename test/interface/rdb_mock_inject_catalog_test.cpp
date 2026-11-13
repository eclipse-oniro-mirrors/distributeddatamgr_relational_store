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

// Funnel-coverage tests for the RDB fault-injection mock layer. The RdbStore
// base class funnels every public overload of a CRUD family into the final
// virtual sink that RdbStoreImpl overrides (see rdb_store.cpp), and the
// hooks sit in exactly those sinks. Each case enables one family and calls
// the full public overload spectrum, asserting that every path surfaces
// E_SQLITE_CORRUPT.

#define LOG_TAG "RdbMockInject"
#include <gtest/gtest.h>

#include <cerrno>
#include <cstdio>
#include <fstream>
#include <memory>
#include <string>
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
#include "result_set.h"
#include "transaction.h"
#include "values_bucket.h"

using namespace testing::ext;
using namespace OHOS::NativeRdb;

namespace {
constexpr const char *CATALOG_DB = "mock_inject_catalog.db";

class CatalogOpenCallback : public RdbOpenCallback {
public:
    int OnCreate(RdbStore &store) override
    {
        return store.ExecuteSql(createTable);
    }
    int OnUpgrade(RdbStore &store, int oldVersion, int newVersion) override
    {
        return E_OK;
    }
    static constexpr const char *createTable =
        "CREATE TABLE IF NOT EXISTS mock_test (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT NOT NULL)";
};

} // namespace

class RdbMockInjectCatalogTest : public testing::Test {
public:
    static void SetUpTestCase()
    {
    }
    static void TearDownTestCase()
    {
    }
    void SetUp() override
    {
        RemoveCatalogConfig();
    }
    void TearDown() override
    {
        RemoveCatalogConfig();
    }

protected:
    void WriteCatalogConfig(const std::string &content)
    {
        std::ofstream fout(RDB_TEST_PATH + "rdb_fault_inject_config.json", std::ios::trunc);
        ASSERT_TRUE(fout.is_open());
        fout << content << std::endl;
    }

    void RemoveCatalogConfig()
    {
        // Absence is not an error (the config may never have been written);
        // any other removal failure is surfaced so it is not silently lost.
        if (remove((RDB_TEST_PATH + "rdb_fault_inject_config.json").c_str()) != 0 && errno != ENOENT) {
            GTEST_LOG_(WARNING) << "remove catalog config failed, errno: " << errno;
        }
    }

    // Opens the catalog database from scratch (the config is written by the
    // caller); each case gets a fresh store and database file.
    std::shared_ptr<RdbStore> OpenCatalogStore()
    {
        RdbHelper::ClearCache();
        RdbHelper::DeleteRdbStore(RDB_TEST_PATH + CATALOG_DB);
        RdbStoreConfig config(RDB_TEST_PATH + CATALOG_DB);
        CatalogOpenCallback callback;
        int errCode = E_ERROR;
        return RdbHelper::GetRdbStore(config, 1, callback, errCode);
    }
};

/**
 * @tc.name: RdbFaultInject_InsertOverloads_001
 * @tc.desc: Verify that every public insert overload funnels into the hooked
 *           RdbStoreImpl sink and returns E_SQLITE_CORRUPT.
 * @tc.type: FUNC
 */
HWTEST_F(RdbMockInjectCatalogTest, RdbFaultInject_InsertOverloads_001, TestSize.Level2)
{
    WriteCatalogConfig(R"({"databases": {"mock_inject_catalog.db": {"operations": {"insert": {"count": 99}}}}})");
    auto store = OpenCatalogStore();
    ASSERT_NE(store, nullptr) << "open must succeed with only an insert entry";

    ValuesBucket bucket;
    bucket.PutString("name", "alice");
    RdbStore::Rows rows = { bucket, bucket };
    RdbStore::RefRows refRows;
    refRows.Put(bucket);

    int64_t outRowId = 0;
    int64_t outInsertNum = 0;
    EXPECT_EQ(store->Insert(outRowId, "mock_test", bucket), E_SQLITE_CORRUPT) << "Insert(outRowId)";
    EXPECT_EQ(
        store->InsertWithConflictResolution(outRowId, "mock_test", bucket, ConflictResolution::ON_CONFLICT_REPLACE),
        E_SQLITE_CORRUPT)
        << "InsertWithConflictResolution";
    EXPECT_EQ(store->Replace(outRowId, "mock_test", bucket), E_SQLITE_CORRUPT) << "Replace";
    EXPECT_EQ(store->Insert("mock_test", bucket).first, E_SQLITE_CORRUPT) << "Insert(resolution) pair form";
    EXPECT_EQ(store->Insert("mock_test", bucket, InsertConfig{}).first, E_SQLITE_CORRUPT)
        << "Insert(InsertConfig) pair form";
    EXPECT_EQ(store->BatchInsert(outInsertNum, "mock_test", rows), E_SQLITE_CORRUPT) << "BatchInsert(Rows)";
    EXPECT_EQ(store->BatchInsert("mock_test", refRows).first, E_SQLITE_CORRUPT) << "BatchInsert(RefRows) pair form";
    EXPECT_EQ(store->BatchInsert("mock_test", refRows, BatchInsertConfig{}).first, E_SQLITE_CORRUPT)
        << "BatchInsert(BatchInsertConfig) pair form";
}

/**
 * @tc.name: RdbFaultInject_UpdateOverloads_002
 * @tc.desc: Verify that every public update overload funnels into the hooked
 *           RdbStoreImpl sink and returns E_SQLITE_CORRUPT.
 * @tc.type: FUNC
 */
HWTEST_F(RdbMockInjectCatalogTest, RdbFaultInject_UpdateOverloads_002, TestSize.Level2)
{
    WriteCatalogConfig(R"({"databases": {"mock_inject_catalog.db": {"operations": {"update": {"count": 99}}}}})");
    auto store = OpenCatalogStore();
    ASSERT_NE(store, nullptr);

    ValuesBucket bucket;
    bucket.PutString("name", "alice");
    RdbPredicates predicates("mock_test");
    RdbStore::Olds olds;
    RdbStore::Values values;
    int changedRows = 0;
    EXPECT_EQ(store->Update(changedRows, "mock_test", bucket, "name = ?", olds), E_SQLITE_CORRUPT)
        << "Update(whereClause, Olds)";
    EXPECT_EQ(store->Update(changedRows, "mock_test", bucket, "name = ?", values), E_SQLITE_CORRUPT)
        << "Update(whereClause, Values)";
    EXPECT_EQ(store->Update(changedRows, bucket, predicates), E_SQLITE_CORRUPT) << "Update(predicates)";
    EXPECT_EQ(store->UpdateWithConflictResolution(
                  changedRows, "mock_test", bucket, "name = ?", olds, ConflictResolution::ON_CONFLICT_REPLACE),
        E_SQLITE_CORRUPT)
        << "UpdateWithConflictResolution(Olds)";
    EXPECT_EQ(store->UpdateWithConflictResolution(
                  changedRows, "mock_test", bucket, "name = ?", values, ConflictResolution::ON_CONFLICT_REPLACE),
        E_SQLITE_CORRUPT)
        << "UpdateWithConflictResolution(Values)";
    EXPECT_EQ(store->Update(bucket, predicates, UpdateConfig{}).first, E_SQLITE_CORRUPT)
        << "Update(UpdateConfig) pair form";
}

/**
 * @tc.name: RdbFaultInject_DeleteOverloads_003
 * @tc.desc: Verify that every public delete overload funnels into the hooked
 *           RdbStoreImpl sink and returns E_SQLITE_CORRUPT.
 * @tc.type: FUNC
 */
HWTEST_F(RdbMockInjectCatalogTest, RdbFaultInject_DeleteOverloads_003, TestSize.Level2)
{
    WriteCatalogConfig(R"({"databases": {"mock_inject_catalog.db": {"operations": {"delete": {"count": 99}}}}})");
    auto store = OpenCatalogStore();
    ASSERT_NE(store, nullptr);

    RdbPredicates predicates("mock_test");
    RdbStore::Olds olds;
    RdbStore::Values values;
    int deletedRows = 0;
    EXPECT_EQ(store->Delete(deletedRows, "mock_test", "name = ?", olds), E_SQLITE_CORRUPT)
        << "Delete(whereClause, Olds)";
    EXPECT_EQ(store->Delete(deletedRows, "mock_test", "name = ?", values), E_SQLITE_CORRUPT)
        << "Delete(whereClause, Values)";
    EXPECT_EQ(store->Delete(deletedRows, predicates), E_SQLITE_CORRUPT) << "Delete(predicates)";
    EXPECT_EQ(store->Delete(predicates).first, E_SQLITE_CORRUPT) << "Delete(DeleteConfig) pair form";
    EXPECT_EQ(store->Delete(predicates, ReturningConfig{}).first, E_SQLITE_CORRUPT)
        << "Delete(ReturningConfig) pair form";
}

/**
 * @tc.name: RdbFaultInject_QueryOverloads_004
 * @tc.desc: Verify that every public query overload funnels into the hooked
 *           QuerySql/QueryByStep sinks and returns a null result set (Count
 *           returns the corruption error).
 * @tc.type: FUNC
 */
HWTEST_F(RdbMockInjectCatalogTest, RdbFaultInject_QueryOverloads_004, TestSize.Level2)
{
    WriteCatalogConfig(R"({"databases": {"mock_inject_catalog.db": {"operations": {"query": {"count": 99}}}}})");
    auto store = OpenCatalogStore();
    ASSERT_NE(store, nullptr);

    RdbPredicates predicates("mock_test");
    RdbStore::Fields columns = { "id" };
    RdbStore::Olds olds;
    RdbStore::Values values;
    int queryErr = E_OK;
    int64_t count = 0;
    EXPECT_EQ(store->Query(queryErr, false, "mock_test", columns, "name = ?", values, "", "", "", 0, 0), nullptr)
        << "Query(distinct form)";
    EXPECT_EQ(store->Query(predicates, columns), nullptr) << "Query(predicates)";
    EXPECT_EQ(store->QuerySql("SELECT id FROM mock_test", olds), nullptr) << "QuerySql(Olds)";
    EXPECT_EQ(store->QuerySql("SELECT id FROM mock_test", values), nullptr) << "QuerySql(Values)";
    EXPECT_EQ(store->QuerySql("SELECT id FROM mock_test", values, QueryConfig{}), nullptr) << "QuerySql(QueryConfig)";
    EXPECT_EQ(store->QueryByStep("SELECT id FROM mock_test", olds), nullptr) << "QueryByStep(Olds)";
    EXPECT_EQ(store->QueryByStep(predicates, columns, false), nullptr) << "QueryByStep(predicates, preCount)";
    EXPECT_EQ(store->QueryByStep("SELECT id FROM mock_test", values, false), nullptr) << "QueryByStep(sql, preCount)";
    EXPECT_EQ(store->QueryByStep(predicates, columns, OHOS::DistributedRdb::QueryOptions{}), nullptr)
        << "QueryByStep(predicates, options)";
    EXPECT_EQ(store->QueryByStep("SELECT id FROM mock_test", values, OHOS::DistributedRdb::QueryOptions{}), nullptr)
        << "QueryByStep(sql, options)";
    EXPECT_EQ(
        store->QueryByStep("SELECT id FROM mock_test", values, OHOS::DistributedRdb::QueryOptions{}, QueryConfig{}),
        nullptr)
        << "QueryByStep(sql, options, config)";
    EXPECT_EQ(store->Count(count, predicates), E_SQLITE_CORRUPT) << "Count funnels into ExecuteAndGetLong";
}

/**
 * @tc.name: RdbFaultInject_ExecuteOverloads_005
 * @tc.desc: Verify that every public execute overload funnels into the
 *           hooked execute sinks and returns E_SQLITE_CORRUPT.
 * @tc.type: FUNC
 */
HWTEST_F(RdbMockInjectCatalogTest, RdbFaultInject_ExecuteOverloads_005, TestSize.Level2)
{
    WriteCatalogConfig(R"({"databases": {"mock_inject_catalog.db": {"operations": {"executeSql": {"count": 99}}}}})");
    auto store = OpenCatalogStore();
    ASSERT_NE(store, nullptr);

    RdbStore::Values values;
    int64_t outLong = 0;
    std::string outString;
    EXPECT_EQ(store->ExecuteSql("INSERT INTO mock_test (name) VALUES ('x')", values), E_SQLITE_CORRUPT) << "ExecuteSql";
    EXPECT_EQ(store->Execute("INSERT INTO mock_test (name) VALUES ('x')", values).first, E_SQLITE_CORRUPT)
        << "Execute pair form";
    EXPECT_EQ(store->Execute("INSERT INTO mock_test (name) VALUES ('x')", values, 0).first, E_SQLITE_CORRUPT)
        << "Execute(trxId) pair form";
    EXPECT_EQ(store->ExecuteExt("SELECT id FROM mock_test", values).first, E_SQLITE_CORRUPT) << "ExecuteExt";
    EXPECT_EQ(store->ExecuteAndGetLong(outLong, "SELECT COUNT(*) FROM mock_test", values), E_SQLITE_CORRUPT)
        << "ExecuteAndGetLong";
    EXPECT_EQ(store->ExecuteAndGetString(outString, "SELECT COUNT(*) FROM mock_test", values), E_SQLITE_CORRUPT)
        << "ExecuteAndGetString";
    EXPECT_EQ(store->ExecuteForLastInsertedRowId(outLong, "INSERT INTO mock_test (name) VALUES ('x')", values),
        E_SQLITE_CORRUPT)
        << "ExecuteForLastInsertedRowId";
    EXPECT_EQ(store->ExecuteForChangedRowCount(outLong, "UPDATE mock_test SET name = 'y'", values), E_SQLITE_CORRUPT)
        << "ExecuteForChangedRowCount";
}

/**
 * @tc.name: RdbFaultInject_TransactionOverloads_006
 * @tc.desc: Verify that every store-level transaction entry point (new and
 *           legacy) is hooked and returns E_SQLITE_CORRUPT.
 * @tc.type: FUNC
 */
HWTEST_F(RdbMockInjectCatalogTest, RdbFaultInject_TransactionOverloads_006, TestSize.Level2)
{
    WriteCatalogConfig(R"({"databases": {"mock_inject_catalog.db": {"operations": {"transaction": {"count": 99}}}}})");
    auto store = OpenCatalogStore();
    ASSERT_NE(store, nullptr);

    auto [createRet, transaction] = store->CreateTransaction(Transaction::IMMEDIATE);
    EXPECT_EQ(createRet, E_SQLITE_CORRUPT) << "CreateTransaction injected";
    EXPECT_EQ(transaction, nullptr);
    // The hooks fire before any state validation, so the legacy entry points
    // report the corruption error even without an active transaction.
    EXPECT_EQ(store->BeginTransaction(), E_SQLITE_CORRUPT) << "BeginTransaction";
    EXPECT_EQ(store->BeginTrans().first, E_SQLITE_CORRUPT) << "BeginTrans";
    EXPECT_EQ(store->Commit(), E_SQLITE_CORRUPT) << "Commit";
    EXPECT_EQ(store->Commit(0), E_SQLITE_CORRUPT) << "Commit(trxId)";
    EXPECT_EQ(store->RollBack(), E_SQLITE_CORRUPT) << "RollBack";
    EXPECT_EQ(store->RollBack(0), E_SQLITE_CORRUPT) << "RollBack(trxId)";
}

/**
 * @tc.name: RdbFaultInject_TransactionCrudOverloads_007
 * @tc.desc: Verify that the CRUD families are also injected when issued
 *           through a Transaction object (the TransDB facade inside
 *           TransactionImpl is hooked).
 * @tc.type: FUNC
 */
HWTEST_F(RdbMockInjectCatalogTest, RdbFaultInject_TransactionCrudOverloads_007, TestSize.Level2)
{
    WriteCatalogConfig(R"({"databases": {"mock_inject_catalog.db": {"operations": {
        "insert": {"count": 99}, "update": {"count": 99}, "delete": {"count": 99},
        "query": {"count": 99}, "executeSql": {"count": 99}}}}})");
    auto store = OpenCatalogStore();
    ASSERT_NE(store, nullptr);

    // The transaction family is not configured, so the transaction itself
    // can be created; only its data operations are injected.
    auto [createRet, transaction] = store->CreateTransaction(Transaction::IMMEDIATE);
    EXPECT_EQ(createRet, E_OK);
    ASSERT_NE(transaction, nullptr);

    Transaction::Row row;
    row.PutString("name", "dave");
    RdbPredicates predicates("mock_test");
    RdbStore::Values values;
    EXPECT_EQ(transaction->Insert("mock_test", row).first, E_SQLITE_CORRUPT) << "transaction Insert";
    EXPECT_EQ(transaction->Insert("mock_test", row, InsertConfig{}).first, E_SQLITE_CORRUPT)
        << "transaction Insert(InsertConfig)";
    RdbStore::Rows rows = { row };
    EXPECT_EQ(transaction->BatchInsert("mock_test", rows).first, E_SQLITE_CORRUPT) << "transaction BatchInsert(Rows)";
    RdbStore::RefRows refRows;
    refRows.Put(row);
    EXPECT_EQ(transaction->BatchInsert("mock_test", refRows).first, E_SQLITE_CORRUPT)
        << "transaction BatchInsert(RefRows)";
    EXPECT_EQ(transaction->BatchInsert("mock_test", refRows, BatchInsertConfig{}).first, E_SQLITE_CORRUPT)
        << "transaction BatchInsert(BatchInsertConfig)";
    EXPECT_EQ(transaction->Update(row, predicates, UpdateConfig{}).first, E_SQLITE_CORRUPT) << "transaction Update";
    EXPECT_EQ(transaction->Delete(predicates, DeleteConfig{}).first, E_SQLITE_CORRUPT) << "transaction Delete";
    EXPECT_EQ(transaction->QueryByStep("SELECT id FROM mock_test", values), nullptr) << "transaction QueryByStep(sql)";
    EXPECT_EQ(transaction->QueryByStep(predicates, {}, OHOS::DistributedRdb::QueryOptions{}), nullptr)
        << "transaction QueryByStep(predicates)";
    EXPECT_EQ(transaction->Execute("INSERT INTO mock_test (name) VALUES ('x')", values).first, E_SQLITE_CORRUPT)
        << "transaction Execute";
    EXPECT_EQ(transaction->ExecuteExt("SELECT id FROM mock_test", values).first, E_SQLITE_CORRUPT)
        << "transaction ExecuteExt";
    EXPECT_EQ(transaction->Rollback(), E_OK) << "rollback still converges (family not configured)";
}

/**
 * @tc.name: RdbFaultInject_ResultSetReadSharedOverloads_008
 * @tc.desc: Verify that a result set returned by QuerySql is wrapped and
 *           its read methods report E_SQLITE_CORRUPT while Close stays
 *           available.
 * @tc.type: FUNC
 */
HWTEST_F(RdbMockInjectCatalogTest, RdbFaultInject_ResultSetReadSharedOverloads_008, TestSize.Level2)
{
    WriteCatalogConfig(
        R"({"databases": {"mock_inject_catalog.db": {"operations": {"resultSetRead": {"count": 99}}}}})");
    auto store = OpenCatalogStore();
    ASSERT_NE(store, nullptr);

    auto sharedRs = store->QuerySql("SELECT id FROM mock_test");
    ASSERT_NE(sharedRs, nullptr);
    int count = 0;
    int64_t longValue = 0;
    double doubleValue = 0.0;
    std::string stringValue;
    std::vector<uint8_t> blobValue;
    bool flag = false;
    int position = 0;
    std::vector<std::string> columnNames;
    ColumnType columnType = ColumnType::TYPE_NULL;
    EXPECT_EQ(sharedRs->GoTo(0), E_SQLITE_CORRUPT) << "GoTo";
    EXPECT_EQ(sharedRs->GoToRow(0), E_SQLITE_CORRUPT) << "GoToRow";
    EXPECT_EQ(sharedRs->GoToFirstRow(), E_SQLITE_CORRUPT) << "GoToFirstRow";
    EXPECT_EQ(sharedRs->GoToLastRow(), E_SQLITE_CORRUPT) << "GoToLastRow";
    EXPECT_EQ(sharedRs->GoToNextRow(), E_SQLITE_CORRUPT) << "GoToNextRow";
    EXPECT_EQ(sharedRs->GoToPreviousRow(), E_SQLITE_CORRUPT) << "GoToPreviousRow";
    EXPECT_EQ(sharedRs->GetAllColumnNames(columnNames), E_SQLITE_CORRUPT) << "GetAllColumnNames";
    EXPECT_EQ(sharedRs->GetColumnCount(count), E_SQLITE_CORRUPT) << "GetColumnCount";
    EXPECT_EQ(sharedRs->GetRowCount(count), E_SQLITE_CORRUPT) << "GetRowCount";
    EXPECT_EQ(sharedRs->GetRowIndex(position), E_SQLITE_CORRUPT) << "GetRowIndex";
    EXPECT_EQ(sharedRs->GetColumnType(0, columnType), E_SQLITE_CORRUPT) << "GetColumnType";
    EXPECT_EQ(sharedRs->GetInt(0, count), E_SQLITE_CORRUPT) << "GetInt";
    EXPECT_EQ(sharedRs->GetLong(0, longValue), E_SQLITE_CORRUPT) << "GetLong";
    EXPECT_EQ(sharedRs->GetString(0, stringValue), E_SQLITE_CORRUPT) << "GetString";
    EXPECT_EQ(sharedRs->GetDouble(0, doubleValue), E_SQLITE_CORRUPT) << "GetDouble";
    EXPECT_EQ(sharedRs->GetBlob(0, blobValue), E_SQLITE_CORRUPT) << "GetBlob";
    EXPECT_EQ(sharedRs->IsColumnNull(0, flag), E_SQLITE_CORRUPT) << "IsColumnNull";
    EXPECT_EQ(sharedRs->IsEnded(flag), E_SQLITE_CORRUPT) << "IsEnded";
    EXPECT_EQ(sharedRs->IsStarted(flag), E_SQLITE_CORRUPT) << "IsStarted";
    EXPECT_EQ(sharedRs->IsAtFirstRow(flag), E_SQLITE_CORRUPT) << "IsAtFirstRow";
    EXPECT_EQ(sharedRs->IsAtLastRow(flag), E_SQLITE_CORRUPT) << "IsAtLastRow";
    ValueObject valueObject;
    EXPECT_EQ(sharedRs->Get(0, valueObject), E_SQLITE_CORRUPT) << "Get(ValueObject)";
    EXPECT_EQ(sharedRs->Close(), E_OK) << "Close excluded from the read family";
}

/**
 * @tc.name: RdbFaultInject_ResultSetReadSharedExtraOverloads_018
 * @tc.desc: Verify the remaining result-set overrides (GetWholeColumnNames,
 *           GetColumnIndex, GetColumnName, IsClosed, GetAsset, GetAssets,
 *           GetFloat32Array, GetRow, GetRowData, GetRowsData, GetSize) report
 *           E_SQLITE_CORRUPT while Close stays available.
 * @tc.type: FUNC
 */
HWTEST_F(RdbMockInjectCatalogTest, RdbFaultInject_ResultSetReadSharedExtraOverloads_018, TestSize.Level2)
{
    WriteCatalogConfig(
        R"({"databases": {"mock_inject_catalog.db": {"operations": {"resultSetRead": {"count": 99}}}}})");
    auto store = OpenCatalogStore();
    ASSERT_NE(store, nullptr);

    auto sharedRs = store->QuerySql("SELECT id FROM mock_test");
    ASSERT_NE(sharedRs, nullptr);
    auto wholeNames = sharedRs->GetWholeColumnNames();
    EXPECT_EQ(wholeNames.first, E_SQLITE_CORRUPT) << "GetWholeColumnNames";
    int columnIndex = 0;
    EXPECT_EQ(sharedRs->GetColumnIndex("name", columnIndex), E_SQLITE_CORRUPT) << "GetColumnIndex";
    std::string columnName;
    EXPECT_EQ(sharedRs->GetColumnName(0, columnName), E_SQLITE_CORRUPT) << "GetColumnName";
    EXPECT_TRUE(sharedRs->IsClosed()) << "IsClosed returns true on injection";
    ValueObject::Asset asset;
    EXPECT_EQ(sharedRs->GetAsset(0, asset), E_SQLITE_CORRUPT) << "GetAsset";
    ValueObject::Assets assets;
    EXPECT_EQ(sharedRs->GetAssets(0, assets), E_SQLITE_CORRUPT) << "GetAssets";
    ValueObject::FloatVector floatVec;
    EXPECT_EQ(sharedRs->GetFloat32Array(0, floatVec), E_SQLITE_CORRUPT) << "GetFloat32Array";
    RowEntity rowEntity;
    EXPECT_EQ(sharedRs->GetRow(rowEntity), E_SQLITE_CORRUPT) << "GetRow";
    auto rowData = sharedRs->GetRowData();
    EXPECT_EQ(rowData.first, E_SQLITE_CORRUPT) << "GetRowData";
    auto rowsData = sharedRs->GetRowsData(10, 0);
    EXPECT_EQ(rowsData.first, E_SQLITE_CORRUPT) << "GetRowsData";
    size_t columnSize = 0;
    EXPECT_EQ(sharedRs->GetSize(0, columnSize), E_SQLITE_CORRUPT) << "GetSize";
    EXPECT_EQ(sharedRs->Close(), E_OK) << "Close excluded from the read family";
}

/**
 * @tc.name: RdbFaultInject_ResultSetReadStepOverloads_009
 * @tc.desc: Verify that a step result set returned by QueryByStep is
 *           wrapped and its read methods report E_SQLITE_CORRUPT while
 *           Close stays available.
 * @tc.type: FUNC
 */
HWTEST_F(RdbMockInjectCatalogTest, RdbFaultInject_ResultSetReadStepOverloads_009, TestSize.Level2)
{
    WriteCatalogConfig(
        R"({"databases": {"mock_inject_catalog.db": {"operations": {"resultSetRead": {"count": 99}}}}})");
    auto store = OpenCatalogStore();
    ASSERT_NE(store, nullptr);

    auto stepRs = store->QueryByStep("SELECT id FROM mock_test");
    ASSERT_NE(stepRs, nullptr);
    int count = 0;
    int64_t longValue = 0;
    double doubleValue = 0.0;
    std::string stringValue;
    std::vector<uint8_t> blobValue;
    bool flag = false;
    EXPECT_EQ(stepRs->GoTo(0), E_SQLITE_CORRUPT) << "GoTo";
    EXPECT_EQ(stepRs->GoToNextRow(), E_SQLITE_CORRUPT) << "GoToNextRow";
    EXPECT_EQ(stepRs->GetColumnCount(count), E_SQLITE_CORRUPT) << "GetColumnCount";
    EXPECT_EQ(stepRs->GetRowCount(count), E_SQLITE_CORRUPT) << "GetRowCount";
    EXPECT_EQ(stepRs->GetInt(0, count), E_SQLITE_CORRUPT) << "GetInt";
    EXPECT_EQ(stepRs->GetLong(0, longValue), E_SQLITE_CORRUPT) << "GetLong";
    EXPECT_EQ(stepRs->GetString(0, stringValue), E_SQLITE_CORRUPT) << "GetString";
    EXPECT_EQ(stepRs->GetDouble(0, doubleValue), E_SQLITE_CORRUPT) << "GetDouble";
    EXPECT_EQ(stepRs->GetBlob(0, blobValue), E_SQLITE_CORRUPT) << "GetBlob";
    EXPECT_EQ(stepRs->IsColumnNull(0, flag), E_SQLITE_CORRUPT) << "IsColumnNull";
    EXPECT_EQ(stepRs->IsEnded(flag), E_SQLITE_CORRUPT) << "IsEnded";
    EXPECT_EQ(stepRs->Close(), E_OK) << "Close excluded from the read family";
}

/**
 * @tc.name: RdbFaultInject_ResultSetReadTransactionOverloads_010
 * @tc.desc: Verify that a result set returned by Transaction::QueryByStep
 *           is wrapped and its read methods report E_SQLITE_CORRUPT while
 *           Close stays available.
 * @tc.type: FUNC
 */
HWTEST_F(RdbMockInjectCatalogTest, RdbFaultInject_ResultSetReadTransactionOverloads_010, TestSize.Level2)
{
    WriteCatalogConfig(
        R"({"databases": {"mock_inject_catalog.db": {"operations": {"resultSetRead": {"count": 99}}}}})");
    auto store = OpenCatalogStore();
    ASSERT_NE(store, nullptr);

    auto [createRet, transaction] = store->CreateTransaction(Transaction::IMMEDIATE);
    EXPECT_EQ(createRet, E_OK);
    ASSERT_NE(transaction, nullptr);
    auto transRs = transaction->QueryByStep("SELECT id FROM mock_test", {});
    ASSERT_NE(transRs, nullptr);
    int count = 0;
    int64_t longValue = 0;
    std::string stringValue;
    bool flag = false;
    EXPECT_EQ(transRs->GoTo(0), E_SQLITE_CORRUPT) << "GoTo";
    EXPECT_EQ(transRs->GoToNextRow(), E_SQLITE_CORRUPT) << "GoToNextRow";
    EXPECT_EQ(transRs->GetColumnCount(count), E_SQLITE_CORRUPT) << "GetColumnCount";
    EXPECT_EQ(transRs->GetRowCount(count), E_SQLITE_CORRUPT) << "GetRowCount";
    EXPECT_EQ(transRs->GetInt(0, count), E_SQLITE_CORRUPT) << "GetInt";
    EXPECT_EQ(transRs->GetLong(0, longValue), E_SQLITE_CORRUPT) << "GetLong";
    EXPECT_EQ(transRs->GetString(0, stringValue), E_SQLITE_CORRUPT) << "GetString";
    EXPECT_EQ(transRs->IsColumnNull(0, flag), E_SQLITE_CORRUPT) << "IsColumnNull";
    EXPECT_EQ(transRs->IsEnded(flag), E_SQLITE_CORRUPT) << "IsEnded";
    EXPECT_EQ(transRs->Close(), E_OK) << "Close excluded from the read family";
    EXPECT_EQ(transaction->Rollback(), E_OK);
}

/**
 * @tc.name: RdbFaultInject_StoreQueryByStepSqlInjection_011
 * @tc.desc: Verify that the store QueryByStep(sql) sink injects
 *           E_SQLITE_CORRUPT when the query family is configured.
 * @tc.type: FUNC
 */
HWTEST_F(RdbMockInjectCatalogTest, RdbFaultInject_StoreQueryByStepSqlInjection_011, TestSize.Level1)
{
    WriteCatalogConfig(R"({"databases": {"mock_inject_catalog.db": {"operations": {"query": {"count": 99}}}}})");
    auto store = OpenCatalogStore();
    ASSERT_NE(store, nullptr);

    auto resultSet = store->QueryByStep("SELECT id FROM mock_test");
    EXPECT_EQ(resultSet, nullptr) << "QueryByStep(sql) returns nullptr on injection";
}

/**
 * @tc.name: RdbFaultInject_StoreQueryByStepPredicatesInjection_012
 * @tc.desc: Verify that the store QueryByStep(predicates) sink injects
 *           E_SQLITE_CORRUPT when the query family is configured.
 * @tc.type: FUNC
 */
HWTEST_F(RdbMockInjectCatalogTest, RdbFaultInject_StoreQueryByStepPredicatesInjection_012, TestSize.Level1)
{
    WriteCatalogConfig(R"({"databases": {"mock_inject_catalog.db": {"operations": {"query": {"count": 99}}}}})");
    auto store = OpenCatalogStore();
    ASSERT_NE(store, nullptr);

    RdbPredicates predicates("mock_test");
    auto resultSet = store->QueryByStep(predicates);
    EXPECT_EQ(resultSet, nullptr) << "QueryByStep(predicates) returns nullptr on injection";
}

/**
 * @tc.name: RdbFaultInject_StoreExecuteForLastInsertedRowIdInjection_013
 * @tc.desc: Verify that the ExecuteForLastInsertedRowId sink injects
 *           E_SQLITE_CORRUPT when the executeSql family is configured.
 * @tc.type: FUNC
 */
HWTEST_F(RdbMockInjectCatalogTest, RdbFaultInject_StoreExecuteForLastInsertedRowIdInjection_013, TestSize.Level1)
{
    WriteCatalogConfig(R"({"databases": {"mock_inject_catalog.db": {"operations": {"executeSql": {"count": 99}}}}})");
    auto store = OpenCatalogStore();
    ASSERT_NE(store, nullptr);

    int64_t outRowId = 0;
    EXPECT_EQ(store->ExecuteForLastInsertedRowId(
                  outRowId, "INSERT INTO mock_test (name) VALUES ('inject')", {}),
        E_SQLITE_CORRUPT) << "ExecuteForLastInsertedRowId injected";
}

/**
 * @tc.name: RdbFaultInject_TransactionQueryByStepSqlInjection_014
 * @tc.desc: Verify that the transaction QueryByStep(sql) sink injects
 *           E_SQLITE_CORRUPT when the query family is configured.
 * @tc.type: FUNC
 */
HWTEST_F(RdbMockInjectCatalogTest, RdbFaultInject_TransactionQueryByStepSqlInjection_014, TestSize.Level2)
{
    WriteCatalogConfig(R"({"databases": {"mock_inject_catalog.db": {"operations": {"query": {"count": 99}}}}})");
    auto store = OpenCatalogStore();
    ASSERT_NE(store, nullptr);

    auto [createRet, transaction] = store->CreateTransaction(Transaction::IMMEDIATE);
    EXPECT_EQ(createRet, E_OK);
    ASSERT_NE(transaction, nullptr);
    auto resultSet = transaction->QueryByStep("SELECT id FROM mock_test");
    EXPECT_EQ(resultSet, nullptr) << "transaction QueryByStep(sql) returns nullptr on injection";
}

/**
 * @tc.name: RdbFaultInject_TransactionQueryByStepPredicatesInjection_015
 * @tc.desc: Verify that the transaction QueryByStep(predicates) sink injects
 *           E_SQLITE_CORRUPT when the query family is configured.
 * @tc.type: FUNC
 */
HWTEST_F(RdbMockInjectCatalogTest, RdbFaultInject_TransactionQueryByStepPredicatesInjection_015, TestSize.Level2)
{
    WriteCatalogConfig(R"({"databases": {"mock_inject_catalog.db": {"operations": {"query": {"count": 99}}}}})");
    auto store = OpenCatalogStore();
    ASSERT_NE(store, nullptr);

    auto [createRet, transaction] = store->CreateTransaction(Transaction::IMMEDIATE);
    EXPECT_EQ(createRet, E_OK);
    ASSERT_NE(transaction, nullptr);
    RdbPredicates predicates("mock_test");
    auto resultSet = transaction->QueryByStep(predicates);
    EXPECT_EQ(resultSet, nullptr) << "transaction QueryByStep(predicates) returns nullptr on injection";
}

/**
 * @tc.name: RdbFaultInject_TransactionRollbackInjection_016
 * @tc.desc: Verify that the transaction Rollback sink injects
 *           E_SQLITE_CORRUPT when the transaction family is configured.
 * @tc.type: FUNC
 */
HWTEST_F(RdbMockInjectCatalogTest, RdbFaultInject_TransactionRollbackInjection_016, TestSize.Level1)
{
    // Target Rollback by method name so CreateTransaction still succeeds.
    WriteCatalogConfig(
        R"({"databases": {"mock_inject_catalog.db": {"methods": {"Rollback": {"count": 99}}}}})");
    auto store = OpenCatalogStore();
    ASSERT_NE(store, nullptr);

    auto [createRet, transaction] = store->CreateTransaction(Transaction::IMMEDIATE);
    EXPECT_EQ(createRet, E_OK);
    ASSERT_NE(transaction, nullptr);
    EXPECT_EQ(transaction->Rollback(), E_SQLITE_CORRUPT) << "Rollback injected";
}

/**
 * @tc.name: RdbFaultInject_TransactionCloseInjection_017
 * @tc.desc: Verify that the transaction Close sink injects
 *           E_SQLITE_CORRUPT when the transaction family is configured.
 * @tc.type: FUNC
 */
HWTEST_F(RdbMockInjectCatalogTest, RdbFaultInject_TransactionCloseInjection_017, TestSize.Level1)
{
    // Target Close by method name so CreateTransaction still succeeds.
    WriteCatalogConfig(
        R"({"databases": {"mock_inject_catalog.db": {"methods": {"Close": {"count": 99}}}}})");
    auto store = OpenCatalogStore();
    ASSERT_NE(store, nullptr);

    auto [createRet, transaction] = store->CreateTransaction(Transaction::IMMEDIATE);
    EXPECT_EQ(createRet, E_OK);
    ASSERT_NE(transaction, nullptr);
    EXPECT_EQ(transaction->Close(), E_SQLITE_CORRUPT) << "Close injected";
}


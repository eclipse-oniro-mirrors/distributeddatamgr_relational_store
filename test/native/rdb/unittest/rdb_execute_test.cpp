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

#include <gtest/gtest.h>

#include <string>

#include "common.h"
#include "rdb_errno.h"
#include "rdb_helper.h"
#include "rdb_open_callback.h"
#include "value_object.h"

using namespace testing::ext;
using namespace OHOS::NativeRdb;
namespace OHOS::RdbExecuteTest {
struct RdbTestParam {
    std::shared_ptr<RdbStore> store;
    std::string mode;
    operator std::shared_ptr<RdbStore>()
    {
        return store;
    }
};
static RdbTestParam g_store;
static RdbTestParam g_memDb;

class RdbExecuteTest : public testing::TestWithParam<RdbTestParam *> {
public:
    static void SetUpTestCase(void);
    static void TearDownTestCase(void);
    void SetUp();
    void TearDown();

    std::shared_ptr<RdbStore> store_;
    static const std::string DATABASE_NAME;
};
constexpr const char *CREATE_TABLE_TEST = "CREATE TABLE IF NOT EXISTS test "
                                    "(id INTEGER PRIMARY KEY AUTOINCREMENT, "
                                    "name TEXT NOT NULL, age INTEGER, salary REAL, "
                                    "blobType BLOB)";
const std::string RdbExecuteTest::DATABASE_NAME = RDB_TEST_PATH + "execute_test.db";

class ExecuteTestOpenCallback : public RdbOpenCallback {
public:
    int OnCreate(RdbStore &store) override;
    int OnUpgrade(RdbStore &store, int oldVersion, int newVersion) override;
};

int ExecuteTestOpenCallback::OnCreate(RdbStore &store)
{
    return E_OK;
}

int ExecuteTestOpenCallback::OnUpgrade(RdbStore &store, int oldVersion, int newVersion)
{
    return E_OK;
}

void RdbExecuteTest::SetUpTestCase(void)
{
    int errCode = E_OK;
    RdbHelper::DeleteRdbStore(DATABASE_NAME);
    RdbStoreConfig config(RdbExecuteTest::DATABASE_NAME);
    ExecuteTestOpenCallback helper;
    g_store.store = RdbHelper::GetRdbStore(config, 1, helper, errCode);
    ASSERT_NE(g_store.store, nullptr);
    g_store.mode = "wal";

    config.SetStorageMode(StorageMode::MODE_MEMORY);
    g_memDb.store = RdbHelper::GetRdbStore(config, 1, helper, errCode);
    ASSERT_NE(g_memDb.store, nullptr);
    g_memDb.mode = "memory";
}

void RdbExecuteTest::TearDownTestCase(void)
{
    RdbStoreConfig config(RdbExecuteTest::DATABASE_NAME);
    RdbHelper::DeleteRdbStore(config);
    config.SetStorageMode(StorageMode::MODE_MEMORY);
    RdbHelper::DeleteRdbStore(config);
}

void RdbExecuteTest::SetUp(void)
{
    store_ = *GetParam();
    ASSERT_NE(store_, nullptr);
    store_->ExecuteSql(CREATE_TABLE_TEST);
}

void RdbExecuteTest::TearDown(void)
{
    store_ = *GetParam();
    ASSERT_NE(store_, nullptr);
    store_->ExecuteSql("DROP TABLE test");
}

/**
 * @tc.name: RdbStore_Execute_001
 * @tc.desc: test RdbStore Execute
 * @tc.type: FUNC
 */
HWTEST_P(RdbExecuteTest, RdbStore_Execute_001, TestSize.Level1)
{
    int64_t id;
    ValuesBucket values;

    values.PutInt("id", 1);
    values.PutString("name", std::string("zhangsan"));
    values.PutInt("age", 18);
    values.PutDouble("salary", 100.5);
    values.PutBlob("blobType", std::vector<uint8_t>{ 1, 2, 3 });
    int ret = store_->Insert(id, "test", values);
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(1, id);

    values.Clear();
    values.PutInt("id", 2);
    values.PutString("name", std::string("lisi"));
    values.PutInt("age", 19);
    values.PutDouble("salary", 200.5);
    values.PutBlob("blobType", std::vector<uint8_t>{ 4, 5, 6 });
    ret = store_->Insert(id, "test", values);
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(2, id);

    values.Clear();
    values.PutInt("id", 3);
    values.PutString("name", std::string("wangyjing"));
    values.PutInt("age", 20);
    values.PutDouble("salary", 300.5);
    values.PutBlob("blobType", std::vector<uint8_t>{ 7, 8, 9 });
    ret = store_->Insert(id, "test", values);
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(3, id);

    int64_t count;
    ret = store_->ExecuteAndGetLong(count, "SELECT COUNT(*) FROM test");
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(count, 3);

    ret = store_->ExecuteSql("DELETE FROM test WHERE age = ? OR age = ?",
        std::vector<ValueObject>{ ValueObject(std::string("18")), ValueObject(std ::string("20")) });
    EXPECT_EQ(ret, E_OK);

    ret = store_->ExecuteAndGetLong(count, "SELECT COUNT(*) FROM test where age = 19");
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(count, 1);

    ret = store_->ExecuteSql("DELETE FROM test WHERE age = 19");
    EXPECT_EQ(ret, E_OK);

    ret = store_->ExecuteAndGetLong(count, "SELECT COUNT(*) FROM test");
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(count, 0);
}

/**
 * @tc.name: RdbStore_Execute_002
 * @tc.desc: test RdbStore Execute
 * @tc.type: FUNC
 */
HWTEST_P(RdbExecuteTest, RdbStore_Execute_002, TestSize.Level1)
{
    int64_t id;
    ValuesBucket values;

    int ret = store_->Insert(id, "test", UTUtils::SetRowData(UTUtils::g_rowData[0]));
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(1, id);

    ret = store_->Insert(id, "test", UTUtils::SetRowData(UTUtils::g_rowData[1]));
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(2, id);

    ret = store_->Insert(id, "test", UTUtils::SetRowData(UTUtils::g_rowData[2]));
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(3, id);

    int64_t count;
    ret = store_->ExecuteAndGetLong(count, "SELECT COUNT(*) FROM test", std::vector<ValueObject>());
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(count, 3);

    ret = store_->ExecuteSql("DELETE FROM test WHERE age = ? OR age = ?",
        std::vector<ValueObject>{ ValueObject(std::string("18")), ValueObject(std ::string("20")) });
    EXPECT_EQ(ret, E_OK);

    ret = store_->ExecuteAndGetLong(
        count, "SELECT COUNT(*) FROM test where age = ?", std::vector<ValueObject>{ ValueObject(std::string("19")) });
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(count, 1);

    ret = store_->ExecuteSql("DELETE FROM test WHERE age = 19");
    EXPECT_EQ(ret, E_OK);

    ret = store_->ExecuteAndGetLong(count, "SELECT COUNT(*) FROM test", std::vector<ValueObject>());
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(count, 0);

    ret = store_->ExecuteSql("DROP TABLE IF EXISTS test");
    EXPECT_EQ(ret, E_OK);

    ret = store_->ExecuteAndGetLong(count, "SELECT COUNT(*) FROM test");
    EXPECT_EQ(ret, E_SQLITE_ERROR);
}

/**
 * @tc.name: RdbStore_Execute_003
 * @tc.desc: test RdbStore Execute
 * @tc.type: FUNC
 */
HWTEST_P(RdbExecuteTest, RdbStore_Execute_003, TestSize.Level1)
{
    int64_t pageSize;
    int ret = store_->ExecuteAndGetLong(pageSize, "PRAGMA page_size");
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(pageSize, 4096);

    int64_t journalSize;
    ret = store_->ExecuteAndGetLong(journalSize, "PRAGMA journal_size_limit");
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(journalSize, 1048576);

    std::string journalMode;
    ret = store_->ExecuteAndGetString(journalMode, "PRAGMA journal_mode");
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(journalMode, GetParam()->mode);
}

/**
 * @tc.name: RdbStore_Execute_004
 * @tc.desc: Abnormal testCase for ExecuteAndGetString, if sqlstatementtype is special
 * @tc.type: FUNC
 */
HWTEST_P(RdbExecuteTest, RdbStore_Execute_004, TestSize.Level4)
{
    std::shared_ptr<RdbStore> store = *GetParam();

    std::string outValue;
    int ret = store_->ExecuteAndGetString(outValue, "BEGIN;");
    EXPECT_NE(E_OK, ret);
}

/**
 * @tc.name: RdbStore_Execute_005
 * @tc.desc: Abnormal testCase for ExecuteForLastInsertedRowId, if sql is invalid
 * @tc.type: FUNC
 * @tc.type: FUNC
 */
HWTEST_P(RdbExecuteTest, RdbStore_Execute_005, TestSize.Level4)
{
    int64_t outValue;
    int ret = store_->ExecuteForLastInsertedRowId(outValue, "", {});
    EXPECT_NE(E_OK, ret);
}

/**
 * @tc.name: RdbStore_Execute_006
 * @tc.desc: Abnormal testCase for ExecuteForChangedRowCount, if sql is invalid
 * @tc.type: FUNC
 */
HWTEST_P(RdbExecuteTest, RdbStore_Execute_006, TestSize.Level4)
{
    int64_t outValue;
    int ret = store_->ExecuteForChangedRowCount(outValue, "", {});
    EXPECT_NE(E_OK, ret);
}

/**
 * @tc.name: RdbStore_Execute_007
 * @tc.desc: Normal testCase for ExecuteAndGetString, check integrity for store
 * @tc.type: FUNC
 */
HWTEST_P(RdbExecuteTest, RdbStore_Execute_007, TestSize.Level1)
{
    auto [ret, outValue] = store_->Execute("PRAGMA integrity_check");
    EXPECT_EQ(E_OK, ret);
    EXPECT_EQ(ValueObjectType::TYPE_STRING, outValue.GetType());

    std::string outputResult;
    outValue.GetString(outputResult);
    EXPECT_EQ("ok", outputResult);
}

/**
 * @tc.name: RdbStore_Execute_008
 * @tc.desc: Normal testCase for Execute, check integrity for store
 * @tc.type: FUNC
 */
HWTEST_P(RdbExecuteTest, RdbStore_Execute_008, TestSize.Level1)
{
    auto [ret, outValue] = store_->Execute("PRAGMA quick_check");
    EXPECT_EQ(E_OK, ret);
    EXPECT_EQ(ValueObjectType::TYPE_STRING, outValue.GetType());

    std::string outputResult;
    outValue.GetString(outputResult);
    EXPECT_EQ("ok", outputResult);
}

/**
 * @tc.name: RdbStore_Execute_009
 * @tc.desc: Normal testCase for Execute, get user_version of store
 * @tc.type: FUNC
 */
HWTEST_P(RdbExecuteTest, RdbStore_Execute_009, TestSize.Level1)
{
    // set user_version as 5
    store_->SetVersion(5);
    auto [ret, outValue] = store_->Execute("PRAGMA user_version");
    EXPECT_EQ(E_OK, ret);
    EXPECT_EQ(ValueObjectType::TYPE_INT, outValue.GetType());

    int64_t outputResult{ 0 };
    outValue.GetLong(outputResult);
    EXPECT_EQ(5, outputResult);

    // set user_version as 0
    store_->SetVersion(0);
}

/**
 * @tc.name: RdbStore_Execute_0010
 * @tc.desc: AbNormal testCase for Execute, execute select sql
 * @tc.type: FUNC
 */
HWTEST_P(RdbExecuteTest, RdbStore_Execute_0010, TestSize.Level1)
{
    auto [ret, outValue] = store_->Execute("SELECT * FROM test");
    EXPECT_EQ(E_NOT_SUPPORT_THE_SQL, ret);

    auto [code, result] = store_->ExecuteExt("SELECT * FROM test");
    EXPECT_EQ(E_NOT_SUPPORT_THE_SQL, code);
}

/**
 * @tc.name: RdbStore_Execute_0011
 * @tc.desc: Normal testCase for Execute, execute sql for inserting data
 * @tc.type: FUNC
 */
HWTEST_P(RdbExecuteTest, RdbStore_Execute_0011, TestSize.Level1)
{
    std::vector<ValueObject> args = { ValueObject(std::string("tt")), ValueObject(int(28)),
        ValueObject(double(50000.0)) };
    auto [ret, outValue] = store_->Execute("INSERT INTO test(name, age, salary) VALUES (?, ?, ?);", args);
    EXPECT_EQ(E_OK, ret);
    EXPECT_EQ(ValueObjectType::TYPE_INT, outValue.GetType());

    int64_t outputResult;
    outValue.GetLong(outputResult);
    // 1 represent that the last data is inserted in the first row
    EXPECT_EQ(1, outputResult);
}

/**
 * @tc.name: RdbStore_Execute_0012
 * @tc.desc: Normal testCase for Execute, execute sql for batch insert data
 * @tc.type: FUNC
 */
HWTEST_P(RdbExecuteTest, RdbStore_Execute_0012, TestSize.Level1)
{
    std::vector<ValueObject> args = { ValueObject(std::string("tt")), ValueObject(int(28)),
        ValueObject(double(50000.0)), ValueObject(std::string("ttt")), ValueObject(int(58)),
        ValueObject(double(500080.0)) };
    auto [ret, outValue] = store_->Execute("INSERT INTO test(name, age, salary) VALUES (?, ?, ?), (?, ?, ?)", args);
    EXPECT_EQ(E_OK, ret);

    EXPECT_EQ(ValueObjectType::TYPE_INT, outValue.GetType());

    int64_t outputResult;
    outValue.GetLong(outputResult);
    // 2 represent that the last data is inserted in the second row
    EXPECT_EQ(2, outputResult);
}

/**
 * @tc.name: RdbStore_Execute_0013
 * @tc.desc: Normal testCase for Execute, execute sql for updating data
 * @tc.type: FUNC
 */
HWTEST_P(RdbExecuteTest, RdbStore_Execute_0013, TestSize.Level1)
{
    std::vector<ValueObject> args = { ValueObject(std::string("tt")), ValueObject(int(28)),
        ValueObject(double(50000.0)), ValueObject(std::string("ttt")), ValueObject(int(58)),
        ValueObject(double(500080.0)) };
    auto [ret1, outValue1] = store_->Execute("INSERT INTO test(name, age, salary) VALUES (?, ?, ?), (?, ?, ?)", args);
    EXPECT_EQ(E_OK, ret1);
    EXPECT_EQ(ValueObjectType::TYPE_INT, outValue1.GetType());

    int64_t outputResult;
    outValue1.GetLong(outputResult);
    // 2 represent that the last data is inserted in the second row
    EXPECT_EQ(2, outputResult);

    auto [ret2, outValue2] = store_->Execute("UPDATE test SET name='dd' WHERE id = 2");
    EXPECT_EQ(E_OK, ret2);
    EXPECT_EQ(ValueObjectType::TYPE_INT, outValue2.GetType());

    outValue2.GetLong(outputResult);
    // 1 represent that effected row id
    EXPECT_EQ(1, outputResult);
}

/**
 * @tc.name: RdbStore_Execute_0014
 * @tc.desc: Normal testCase for Execute, execute sql for deleting data
 * @tc.type: FUNC
 */
HWTEST_P(RdbExecuteTest, RdbStore_Execute_0014, TestSize.Level1)
{
    std::vector<ValueObject> args = { ValueObject(std::string("tt")), ValueObject(int(28)),
        ValueObject(double(50000.0)), ValueObject(std::string("ttt")), ValueObject(int(82)),
        ValueObject(double(500080.0)) };
    auto [ret1, outValue1] = store_->Execute("INSERT INTO test(name, age, salary) VALUES (?, ?, ?), (?, ?, ?)", args);
    EXPECT_EQ(E_OK, ret1);
    EXPECT_EQ(ValueObjectType::TYPE_INT, outValue1.GetType());

    int64_t outputResult;
    outValue1.GetLong(outputResult);
    // 2 represent that the last data is inserted in the second row
    EXPECT_EQ(2, outputResult);

    auto [ret2, outValue2] = store_->Execute("DELETE FROM test");
    EXPECT_EQ(E_OK, ret2);
    EXPECT_EQ(ValueObjectType::TYPE_INT, outValue2.GetType());

    outValue2.GetLong(outputResult);
    // 2 represent that effected row id
    EXPECT_EQ(2, outputResult);
}

/**
 * @tc.name: RdbStore_Execute_0015
 * @tc.desc: AbNormal testCase for Execute, execute sql for attaching database and transaction
 * @tc.type: FUNC
 */
HWTEST_P(RdbExecuteTest, RdbStore_Execute_0015, TestSize.Level1)
{
    auto [ret1, outValue1] = store_->Execute("ATTACH DATABASE 'execute_attach_test.db' AS 'attach.db'");
    EXPECT_EQ(E_NOT_SUPPORT_THE_SQL, ret1);

    auto [ret2, outValue2] = store_->Execute("DETACH DATABASE 'attach.db'");
    EXPECT_EQ(E_NOT_SUPPORT_THE_SQL, ret2);

    auto [ret3, outValue3] = store_->Execute("BEGIN TRANSACTION");
    EXPECT_EQ(E_NOT_SUPPORT_THE_SQL, ret3);

    auto [ret4, outValue4] = store_->Execute("COMMIT");
    EXPECT_EQ(E_NOT_SUPPORT_THE_SQL, ret4);

    auto [ret5, outValue5] = store_->Execute("ROLLBACK");
    EXPECT_EQ(E_NOT_SUPPORT_THE_SQL, ret5);

    auto [ret6, outValue6] = store_->ExecuteExt("ROLLBACK");
    EXPECT_EQ(E_NOT_SUPPORT_THE_SQL, ret6);
}

/**
 * @tc.name: RdbStore_Execute_0016
 * @tc.desc: Normal testCase for Execute, execute DDL sql for creating and dropping table
 * @tc.type: FUNC
 */
HWTEST_P(RdbExecuteTest, RdbStore_Execute_0016, TestSize.Level1)
{
    int64_t intOutValue;

    constexpr const char *CREATE_TABLE_TEST2 = "CREATE TABLE IF NOT EXISTS test2 "
                                                "(id INTEGER PRIMARY KEY AUTOINCREMENT, "
                                                "name TEXT NOT NULL, age INTEGER, salary REAL, "
                                                "blobType BLOB)";
    constexpr const char *DROP_TABLE_TEST2 = "DROP TABLE test2";
    constexpr const char *TEST_TABLE_IS_EXIST =
        "SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name='test2'";

    auto [ret1, outValue1] = store_->Execute(CREATE_TABLE_TEST2);
    EXPECT_EQ(E_OK, ret1);
    EXPECT_EQ(ValueObjectType::TYPE_NULL, outValue1.GetType());

    std::shared_ptr<ResultSet> resultSet = store_->QuerySql(TEST_TABLE_IS_EXIST);
    EXPECT_NE(nullptr, resultSet);
    resultSet->GoToFirstRow();
    // 0 represent that get count of table test in the first row
    resultSet->GetLong(0, intOutValue);
    // 1 represent that the table exists
    EXPECT_EQ(1, intOutValue);
    resultSet->Close();

    auto [ret2, outValue2] = store_->ExecuteExt(DROP_TABLE_TEST2);
    EXPECT_EQ(E_OK, ret2);
    EXPECT_EQ(0, outValue2.changed);
    int rowCount = -1;
    outValue2.results->GetRowCount(rowCount);
    EXPECT_EQ(0, rowCount);

    resultSet = store_->QuerySql(TEST_TABLE_IS_EXIST);
    EXPECT_NE(nullptr, resultSet);
    resultSet->GoToFirstRow();
    // 0 represent that get count of table test in the first column
    resultSet->GetLong(0, intOutValue);
    // 0 represent the table does not exist
    EXPECT_EQ(0, intOutValue);
    resultSet->Close();
}

/**
 * @tc.name: RdbStore_Execute_0017
 * @tc.desc: Normal testCase for Execute, execute sql for creating table and insert, query data
 * @tc.type: FUNC
 */
HWTEST_P(RdbExecuteTest, RdbStore_Execute_0017, TestSize.Level1)
{
    int64_t intOutValue;
    int intOutResultSet;

    constexpr const char *CREATE_TABLE_TEST2 = "CREATE TABLE IF NOT EXISTS test2 "
                                           "(id INTEGER PRIMARY KEY AUTOINCREMENT, "
                                           "name TEXT NOT NULL, age INTEGER, salary REAL, "
                                           "blobType BLOB)";
    constexpr const char *DROP_TABLE_TEST2 = "DROP TABLE test2";

    auto [ret1, outValue1] = store_->Execute(CREATE_TABLE_TEST2);
    EXPECT_EQ(E_OK, ret1);

    std::vector<ValueObject> args = { ValueObject("tt"), ValueObject(28), ValueObject(50000) };
    auto [ret2, outValue2] = store_->Execute("INSERT INTO test2(name, age, salary) VALUES (?, ?, ?)", args);
    EXPECT_EQ(E_OK, ret2);
    outValue2.GetLong(intOutValue);
    // 1 represent that the last data is inserted in the first row
    EXPECT_EQ(1, intOutValue);

    std::shared_ptr<ResultSet> resultSet = store_->QuerySql("SELECT * FROM test2");
    EXPECT_NE(nullptr, resultSet);
    EXPECT_EQ(E_OK, resultSet->GetRowCount(intOutResultSet));
    // 1 represent that the row number of resultSet
    EXPECT_EQ(1, intOutResultSet);
    resultSet->Close();

    auto [ret3, outValue3] = store_->Execute(DROP_TABLE_TEST2);
    EXPECT_EQ(E_OK, ret3);
}

/**
 * @tc.name: RdbStore_Execute_0018
 * @tc.desc: AbNormal testCase for Execute, execute sql for inserting data but args is []
 * @tc.type: FUNC
 */
HWTEST_P(RdbExecuteTest, RdbStore_Execute_0018, TestSize.Level1)
{
    ValueObject outValue;

    auto [ret1, outValue1] = store_->Execute("INSERT INTO test(name, age, salary) VALUES (?, ?, ?), (?, ?, ?)");
    EXPECT_NE(E_OK, ret1);
}

/**
 * @tc.name: RdbStore_Execute_0019
 * @tc.desc: Normal testCase for Execute, set user_version of store
 * @tc.type: FUNC
 */
HWTEST_P(RdbExecuteTest, RdbStore_Execute_0019, TestSize.Level1)
{
    // set user_version as 5
    auto [ret, outValue] = store_->Execute("PRAGMA user_version=5");
    EXPECT_EQ(E_OK, ret);
    EXPECT_EQ(ValueObjectType::TYPE_NULL, outValue.GetType());

    // set user_version as 0
    std::tie(ret, outValue) = store_->Execute("PRAGMA user_version=0");
    EXPECT_EQ(E_OK, ret);
    EXPECT_EQ(ValueObjectType::TYPE_NULL, outValue.GetType());
}

/**
 * @tc.name: RdbStore_Execute_0020
 * @tc.desc: AbNormal testCase for Execute, get table_info
 * @tc.type: FUNC
 */
HWTEST_P(RdbExecuteTest, RdbStore_Execute_0020, TestSize.Level1)
{
    auto [ret, outValue] = store_->Execute("PRAGMA table_info(test)");
    EXPECT_EQ(E_NOT_SUPPORT_THE_SQL, ret);
}

/**
 * @tc.name: RdbStore_Execute_0021
 * @tc.desc: Normal testCase for Execute insert, update and delete 2 rows with returning
 * @tc.type: FUNC
 */
HWTEST_P(RdbExecuteTest, RdbStore_Execute_0021, TestSize.Level1)
{
    std::vector<ValueObject> args = { "tt", 28, 50000.0, "ttt", 58, 500080.0 };
    auto [status, result] =
        store_->ExecuteExt("INSERT INTO test(name, age, salary) VALUES (?, ?, ?), (?, ?, ?) returning name", args);
    EXPECT_EQ(status, E_OK);
    EXPECT_EQ(result.changed, 2);
    int rowCount = -1;
    ASSERT_EQ(result.results->GetRowCount(rowCount), E_OK);
    ASSERT_EQ(rowCount, 2);
    int columnIndex = -1;
    ASSERT_EQ(result.results->GetColumnIndex("name", columnIndex), E_OK);
    std::string value;
    ASSERT_EQ(result.results->GetString(columnIndex, value), E_OK);
    EXPECT_EQ(value, "tt");
    ASSERT_EQ(result.results->GoToNextRow(), E_OK);
    ASSERT_EQ(result.results->GetString(columnIndex, value), E_OK);
    EXPECT_EQ(value, "ttt");

    std::tie(status, result) =
        store_->ExecuteExt("update test set name = ? where name = ? returning name", {"update", "tt"});
    EXPECT_EQ(status, E_OK);
    EXPECT_EQ(result.changed, 1);
    rowCount = -1;
    ASSERT_EQ(result.results->GetRowCount(rowCount), E_OK);
    ASSERT_EQ(rowCount, 1);
    ASSERT_EQ(result.results->GetColumnIndex("name", columnIndex), E_OK);
    ASSERT_EQ(result.results->GetString(columnIndex, value), E_OK);
    EXPECT_EQ(value, "update");

    std::tie(status, result) = store_->ExecuteExt("delete from test returning name");
    EXPECT_EQ(status, E_OK);
    EXPECT_EQ(result.changed, 2);
    rowCount = -1;
    ASSERT_EQ(result.results->GetRowCount(rowCount), E_OK);
    ASSERT_EQ(rowCount, 2);
    ASSERT_EQ(result.results->GetColumnIndex("name", columnIndex), E_OK);
    ASSERT_EQ(result.results->GetString(columnIndex, value), E_OK);
    EXPECT_EQ(value, "update");
    ASSERT_EQ(result.results->GoToNextRow(), E_OK);
    ASSERT_EQ(result.results->GetString(columnIndex, value), E_OK);
    EXPECT_EQ(value, "ttt");
}

/**
 * @tc.name: RdbStore_Execute_0022
 * @tc.desc: Normal testCase for Execute insert, update and delete over limit with returning
 * @tc.type: FUNC
 */
HWTEST_P(RdbExecuteTest, RdbStore_Execute_0022, TestSize.Level1)
{
    std::vector<ValueObject> args = { "0", 0 };
    std::string sql = "INSERT INTO test(name, age) VALUES (?, ?)";
    for (int32_t i = 1; i < 1025; i++) {
        sql.append(", (?, ?)");
        args.push_back(std::to_string(i));
        args.push_back(i);
    }
    auto [status, result] = store_->ExecuteExt(sql + " returning name", args);
    EXPECT_EQ(status, E_OK);
    EXPECT_EQ(result.changed, 1025);
    int rowCount = -1;
    ASSERT_EQ(result.results->GetRowCount(rowCount), E_OK);
    ASSERT_EQ(rowCount, 1024);
    int columnIndex = -1;
    ASSERT_EQ(result.results->GetColumnIndex("name", columnIndex), E_OK);
    std::string value;
    ASSERT_EQ(result.results->GetString(columnIndex, value), E_OK);
    EXPECT_EQ(value, "0");
    ASSERT_EQ(result.results->GoToRow(1000), E_OK);
    ASSERT_EQ(result.results->GetString(columnIndex, value), E_OK);
    EXPECT_EQ(value, "1000");

    std::tie(status, result) = store_->ExecuteExt("update test set name = ? returning name", { "update" });
    EXPECT_EQ(status, E_OK);
    EXPECT_EQ(result.changed, 1025);
    rowCount = -1;
    ASSERT_EQ(result.results->GetRowCount(rowCount), E_OK);
    ASSERT_EQ(rowCount, 1024);
    ASSERT_EQ(result.results->GetColumnIndex("name", columnIndex), E_OK);
    ASSERT_EQ(result.results->GetString(columnIndex, value), E_OK);
    EXPECT_EQ(value, "update");


    std::tie(status, result) = store_->ExecuteExt("delete from test returning name");
    EXPECT_EQ(status, E_OK);
    EXPECT_EQ(result.changed, 1025);
    rowCount = -1;
    ASSERT_EQ(result.results->GetRowCount(rowCount), E_OK);
    ASSERT_EQ(rowCount, 1024);
    ASSERT_EQ(result.results->GetColumnIndex("name", columnIndex), E_OK);
    ASSERT_EQ(result.results->GetString(columnIndex, value), E_OK);
    EXPECT_EQ(value, "update");
}

/**
 * @tc.name: RdbStore_Execute_0023
 * @tc.desc: normal testCase for Execute insert, update and delete 0 rows with returning
 * @tc.type: FUNC
 */
HWTEST_P(RdbExecuteTest, RdbStore_Execute_0023, TestSize.Level1)
{
    std::vector<ValueObject> args = { 1, "tt", 28, 50000.0 };
    auto [status, result] =
        store_->ExecuteExt("INSERT INTO test(id, name, age, salary) VALUES (?, ?, ?, ?) returning id", args);
    EXPECT_EQ(status, E_OK);
    EXPECT_EQ(result.changed, 1);
    
    int rowCount = -1;
    ASSERT_EQ(result.results->GetRowCount(rowCount), E_OK);
    ASSERT_EQ(rowCount, 1);
    int columnIndex = -1;
    ASSERT_EQ(result.results->GetColumnIndex("id", columnIndex), E_OK);
    int64_t value;
    ASSERT_EQ(result.results->GetLong(columnIndex, value), E_OK);
    EXPECT_EQ(value, 1);
    
    std::tie(status, result) =
        store_->ExecuteExt("INSERT INTO test(id, name, age, salary) VALUES (?, ?, ?, ?) returning id", args);
    EXPECT_EQ(status, E_SQLITE_CONSTRAINT);
    EXPECT_EQ(result.changed, 0);
    rowCount = -1;
    ASSERT_EQ(result.results->GetRowCount(rowCount), E_OK);
    ASSERT_EQ(rowCount, 0);

    std::tie(status, result) =
        store_->ExecuteExt("update test set name = ? where name = ? returning name", { "update", "noExist" });
    EXPECT_EQ(status, E_OK);
    EXPECT_EQ(result.changed, 0);
    rowCount = -1;
    ASSERT_EQ(result.results->GetRowCount(rowCount), E_OK);
    ASSERT_EQ(rowCount, 0);

    std::tie(status, result) = store_->ExecuteExt("delete from test where name = ? returning name", { "noExist" });
    EXPECT_EQ(status, E_OK);
    EXPECT_EQ(result.changed, 0);
    rowCount = -1;
    ASSERT_EQ(result.results->GetRowCount(rowCount), E_OK);
    ASSERT_EQ(rowCount, 0);
}

/**
 * @tc.name: RdbStore_Execute_0024
 * @tc.desc: abnormal testCase for Execute insert, update and delete with returning field not exist
 * @tc.type: FUNC
 */
HWTEST_P(RdbExecuteTest, RdbStore_Execute_0024, TestSize.Level1)
{
    std::vector<ValueObject> args = { 1, "tt", 28, 50000.0 };
    auto [status, result] =
        store_->ExecuteExt("INSERT INTO test(id, name, age, salary) VALUES (?, ?, ?, ?) returning noExist", args);
    EXPECT_EQ(status, E_SQLITE_ERROR);
    EXPECT_EQ(result.changed, -1);
    ASSERT_EQ(result.results, nullptr);

    std::tie(status, result) =
        store_->ExecuteExt("update test set name = ? where name = ? returning noExist", { "update", "noExist" });
    EXPECT_EQ(status, E_SQLITE_ERROR);
    EXPECT_EQ(result.changed, -1);
    ASSERT_EQ(result.results, nullptr);

    std::tie(status, result) = store_->ExecuteExt("delete from test where name = ? returning noExist", { "noExist" });
    EXPECT_EQ(status, E_SQLITE_ERROR);
    EXPECT_EQ(result.changed, -1);
    ASSERT_EQ(result.results, nullptr);
}

/**
 * @tc.name: RdbStore_Execute_0025
 * @tc.desc: normal test. batch insert into virtual table with returning
 * @tc.type: FUNC
 * @tc.require:
 * @tc.author:
*/
HWTEST_P(RdbExecuteTest, RdbStore_Execute_0025, TestSize.Level1)
{
    store_->Execute("CREATE VIRTUAL TABLE IF NOT EXISTS articles USING fts5(title, content);");

    std::vector<ValueObject> args = { "title", "content" };
    auto [status, result] =
        store_->ExecuteExt("INSERT INTO articles(title, content) VALUES (?, ?) returning title", args);
    EXPECT_EQ(status, E_OK);
    EXPECT_EQ(result.changed, 1);
    int rowCount = -1;
    ASSERT_EQ(result.results->GetRowCount(rowCount), E_OK);
    ASSERT_EQ(rowCount, 1);
    int columnIndex = -1;
    ASSERT_EQ(result.results->GetColumnIndex("title", columnIndex), E_OK);
    std::string value;
    ASSERT_EQ(result.results->GetString(columnIndex, value), E_OK);
    EXPECT_EQ(value, "title");
    
    std::vector<ValueObject> args2 = { "title2", "title" };
    std::tie(status, result) =
        store_->ExecuteExt("UPDATE articles set title = ? where title = ? returning title", args2);
    EXPECT_EQ(status, E_SQLITE_ERROR);
    EXPECT_EQ(result.changed, -1);
    ASSERT_EQ(result.results, nullptr);

    // DELETE RETURNING is not available on virtual tables
    std::tie(status, result) = store_->ExecuteExt("DELETE FROM articles where title = ? returning title", { "title" });
    EXPECT_EQ(status, E_SQLITE_ERROR);
    EXPECT_EQ(result.changed, -1);
    ASSERT_EQ(result.results, nullptr);
    store_->Execute("Drop TABLE articles");
}

/**
 * @tc.name: RdbStore_Execute_0026
 * @tc.desc: normal test. create trigger before update, delete data in trigger, then update data
 * @tc.type: FUNC
 * @tc.require:
 * @tc.author:
*/
HWTEST_P(RdbExecuteTest, RdbStore_Execute_0026, TestSize.Level1)
{
    auto [code, result1] = store_->Execute(
        "CREATE TRIGGER before_update BEFORE UPDATE ON test"
        " BEGIN DELETE FROM test WHERE name = 'wang'; END");

    EXPECT_EQ(code, E_OK);

    ValuesBuckets rows;
    ValuesBucket row;
    row.Put("id", 200);
    row.Put("name", "wang");
    rows.Put(std::move(row));
    row.Put("id", 201);
    row.Put("name", "zhang");
    rows.Put(std::move(row));

    auto [insertStatus, insertResult] =
        store_->BatchInsert("test", rows, { "name" }, NativeRdb::ConflictResolution::ON_CONFLICT_IGNORE);
    EXPECT_EQ(insertStatus, E_OK);
    EXPECT_EQ(insertResult.changed, 2);

    std::vector<ValueObject> args2 = { "liu", "zhang" };
    auto [status, res] =
        store_->ExecuteExt("UPDATE test set name = ? where name = ? returning name", args2);

    EXPECT_EQ(status, E_OK);
    EXPECT_EQ(res.changed, 1);
    int rowCount = -1;
    ASSERT_EQ(res.results->GetRowCount(rowCount), E_OK);
    ASSERT_EQ(rowCount, 1);
    int columnIndex = -1;
    ASSERT_EQ(res.results->GetColumnIndex("name", columnIndex), E_OK);
    std::string value;
    ASSERT_EQ(res.results->GetString(columnIndex, value), E_OK);
    EXPECT_EQ(value, "liu");

    // Check the trigger effect
    auto resultSet = store_->QuerySql("select name from test where id = 200");

    rowCount = -1;
    resultSet->GetRowCount(rowCount);
    ASSERT_EQ(rowCount, 0);
    store_->Execute("DROP TRIGGER IF EXISTS before_update");
}

INSTANTIATE_TEST_SUITE_P(ExecuteTest, RdbExecuteTest, testing::Values(&g_store, &g_memDb));

} // namespace OHOS::RdbExecuteTest
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
#include "logger.h"
#include "rdb_errno.h"
#include "rdb_helper.h"
#include "rdb_open_callback.h"

using namespace testing::ext;
using namespace OHOS::NativeRdb;

class RdbStepResultSetGetRowTest : public testing::Test {
public:
    static void SetUpTestCase(void);
    static void TearDownTestCase(void);
    void SetUp();
    void TearDown();

    static const std::string DATABASE_NAME;
    static std::shared_ptr<RdbStore> store;
};

const std::string RdbStepResultSetGetRowTest::DATABASE_NAME = RDB_TEST_PATH + "stepResultSet_getRow_test.db";
std::shared_ptr<RdbStore> RdbStepResultSetGetRowTest::store = nullptr;

class RdbStepResultSetGetOpenCallback : public RdbOpenCallback {
public:
    int OnCreate(RdbStore &rdbStore) override;
    int OnUpgrade(RdbStore &rdbStore, int oldVersion, int newVersion) override;
};

int RdbStepResultSetGetOpenCallback::OnCreate(RdbStore &store)
{
    return E_OK;
}

int RdbStepResultSetGetOpenCallback::OnUpgrade(RdbStore &store, int oldVersion, int newVersion)
{
    return E_OK;
}

void RdbStepResultSetGetRowTest::SetUpTestCase(void)
{
    int errCode = E_OK;
    RdbHelper::DeleteRdbStore(DATABASE_NAME);
    RdbStoreConfig config(RdbStepResultSetGetRowTest::DATABASE_NAME);
    RdbStepResultSetGetOpenCallback helper;
    RdbStepResultSetGetRowTest::store = RdbHelper::GetRdbStore(config, 1, helper, errCode);
    EXPECT_NE(RdbStepResultSetGetRowTest::store, nullptr);
    EXPECT_EQ(errCode, E_OK);
}

void RdbStepResultSetGetRowTest::TearDownTestCase(void)
{
    RdbHelper::ClearCache();
    RdbHelper::DeleteRdbStore(RdbStepResultSetGetRowTest::DATABASE_NAME);
}

void RdbStepResultSetGetRowTest::SetUp(void)
{
    store->ExecuteSql("CREATE TABLE test (id INTEGER PRIMARY KEY AUTOINCREMENT, data1 TEXT, "
                      "data2 INTEGER, data3 FLOAT, data4 BLOB, data5 BOOLEAN);");
}

void RdbStepResultSetGetRowTest::TearDown(void)
{
    store->ExecuteSql("DROP TABLE IF EXISTS test");
}

/* *
 * @tc.name: RdbStore_StepResultSet_GetRow_001
 * @tc.desc: test StepResultSet GetRow
 * @tc.type: FUNC
 * @tc.require: AR000FKD4F
 */
HWTEST_F(RdbStepResultSetGetRowTest, RdbStore_StepResultSet_GetRow_001, TestSize.Level1)
{
    int64_t rowId;
    ValuesBucket valuesBucket;
    valuesBucket.PutInt("id", ValueObject(1));
    int errorCode = RdbStepResultSetGetRowTest::store->Insert(rowId, "test", valuesBucket);
    EXPECT_EQ(E_OK, errorCode);
    EXPECT_EQ(1, rowId);

    std::unique_ptr<ResultSet> resultSet = RdbStepResultSetGetRowTest::store->QueryByStep("SELECT * FROM test");
    EXPECT_NE(resultSet, nullptr);

    EXPECT_EQ(E_OK, resultSet->GoToFirstRow());

    int iRet = E_ERROR;
    ValuesBucket valuesBucket_ret;
    iRet = resultSet->GetRow(valuesBucket_ret);
    std::map<std::string, ValueObject> valuesMap;
    valuesBucket_ret.GetAll(valuesMap);
    EXPECT_EQ(E_OK, iRet);

    int idValue = valuesMap["id"];

    EXPECT_EQ(1, idValue);

    resultSet->Close();
}

/* *
 * @tc.name: RdbStore_StepResultSet_GetRow_002
 * @tc.desc: test StepResultSet GetRow
 * @tc.type: FUNC
 * @tc.require: AR000FKD4F
 */
HWTEST_F(RdbStepResultSetGetRowTest, RdbStore_StepResultSet_GetRow_002, TestSize.Level1)
{
    ValuesBucket valuesBucket;
    valuesBucket.PutNull("data1");
    valuesBucket.PutNull("data2");
    valuesBucket.PutNull("data3");
    valuesBucket.PutNull("data4");
    valuesBucket.PutNull("data5");
    int64_t rowId;
    int errorCode = RdbStepResultSetGetRowTest::store->Insert(rowId, "test", valuesBucket);
    EXPECT_EQ(E_OK, errorCode);
    EXPECT_EQ(1, rowId);

    std::unique_ptr<ResultSet> resultSet = RdbStepResultSetGetRowTest::store->QueryByStep("SELECT * FROM test");
    EXPECT_NE(resultSet, nullptr);

    EXPECT_EQ(E_OK, resultSet->GoToFirstRow());

    int iRet = E_ERROR;
    ValuesBucket valuesBucket_ret;
    iRet = resultSet->GetRow(valuesBucket_ret);
    std::map<std::string, ValueObject> valuesMap;
    valuesBucket_ret.GetAll(valuesMap);
    EXPECT_EQ(E_OK, iRet);

    int idValue = valuesMap["id"];

    EXPECT_EQ(1, idValue);

    resultSet->Close();
}

/* *
 * @tc.name: RdbStore_StepResultSet_GetRow_003
 * @tc.desc: test StepResultSet GetRow
 * @tc.type: FUNC
 * @tc.require: AR000FKD4F
 */
HWTEST_F(RdbStepResultSetGetRowTest, RdbStore_StepResultSet_GetRow_003, TestSize.Level1)
{
    ValuesBucket valuesBucket;
    valuesBucket.PutString("data1", "olleh");
    valuesBucket.PutInt("data2", 20);
    valuesBucket.PutDouble("data3", 2.0);
    valuesBucket.PutBlob("data4", { 4, 3, 2, 1 });
    valuesBucket.PutBool("data5", true);
    int64_t rowId;
    int errorCode = RdbStepResultSetGetRowTest::store->Insert(rowId, "test", valuesBucket);
    EXPECT_EQ(E_OK, errorCode);
    EXPECT_EQ(1, rowId);

    std::unique_ptr<ResultSet> resultSet = RdbStepResultSetGetRowTest::store->QueryByStep("SELECT * FROM test");
    EXPECT_NE(resultSet, nullptr);

    EXPECT_EQ(E_OK, resultSet->GoToFirstRow());

    int iRet = E_ERROR;
    ValuesBucket valuesBucket_ret;
    iRet = resultSet->GetRow(valuesBucket_ret);
    std::map<std::string, ValueObject> valuesMap;
    valuesBucket_ret.GetAll(valuesMap);
    EXPECT_EQ(E_OK, iRet);

    int idValue = valuesMap["id"];
    std::string data1Value = valuesMap["data1"];
    int data2Value = valuesMap["data2"];
    double data3Value = valuesMap["data3"];
    std::vector<uint8_t> data4Value = valuesMap["data4"];
    int data5Value = valuesMap["data5"];

    EXPECT_EQ(1, idValue);
    EXPECT_EQ("olleh", data1Value);
    EXPECT_EQ(20, data2Value);
    EXPECT_EQ(2.0, data3Value);
    EXPECT_EQ(1, data4Value[3]);
    EXPECT_EQ(1, data5Value);

    resultSet->Close();
}

/* *
 * @tc.name: RdbStore_StepResultSet_GetRow_004
 * @tc.desc: test StepResultSet GetRow
 * @tc.type: FUNC
 * @tc.require: AR000FKD4F
 */
HWTEST_F(RdbStepResultSetGetRowTest, RdbStore_StepResultSet_GetRow_004, TestSize.Level1)
{
    ValuesBucket valuesBucket;
    valuesBucket.PutString("data1", "hello");
    valuesBucket.PutInt("data2", 10);
    valuesBucket.PutDouble("data3", 1.0);
    valuesBucket.PutBlob("data4", { 1, 2, 3, 4 });
    valuesBucket.PutBool("data5", true);
    int64_t rowId;
    int errorCode = RdbStepResultSetGetRowTest::store->Insert(rowId, "test", valuesBucket);
    EXPECT_EQ(E_OK, errorCode);
    EXPECT_EQ(1, rowId);

    std::unique_ptr<ResultSet> resultSet = RdbStepResultSetGetRowTest::store->QueryByStep("SELECT * FROM test");
    EXPECT_NE(resultSet, nullptr);

    EXPECT_EQ(E_OK, resultSet->GoToFirstRow());

    int iRet = E_ERROR;
    ValuesBucket valuesBucket_ret;
    iRet = resultSet->GetRow(valuesBucket_ret);
    std::map<std::string, ValueObject> valuesMap;
    valuesBucket_ret.GetAll(valuesMap);
    EXPECT_EQ(E_OK, iRet);

    int idValue = valuesMap["id"];
    std::string data1Value = valuesMap["data1"];
    int data2Value = valuesMap["data2"];
    double data3Value = valuesMap["data3"];
    std::vector<uint8_t> data4Value = valuesMap["data4"];
    int data5Value = valuesMap["data5"];

    EXPECT_EQ(1, idValue);
    EXPECT_EQ("hello", data1Value);
    EXPECT_EQ(10, data2Value);
    EXPECT_EQ(1.0, data3Value);
    EXPECT_EQ(4, data4Value[3]);
    EXPECT_EQ(1, data5Value);

    resultSet->Close();
}

/* *
 * @tc.name: RdbStore_StepResultSet_GetRow_005
 * @tc.desc: test StepResultSet GetRow
 * @tc.type: FUNC
 * @tc.require: AR000FKD4F
 */
HWTEST_F(RdbStepResultSetGetRowTest, RdbStore_StepResultSet_GetRow_005, TestSize.Level1)
{
    ValuesBucket valuesBucket;
    valuesBucket.PutString("data1", "hello");
    valuesBucket.PutInt("data2", 10);
    valuesBucket.PutDouble("data3", 1.0);
    valuesBucket.PutBlob("data4", { 1, 2, 3, 4 });
    valuesBucket.PutBool("data5", true);
    int64_t rowId;
    int errorCode = RdbStepResultSetGetRowTest::store->Insert(rowId, "test", valuesBucket);
    EXPECT_EQ(E_OK, errorCode);
    EXPECT_EQ(1, rowId);

    std::unique_ptr<ResultSet> resultSet = RdbStepResultSetGetRowTest::store->QueryByStep("SELECT data1, data2 FROM test");
    EXPECT_NE(resultSet, nullptr);

    EXPECT_EQ(E_OK, resultSet->GoToFirstRow());

    int iRet = E_ERROR;
    ValuesBucket valuesBucket_ret;
    iRet = resultSet->GetRow(valuesBucket_ret);
    std::map<std::string, ValueObject> valuesMap;
    valuesBucket_ret.GetAll(valuesMap);
    EXPECT_EQ(E_OK, iRet);

    std::string data1Value = valuesMap["data1"];
    int data2Value = valuesMap["data2"];

    EXPECT_EQ("hello", data1Value);
    EXPECT_EQ(10, data2Value);
    EXPECT_EQ(0, valuesMap.count("data3"));

    resultSet->Close();
}
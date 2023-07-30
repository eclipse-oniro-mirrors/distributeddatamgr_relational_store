/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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
#include "relational_store.h"

using namespace testing::ext;
using namespace OHOS::NativeRdb;

class RdbNdkCursorTest : public testing::Test {
public:
    static void SetUpTestCase(void);
    static void TearDownTestCase(void);
    void SetUp();
    void TearDown();
};

std::string cursorTestPath_ = RDB_TEST_PATH + "rdb_cursor_test.db";
OH_Rdb_Store *cursorTestRdbStore_;

void RdbNdkCursorTest::SetUpTestCase(void)
{
    OH_Rdb_Config config = {0};
    config.path = cursorTestPath_.c_str();
    config.securityLevel = OH_Rdb_SecurityLevel::S1;
    config.isEncrypt = false;

    int errCode = 0;
    char table[] = "test";
    cursorTestRdbStore_ = OH_Rdb_GetOrOpen(&config, &errCode);
    EXPECT_NE(cursorTestRdbStore_, NULL);

    char createTableSql[] = "CREATE TABLE test (id INTEGER PRIMARY KEY AUTOINCREMENT, data1 TEXT, data2 INTEGER, "
                            "data3 FLOAT, data4 BLOB, data5 TEXT);";
    errCode = OH_Rdb_Execute(cursorTestRdbStore_, createTableSql);

    OH_VBucket *valueBucket = OH_Rdb_CreateValuesBucket();
    valueBucket->putInt64(valueBucket, "id", 1);
    valueBucket->putText(valueBucket, "data1", "zhangSan");
    valueBucket->putInt64(valueBucket, "data2", 12800);
    valueBucket->putReal(valueBucket, "data3", 100.1);
    uint8_t arr[] = {1, 2, 3, 4, 5};
    int len = sizeof(arr) / sizeof(arr[0]);
    valueBucket->putBlob(valueBucket, "data4", arr, len);
    valueBucket->putText(valueBucket, "data5", "ABCDEFG");
    errCode = OH_Rdb_Insert(cursorTestRdbStore_, table, valueBucket);
    EXPECT_EQ(errCode, 1);

    valueBucket->clear(valueBucket);
    valueBucket->putInt64(valueBucket, "id", 2);
    valueBucket->putText(valueBucket, "data1", "liSi");
    valueBucket->putInt64(valueBucket, "data2", 13800);
    valueBucket->putReal(valueBucket, "data3", 200.1);
    valueBucket->putText(valueBucket, "data5", "ABCDEFGH");
    errCode = OH_Rdb_Insert(cursorTestRdbStore_, table, valueBucket);
    EXPECT_EQ(errCode, 2);

    valueBucket->clear(valueBucket);
    valueBucket->putInt64(valueBucket, "id", 3);
    valueBucket->putText(valueBucket, "data1", "wangWu");
    valueBucket->putInt64(valueBucket, "data2", 14800);
    valueBucket->putReal(valueBucket, "data3", 300.1);
    valueBucket->putText(valueBucket, "data5", "ABCDEFGHI");
    errCode = OH_Rdb_Insert(cursorTestRdbStore_, table, valueBucket);
    EXPECT_EQ(errCode, 3);

    valueBucket->destroyValuesBucket(valueBucket);
}

void RdbNdkCursorTest::TearDownTestCase(void)
{
    delete cursorTestRdbStore_;
    cursorTestRdbStore_ = NULL;
    OH_Rdb_DeleteStore(cursorTestPath_.c_str());
}

void RdbNdkCursorTest::SetUp(void)
{
}

void RdbNdkCursorTest::TearDown(void)
{
}

/**
 * @tc.name: RDB_NDK_cursor_test_001
 * @tc.desc: Normal testCase of NDK cursor for GetColumnType.
 * @tc.type: FUNC
 */
HWTEST_F(RdbNdkCursorTest, RDB_NDK_cursor_test_001, TestSize.Level1)
{
    int errCode = 0;
    OH_Predicates *predicates = OH_Rdb_CreatePredicates("test");

    OH_Cursor *cursor = OH_Rdb_Query(cursorTestRdbStore_, predicates, NULL, 0);
    EXPECT_NE(cursor, NULL);
    cursor->goToNextRow(cursor);

    OH_ColumnType type;
    errCode = cursor->getColumnType(cursor, 0, &type);
    EXPECT_EQ(type, OH_ColumnType::TYPE_INT64);

    errCode = cursor->getColumnType(cursor, 1, &type);
    EXPECT_EQ(type, OH_ColumnType::TYPE_TEXT);

    errCode = cursor->getColumnType(cursor, 2, &type);
    EXPECT_EQ(type, OH_ColumnType::TYPE_INT64);

    errCode = cursor->getColumnType(cursor, 3, &type);
    EXPECT_EQ(type, OH_ColumnType::TYPE_REAL);

    errCode = cursor->getColumnType(cursor, 4, &type);
    EXPECT_EQ(type, OH_ColumnType::TYPE_BLOB);

    errCode = cursor->getColumnType(cursor, 5, &type);
    EXPECT_EQ(type, OH_ColumnType::TYPE_TEXT);

    predicates->destroyPredicates(predicates);
    cursor->close(cursor);
}

/**
 * @tc.name: RDB_NDK_cursor_test_002
 * @tc.desc: Normal testCase of NDK cursor for GetColumnIndex.
 * @tc.type: FUNC
 */
HWTEST_F(RdbNdkCursorTest, RDB_NDK_cursor_test_002, TestSize.Level1)
{
    int errCode = 0;
    OH_Predicates *predicates = OH_Rdb_CreatePredicates("test");

    OH_Cursor *cursor = OH_Rdb_Query(cursorTestRdbStore_, predicates, NULL, 0);
    EXPECT_NE(cursor, NULL);

    int columnIndex;
    errCode = cursor->getColumnIndex(cursor, "data1", &columnIndex);
    EXPECT_EQ(columnIndex, 1);

    errCode = cursor->getColumnIndex(cursor, "data2", &columnIndex);
    EXPECT_EQ(columnIndex, 2);

    errCode = cursor->getColumnIndex(cursor, "data3", &columnIndex);
    EXPECT_EQ(columnIndex, 3);

    errCode = cursor->getColumnIndex(cursor, "data4", &columnIndex);
    EXPECT_EQ(columnIndex, 4);

    errCode = cursor->getColumnIndex(cursor, "data5", &columnIndex);
    EXPECT_EQ(columnIndex, 5);

    predicates->destroyPredicates(predicates);
    cursor->close(cursor);
}

/**
 * @tc.name: RDB_NDK_cursor_test_003
 * @tc.desc: Normal testCase of NDK cursor for GetColumnName.
 * @tc.type: FUNC
 */
HWTEST_F(RdbNdkCursorTest, RDB_NDK_cursor_test_003, TestSize.Level1)
{
    int errCode = 0;
    OH_Predicates *predicates = OH_Rdb_CreatePredicates("test");

    OH_Cursor *cursor = OH_Rdb_Query(cursorTestRdbStore_, predicates, NULL, 0);
    EXPECT_NE(cursor, NULL);

    char name[10];
    errCode = cursor->getColumnName(cursor, 1, name, 10);
    EXPECT_EQ(strcmp(name, "data1"), 0);

    errCode = cursor->getColumnName(cursor, 2, name, 6);
    EXPECT_EQ(strcmp(name, "data2"), 0);

    errCode = cursor->getColumnName(cursor, 3, name, 6);
    EXPECT_EQ(strcmp(name, "data3"), 0);

    errCode = cursor->getColumnName(cursor, 4, name, 6);
    EXPECT_EQ(strcmp(name, "data4"), 0);

    errCode = cursor->getColumnName(cursor, 5, name, 6);
    EXPECT_EQ(strcmp(name, "data5"), 0);

    predicates->destroyPredicates(predicates);
    cursor->close(cursor);
}

/**
 * @tc.name: RDB_NDK_cursor_test_004
 * @tc.desc: Normal testCase of NDK cursor for Getxxx.
 * @tc.type: FUNC
 */
HWTEST_F(RdbNdkCursorTest, RDB_NDK_cursor_test_004, TestSize.Level1)
{
    OH_Predicates *predicates = OH_Rdb_CreatePredicates("test");

    const char *columnNames[] = {"data1", "data2", "data3", "data4"};
    int len = sizeof(columnNames) / sizeof(columnNames[0]);
    OH_Cursor *cursor = OH_Rdb_Query(cursorTestRdbStore_, predicates, columnNames, len);
    EXPECT_NE(cursor, NULL);

    int rowCount = 0;
    cursor->getRowCount(cursor, &rowCount);
    EXPECT_EQ(rowCount, 3);

    cursor->goToNextRow(cursor);

    int columnCount = 0;
    cursor->getColumnCount(cursor, &columnCount);
    EXPECT_EQ(columnCount, 4);

    size_t size = 0;
    cursor->getSize(cursor, 0, &size);
    char data1Value[size + 1];
    cursor->getText(cursor, 0, data1Value, size + 1);
    EXPECT_EQ(strcmp(data1Value, "zhangSan"), 0);

    int64_t data2Value;
    cursor->getInt64(cursor, 1, &data2Value);
    EXPECT_EQ(data2Value, 12800);

    double data3Value;
    cursor->getReal(cursor, 2, &data3Value);
    EXPECT_EQ(data3Value, 100.1);

    cursor->getSize(cursor, 3, &size);
    unsigned char data4Value[size];
    cursor->getBlob(cursor, 3, data4Value, size);
    EXPECT_EQ(data4Value[0], 1);
    EXPECT_EQ(data4Value[1], 2);

    cursor->goToNextRow(cursor);

    cursor->getSize(cursor, 0, &size);
    char data1Value_1[size + 1];
    cursor->getText(cursor, 0, data1Value_1, size + 1);
    EXPECT_EQ(strcmp(data1Value_1, "liSi"), 0);

    cursor->getInt64(cursor, 1, &data2Value);
    EXPECT_EQ(data2Value, 13800);

    cursor->getReal(cursor, 2, &data3Value);
    EXPECT_EQ(data3Value, 200.1);

    bool isNull = false;
    cursor->isNull(cursor, 3, &isNull);
    EXPECT_EQ(isNull, true);

    predicates->destroyPredicates(predicates);
    cursor->close(cursor);
}
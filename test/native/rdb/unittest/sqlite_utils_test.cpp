/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
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

#include "sqlite_utils.h"

#include <gtest/gtest.h>

#include <climits>
#include <string>

using namespace testing::ext;
using namespace OHOS::NativeRdb;

class SqliteUtilsTest : public testing::Test {
public:
    static void SetUpTestCase(void);
    static void TearDownTestCase(void);
    void SetUp(void){};
    void TearDown(void){};
};

void SqliteUtilsTest::SetUpTestCase(void)
{
}

void SqliteUtilsTest::TearDownTestCase(void)
{
}

HWTEST_F(SqliteUtilsTest, SqliteUtils_Test_001, TestSize.Level1)
{
    EXPECT_EQ(SqliteUtils::Anonymous("30005245854585524412855412_rdb_test.db"), "***5412_rdb_test.db");
}

HWTEST_F(SqliteUtilsTest, SqliteUtils_Test_002, TestSize.Level1)
{
    EXPECT_EQ(SqliteUtils::Anonymous("rdb_test_30005245854585524412855412.db"), "rdb_test_***5412.db");
}

HWTEST_F(SqliteUtilsTest, SqliteUtils_Test_003, TestSize.Level1)
{
    EXPECT_EQ(SqliteUtils::Anonymous("rdb_30005245854585524412855412_test.db"), "rdb_***5412_test.db");
}

HWTEST_F(SqliteUtilsTest, SqliteUtils_Test_004, TestSize.Level1)
{
    EXPECT_EQ(SqliteUtils::Anonymous("rdb_300052_test.db"), "rdb_***052_test.db");
}

HWTEST_F(SqliteUtilsTest, SqliteUtils_Test_005, TestSize.Level1)
{
    EXPECT_EQ(SqliteUtils::Anonymous("rdb_30005_test.db"), "rdb_30005_test.db");
}

HWTEST_F(SqliteUtilsTest, SqliteUtils_Test_006, TestSize.Level1)
{
    EXPECT_EQ(SqliteUtils::Anonymous("rdb_3000523_test.db"), "rdb_***0523_test.db");
}

HWTEST_F(SqliteUtilsTest, SqliteUtils_Test_007, TestSize.Level1)
{
    EXPECT_EQ(SqliteUtils::Anonymous(
                  "file /data/stage/el2/database/rdb/ddddddd/30005245854585524412855412_rdb_test.db"),
        "file /***/el2/***/***5412_rdb_test.db");
}

HWTEST_F(SqliteUtilsTest, SqliteUtils_Test_008, TestSize.Level1)
{
    EXPECT_EQ(
        SqliteUtils::Anonymous("file /data/stage/database/rdb/ddddddd/30005245854585524412855412_rdb_test.db"),
        "file /***/***5412_rdb_test.db");
}

HWTEST_F(SqliteUtilsTest, SqliteUtils_Test_009, TestSize.Level1)
{
    EXPECT_EQ(SqliteUtils::Anonymous(
                  "file /data/stage/el2/database/rdb/ddddddd/3E00mnj5H54efg5G4K1ABC5412_rdb_test.db"),
        "file /***/el2/***/3E00mnj5H54efg5G4K***5412_rdb_test.db");
}

HWTEST_F(SqliteUtilsTest, SqliteUtils_Test_010, TestSize.Level1)
{
    EXPECT_EQ(SqliteUtils::Anonymous("/data/stage/el2/database/rdb/ddddddd/3E00mnj5H54efg5G4K1ABC5412_rdb_test.db"),
        "/***/el2/***/3E00mnj5H54efg5G4K***5412_rdb_test.db");
}

HWTEST_F(SqliteUtilsTest, SqliteUtils_Test_0011, TestSize.Level1)
{
    EXPECT_EQ(SqliteUtils::Anonymous("30005245854585524412855412.db"), "***5412.db");
}

HWTEST_F(SqliteUtilsTest, SqliteUtils_Test_0012, TestSize.Level1)
{
    EXPECT_EQ(SqliteUtils::Anonymous("thequickbrownfoxjumpoverthelazydog.db"), "thequickbrownfoxjumpoverthelazydog.db");
}

HWTEST_F(SqliteUtilsTest, SqliteUtils_Test_0013, TestSize.Level1)
{
    EXPECT_EQ(SqliteUtils::Anonymous("123edf4.db"), "***edf4.db");
}

HWTEST_F(SqliteUtilsTest, SqliteUtils_Test_0014, TestSize.Level1)
{
    EXPECT_EQ(SqliteUtils::Anonymous("K123edfK.db"), "K***edfK.db");
}

HWTEST_F(SqliteUtilsTest, SqliteUtils_Test_0015, TestSize.Level1)
{
    EXPECT_EQ(SqliteUtils::Anonymous("K23edfK.db"), "K23edfK.db");
}

HWTEST_F(SqliteUtilsTest, SqliteUtils_Test_0016, TestSize.Level1)
{
    EXPECT_EQ(SqliteUtils::Anonymous("__23edf__.db"), "__23edf__.db");
}

HWTEST_F(SqliteUtilsTest, SqliteUtils_Test_0017, TestSize.Level1)
{
    EXPECT_EQ(SqliteUtils::Anonymous("K3edfK.db"), "K3edfK.db");
}

HWTEST_F(SqliteUtilsTest, SqliteUtils_Test_0018, TestSize.Level1)
{
    EXPECT_EQ(SqliteUtils::Anonymous("K23564edfK.db"), "K***4edfK.db");
}

HWTEST_F(SqliteUtilsTest, SqliteUtils_Test_0019, TestSize.Level1)
{
    EXPECT_EQ(SqliteUtils::Anonymous("K235648edfK.db"), "K***8edfK.db");
}

HWTEST_F(SqliteUtilsTest, SqliteUtils_Test_0020, TestSize.Level1)
{
    EXPECT_EQ(SqliteUtils::Anonymous("K2356489edfK.db"), "K***9edfK.db");
}
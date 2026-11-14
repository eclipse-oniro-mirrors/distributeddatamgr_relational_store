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

#include <map>
#include <string>
#include <vector>

#include "cache_result_set.h"
#include "rdb_errno.h"
#include "values_bucket.h"

using namespace testing::ext;
using namespace OHOS::NativeRdb;

class CacheResultSetErrMsgTest : public testing::Test {
public:
    static void SetUpTestCase(void);
    static void TearDownTestCase(void);
    void SetUp();
    void TearDown();

    static std::shared_ptr<CacheResultSet> resultSet_;
};

std::shared_ptr<CacheResultSet> CacheResultSetErrMsgTest::resultSet_ = nullptr;

void CacheResultSetErrMsgTest::SetUpTestCase(void)
{
}

void CacheResultSetErrMsgTest::TearDownTestCase(void)
{
}

void CacheResultSetErrMsgTest::SetUp(void)
{
    std::vector<ValuesBucket> buckets;
    ValuesBucket bucket(std::map<std::string, ValueObject> { { "id", ValueObject(1) },
        { "name", ValueObject("zhangsan") } });
    buckets.push_back(std::move(bucket));
    resultSet_ = std::make_shared<CacheResultSet>(std::move(buckets));
}

void CacheResultSetErrMsgTest::TearDown(void)
{
    resultSet_ = nullptr;
}

/* *
 * @tc.name: CacheErrMsg_001
 * @tc.desc: Verify GetColumnIndex with an unknown column name sets the semantic message.
 * @tc.type: FUNC
 */
HWTEST_F(CacheResultSetErrMsgTest, CacheErrMsg_001, TestSize.Level1)
{
    int columnIndex = -1;
    int errCode = resultSet_->GetColumnIndex("nonexistent", columnIndex);
    EXPECT_EQ(errCode, E_ERROR);
    std::string msg = resultSet_->GetLastErrorMsg();
    EXPECT_NE(msg.find("Failed to get column index: column name not found in cache"), std::string::npos);
}

/* *
 * @tc.name: CacheErrMsg_002
 * @tc.desc: Verify GetLastErrorMsg consumes the message read-once (second read empty).
 * @tc.type: FUNC
 */
HWTEST_F(CacheResultSetErrMsgTest, CacheErrMsg_002, TestSize.Level1)
{
    int columnIndex = -1;
    int errCode = resultSet_->GetColumnIndex("nonexistent", columnIndex);
    EXPECT_EQ(errCode, E_ERROR);
    std::string first = resultSet_->GetLastErrorMsg();
    EXPECT_FALSE(first.empty());
    std::string second = resultSet_->GetLastErrorMsg();
    EXPECT_TRUE(second.empty());
}

/* *
 * @tc.name: CacheErrMsg_003
 * @tc.desc: Verify GetLastErrorMsg returns empty on a fresh result set and the
 *           SetLastErrorMsg/GetLastErrorMsg round trip works.
 * @tc.type: FUNC
 */
HWTEST_F(CacheResultSetErrMsgTest, CacheErrMsg_003, TestSize.Level1)
{
    // Fresh instance: no message recorded, the read returns empty
    EXPECT_TRUE(resultSet_->GetLastErrorMsg().empty());

    // Round trip: write then read-once
    resultSet_->SetLastErrorMsg("cache error message");
    EXPECT_EQ(resultSet_->GetLastErrorMsg(), "cache error message");
    EXPECT_TRUE(resultSet_->GetLastErrorMsg().empty());
}

/* *
 * @tc.name: CacheErrMsg_004
 * @tc.desc: Verify normal column lookup succeeds and no message is recorded.
 * @tc.type: FUNC
 */
HWTEST_F(CacheResultSetErrMsgTest, CacheErrMsg_004, TestSize.Level1)
{
    int columnIndex = -1;
    int errCode = resultSet_->GetColumnIndex("id", columnIndex);
    EXPECT_EQ(errCode, E_OK);
    EXPECT_EQ(columnIndex, 0);
    EXPECT_TRUE(resultSet_->GetLastErrorMsg().empty());
}

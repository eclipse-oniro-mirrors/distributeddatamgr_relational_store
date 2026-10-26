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
#include <unistd.h>

#include <string>

#include "common.h"
#include "rdb_errno.h"
#include "rdb_helper.h"
#include "rdb_open_callback.h"
#include "sqlite_utils.h"

using namespace testing::ext;
using namespace OHOS::NativeRdb;

namespace RdbTest {
class DumpStackDeleteTest : public testing::Test {
public:
    static constexpr const char *TEST_DIR = "/data/test/dump_stack_delete_test/";
    static void SetUpTestCase(void){};
    static void TearDownTestCase(void){};
    void SetUp(void)
    {
        mkdir(TEST_DIR, S_IRWXU | S_IRWXG | S_IRWXO);
    }
    void TearDown(void) { RdbHelper::ClearCache(); }
};

class DumpStackDeleteCallback : public RdbOpenCallback {
public:
    int OnCreate(RdbStore &store) override;
    int OnUpgrade(RdbStore &store, int oldVersion, int newVersion) override;
};

int DumpStackDeleteCallback::OnCreate(RdbStore &store)
{
    return E_OK;
}

int DumpStackDeleteCallback::OnUpgrade(RdbStore &store, int oldVersion, int newVersion)
{
    return E_OK;
}
} // namespace RdbTest
using namespace RdbTest;

/**
 * @tc.name: DumpStackDelete_ClearTagsBeforeDelete_001
 * @tc.desc: test DeleteRdbStore on a dump-stack-enabled store succeeds and removes files
 * @tc.type: FUNC
 */
HWTEST_F(DumpStackDeleteTest, DumpStackDelete_ClearTagsBeforeDelete_001, TestSize.Level1)
{
    const std::string dbPath = std::string(TEST_DIR) + "delete_on.db";
    RdbStoreConfig config(dbPath);
    config.EnableDumpStackOnFileDel(true);
    DumpStackDeleteCallback helper;
    int errCode = E_ERROR;
    std::shared_ptr<RdbStore> store = RdbHelper::GetRdbStore(config, 1, helper, errCode);
    ASSERT_NE(store, nullptr);
    EXPECT_EQ(errCode, E_OK);

    // AC-2.1/BR-5: deletion must succeed even where the ioctl degrades (non-HMFS host),
    // and the db file must be removed. Tag-cleared-before-delete ordering and the absence
    // of call stack output are validated on HMFS devices (device integration).
    int ret = RdbHelper::DeleteRdbStore(config);
    EXPECT_EQ(ret, E_OK);
    struct stat fileStat;
    EXPECT_LT(stat(dbPath.c_str(), &fileStat), 0);

    store = nullptr;
}

/**
 * @tc.name: DumpStackDelete_UntaggedStore_002
 * @tc.desc: test DeleteRdbStore on a store without the dump-stack flag succeeds (EX-6)
 * @tc.type: FUNC
 */
HWTEST_F(DumpStackDeleteTest, DumpStackDelete_UntaggedStore_002, TestSize.Level1)
{
    const std::string dbPath = std::string(TEST_DIR) + "delete_off.db";
    RdbStoreConfig config(dbPath);
    DumpStackDeleteCallback helper;
    int errCode = E_ERROR;
    std::shared_ptr<RdbStore> store = RdbHelper::GetRdbStore(config, 1, helper, errCode);
    ASSERT_NE(store, nullptr);
    EXPECT_EQ(errCode, E_OK);

    // EX-6: no tag on the files; the delete flow proceeds normally.
    int ret = RdbHelper::DeleteRdbStore(config);
    EXPECT_EQ(ret, E_OK);
    struct stat fileStat;
    EXPECT_LT(stat(dbPath.c_str(), &fileStat), 0);

    store = nullptr;
}

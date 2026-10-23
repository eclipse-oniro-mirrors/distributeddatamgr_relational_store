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

namespace {
constexpr int32_t SERVICE_GID = 3012;
const std::string TEST_DIR = RDB_TEST_PATH + "dump_stack_acl_test/";

class DumpStackAclTest : public testing::Test {
public:
    static void SetUpTestCase(void){};
    static void TearDownTestCase(void){};
    void SetUp(void)
    {
        mkdir(TEST_DIR.c_str(), S_IRWXU | S_IRWXG | S_IRWXO);
    };
    void TearDown(void) { RdbHelper::ClearCache(); }
};

class DumpStackAclCallback : public RdbOpenCallback {
public:
    int OnCreate(RdbStore &store) override;
    int OnUpgrade(RdbStore &store, int oldVersion, int newVersion) override;
};

int DumpStackAclCallback::OnCreate(RdbStore &store)
{
    return E_OK;
}

int DumpStackAclCallback::OnUpgrade(RdbStore &store, int oldVersion, int newVersion)
{
    return E_OK;
}
} // namespace

/**
 * @tc.name: DumpStackAcl_GrantedWhenEnabled_001
 * @tc.desc: test dumpStackOnFileDel grants service ACL through the existing isNeedSetAcl path
 * @tc.type: FUNC
 */
HWTEST_F(DumpStackAclTest, DumpStackAcl_GrantedWhenEnabled_001, TestSize.Level1)
{
    const std::string dbPath = TEST_DIR + "acl_on.db";
    RdbStoreConfig config(dbPath);
    config.EnableDumpStackOnFileDel(true);
    DumpStackAclCallback helper;
    int errCode = E_ERROR;
    std::shared_ptr<RdbStore> store = RdbHelper::GetRdbStore(config, 1, helper, errCode);
    ASSERT_NE(store, nullptr);
    EXPECT_EQ(errCode, E_OK);

    // BR-8: the flag reuses the existing isNeedSetAcl/SetFileGid path, so the db file
    // must be authorized for the service group (same surface as distributed sync).
    EXPECT_TRUE(SqliteUtils::HasAccessAcl(dbPath, SERVICE_GID));
    // wal/shm are lazily created; assert them only when they exist.
    struct stat fileStat;
    if (stat((dbPath + "-wal").c_str(), &fileStat) == 0) {
        EXPECT_TRUE(SqliteUtils::HasAccessAcl(dbPath + "-wal", SERVICE_GID));
    }
    if (stat((dbPath + "-shm").c_str(), &fileStat) == 0) {
        EXPECT_TRUE(SqliteUtils::HasAccessAcl(dbPath + "-shm", SERVICE_GID));
    }

    store = nullptr;
    RdbHelper::DeleteRdbStore(dbPath);
}

/**
 * @tc.name: DumpStackAcl_NotGrantedByDefault_002
 * @tc.desc: test service ACL is not granted when dumpStackOnFileDel is not set
 * @tc.type: FUNC
 */
HWTEST_F(DumpStackAclTest, DumpStackAcl_NotGrantedByDefault_002, TestSize.Level1)
{
    const std::string dbPath = TEST_DIR + "acl_off.db";
    RdbStoreConfig config(dbPath);
    DumpStackAclCallback helper;
    int errCode = E_ERROR;
    std::shared_ptr<RdbStore> store = RdbHelper::GetRdbStore(config, 1, helper, errCode);
    ASSERT_NE(store, nullptr);
    EXPECT_EQ(errCode, E_OK);
    EXPECT_FALSE(SqliteUtils::HasAccessAcl(dbPath, SERVICE_GID));

    store = nullptr;
    RdbHelper::DeleteRdbStore(dbPath);
}

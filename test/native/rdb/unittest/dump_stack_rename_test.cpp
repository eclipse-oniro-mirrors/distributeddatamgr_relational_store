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
static constexpr const char *TEST_DIR = "/data/test/dump_stack_rename_test/";
static constexpr const char *CREATE_TABLE_TEST =
    "CREATE TABLE IF NOT EXISTS test (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT NOT NULL)";
namespace RdbTest {
class DumpStackRenameTest : public testing::Test {
public:
    static void SetUpTestCase(void){};
    static void TearDownTestCase(void){};
    void SetUp(void)
    {
        mkdir(TEST_DIR, S_IRWXU | S_IRWXG | S_IRWXO);
    }
    void TearDown(void) { RdbHelper::ClearCache(); }
};

class DumpStackRenameCallback : public RdbOpenCallback {
public:
    int OnCreate(RdbStore &store) override;
    int OnUpgrade(RdbStore &store, int oldVersion, int newVersion) override;
};

int DumpStackRenameCallback::OnCreate(RdbStore &store)
{
    return store.ExecuteSql(CREATE_TABLE_TEST);
}

int DumpStackRenameCallback::OnUpgrade(RdbStore &store, int oldVersion, int newVersion)
{
    return E_OK;
}
} // namespace RdbTest
using namespace RdbTest;

/**
 * @tc.name: DumpStackRename_RenameWithTaggedFile_001
 * @tc.desc: test SqliteUtils::RenameFile succeeds on a dump-stack-enabled db file
 * @tc.type: FUNC
 */
HWTEST_F(DumpStackRenameTest, DumpStackRename_RenameWithTaggedFile_001, TestSize.Level1)
{
    const std::string dbPath = std::string(TEST_DIR) + "rename_src.db";
    const std::string destPath = std::string(TEST_DIR) + "rename_dest.db";
    RdbStoreConfig config(dbPath);
    config.EnableDumpStackOnFileDel(true);
    DumpStackRenameCallback helper;
    int errCode = E_ERROR;
    std::shared_ptr<RdbStore> store = RdbHelper::GetRdbStore(config, 1, helper, errCode);
    ASSERT_NE(store, nullptr);
    EXPECT_EQ(errCode, E_OK);

    // Write a row so the db file has real content to carry across the rename.
    int64_t id = 0;
    ValuesBucket values;
    values.PutInt("id", 1);
    values.PutString("name", std::string("before_rename"));
    EXPECT_EQ(store->Insert(id, "test", values), E_OK);

    store = nullptr;
    RdbHelper::ClearCache();

    // BR-4/R9: rename triggers the HMFS dump-stack for tagged key files (the source
    // disappears from its monitored path, the destination is replaced), so tags must be
    // cleared before renaming. On non-HMFS hosts the GETFLAGS ioctl degrades silently
    // (BR-5/EX-1) and the rename must still succeed.
    EXPECT_TRUE(SqliteUtils::RenameFile(dbPath, destPath));
    struct stat fileStat;
    EXPECT_LT(stat(dbPath.c_str(), &fileStat), 0);
    EXPECT_EQ(stat(destPath.c_str(), &fileStat), 0);

    // Rename the wal and shm files too if they exist (same chokepoint, BR-4).
    if (stat((dbPath + "-wal").c_str(), &fileStat) == 0) {
        EXPECT_TRUE(SqliteUtils::RenameFile(dbPath + "-wal", destPath + "-wal"));
    }
    if (stat((dbPath + "-shm").c_str(), &fileStat) == 0) {
        EXPECT_TRUE(SqliteUtils::RenameFile(dbPath + "-shm", destPath + "-shm"));
    }

    RdbHelper::DeleteRdbStore(destPath);
}

/**
 * @tc.name: DumpStackRename_RenameUntaggedFile_002
 * @tc.desc: test SqliteUtils::RenameFile on an untagged file skips tag clearing (EX-6)
 * @tc.type: FUNC
 */
HWTEST_F(DumpStackRenameTest, DumpStackRename_RenameUntaggedFile_002, TestSize.Level1)
{
    const std::string srcPath = std::string(TEST_DIR) + "untagged_src.db";
    const std::string destPath = std::string(TEST_DIR) + "untagged_dest.db";

    // Create a plain db file without the dump-stack flag (no tag ever set).
    RdbStoreConfig config(srcPath);
    DumpStackRenameCallback helper;
    int errCode = E_ERROR;
    std::shared_ptr<RdbStore> store = RdbHelper::GetRdbStore(config, 1, helper, errCode);
    ASSERT_NE(store, nullptr);
    EXPECT_EQ(errCode, E_OK);
    store = nullptr;
    RdbHelper::ClearCache();

    // EX-6: the file was never tagged; GETFLAGS returns negative (non-HMFS) or FLAG_UNSET,
    // so ClearKeyFileFlag is skipped and the rename proceeds normally.
    EXPECT_TRUE(SqliteUtils::RenameFile(srcPath, destPath));
    struct stat fileStat;
    EXPECT_LT(stat(srcPath.c_str(), &fileStat), 0);
    EXPECT_EQ(stat(destPath.c_str(), &fileStat), 0);

    RdbHelper::DeleteRdbStore(destPath);
}

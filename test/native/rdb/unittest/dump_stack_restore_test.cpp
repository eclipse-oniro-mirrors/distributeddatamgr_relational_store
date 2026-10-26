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
class DumpStackRestoreTest : public testing::Test {
public:
    static constexpr const char *TEST_DIR = "/data/test/dump_stack_restore_test/";
    static constexpr const char *BACKUP_NAME = "dump_stack_backup.db";
    static void SetUpTestCase(void){};
    static void TearDownTestCase(void){};
    void SetUp(void)
    {
        mkdir(TEST_DIR, S_IRWXU | S_IRWXG | S_IRWXO);
    }
    void TearDown(void) { RdbHelper::ClearCache(); }
};

class DumpStackRestoreCallback : public RdbOpenCallback {
public:
    static constexpr const char *CREATE_TABLE_TEST =
        "CREATE TABLE IF NOT EXISTS test (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT NOT NULL)";
    int OnCreate(RdbStore &store) override;
    int OnUpgrade(RdbStore &store, int oldVersion, int newVersion) override;
};

int DumpStackRestoreCallback::OnCreate(RdbStore &store)
{
    return store.ExecuteSql(DumpStackRestoreCallback::CREATE_TABLE_TEST);
}

int DumpStackRestoreCallback::OnUpgrade(RdbStore &store, int oldVersion, int newVersion)
{
    return E_OK;
}
} // namespace RdbTest
using namespace RdbTest;

/**
 * @tc.name: DumpStackRestore_BackupRestoreWithFlag_001
 * @tc.desc: test backup/restore flow succeeds on a dump-stack-enabled store
 * @tc.type: FUNC
 */
HWTEST_F(DumpStackRestoreTest, DumpStackRestore_BackupRestoreWithFlag_001, TestSize.Level1)
{
    const std::string dbPath = std::string(TEST_DIR) + "restore_on.db";
    RdbStoreConfig config(dbPath);
    config.EnableDumpStackOnFileDel(true);
    DumpStackRestoreCallback helper;
    int errCode = E_ERROR;
    std::shared_ptr<RdbStore> store = RdbHelper::GetRdbStore(config, 1, helper, errCode);
    ASSERT_NE(store, nullptr);
    EXPECT_EQ(errCode, E_OK);

    // AC-2.2/2.3/BR-4: the restore flow replaces/deletes old files; on non-HMFS hosts the
    // tag-clear ioctl degrades silently (BR-5) and the flow must still succeed. Tag
    // lifecycle (cleared before delete, re-tagged via the Enable notification after
    // restore) is validated on HMFS devices (device integration).
    int ret = store->Backup(DumpStackRestoreTest::BACKUP_NAME);
    EXPECT_EQ(ret, E_OK);
    ret = store->Restore(DumpStackRestoreTest::BACKUP_NAME);
    EXPECT_EQ(ret, E_OK);

    // The restored store must remain usable.
    int64_t id = 0;
    ValuesBucket values;
    values.PutInt("id", 1);
    values.PutString("name", std::string("after_restore"));
    EXPECT_EQ(store->Insert(id, "test", values), E_OK);

    store = nullptr;
    RdbHelper::DeleteRdbStore(dbPath);
}

/**
 * @tc.name: DumpStackRestore_RestoreWithoutFlag_002
 * @tc.desc: test restore flow on a store without the dump-stack flag keeps legacy behavior
 * @tc.type: FUNC
 */
HWTEST_F(DumpStackRestoreTest, DumpStackRestore_RestoreWithoutFlag_002, TestSize.Level1)
{
    const std::string dbPath = std::string(TEST_DIR) + "restore_off.db";
    RdbStoreConfig config(dbPath);
    DumpStackRestoreCallback helper;
    int errCode = E_ERROR;
    std::shared_ptr<RdbStore> store = RdbHelper::GetRdbStore(config, 1, helper, errCode);
    ASSERT_NE(store, nullptr);
    EXPECT_EQ(errCode, E_OK);

    int ret = store->Backup(DumpStackRestoreTest::BACKUP_NAME);
    EXPECT_EQ(ret, E_OK);
    ret = store->Restore(DumpStackRestoreTest::BACKUP_NAME);
    EXPECT_EQ(ret, E_OK);

    store = nullptr;
    RdbHelper::DeleteRdbStore(dbPath);
}

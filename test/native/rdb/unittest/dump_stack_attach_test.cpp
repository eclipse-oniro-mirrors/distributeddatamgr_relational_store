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
class DumpStackAttachTest : public testing::Test {
public:
    static constexpr const char *TEST_DIR = "/data/test/dump_stack_attach_test/";
    static constexpr const char *ATTACH_NAME = "attached_db";
    static void SetUpTestCase(void){};
    static void TearDownTestCase(void){};
    void SetUp(void)
    {
        mkdir(TEST_DIR, S_IRWXU | S_IRWXG | S_IRWXO);
    }
    void TearDown(void) { RdbHelper::ClearCache(); }
};

class DumpStackAttachCallback : public RdbOpenCallback {
public:
    static constexpr const char *CREATE_TABLE_TEST =
        "CREATE TABLE IF NOT EXISTS test (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT NOT NULL)";
    int OnCreate(RdbStore &store) override;
    int OnUpgrade(RdbStore &store, int oldVersion, int newVersion) override;
};

int DumpStackAttachCallback::OnCreate(RdbStore &store)
{
    return store.ExecuteSql(DumpStackAttachCallback::CREATE_TABLE_TEST);
}

int DumpStackAttachCallback::OnUpgrade(RdbStore &store, int oldVersion, int newVersion)
{
    return E_OK;
}
} // namespace RdbTest
using namespace RdbTest;

/**
 * @tc.name: DumpStackAttach_ModeSwitchWithFlag_001
 * @tc.desc: test Attach/Detach mode switch succeeds on a dump-stack-enabled store
 * @tc.type: FUNC
 */
HWTEST_F(DumpStackAttachTest, DumpStackAttach_ModeSwitchWithFlag_001, TestSize.Level1)
{
    const std::string mainPath = std::string(TEST_DIR) + "attach_main.db";
    const std::string attachedPath = std::string(TEST_DIR) + "attach_target.db";
    RdbStoreConfig mainConfig(mainPath);
    mainConfig.EnableDumpStackOnFileDel(true);
    DumpStackAttachCallback helper;
    int errCode = E_ERROR;
    std::shared_ptr<RdbStore> store = RdbHelper::GetRdbStore(mainConfig, 1, helper, errCode);
    ASSERT_NE(store, nullptr);
    EXPECT_EQ(errCode, E_OK);

    // Prepare the attached database.
    RdbStoreConfig attachedConfig(attachedPath);
    std::shared_ptr<RdbStore> attachedStore = RdbHelper::GetRdbStore(attachedConfig, 1, helper, errCode);
    ASSERT_NE(attachedStore, nullptr);
    EXPECT_EQ(errCode, E_OK);
    attachedStore = nullptr;
    RdbHelper::ClearCache();

    // AC-2.5/BR-4: the mode switch clears tags before SQLite removes wal/shm and the
    // flow must succeed on non-HMFS hosts (silent degrade, BR-5). Re-tag notification
    // and tag state assertions are validated on HMFS devices (device integration).
    auto [attachErr, count] = store->Attach(attachedConfig, ATTACH_NAME);
    EXPECT_EQ(attachErr, E_OK);
    EXPECT_EQ(count, 1);

    auto [detachErr, detachCount] = store->Detach(ATTACH_NAME);
    EXPECT_EQ(detachErr, E_OK);
    EXPECT_EQ(detachCount, 0);

    // The main store must remain usable after the mode switch round-trip.
    int64_t id = 0;
    ValuesBucket values;
    values.PutInt("id", 1);
    values.PutString("name", std::string("after_attach"));
    EXPECT_EQ(store->Insert(id, "test", values), E_OK);

    store = nullptr;
    RdbHelper::DeleteRdbStore(mainPath);
    RdbHelper::DeleteRdbStore(attachedPath);
}

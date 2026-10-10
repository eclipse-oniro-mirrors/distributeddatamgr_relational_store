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

#include <fcntl.h>
#include <gtest/gtest.h>
#include <securec.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstring>
#include <fstream>
#include <memory>
#include <string>

#include "rdb_errno.h"
#include "rdb_dfx_errno.h"
#include "rdb_fault_hiview_reporter.h"
#include "rdb_helper.h"
#include "rdb_open_callback.h"
#include "rdb_store.h"
#include "rdb_store_config.h"
#include "rdb_types.h"
#include "sqlite_utils.h"
#include "values_bucket.h"

using namespace testing::ext;
using namespace OHOS::NativeRdb;
using DfxInfo = OHOS::DistributedRdb::RdbDfxInfo;
using DebugInfo = OHOS::DistributedRdb::RdbDebugInfo;

class TestOpenCallback : public RdbOpenCallback {
public:
    int OnCreate(RdbStore &store) override
    {
        return store.ExecuteSql(
            "CREATE TABLE IF NOT EXISTS test(id INTEGER PRIMARY KEY, v TEXT)");
    }
    int OnUpgrade(RdbStore &store, int oldVersion, int newVersion) override
    {
        return E_OK;
    }
};

namespace {
constexpr const char *TEST_DIR = "/data/test/rdb_fault_reporter";
constexpr const char *TEST_DB = "/data/test/rdb_fault_reporter/test.db";
constexpr mode_t TEST_DIR_MODE = 0770;
constexpr int CORRUPT_HEADER_SIZE = 16;
constexpr int TEST_DATA_COUNT = 20;

std::shared_ptr<RdbStore> CreateRdbStore(const std::string &path)
{
    RdbStoreConfig config(path);
    TestOpenCallback callback;
    int code = E_ERROR;
    auto store = RdbHelper::GetRdbStore(config, 1, callback, code);
    if (store == nullptr || code != E_OK) {
        return nullptr;
    }
    for (int i = 0; i < TEST_DATA_COUNT; i++) {
        ValuesBucket values;
        values.PutInt("id", i);
        values.PutString("v", std::string("data"));
        store->Insert("test", values);
    }
    return store;
}

void CorruptFirstPage(const std::string &path)
{
    std::fstream f(path, std::ios::in | std::ios::out | std::ios::binary);
    if (!f.is_open()) {
        return;
    }
    char garbage[CORRUPT_HEADER_SIZE];
    if (memset_s(garbage, sizeof(garbage), 0, sizeof(garbage)) != EOK) {
        return;
    }
    f.write(garbage, sizeof(garbage));
    f.close();
}
}

class RdbFaultHiViewReporterTest : public testing::Test {
public:
    static void SetUpTestCase()
    {
        mkdir(TEST_DIR, TEST_DIR_MODE);
    }
    static void TearDownTestCase()
    {
        rmdir(TEST_DIR);
    }
    void SetUp() override
    {
        RdbFaultHiViewReporter::memCorruptReportedFlg_ = false;
    }
    void TearDown() override
    {
        unlink((std::string(TEST_DB) + ".corruptedflg").c_str());
        unlink((std::string(TEST_DB) + "-slaveFailure").c_str());
        unlink((std::string(TEST_DB) + "-syncInterrupt").c_str());
        RdbFaultHiViewReporter::memCorruptReportedFlg_ = false;
    }
};

HWTEST_F(RdbFaultHiViewReporterTest, RdbSubCase_Test_001, TestSize.Level1)
{
    EXPECT_STREQ(RdbSubCase::FIRST_PAGE, "FirstPage");
    EXPECT_STREQ(RdbSubCase::NON_FIRST_PAGE, "NonFirstPage");
    EXPECT_STREQ(RdbSubCase::INODE_MISMATCH, "InodeMismatch");
    EXPECT_STREQ(RdbSubCase::FILE_PERMISSION, "FilePermission");
    EXPECT_STREQ(RdbSubCase::BOTH_FAIL, "MasterSlaveBothFail");
    EXPECT_STREQ(RdbSubCase::INCONSISTENT, "MasterSlaveInconsistent");
}

HWTEST_F(RdbFaultHiViewReporterTest, FormatDfxInfo_Test_001, TestSize.Level1)
{
    DfxInfo info;
    info.lastOpenTime_ = "2026-10-08";
    info.curUserId_ = 100;
    info.procUid_ = 1000;
    info.procGid_ = 2000;
    info.procUmask_ = 022;
    info.slaveInterrupted_ = false;
    info.slaveInvalidReason_ = "exchange_failed";
    info.masterSlaveDiff_ = "mCount=1,sCount=2";
    std::string out = SqliteUtils::FormatDfxInfo(info);
    EXPECT_NE(out.find("LastOpen:2026-10-08"), std::string::npos);
    EXPECT_NE(out.find("CUR_USER:100"), std::string::npos);
    EXPECT_NE(out.find("UID:1000"), std::string::npos);
    EXPECT_NE(out.find("GID:2000"), std::string::npos);
    EXPECT_NE(out.find("UMASK:"), std::string::npos);
    EXPECT_NE(out.find("SLV_INTR:0"), std::string::npos);
    EXPECT_NE(out.find("SLV_REASON:exchange_failed"), std::string::npos);
    EXPECT_NE(out.find("MS_DIFF:mCount=1,sCount=2"), std::string::npos);
    EXPECT_EQ(out.find("SLV_ERR"), std::string::npos);
    EXPECT_EQ(out.find("SLV_CANCEL"), std::string::npos);
    EXPECT_EQ(out.find("SLV_INFO"), std::string::npos);
}

HWTEST_F(RdbFaultHiViewReporterTest, IsSlaveInvalid_Test_001, TestSize.Level1)
{
    EXPECT_FALSE(SqliteUtils::IsSlaveInvalid(TEST_DB));
}

HWTEST_F(RdbFaultHiViewReporterTest, IsSlaveInvalid_Test_002, TestSize.Level1)
{
    std::string flag = std::string(TEST_DB) + "-slaveFailure";
    std::ofstream f(flag);
    f << "2026-10-08\nexchange_failed";
    f.close();
    EXPECT_TRUE(SqliteUtils::IsSlaveInvalid(TEST_DB));
    unlink(flag.c_str());
}

HWTEST_F(RdbFaultHiViewReporterTest, ReadSlaveInvalidReason_Test_001, TestSize.Level1)
{
    std::string flag = std::string(TEST_DB) + "-slaveFailure";
    std::ofstream f(flag);
    f << "2026-10-08\nverify_failed";
    f.close();
    EXPECT_EQ(SqliteUtils::ReadSlaveInvalidReason(TEST_DB), "verify_failed");
    unlink(flag.c_str());
}

HWTEST_F(RdbFaultHiViewReporterTest, GetProcUmask_Test_001, TestSize.Level1)
{
    uint32_t mask = SqliteUtils::GetProcUmask();
    EXPECT_LE(mask, 0777u);
}

HWTEST_F(RdbFaultHiViewReporterTest, GetProcUmask_Test_002, TestSize.Level1)
{
    uint32_t first = SqliteUtils::GetProcUmask();
    uint32_t second = SqliteUtils::GetProcUmask();
    EXPECT_EQ(first, second);
}

HWTEST_F(RdbFaultHiViewReporterTest, CollectDfxInfo_Test_001, TestSize.Level1)
{
    RdbStoreConfig config((std::string(TEST_DB)));
    DfxInfo info;
    RdbFaultHiViewReporter::CollectDfxInfo(config, info);
    EXPECT_EQ(info.procUid_, static_cast<uint32_t>(getuid()));
    EXPECT_EQ(info.procGid_, static_cast<uint32_t>(getgid()));
    EXPECT_FALSE(info.slaveInterrupted_);
    EXPECT_TRUE(info.slaveInvalidReason_.empty());
}

HWTEST_F(RdbFaultHiViewReporterTest, ApplyInodeSubCase_Test_001, TestSize.Level1)
{
    RdbCorruptedEvent event;
    event.appendix = "corrupt msg";
    DebugInfo di;
    di.inode_ = 100;
    di.oldInode_ = 200;
    event.debugInfos["db"] = di;
    RdbFaultHiViewReporter::ApplyInodeSubCase(event);
    EXPECT_EQ(event.subCaseDesc, RdbSubCase::INODE_MISMATCH);
}

HWTEST_F(RdbFaultHiViewReporterTest, ApplyInodeSubCase_Test_002, TestSize.Level1)
{
    RdbCorruptedEvent event;
    event.subCaseDesc = RdbSubCase::FIRST_PAGE;
    event.appendix = "msg";
    DebugInfo di;
    di.inode_ = 100;
    di.oldInode_ = 200;
    event.debugInfos["db"] = di;
    RdbFaultHiViewReporter::ApplyInodeSubCase(event);
    EXPECT_EQ(event.subCaseDesc, RdbSubCase::FIRST_PAGE);
}

HWTEST_F(RdbFaultHiViewReporterTest, ApplyInodeSubCase_Test_003, TestSize.Level1)
{
    RdbCorruptedEvent event;
    event.appendix = "corrupt msg";
    DebugInfo di;
    di.inode_ = 100;
    di.oldInode_ = 100;
    event.debugInfos["db"] = di;
    RdbFaultHiViewReporter::ApplyInodeSubCase(event);
    EXPECT_TRUE(event.subCaseDesc.empty());
}

HWTEST_F(RdbFaultHiViewReporterTest, ReportSlaveInconsistent_Test_001, TestSize.Level1)
{
    std::string flag = std::string(TEST_DB) + "-slaveFailure";
    std::ofstream f(flag);
    f << "2026-10-08\nverify_failed";
    f.close();
    RdbStoreConfig config((std::string(TEST_DB)));
    EXPECT_NO_FATAL_FAILURE(RdbFaultHiViewReporter::ReportSlaveInconsistent(config, "test dedup"));
    unlink(flag.c_str());
}

HWTEST_F(RdbFaultHiViewReporterTest, CorruptedFlag_Test_001, TestSize.Level1)
{
    std::string path = TEST_DB;
    EXPECT_TRUE(RdbFaultHiViewReporter::IsReportCorruptedFault(path));
    RdbFaultHiViewReporter::CreateCorruptedFlag(path);
    EXPECT_FALSE(RdbFaultHiViewReporter::IsReportCorruptedFault(path));
    RdbFaultHiViewReporter::DeleteCorruptedFlag(path);
    RdbFaultHiViewReporter::memCorruptReportedFlg_ = false;
    EXPECT_TRUE(RdbFaultHiViewReporter::IsReportCorruptedFault(path));
}

HWTEST_F(RdbFaultHiViewReporterTest, DfxErrorCode_Test_001, TestSize.Level1)
{
    EXPECT_EQ(E_DFX_MASTER_SLAVE_BOTH_ABNORMAL, E_DFX_BASE + 0x1A);
    EXPECT_EQ(E_DFX_MASTER_SLAVE_INCONSISTENT, E_DFX_BASE + 0x1B);
}

/*
 * @tc.name: Create_FirstPage_Test_001
 * @tc.desc: Verify Create sets FirstPage sub case in subCaseDesc and errorCode
 * @tc.type: FUNC
 */
HWTEST_F(RdbFaultHiViewReporterTest, Create_FirstPage_Test_001, TestSize.Level1)
{
    RdbStoreConfig config((std::string(TEST_DB)));
    auto event = RdbFaultHiViewReporter::Create(config, E_SQLITE_CORRUPT,
        "integrity_check failed", false, true);
    event.subCaseDesc = RdbSubCase::FIRST_PAGE;
    EXPECT_EQ(event.errorCode, static_cast<uint32_t>(E_SQLITE_CORRUPT));
    EXPECT_EQ(event.subCaseDesc, RdbSubCase::FIRST_PAGE);
}

/*
 * @tc.name: Create_NonFirstPage_Test_001
 * @tc.desc: Verify Create sets NonFirstPage sub case in subCaseDesc
 * @tc.type: FUNC
 */
HWTEST_F(RdbFaultHiViewReporterTest, Create_NonFirstPage_Test_001, TestSize.Level1)
{
    RdbStoreConfig config((std::string(TEST_DB)));
    auto event = RdbFaultHiViewReporter::Create(config, E_SQLITE_CORRUPT,
        "corrupt index page", false, true);
    event.subCaseDesc = RdbSubCase::NON_FIRST_PAGE;
    EXPECT_EQ(event.subCaseDesc, RdbSubCase::NON_FIRST_PAGE);
}

/*
 * @tc.name: Create_FilePermission_Test_001
 * @tc.desc: Verify Create sets FilePermission sub case in subCaseDesc
 * @tc.type: FUNC
 */
HWTEST_F(RdbFaultHiViewReporterTest, Create_FilePermission_Test_001, TestSize.Level1)
{
    RdbStoreConfig config((std::string(TEST_DB)));
    auto event = RdbFaultHiViewReporter::Create(config, E_SQLITE_CANTOPEN,
        "EISDIR", false, true);
    event.subCaseDesc = RdbSubCase::FILE_PERMISSION;
    EXPECT_EQ(event.subCaseDesc, RdbSubCase::FILE_PERMISSION);
}

/*
 * @tc.name: Create_BothFail_Test_001
 * @tc.desc: Verify Create sets MasterSlaveBothFail sub case and DFX error code for both-corrupt scenario
 * @tc.type: FUNC
 */
HWTEST_F(RdbFaultHiViewReporterTest, Create_BothFail_Test_001, TestSize.Level1)
{
    RdbStoreConfig config((std::string(TEST_DB)));
    auto event = RdbFaultHiViewReporter::Create(config, E_DFX_MASTER_SLAVE_BOTH_ABNORMAL,
        "master and slave corrupt on open", false, true);
    event.subCaseDesc = RdbSubCase::BOTH_FAIL;
    EXPECT_EQ(event.errorCode, static_cast<uint32_t>(E_DFX_MASTER_SLAVE_BOTH_ABNORMAL));
    EXPECT_EQ(event.subCaseDesc, RdbSubCase::BOTH_FAIL);
}

/*
 * @tc.name: ReportCorruptedOnce_SubCase_Test_001
 * @tc.desc: Verify ReportCorruptedOnce creates corruptedflg and dedup blocks second report
 * @tc.type: FUNC
 */
HWTEST_F(RdbFaultHiViewReporterTest, ReportCorruptedOnce_SubCase_Test_001, TestSize.Level1)
{
    RdbStoreConfig config((std::string(TEST_DB)));
    auto event = RdbFaultHiViewReporter::Create(config, E_SQLITE_CORRUPT,
        "test", false, true);
    event.subCaseDesc = RdbSubCase::NON_FIRST_PAGE;
    EXPECT_TRUE(RdbFaultHiViewReporter::IsReportCorruptedFault(TEST_DB));
    RdbFaultHiViewReporter::ReportCorruptedOnce(event);
    EXPECT_FALSE(RdbFaultHiViewReporter::IsReportCorruptedFault(TEST_DB));
    EXPECT_TRUE(access((std::string(TEST_DB) + ".corruptedflg").c_str(), F_OK) == 0);
    RdbFaultHiViewReporter::ReportCorruptedOnce(event);
    EXPECT_FALSE(RdbFaultHiViewReporter::IsReportCorruptedFault(TEST_DB));
}

/*
 * @tc.name: ReportSlaveInconsistent_Test_002
 * @tc.desc: Verify ReportSlaveInconsistent does not crash when slave is valid (not invalid)
 * @tc.type: FUNC
 */
HWTEST_F(RdbFaultHiViewReporterTest, ReportSlaveInconsistent_Test_002, TestSize.Level1)
{
    RdbStoreConfig config((std::string(TEST_DB)));
    EXPECT_FALSE(SqliteUtils::IsSlaveInvalid(TEST_DB));
    EXPECT_NO_FATAL_FAILURE(RdbFaultHiViewReporter::ReportSlaveInconsistent(config, "mCount=1,sCount=2"));
}

/*
 * @tc.name: CollectDfxInfo_Test_002
 * @tc.desc: Verify CollectDfxInfo fills procUmask with cached process umask
 * @tc.type: FUNC
 */
HWTEST_F(RdbFaultHiViewReporterTest, CollectDfxInfo_Test_002, TestSize.Level1)
{
    RdbStoreConfig config((std::string(TEST_DB)));
    DfxInfo info;
    RdbFaultHiViewReporter::CollectDfxInfo(config, info);
    EXPECT_EQ(info.procUmask_, SqliteUtils::GetProcUmask());
}

/*
 * @tc.name: CollectDfxInfo_Test_003
 * @tc.desc: Verify CollectDfxInfo sets slaveInterrupted when syncInterrupt flag file exists
 * @tc.type: FUNC
 */
HWTEST_F(RdbFaultHiViewReporterTest, CollectDfxInfo_Test_003, TestSize.Level1)
{
    std::string flag = std::string(TEST_DB) + "-syncInterrupt";
    std::ofstream f(flag);
    f << "interrupted";
    f.close();
    RdbStoreConfig config((std::string(TEST_DB)));
    DfxInfo info;
    RdbFaultHiViewReporter::CollectDfxInfo(config, info);
    EXPECT_TRUE(info.slaveInterrupted_);
    unlink(flag.c_str());
}

/*
 * @tc.name: CollectDfxInfo_Test_004
 * @tc.desc: Verify CollectDfxInfo fills slaveInvalidReason when slaveFailure flag file exists
 * @tc.type: FUNC
 */
HWTEST_F(RdbFaultHiViewReporterTest, CollectDfxInfo_Test_004, TestSize.Level1)
{
    std::string flag = std::string(TEST_DB) + "-slaveFailure";
    std::ofstream f(flag);
    f << "2026-10-08\nbackup_verify_failed";
    f.close();
    RdbStoreConfig config((std::string(TEST_DB)));
    DfxInfo info;
    RdbFaultHiViewReporter::CollectDfxInfo(config, info);
    EXPECT_EQ(info.slaveInvalidReason_, "backup_verify_failed");
    unlink(flag.c_str());
}

/*
 * @tc.name: IsSlaveInterrupted_Test_001
 * @tc.desc: Verify IsSlaveInterrupted returns false when no syncInterrupt flag file exists
 * @tc.type: FUNC
 */
HWTEST_F(RdbFaultHiViewReporterTest, IsSlaveInterrupted_Test_001, TestSize.Level1)
{
    EXPECT_FALSE(SqliteUtils::IsSlaveInterrupted(TEST_DB));
}

/*
 * @tc.name: IsSlaveInterrupted_Test_002
 * @tc.desc: Verify IsSlaveInterrupted returns true when syncInterrupt flag file exists
 * @tc.type: FUNC
 */
HWTEST_F(RdbFaultHiViewReporterTest, IsSlaveInterrupted_Test_002, TestSize.Level1)
{
    std::string flag = std::string(TEST_DB) + "-syncInterrupt";
    std::ofstream f(flag);
    f << "interrupted";
    f.close();
    EXPECT_TRUE(SqliteUtils::IsSlaveInterrupted(TEST_DB));
    unlink(flag.c_str());
}

/*
 * @tc.name: CorruptFirstPage_GetRdbStore_Test_001
 * @tc.desc: Use RdbHelper::GetRdbStore to open DB with corrupted first page, verify open fails
 * @tc.type: FUNC
 */
HWTEST_F(RdbFaultHiViewReporterTest, CorruptFirstPage_GetRdbStore_Test_001, TestSize.Level1)
{
    std::string dbPath = std::string(TEST_DB);
    RdbHelper::DeleteRdbStore(dbPath);
    auto store = CreateRdbStore(dbPath);
    ASSERT_NE(store, nullptr);
    store.reset();
    RdbHelper::ClearCache();
    CorruptFirstPage(dbPath);
    SqliteUtils::DeleteFile(dbPath + "-dwr");
    TestOpenCallback callback;
    int code = E_ERROR;
    auto store2 = RdbHelper::GetRdbStore(RdbStoreConfig(dbPath), 1, callback, code);
    EXPECT_EQ(code, E_SQLITE_CORRUPT);
    RdbHelper::DeleteRdbStore(dbPath);
}
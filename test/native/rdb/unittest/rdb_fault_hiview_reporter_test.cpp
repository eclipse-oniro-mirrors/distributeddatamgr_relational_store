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
#include <sys/stat.h>
#include <unistd.h>

#include <fstream>
#include <string>

#include "rdb_dfx_errno.h"
#include "rdb_fault_hiview_reporter.h"
#include "rdb_store_config.h"
#include "rdb_types.h"
#include "sqlite_utils.h"

using namespace testing::ext;
using namespace OHOS::NativeRdb;
using DfxInfo = OHOS::DistributedRdb::RdbDfxInfo;
using DebugInfo = OHOS::DistributedRdb::RdbDebugInfo;

namespace {
constexpr const char *TEST_DIR = "/data/test/rdb_fault_reporter";
constexpr const char *TEST_DB = "/data/test/rdb_fault_reporter/test.db";
constexpr mode_t TEST_DIR_MODE = 0770;
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
        RdbFaultHiViewReporter::memCorruptReportedFlg_ = false;
    }
};

HWTEST_F(RdbFaultHiViewReporterTest, RdbSubCase_Test_001, TestSize.Level1)
{
    EXPECT_STREQ(RdbSubCase::TAG, "SubCase:");
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
    info.slaveHasError_ = true;
    info.slaveInterrupted_ = false;
    info.slaveCancelled_ = false;
    info.slaveInvalidReason_ = "exchange_failed";
    info.masterSlaveDiff_ = "mCount=1,sCount=2";
    std::string out = SqliteUtils::FormatDfxInfo(info);
    EXPECT_NE(out.find("LastOpen:2026-10-08"), std::string::npos);
    EXPECT_NE(out.find("CUR_USER:100"), std::string::npos);
    EXPECT_NE(out.find("UID:1000"), std::string::npos);
    EXPECT_NE(out.find("GID:2000"), std::string::npos);
    EXPECT_NE(out.find("UMASK:"), std::string::npos);
    EXPECT_NE(out.find("SLV_ERR:1"), std::string::npos);
    EXPECT_NE(out.find("SLV_INTR:0"), std::string::npos);
    EXPECT_NE(out.find("SLV_CANCEL:0"), std::string::npos);
    EXPECT_NE(out.find("SLV_REASON:exchange_failed"), std::string::npos);
    EXPECT_NE(out.find("MS_DIFF:mCount=1,sCount=2"), std::string::npos);
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

HWTEST_F(RdbFaultHiViewReporterTest, ApplySubCase_Test_001, TestSize.Level1)
{
    RdbCorruptedEvent event;
    DebugInfo di;
    di.inode_ = 100;
    di.oldInode_ = 200;
    event.debugInfos["db"] = di;
    RdbFaultHiViewReporter::ApplySubCase(event);
    EXPECT_EQ(event.subCaseDesc, RdbSubCase::INODE_MISMATCH);
}

HWTEST_F(RdbFaultHiViewReporterTest, ApplySubCase_Test_002, TestSize.Level1)
{
    RdbCorruptedEvent event;
    event.subCaseDesc = RdbSubCase::FIRST_PAGE;
    DebugInfo di;
    di.inode_ = 100;
    di.oldInode_ = 200;
    event.debugInfos["db"] = di;
    RdbFaultHiViewReporter::ApplySubCase(event);
    EXPECT_EQ(event.subCaseDesc, RdbSubCase::FIRST_PAGE);
}

HWTEST_F(RdbFaultHiViewReporterTest, ApplySubCase_Test_003, TestSize.Level1)
{
    RdbCorruptedEvent event;
    DebugInfo di;
    di.inode_ = 100;
    di.oldInode_ = 100;
    event.debugInfos["db"] = di;
    RdbFaultHiViewReporter::ApplySubCase(event);
    EXPECT_TRUE(event.subCaseDesc.empty());
}

HWTEST_F(RdbFaultHiViewReporterTest, CollectDfxInfo_Test_001, TestSize.Level1)
{
    RdbStoreConfig config((std::string(TEST_DB)));
    DfxInfo info;
    RdbFaultHiViewReporter::CollectDfxInfo(config, info);
    EXPECT_EQ(info.procUid_, static_cast<uint32_t>(getuid()));
    EXPECT_EQ(info.procGid_, static_cast<uint32_t>(getgid()));
    EXPECT_FALSE(info.slaveHasError_);
    EXPECT_FALSE(info.slaveInterrupted_);
    EXPECT_TRUE(info.slaveInvalidReason_.empty());
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
    EXPECT_EQ(E_DFX_MASTER_SLAVE_BOTH_FAIL_OPEN, E_DFX_BASE + 0x1A);
    EXPECT_EQ(E_DFX_MASTER_SLAVE_INCONSISTENT, E_DFX_BASE + 0x1C);
}

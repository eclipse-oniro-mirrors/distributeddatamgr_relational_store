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

#include "hmfs_tag_util.h"

#include <gtest/gtest.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <string>

#include "common.h"

using namespace testing::ext;
using namespace OHOS::NativeRdb;

namespace {
class HmfsTagUtilTest : public testing::Test {
public:
    static void SetUpTestCase(void){};
    static void TearDownTestCase(void){};
    void SetUp(void){};
    void TearDown(void){};
};

const std::string TEST_DIR = RDB_TEST_PATH + "hmfs_tag_util_test/";
} // namespace

/**
 * @tc.name: HmfsTagUtil_IsValidFileName_001
 * @tc.desc: test HmfsTagUtil IsValidFileName with valid and invalid file names
 * @tc.type: FUNC
 */
HWTEST_F(HmfsTagUtilTest, HmfsTagUtil_IsValidFileName_001, TestSize.Level1)
{
    EXPECT_TRUE(HmfsTagUtil::IsValidFileName("test.db"));
    EXPECT_TRUE(HmfsTagUtil::IsValidFileName("test.db-wal"));
    EXPECT_TRUE(HmfsTagUtil::IsValidFileName("test.db-shm"));
    EXPECT_TRUE(HmfsTagUtil::IsValidFileName(std::string(HmfsTagUtil::MAX_FILE_NAME_LEN, 'a')));

    EXPECT_FALSE(HmfsTagUtil::IsValidFileName(""));
    EXPECT_FALSE(HmfsTagUtil::IsValidFileName(std::string(HmfsTagUtil::MAX_FILE_NAME_LEN + 1, 'a')));
    EXPECT_FALSE(HmfsTagUtil::IsValidFileName("."));
    EXPECT_FALSE(HmfsTagUtil::IsValidFileName(".."));
    EXPECT_FALSE(HmfsTagUtil::IsValidFileName("dir/test.db"));
    EXPECT_FALSE(HmfsTagUtil::IsValidFileName("a/../test.db"));
    EXPECT_FALSE(HmfsTagUtil::IsValidFileName("/test.db"));
}

/**
 * @tc.name: HmfsTagUtil_Degrade_001
 * @tc.desc: test HmfsTagUtil set/get/clear degrade silently on non-HMFS file systems
 * @tc.type: FUNC
 */
HWTEST_F(HmfsTagUtilTest, HmfsTagUtil_Degrade_001, TestSize.Level1)
{
    mkdir(TEST_DIR.c_str(), S_IRWXU | S_IRWXG | S_IRWXO);
    const std::string filePath = TEST_DIR + "degrade.db";
    int fd = open(filePath.c_str(), O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    ASSERT_GE(fd, 0);
    close(fd);

    // On non-HMFS file systems (unit-test environments such as ext4/tmpfs), the ioctl
    // is not supported and a negative value must be returned without crashing.
    // Behavior on real HMFS devices (0 / FLAG_SET) is validated by device integration.
    EXPECT_LT(HmfsTagUtil::SetKeyFileFlag(filePath), 0);
    EXPECT_LT(HmfsTagUtil::GetKeyFileFlag(filePath), 0);
    EXPECT_LT(HmfsTagUtil::ClearKeyFileFlag(filePath), 0);

    unlink(filePath.c_str());
    rmdir(TEST_DIR.c_str());
}

/**
 * @tc.name: HmfsTagUtil_InvalidPath_001
 * @tc.desc: test HmfsTagUtil rejects invalid paths before invoking ioctl
 * @tc.type: FUNC
 */
HWTEST_F(HmfsTagUtilTest, HmfsTagUtil_InvalidPath_001, TestSize.Level1)
{
    EXPECT_LT(HmfsTagUtil::SetKeyFileFlag(""), 0);
    EXPECT_LT(HmfsTagUtil::GetKeyFileFlag(""), 0);
    EXPECT_LT(HmfsTagUtil::ClearKeyFileFlag(""), 0);
    EXPECT_LT(HmfsTagUtil::SetKeyFileFlag("noSlash.db"), 0);
    EXPECT_LT(HmfsTagUtil::GetKeyFileFlag("/"), 0);
    EXPECT_LT(HmfsTagUtil::ClearKeyFileFlag("/trailingSlash/"), 0);
}

/**
 * @tc.name: HmfsTagUtil_DirFd_001
 * @tc.desc: test HmfsTagUtil fd-based API validates file names and degrades on non-HMFS
 * @tc.type: FUNC
 */
HWTEST_F(HmfsTagUtilTest, HmfsTagUtil_DirFd_001, TestSize.Level1)
{
    mkdir(TEST_DIR.c_str(), S_IRWXU | S_IRWXG | S_IRWXO);
    int dirFd = open(TEST_DIR.c_str(), O_RDONLY | O_DIRECTORY);
    ASSERT_GE(dirFd, 0);

    // Invalid file names must be rejected before ioctl.
    EXPECT_LT(HmfsTagUtil::SetKeyFileFlag(dirFd, ""), 0);
    EXPECT_LT(HmfsTagUtil::GetKeyFileFlag(dirFd, "../escape.db"), 0);
    EXPECT_LT(HmfsTagUtil::ClearKeyFileFlag(dirFd, std::string(300, 'a')), 0);

    // Valid file name on a non-HMFS file system degrades with a negative return.
    EXPECT_LT(HmfsTagUtil::SetKeyFileFlag(dirFd, "valid.db"), 0);
    EXPECT_LT(HmfsTagUtil::GetKeyFileFlag(dirFd, "valid.db"), 0);
    EXPECT_LT(HmfsTagUtil::ClearKeyFileFlag(dirFd, "valid.db"), 0);

    close(dirFd);
    rmdir(TEST_DIR.c_str());
}

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

#include <memory>
#include <string>

#include "common.h"
#include "grd_type_export.h"
#include "rdb_errno.h"
#include "rdb_helper.h"
#include "rdb_open_callback.h"
#include "rdb_store.h"
#include "rdb_store_config.h"

using namespace testing::ext;
using namespace OHOS::NativeRdb;

// Accessors implemented by grd_api_manager_stub.cpp (no shared header by design).
namespace OHOS {
namespace NativeRdb {
int32_t GetStubOpenCallCount();
std::string GetStubLastConfigStr();
void ResetStubState();
} // namespace NativeRdb
} // namespace OHOS

namespace {
constexpr int DB_VERSION = 1;
const std::string TABLE_DB_NAME = RDB_TEST_PATH + "threshold_table_encrypt_db.db";
const std::string COLUMN_DB_NAME = RDB_TEST_PATH + "threshold_column_encrypt_db.db";
const std::string COLUMN_ACCESS_CONTROL_DB_NAME = RDB_TEST_PATH + "row_enc_threshold_db.db";
const std::string DEFAULT_DB_NAME = RDB_TEST_PATH + "threshold_default_db.db";
constexpr const char *THRESHOLD_BUNDLE_NAME = "com.example.test_vector_threshold";

void ExpectConfigStrContains(const std::string &expected)
{
    std::string configStr = GetStubLastConfigStr();
    EXPECT_NE(configStr.find(expected), std::string::npos)
        << "expected substring \"" << expected << "\" in configStr: " << configStr;
}
} // namespace

class ThresholdEncryptOpenCallback : public RdbOpenCallback {
public:
    int OnCreate(RdbStore &store) override
    {
        (void)store;
        return E_OK;
    }

    int OnUpgrade(RdbStore &store, int currentVersion, int targetVersion) override
    {
        (void)store;
        (void)currentVersion;
        (void)targetVersion;
        return E_OK;
    }
};

class MockRdbVectorThresholdEncryptTest : public testing::Test {
public:
    static RdbStoreConfig GetTableConfig();
    static RdbStoreConfig GetColumnConfig();
    static RdbStoreConfig GetColumnAccessControlConfig();
    static RdbStoreConfig GetDefaultConfig();

    void SetUp() override;
    void TearDown() override;

protected:
    void DeleteDbFiles();

    std::shared_ptr<RdbStore> store_ = nullptr;
    ThresholdEncryptOpenCallback callback_;
};

RdbStoreConfig MockRdbVectorThresholdEncryptTest::GetTableConfig()
{
    RdbStoreConfig config(TABLE_DB_NAME);
    config.SetIsVector(true);
    config.SetEncryptionGranularity(TABLE_ENCRYPTED_CONFIG);
    config.SetBaseSecurityLevel(EL4_SECURITY_LEVEL);
    config.SetAccessControl(true);
    return config;
}

RdbStoreConfig MockRdbVectorThresholdEncryptTest::GetColumnConfig()
{
    RdbStoreConfig config(COLUMN_DB_NAME);
    config.SetIsVector(true);
    config.SetEncryptionGranularity(COLUMN_ENCRYPTED_CONFIG);
    config.SetBaseSecurityLevel(EL1_SECURITY_LEVEL);
    config.SetAccessControl(false);
    return config;
}

RdbStoreConfig MockRdbVectorThresholdEncryptTest::GetColumnAccessControlConfig()
{
    RdbStoreConfig config(COLUMN_ACCESS_CONTROL_DB_NAME);
    config.SetIsVector(true);
    config.SetEncryptionGranularity(COLUMN_ENCRYPTED_CONFIG);
    config.SetBaseSecurityLevel(EL1_SECURITY_LEVEL);
    config.SetAccessControl(true);
    return config;
}

RdbStoreConfig MockRdbVectorThresholdEncryptTest::GetDefaultConfig()
{
    RdbStoreConfig config(DEFAULT_DB_NAME);
    config.SetIsVector(true);
    config.SetBundleName(THRESHOLD_BUNDLE_NAME);
    return config;
}

void MockRdbVectorThresholdEncryptTest::SetUp()
{
    // Arrange: reset captured stub state and remove leftovers, but do not open a store.
    ResetStubState();
    RdbHelper::ClearCache();
    DeleteDbFiles();
}

void MockRdbVectorThresholdEncryptTest::TearDown()
{
    RdbHelper::ClearCache();
    store_ = nullptr;
    DeleteDbFiles();
}

void MockRdbVectorThresholdEncryptTest::DeleteDbFiles()
{
    RdbHelper::DeleteRdbStore(TABLE_DB_NAME);
    RdbHelper::DeleteRdbStore(COLUMN_DB_NAME);
    RdbHelper::DeleteRdbStore(COLUMN_ACCESS_CONTROL_DB_NAME);
    RdbHelper::DeleteRdbStore(DEFAULT_DB_NAME);
}

/**
 * @brief 验证 RdbStoreConfig 新字段默认值：加密粒度整库(0)、基础安全等级 EL2(1)、访问控制关闭
 * @since 1.0
 */
HWTEST_F(MockRdbVectorThresholdEncryptTest, Threshold_Config_Defaults_001, TestSize.Level0)
{
    // Arrange & Act: only construct a config object, without setting any new field.
    RdbStoreConfig config(DEFAULT_DB_NAME);
    // Assert: the defaults are the documented ones.
    EXPECT_EQ(config.GetEncryptionGranularity(), static_cast<int32_t>(DB_ENCRYPTED_CONFIG))
        << "unexpected default encryption granularity";
    EXPECT_EQ(config.GetBaseSecurityLevel(), static_cast<int32_t>(EL2_SECURITY_LEVEL))
        << "unexpected default base security level";
    EXPECT_FALSE(config.IsAccessControl()) << "access control should be disabled by default";
}

/**
 * @brief 验证 RdbStoreConfig 新字段 setter/getter 在配置层可正确往返
 * @since 1.0
 */
HWTEST_F(MockRdbVectorThresholdEncryptTest, Threshold_Config_Setters_002, TestSize.Level0)
{
    // Arrange & Act: set the vector flag, the column granularity and the EL1 security level.
    RdbStoreConfig config(DEFAULT_DB_NAME);
    config.SetIsVector(true);
    config.SetEncryptionGranularity(COLUMN_ENCRYPTED_CONFIG);
    config.SetBaseSecurityLevel(EL1_SECURITY_LEVEL);
    // Assert: the getters return exactly what was set.
    EXPECT_TRUE(config.IsVector()) << "SetIsVector(true) was not reflected by IsVector()";
    EXPECT_EQ(config.GetEncryptionGranularity(), static_cast<int32_t>(COLUMN_ENCRYPTED_CONFIG))
        << "encryption granularity setter/getter mismatch";
    EXPECT_EQ(config.GetBaseSecurityLevel(), static_cast<int32_t>(EL1_SECURITY_LEVEL))
        << "base security level setter/getter mismatch";
}

/**
 * @brief 验证 TABLE 加密粒度 + EL4 + 开启访问控制经打开链路写入引擎 configStr
 * @since 1.0
 */
HWTEST_F(MockRdbVectorThresholdEncryptTest, Threshold_TableGranularity_ReachesRdUtils_004, TestSize.Level0)
{
    // Arrange & Act: open the store with the TABLE/EL4/access-control-enabled config.
    int errCode = E_ERROR;
    store_ = RdbHelper::GetRdbStore(GetTableConfig(), DB_VERSION, callback_, errCode);
    ASSERT_NE(store_, nullptr) << "GetRdbStore returned null, errCode=" << errCode;
    ASSERT_EQ(errCode, E_OK) << "GetRdbStore failed, errCode=" << errCode;
    // Assert: the engine received the serialized granularity/security level/access control.
    EXPECT_GE(GetStubOpenCallCount(), 1) << "stub DBOpen was never called";
    ExpectConfigStrContains("\"encryptionGranularity\":1");
    ExpectConfigStrContains("\"baseSecurityLevel\":2");
    ExpectConfigStrContains("\"accessControl\":1");
}

/**
 * @brief 验证 COLUMN 加密粒度 + EL1 + 关闭访问控制经打开链路写入引擎 configStr
 * @since 1.0
 */
HWTEST_F(MockRdbVectorThresholdEncryptTest, Threshold_ColumnGranularity_ReachesRdUtils_007, TestSize.Level0)
{
    // Arrange & Act: open the store with the COLUMN/EL1/access-control-disabled config.
    int errCode = E_ERROR;
    store_ = RdbHelper::GetRdbStore(GetColumnConfig(), DB_VERSION, callback_, errCode);
    ASSERT_NE(store_, nullptr) << "GetRdbStore returned null, errCode=" << errCode;
    ASSERT_EQ(errCode, E_OK) << "GetRdbStore failed, errCode=" << errCode;
    // Assert: the engine received the serialized granularity/security level/access control.
    EXPECT_GE(GetStubOpenCallCount(), 1) << "stub DBOpen was never called";
    ExpectConfigStrContains("\"encryptionGranularity\":2");
    ExpectConfigStrContains("\"baseSecurityLevel\":0");
    ExpectConfigStrContains("\"accessControl\":0");
}

/**
 * @brief 验证 COLUMN 加密粒度 + EL1 + 开启访问控制的全量组合经打开链路写入引擎 configStr
 * @since 1.0
 */
HWTEST_F(MockRdbVectorThresholdEncryptTest, Threshold_ColumnFullConfig_ReachesRdUtils_011, TestSize.Level0)
{
    // Arrange & Act: open the store with the COLUMN/EL1/access-control-enabled config.
    int errCode = E_ERROR;
    store_ = RdbHelper::GetRdbStore(GetColumnAccessControlConfig(), DB_VERSION, callback_, errCode);
    ASSERT_NE(store_, nullptr) << "GetRdbStore returned null, errCode=" << errCode;
    ASSERT_EQ(errCode, E_OK) << "GetRdbStore failed, errCode=" << errCode;
    // Assert: the engine received the serialized granularity/security level/access control.
    EXPECT_GE(GetStubOpenCallCount(), 1) << "stub DBOpen was never called";
    ExpectConfigStrContains("\"encryptionGranularity\":2");
    ExpectConfigStrContains("\"baseSecurityLevel\":0");
    ExpectConfigStrContains("\"accessControl\":1");
}

/**
 * @brief 验证未设置新字段时默认值(0/1/0)仍被 RD 层序列化进引擎 configStr
 * @since 1.0
 */
HWTEST_F(MockRdbVectorThresholdEncryptTest, Threshold_DefaultConfig_ReachesRdUtils_012, TestSize.Level0)
{
    // Arrange & Act: open the store with a vector config that sets none of the new fields.
    int errCode = E_ERROR;
    store_ = RdbHelper::GetRdbStore(GetDefaultConfig(), DB_VERSION, callback_, errCode);
    ASSERT_NE(store_, nullptr) << "GetRdbStore returned null, errCode=" << errCode;
    ASSERT_EQ(errCode, E_OK) << "GetRdbStore failed, errCode=" << errCode;
    // Assert: defaults are serialized instead of being omitted.
    EXPECT_GE(GetStubOpenCallCount(), 1) << "stub DBOpen was never called";
    ExpectConfigStrContains("\"encryptionGranularity\":0");
    ExpectConfigStrContains("\"baseSecurityLevel\":1");
    ExpectConfigStrContains("\"accessControl\":0");
}

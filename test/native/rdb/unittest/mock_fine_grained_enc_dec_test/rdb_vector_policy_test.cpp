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
#include "grd_api_manager.h"
#include "grd_type_export.h"
#include "rdb_errno.h"
#include "rdb_helper.h"
#include "rdb_open_callback.h"
#include "rdb_store.h"
#include "rdb_store_config.h"
#include "rdb_store_impl.h"

using namespace testing::ext;
using namespace OHOS::NativeRdb;

// Accessors implemented by grd_api_manager_stub.cpp (no shared header by design).
namespace OHOS {
namespace NativeRdb {
int32_t GetStubOpenCallCount();
std::string GetStubLastConfigStr();
int32_t GetStubAddPolicyCallCount();
std::string GetStubLastAddPolicy();
int32_t GetStubRemovePolicyCallCount();
std::string GetStubLastRemovePolicy();
int32_t GetStubScreenLockStateCallCount();
GRD_ScreenLockState GetStubLastScreenLockState();
int32_t GetStubDeviceHealthyStateCallCount();
GRD_DeviceHealthyState GetStubLastDeviceHealthyState();
void ResetStubState();
} // namespace NativeRdb
} // namespace OHOS

namespace {
constexpr int DB_VERSION = 1;
const std::string DATABASE_NAME = RDB_TEST_PATH + "test_Policy_js_db2.db";
const std::string PERSIST_DATABASE_NAME = RDB_TEST_PATH + "policy_persist_test.db";
constexpr const char *POLICY_DB_PERMIT_APPID1 =
    R"({"name":"db_permit_appid1","effect":"PERMIT","priority":1,)"
    R"("action":["SELECT","INSERT","UPDATE","DELETE"],)"
    R"("resource":{"level":"DATABASE"},"conditions":{"APPID":["1"]}})";
constexpr const char *POLICY_DB_PERMIT_APPID1_SELECT =
    R"({"name":"db_permit_appid1","effect":"PERMIT","priority":1,)"
    R"("action":["SELECT"],"resource":{"level":"DATABASE"},)"
    R"("conditions":{"APPID":["1"]}})";
const char *g_policyNamesToRemove[] = {
    "db_permit_appid1",
    "tbl_permit_appid1",
    "db_deny_screen_locked",
    "db_permit_appid1_unlocked",
    "tbl_deny_device_nonroot",
    "policy_dup",
    "policy_persist",
};
} // namespace

class VectorPolicyOpenCallback : public RdbOpenCallback {
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

class MockRdbVectorPolicyTest : public testing::Test {
public:
    static RdbStoreConfig GetPolicyConfig();

    void SetUp() override;
    void TearDown() override;

protected:
    void RemoveAllPolicies();
    void DeleteDbFiles();

    std::shared_ptr<RdbStore> store_ = nullptr;
    VectorPolicyOpenCallback callback_;
};

RdbStoreConfig MockRdbVectorPolicyTest::GetPolicyConfig()
{
    RdbStoreConfig config(DATABASE_NAME);
    config.SetIsVector(true);
    config.SetDBType(DB_VECTOR);
    config.SetAccessControl(true);
    config.SetBundleName("com.example.test_vector_policy");
    return config;
}

void MockRdbVectorPolicyTest::SetUp()
{
    // Arrange: reset captured stub state, clear the store cache and any stale DB file.
    ResetStubState();
    RdbHelper::ClearCache();
    DeleteDbFiles();
    int errCode = E_ERROR;
    store_ = RdbHelper::GetRdbStore(GetPolicyConfig(), DB_VERSION, callback_, errCode);
    ASSERT_NE(store_, nullptr) << "GetRdbStore returned null, errCode=" << errCode;
    ASSERT_EQ(errCode, E_OK) << "GetRdbStore failed, errCode=" << errCode;
}

void MockRdbVectorPolicyTest::TearDown()
{
    if (store_ != nullptr) {
        RemoveAllPolicies();
        EXPECT_EQ(store_->SetScreenState(SCREEN_UNLOCK), E_OK) << "reset screen state failed";
        EXPECT_EQ(store_->SetDeviceHealthyState(DEVICE_UNROOT), E_OK) << "reset device healthy state failed";
    }
    RdbHelper::ClearCache();
    store_ = nullptr;
    DeleteDbFiles();
}

void MockRdbVectorPolicyTest::RemoveAllPolicies()
{
    for (const auto &policyName : g_policyNamesToRemove) {
        int errCode = store_->RemovePolicy(policyName);
        EXPECT_EQ(errCode, E_OK) << "RemovePolicy failed, policyName=" << policyName << ", errCode=" << errCode;
    }
}

void MockRdbVectorPolicyTest::DeleteDbFiles()
{
    RdbHelper::DeleteRdbStore(DATABASE_NAME);
    RdbHelper::DeleteRdbStore(PERSIST_DATABASE_NAME);
}

/**
 * @brief 验证 RdbStoreConfig 的 accessControl/encryptionGranularity/baseSecurityLevel 经打开链路到达 GRD 引擎
 * @since 1.0
 */
HWTEST_F(MockRdbVectorPolicyTest, MockRdbVectorPolicyTest_configReachesRdUtils_001, TestSize.Level0)
{
    ASSERT_NE(store_, nullptr) << "store is null";
    // Arrange: the store was opened by SetUp, which exercised the stub DBOpen path.
    // Act: read the captured engine config.
    std::string configStr = GetStubLastConfigStr();
    // Assert: the engine was opened through the stub and received the new serialized fields.
    ASSERT_GE(GetStubOpenCallCount(), 1) << "stub DBOpen was never called";
    ASSERT_FALSE(configStr.empty()) << "captured engine configStr is empty";
    EXPECT_NE(configStr.find("\"accessControl\":1"), std::string::npos) << "configStr: " << configStr;
    EXPECT_NE(configStr.find("\"encryptionGranularity\""), std::string::npos) << "configStr: " << configStr;
    EXPECT_NE(configStr.find("\"baseSecurityLevel\""), std::string::npos) << "configStr: " << configStr;
}

/**
 * @brief 验证合法 PERMIT 策略经 RdbStore::AddPolicy 原样透传到 GRD 引擎且只调用一次
 * @since 1.0
 */
HWTEST_F(MockRdbVectorPolicyTest, MockRdbVectorPolicyTest_addPolicy_normal_001, TestSize.Level0)
{
    ASSERT_NE(store_, nullptr) << "store is null";
    // Arrange: a valid DATABASE-level PERMIT policy.
    std::string policy = POLICY_DB_PERMIT_APPID1;
    // Act: add the policy through the store.
    int errCode = store_->AddPolicy(policy);
    // Assert: E_OK, exactly one engine call, byte-for-byte pass-through.
    EXPECT_EQ(errCode, E_OK) << "AddPolicy failed, errCode=" << errCode;
    EXPECT_EQ(GetStubAddPolicyCallCount(), 1) << "unexpected engine AddPolicy call count";
    EXPECT_EQ(GetStubLastAddPolicy(), policy) << "engine received policy: " << GetStubLastAddPolicy();
}

/**
 * @brief 验证 RemovePolicy 到达引擎且对不存在的策略名幂等返回 E_OK
 * @since 1.0
 */
HWTEST_F(MockRdbVectorPolicyTest, MockRdbVectorPolicyTest_removePolicy_normal_008, TestSize.Level0)
{
    ASSERT_NE(store_, nullptr) << "store is null";
    // Arrange: add a SELECT-only DATABASE-level PERMIT policy first.
    std::string policy = POLICY_DB_PERMIT_APPID1_SELECT;
    ASSERT_EQ(store_->AddPolicy(policy), E_OK) << "prepare AddPolicy failed";
    // Act: remove the existing policy.
    int errCode = store_->RemovePolicy("db_permit_appid1");
    // Assert: the first removal reaches the engine.
    EXPECT_EQ(errCode, E_OK) << "RemovePolicy failed, errCode=" << errCode;
    EXPECT_EQ(GetStubRemovePolicyCallCount(), 1) << "unexpected engine RemovePolicy call count";
    EXPECT_EQ(GetStubLastRemovePolicy(), "db_permit_appid1") << "engine received: " << GetStubLastRemovePolicy();
    // Act: remove a nonexistent policy name, which must stay idempotent.
    errCode = store_->RemovePolicy("nonexistent_policy");
    // Assert: the second removal is still E_OK and reaches the engine.
    EXPECT_EQ(errCode, E_OK) << "RemovePolicy is not idempotent, errCode=" << errCode;
    EXPECT_EQ(GetStubRemovePolicyCallCount(), 2) << "unexpected engine RemovePolicy call count";
    EXPECT_EQ(GetStubLastRemovePolicy(), "nonexistent_policy") << "engine received: " << GetStubLastRemovePolicy();
}

/**
 * @brief 验证屏幕锁状态/设备健康状态枚举经映射后正确到达 GRD 引擎
 * @since 1.0
 */
HWTEST_F(MockRdbVectorPolicyTest, MockRdbVectorPolicyTest_setScreenStateAndDeviceHealthyState_010, TestSize.Level0)
{
    ASSERT_NE(store_, nullptr) << "store is null";
    // Arrange & Act: SCREEN_LOCKED maps to GRD_SCREEN_LOCKED.
    EXPECT_EQ(store_->SetScreenState(SCREEN_LOCKED), E_OK) << "SetScreenState(SCREEN_LOCKED) failed";
    EXPECT_EQ(GetStubScreenLockStateCallCount(), 1) << "unexpected engine screen lock call count";
    EXPECT_EQ(GetStubLastScreenLockState(), GRD_SCREEN_LOCKED) << "engine did not receive GRD_SCREEN_LOCKED";
    // Act: SCREEN_UNLOCK maps to GRD_SCREEN_UNLOCK.
    EXPECT_EQ(store_->SetScreenState(SCREEN_UNLOCK), E_OK) << "SetScreenState(SCREEN_UNLOCK) failed";
    EXPECT_EQ(GetStubScreenLockStateCallCount(), 2) << "unexpected engine screen lock call count";
    EXPECT_EQ(GetStubLastScreenLockState(), GRD_SCREEN_UNLOCK) << "engine did not receive GRD_SCREEN_UNLOCK";
    // Act: DEVICE_UNROOT maps to GRD_DEVICE_UNROOT.
    EXPECT_EQ(store_->SetDeviceHealthyState(DEVICE_UNROOT), E_OK) << "SetDeviceHealthyState(DEVICE_UNROOT) failed";
    EXPECT_EQ(GetStubDeviceHealthyStateCallCount(), 1) << "unexpected engine device healthy call count";
    EXPECT_EQ(GetStubLastDeviceHealthyState(), GRD_DEVICE_UNROOT) << "engine did not receive GRD_DEVICE_UNROOT";
}

class MockRdbPolicyFailureTest : public testing::Test {
protected:
    void SetUp() override
    {
        dbPath_ = RDB_TEST_PATH + testing::UnitTest::GetInstance()->current_test_info()->name() + ".db";
        ResetStubState();
        RdbHelper::ClearCache();
        ASSERT_EQ(RdbHelper::DeleteRdbStore(dbPath_), E_OK);
    }

    void TearDown() override
    {
        RdbHelper::ClearCache();
        EXPECT_EQ(RdbHelper::DeleteRdbStore(dbPath_), E_OK);
    }

    static void ExpectPolicyRejection(RdbStore &store, int expected)
    {
        EXPECT_EQ(store.SetScreenState(SCREEN_LOCKED), expected);
        EXPECT_EQ(store.SetDeviceHealthyState(DEVICE_UNROOT), expected);
        EXPECT_EQ(store.AddPolicy(POLICY_DB_PERMIT_APPID1), expected);
        EXPECT_EQ(store.RemovePolicy("db_permit_appid1"), expected);
        EXPECT_EQ(GetStubScreenLockStateCallCount(), 0);
        EXPECT_EQ(GetStubDeviceHealthyStateCallCount(), 0);
        EXPECT_EQ(GetStubAddPolicyCallCount(), 0);
        EXPECT_EQ(GetStubRemovePolicyCallCount(), 0);
    }

    std::string dbPath_;
    VectorPolicyOpenCallback callback_;
};

/**
 * @brief A vector store accepts a CREATE TABLE statement with a GRD annotation.
 */
HWTEST_F(MockRdbPolicyFailureTest, Sql_AnnotatedCreate_VectorAccepts_005, TestSize.Level0)
{
    RdbStoreConfig config(dbPath_);
    config.SetIsVector(true);
    int errCode = E_ERROR;
    auto store = RdbHelper::GetRdbStore(config, DB_VERSION, callback_, errCode);
    ASSERT_EQ(errCode, E_OK) << "Failed to open vector store: " << dbPath_;
    ASSERT_NE(store, nullptr) << "Missing vector store: " << dbPath_;

    const std::string sql = "[app_level('1') apl_level('NORMAL')] CREATE TABLE t (id INTEGER)";
    auto result = store->Execute(sql);
    EXPECT_EQ(result.first, E_OK) << sql;
}

/**
 * @brief A closed vector store rejects all four policy operations before calling GRD.
 */
HWTEST_F(MockRdbPolicyFailureTest, Policy_ClosedVectorStore_AlreadyClosed_001, TestSize.Level0)
{
    // Arrange: retain the store handle after closing its connection pool.
    RdbStoreConfig config(dbPath_);
    config.SetIsVector(true);
    config.SetAccessControl(true);
    int errCode = E_ERROR;
    auto store = RdbHelper::GetRdbStore(config, DB_VERSION, callback_, errCode);
    ASSERT_EQ(errCode, E_OK);
    ASSERT_NE(store, nullptr);
    std::static_pointer_cast<RdbStoreImpl>(store)->Close();

    // Act & Assert: each operation must reject the closed handle before dispatch.
    ExpectPolicyRejection(*store, E_ALREADY_CLOSED);
}

/**
 * @brief Changing any one vector security setting rejects the reopen and preserves the cached store.
 */
HWTEST_F(MockRdbPolicyFailureTest, Policy_VectorReopen_SecurityConflict_003, TestSize.Level0)
{
    // Arrange: keep the original handle alive with the default (legacy) config version.
    RdbStoreConfig original(dbPath_);
    original.SetIsVector(true);
    int errCode = E_ERROR;
    auto store = RdbHelper::GetRdbStore(original, DB_VERSION, callback_, errCode);
    ASSERT_EQ(errCode, E_OK);
    ASSERT_NE(store, nullptr);
    const auto openCount = GetStubOpenCallCount();

    // Act & Assert: change only one setting per reopen.
    auto expectConflict = [this, &errCode, openCount](const RdbStoreConfig &changed, const char *field) {
        SCOPED_TRACE(field);
        EXPECT_EQ(RdbHelper::GetRdbStore(changed, DB_VERSION, callback_, errCode), nullptr);
        EXPECT_EQ(errCode, E_CONFIG_INVALID_CHANGE);
        EXPECT_EQ(GetStubOpenCallCount(), openCount);
    };
    auto access = original;
    access.SetAccessControl(true);
    expectConflict(access, "accessControl");
    auto granularity = original;
    granularity.SetEncryptionGranularity(TABLE_ENCRYPTED_CONFIG);
    expectConflict(granularity, "encryptionGranularity");
    auto level = original;
    level.SetBaseSecurityLevel(EL4_SECURITY_LEVEL);
    expectConflict(level, "baseSecurityLevel");

    EXPECT_EQ(RdbHelper::GetRdbStore(original, DB_VERSION, callback_, errCode), store);
    EXPECT_EQ(errCode, E_OK);
    EXPECT_EQ(GetStubOpenCallCount(), openCount);
}

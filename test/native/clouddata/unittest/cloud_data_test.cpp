/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
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

#define LOG_TAG "CloudDataTest"

#include <condition_variable>
#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "mock_cloud_remote.h"
#include <unistd.h>

#include "cloud_manager.h"
#include "cloud_notifier_stub.h"
#include "cloud_service_proxy.h"
#include "cloud_types.h"
#include "cloud_types_util.h"
#include "logger.h"
#include "rdb_types.h"

namespace OHOS::CloudData {
using namespace testing::ext;
using namespace OHOS::Rdb;
using namespace DistributedRdb;
static constexpr const char *TEST_BUNDLE_NAME = "bundleName";
static constexpr const char *TEST_ACCOUNT_ID = "testId";
static constexpr const char *TEST_STORE_ID = "storeId";

class MockSyncInfoObserver : public ISyncInfoObserver {
public:
    void OnSyncInfoChanged(const std::map<std::string, QueryLastResults> &data) override
    {
        called_ = true;
        data_ = data;
    }
    void OnSyncInfoChanged(const int32_t triggerMode) override
    {
        called_ = true;
        triggerMode_ = triggerMode;
    }
    bool IsCalled() const
    {
        return called_;
    }
    const std::map<std::string, QueryLastResults> &GetData() const
    {
        return data_;
    }
    const int32_t &GetTriggerMode() const
    {
        return triggerMode_;
    }
    void Reset()
    {
        called_ = false;
        data_.clear();
    }

private:
    bool called_ = false;
    std::map<std::string, QueryLastResults> data_;
    int32_t triggerMode_;
};

class CloudDataTest : public testing::Test {
public:
    static void SetUpTestCase(void);
    static void TearDownTestCase(void);
    void SetUp()
    {
        progressStatus_ = Progress::SYNC_BEGIN;
        code_ = ProgressCode::SUCCESS;
        remote_ = new testing::StrictMock<MockCloudRemote>();
        proxy_ = std::make_shared<CloudServiceProxy>(remote_);
        CloudManager::GetInstance().cloudService_ = proxy_;
        ExpectReply(CloudService::TRANS_INIT_NOTIFIER, CloudService::SUCCESS);
        ASSERT_EQ(proxy_->InitNotifier(), CloudService::SUCCESS) << "local notifier setup failed";
    }
    void TearDown()
    {
        CloudManager::GetInstance().ResetServiceHandle();
        proxy_.reset();
        remote_ = nullptr;
    }

    void ExpectReply(uint32_t code, int32_t status)
    {
        ExpectCloudReply(*remote_, code, status);
    }

    void ExpectSyncCompleteReply(uint32_t sequence)
    {
        auto notifier = proxy_->notifier_;
        ExpectCloudReply(*remote_, CloudService::TRANS_CLOUD_SYNC, CloudService::SUCCESS,
            [notifier, sequence](MessageParcel &request) {
                std::string bundle;
                std::string store;
                CloudService::Option option;
                ASSERT_TRUE(ITypesUtil::Unmarshal(request, bundle, store, option)) << "bad CloudSync request";
                EXPECT_EQ(bundle, TEST_BUNDLE_NAME) << "wrong bundle in request";
                EXPECT_EQ(store, TEST_STORE_ID) << "wrong store in request";
                EXPECT_EQ(option.seqNum, sequence) << "wrong sequence in request";
                MessageParcel data;
                MessageParcel reply;
                // One device detail with no tables: encode the server wire format independently.
                ASSERT_TRUE(ITypesUtil::Marshal(data, sequence, int32_t{1}, std::string("device"),
                    int32_t(Progress::SYNC_FINISH), int32_t(ProgressCode::CLOUD_DISABLED), int32_t{0}, std::string{}))
                    << "bad notification fixture";
                EXPECT_EQ(notifier->OnCompleteInner(data, reply), CloudService::SUCCESS)
                    << "real notifier did not decode the callback";
            });
    }

    sptr<testing::StrictMock<MockCloudRemote>> remote_;
    std::shared_ptr<CloudServiceProxy> proxy_;

    static std::mutex syncCompleteLock_;
    static std::condition_variable syncCompleteCv_;
    static int32_t progressStatus_;
    static int32_t code_;
    static constexpr uint32_t delayTime = 2000;
};

std::mutex CloudDataTest::syncCompleteLock_;
std::condition_variable CloudDataTest::syncCompleteCv_;
int32_t CloudDataTest::progressStatus_ = Progress::SYNC_BEGIN;
int32_t CloudDataTest::code_ = ProgressCode::SUCCESS;

void CloudDataTest::SetUpTestCase(void)
{
    LOG_INFO("SetUpTestCase in.");
}

void CloudDataTest::TearDownTestCase(void)
{
    LOG_INFO("TearDownTestCase in.");
}

/* *
 * @tc.name: CloudSync_SyncComplete_001
 * @tc.desc: Test the CloudSync API with syncComplete callback
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(CloudDataTest, CloudSync_SyncComplete_001, TestSize.Level1)
{
    auto [state, proxy] = CloudManager::GetInstance().GetCloudService();
    ASSERT_EQ(state == CloudService::SUCCESS && proxy != nullptr, true);
    auto progress = [](DistributedRdb::Details &&details) {
        ASSERT_NE(details.size(), 0);
        progressStatus_ = details.begin()->second.progress;
        code_ = details.begin()->second.code;
        LOG_INFO("CloudSync_SyncComplete_001, progressStatus_:%{public}d, code_:%{public}d", progressStatus_, code_);
        if (progressStatus_ == Progress::SYNC_FINISH) {
            LOG_INFO("CloudSync_SyncComplete_001, start to notify, progressStatus_:%{public}d, code_:%{public}d",
                progressStatus_, code_);
            std::unique_lock<std::mutex> lock(syncCompleteLock_);
            syncCompleteCv_.notify_one();
        }
    };
    int32_t syncMode = 4; // 4 is native_first
    uint32_t seqNum = 101;
    ExpectSyncCompleteReply(seqNum);
    auto status = proxy->CloudSync(TEST_BUNDLE_NAME, TEST_STORE_ID, { syncMode, seqNum }, progress);
    EXPECT_EQ(status, CloudService::SUCCESS);
    std::unique_lock<std::mutex> lock(syncCompleteLock_);
    auto result = syncCompleteCv_.wait_for(lock, std::chrono::milliseconds(CloudDataTest::delayTime), [] {
        LOG_INFO("CloudSync_SyncComplete_001, wait_for in, progressStatus_:%{public}d, code_:%{public}d",
            progressStatus_, code_);
        return progressStatus_ == Progress::SYNC_FINISH && code_ == ProgressCode::CLOUD_DISABLED;
    });
    EXPECT_TRUE(result);
    LOG_INFO("CloudSync_SyncComplete_001 test end.");
}

/* *
 * @tc.name: CloudDataTest_003
 * @tc.desc: Test forwarding of the server permission result for the QueryStatistics API
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(CloudDataTest, CloudDataTest_003, TestSize.Level1)
{
    auto [state, proxy] = CloudManager::GetInstance().GetCloudService();
    ASSERT_EQ(state == CloudService::SUCCESS && proxy != nullptr, true);
    ExpectReply(CloudService::TRANS_QUERY_STATISTICS, CloudService::ERROR);
    auto [status, info] = proxy->QueryStatistics(TEST_ACCOUNT_ID, TEST_BUNDLE_NAME, TEST_STORE_ID);
    EXPECT_EQ(status, CloudService::ERROR);
    EXPECT_TRUE(info.empty());
}

/* *
 * @tc.name: EnableCloud001
 * @tc.desc: Test the EnableCloud API
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(CloudDataTest, EnableCloud001, TestSize.Level0)
{
    auto [state, proxy] = CloudManager::GetInstance().GetCloudService();
    ASSERT_EQ(state == CloudService::SUCCESS && proxy != nullptr, true);
    std::map<std::string, int32_t> switches;
    switches.emplace(TEST_BUNDLE_NAME, 0);
    ExpectReply(CloudService::TRANS_ENABLE_CLOUD, CloudService::ERROR);
    auto status = proxy->EnableCloud(TEST_ACCOUNT_ID, switches);
    EXPECT_NE(status, CloudService::SUCCESS);
}

/* *
 * @tc.name: DisableCloud001
 * @tc.desc: Test the DisableCloud API
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(CloudDataTest, DisableCloud001, TestSize.Level0)
{
    auto [state, proxy] = CloudManager::GetInstance().GetCloudService();
    ASSERT_EQ(state == CloudService::SUCCESS && proxy != nullptr, true);
    ExpectReply(CloudService::TRANS_DISABLE_CLOUD, CloudService::ERROR);
    auto status = proxy->DisableCloud(TEST_ACCOUNT_ID);
    EXPECT_NE(status, CloudService::SUCCESS);
}

/* *
 * @tc.name: ChangeAppSwitch001
 * @tc.desc: Test the ChangeAppSwitch API
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(CloudDataTest, ChangeAppSwitch001, TestSize.Level0)
{
    auto [state, proxy] = CloudManager::GetInstance().GetCloudService();
    ASSERT_EQ(state == CloudService::SUCCESS && proxy != nullptr, true);
    SwitchConfig config;
    ExpectReply(CloudService::TRANS_CHANGE_APP_SWITCH, CloudService::ERROR);
    auto status = proxy->ChangeAppSwitch(TEST_ACCOUNT_ID, TEST_BUNDLE_NAME, 0, config);
    EXPECT_NE(status, CloudService::SUCCESS);
    ExpectReply(CloudService::TRANS_CHANGE_APP_SWITCH, CloudService::ERROR);
    status = proxy->ChangeAppSwitch(TEST_ACCOUNT_ID, TEST_BUNDLE_NAME, 1, config);
    EXPECT_NE(status, CloudService::SUCCESS);
}

/* *
 * @tc.name: Clean001
 * @tc.desc: Test the Clean API
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(CloudDataTest, Clean001, TestSize.Level1)
{
    auto [state, proxy] = CloudManager::GetInstance().GetCloudService();
    ASSERT_EQ(state == CloudService::SUCCESS && proxy != nullptr, true);
    std::map<std::string, int32_t> actions;
    actions.emplace(TEST_BUNDLE_NAME, 0);
    std::map<std::string, ClearConfig> configs;
    ExpectReply(CloudService::TRANS_CLEAN, CloudService::ERROR);
    auto status = proxy->Clean(TEST_ACCOUNT_ID, actions, configs);
    EXPECT_EQ(status, CloudService::ERROR);
}

/* *
 * @tc.name: NotifyDataChange001
 * @tc.desc: Test the NotifyDataChange API
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(CloudDataTest, NotifyDataChange001, TestSize.Level1)
{
    auto [state, proxy] = CloudManager::GetInstance().GetCloudService();
    ASSERT_EQ(state == CloudService::SUCCESS && proxy != nullptr, true);
    ExpectReply(CloudService::TRANS_NOTIFY_DATA_CHANGE_EXT, CloudService::INVALID_ARGUMENT);
    auto status = proxy->NotifyDataChange("id", "data", 100);
    EXPECT_EQ(status, CloudService::INVALID_ARGUMENT);
}

/* *
 * @tc.name: NotifyDataChange002
 * @tc.desc: Test the NotifyDataChange API
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(CloudDataTest, NotifyDataChange002, TestSize.Level1)
{
    auto [state, proxy] = CloudManager::GetInstance().GetCloudService();
    ASSERT_EQ(state == CloudService::SUCCESS && proxy != nullptr, true);
    ExpectReply(CloudService::TRANS_NOTIFY_DATA_CHANGE, CloudService::INVALID_ARGUMENT);
    auto status = proxy->NotifyDataChange(TEST_ACCOUNT_ID, TEST_BUNDLE_NAME);
    EXPECT_EQ(status, CloudService::INVALID_ARGUMENT);
}

/* *
 * @tc.name: SetGlobalCloudStrategy001
 * @tc.desc: Test the SetGlobalCloudStrategy API
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(CloudDataTest, SetGlobalCloudStrategy001, TestSize.Level1)
{
    auto [state, proxy] = CloudManager::GetInstance().GetCloudService();
    ASSERT_EQ(state == CloudService::SUCCESS && proxy != nullptr, true);
    std::vector<CommonType::Value> values = { 0 };
    ExpectReply(CloudService::TRANS_SET_GLOBAL_CLOUD_STRATEGY, CloudService::SUCCESS);
    auto status = proxy->SetGlobalCloudStrategy(Strategy::STRATEGY_NETWORK, values);
    EXPECT_EQ(status, CloudService::SUCCESS);
}

/* *
 * @tc.name:SetCloudStrategy001
 * @tc.desc: Test forwarding of the server system-app denial for the SetCloudStrategy001 API
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(CloudDataTest, SetCloudStrategy001, TestSize.Level1)
{
    auto [state, proxy] = CloudManager::GetInstance().GetCloudService();
    ASSERT_EQ(state == CloudService::SUCCESS && proxy != nullptr, true);
    std::vector<CommonType::Value> values;
    values.push_back(CloudData::NetWorkStrategy::WIFI);
    CloudData::Strategy strategy = CloudData::Strategy::STRATEGY_BUTT;
    ExpectReply(CloudService::TRANS_SET_CLOUD_STRATEGY, CloudService::IPC_ERROR);
    auto ret = proxy->SetCloudStrategy(strategy, values);
    EXPECT_EQ(ret, CloudService::IPC_ERROR);
    strategy = CloudData::Strategy::STRATEGY_NETWORK;
    ExpectReply(CloudService::TRANS_SET_CLOUD_STRATEGY, CloudService::SUCCESS);
    ret = proxy->SetCloudStrategy(strategy, values);
    EXPECT_EQ(ret, CloudService::SUCCESS);
}

/* *
 * @tc.name: CloudSync001
 * @tc.desc: Test the invalid param of the CloudSync API
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(CloudDataTest, CloudSync001, TestSize.Level1)
{
    auto [state, proxy] = CloudManager::GetInstance().GetCloudService();
    ASSERT_EQ(state == CloudService::SUCCESS && proxy != nullptr, true);
    auto ret = proxy->CloudSync(TEST_BUNDLE_NAME, TEST_STORE_ID, {}, nullptr);  // no progress
    EXPECT_EQ(ret, CloudService::INVALID_ARGUMENT);
    LOG_INFO("CloudSync001 test end.");
}

/* *
 * @tc.name: CloudSync002
 * @tc.desc: Test the CloudSync API
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(CloudDataTest, CloudSync002, TestSize.Level1)
{
    auto [state, proxy] = CloudManager::GetInstance().GetCloudService();
    ASSERT_EQ(state == CloudService::SUCCESS && proxy != nullptr, true);
    auto progress = [](DistributedRdb::Details &&) {};
    auto status = proxy->CloudSync(TEST_BUNDLE_NAME, TEST_STORE_ID, {}, progress);
    EXPECT_EQ(status, CloudService::INVALID_ARGUMENT);  // invalid syncMode
    LOG_INFO("CloudSync002 test end.");
}

/* *
 * @tc.name: CloudSync003
 * @tc.desc: Test the CloudSync API
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(CloudDataTest, CloudSync003, TestSize.Level1)
{
    auto [state, proxy] = CloudManager::GetInstance().GetCloudService();
    ASSERT_EQ(state == CloudService::SUCCESS && proxy != nullptr, true);
    auto progress = [](DistributedRdb::Details &&) {};
    int32_t syncMode = 4; // 4 is native_first
    uint32_t seqNum = 10;
    ExpectReply(CloudService::TRANS_CLOUD_SYNC, CloudService::SUCCESS);
    auto status = proxy->CloudSync(TEST_BUNDLE_NAME, TEST_STORE_ID, { syncMode, seqNum }, progress);
    EXPECT_EQ(status, CloudService::SUCCESS);
    LOG_INFO("CloudSync003 test end.");
}

/* *
 * @tc.name: CloudSync004
 * @tc.desc: Test the hap permission of the CloudSync API
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(CloudDataTest, CloudSync004, TestSize.Level1)
{
    auto [state, proxy] = CloudManager::GetInstance().GetCloudService();
    ASSERT_EQ(state == CloudService::SUCCESS && proxy != nullptr, true);
    auto progress = [](DistributedRdb::Details &&) {};
    int32_t syncMode = 4; // 4 is native_first
    uint32_t seqNum = 100;
    ExpectReply(CloudService::TRANS_CLOUD_SYNC, CloudService::PERMISSION_DENIED);
    auto status = proxy->CloudSync(TEST_BUNDLE_NAME, TEST_STORE_ID, { syncMode, seqNum }, progress);
    EXPECT_EQ(status, CloudService::PERMISSION_DENIED);
    ExpectReply(CloudService::TRANS_CLOUD_SYNC, CloudService::SUCCESS);
    EXPECT_EQ(proxy->CloudSync(TEST_BUNDLE_NAME, TEST_STORE_ID, { syncMode, seqNum }, progress), CloudService::SUCCESS);
    LOG_INFO("CloudSync004 test end.");
}

/* *
 * @tc.name: CloudSyncFullSync001
 * @tc.desc: Test CloudSync with isFullSync=true in Option
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(CloudDataTest, CloudSyncFullSync001, TestSize.Level1)
{
    auto [state, proxy] = CloudManager::GetInstance().GetCloudService();
    ASSERT_EQ(state == CloudService::SUCCESS && proxy != nullptr, true);
    auto progress = [](DistributedRdb::Details &&) {};
    CloudService::Option option;
    option.syncMode = DistributedRdb::TIME_FIRST;
    option.seqNum = 200;
    option.isDownloadOnly = false;
    option.isFullSync = true;
    ExpectCloudReply(*remote_, CloudService::TRANS_CLOUD_SYNC, CloudService::SUCCESS, [](MessageParcel &request) {
        std::string bundle;
        std::string store;
        CloudService::Option received;
        ASSERT_TRUE(ITypesUtil::Unmarshal(request, bundle, store, received));
        EXPECT_TRUE(received.isFullSync);
    });
    auto status = proxy->CloudSync(TEST_BUNDLE_NAME, TEST_STORE_ID, option, progress);
    EXPECT_EQ(status, CloudService::SUCCESS);
    LOG_INFO("CloudSyncFullSync001 test end.");
}

/* *
 * @tc.name: CloudSyncFullSync002
 * @tc.desc: Test CloudSync with isFullSync=false (default) in Option
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(CloudDataTest, CloudSyncFullSync002, TestSize.Level1)
{
    auto [state, proxy] = CloudManager::GetInstance().GetCloudService();
    ASSERT_EQ(state == CloudService::SUCCESS && proxy != nullptr, true);
    auto progress = [](DistributedRdb::Details &&) {};
    CloudService::Option option;
    option.syncMode = DistributedRdb::TIME_FIRST;
    option.seqNum = 201;
    option.isFullSync = false;
    ExpectCloudReply(*remote_, CloudService::TRANS_CLOUD_SYNC, CloudService::SUCCESS, [](MessageParcel &request) {
        std::string bundle;
        std::string store;
        CloudService::Option received;
        ASSERT_TRUE(ITypesUtil::Unmarshal(request, bundle, store, received));
        EXPECT_FALSE(received.isFullSync);
    });
    auto status = proxy->CloudSync(TEST_BUNDLE_NAME, TEST_STORE_ID, option, progress);
    EXPECT_EQ(status, CloudService::SUCCESS);
    LOG_INFO("CloudSyncFullSync002 test end.");
}

/* *
 * @tc.name: CloudSyncConfigFullSync001
 * @tc.desc: Test CloudSyncConfig has isFullSync field with default false
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(CloudDataTest, CloudSyncConfigFullSync001, TestSize.Level1)
{
    DistributedRdb::CloudSyncConfig config;
    EXPECT_FALSE(config.isFullSync);
    config.isFullSync = true;
    EXPECT_TRUE(config.isFullSync);
    LOG_INFO("CloudSyncConfigFullSync001 test end.");
}

/* *
 * @tc.name: CloudSync005
 * @tc.desc: Test forwarding of the server permission result for the CloudSync API
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(CloudDataTest, CloudSync005, TestSize.Level1)
{
    auto [state, proxy] = CloudManager::GetInstance().GetCloudService();
    ASSERT_EQ(state == CloudService::SUCCESS && proxy != nullptr, true);
    auto progress = [](DistributedRdb::Details &&) {};
    int32_t syncMode = 4; // 4 is native_first
    uint32_t seqNum = 1000;
    ExpectReply(CloudService::TRANS_CLOUD_SYNC, CloudService::CLOUD_CONFIG_PERMISSION_DENIED);
    auto status = proxy->CloudSync(TEST_BUNDLE_NAME, TEST_STORE_ID, { syncMode, seqNum }, progress);
    EXPECT_EQ(status, CloudService::CLOUD_CONFIG_PERMISSION_DENIED);
    LOG_INFO("CloudSync005 test end.");
}

/* *
 * @tc.name: CloudSync006
 * @tc.desc: Test the CloudSync API
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(CloudDataTest, CloudSync006, TestSize.Level1)
{
    auto [state, proxy] = CloudManager::GetInstance().GetCloudService();
    ASSERT_EQ(state == CloudService::SUCCESS && proxy != nullptr, true);
    auto progress = [](DistributedRdb::Details &&) {};
    int32_t syncMode = 4; // 4 is native_first
    uint32_t seqNum = 20;
    ExpectReply(CloudService::TRANS_CLOUD_SYNC, CloudService::SUCCESS);
    auto status = proxy->CloudSync(TEST_BUNDLE_NAME, TEST_STORE_ID, { syncMode, seqNum }, progress);
    EXPECT_EQ(status, CloudService::SUCCESS);

    // same seqNum, register progress failed.
    status = proxy->CloudSync(TEST_BUNDLE_NAME, TEST_STORE_ID, { syncMode, seqNum }, progress);
    EXPECT_EQ(status, CloudService::ERROR);
    LOG_INFO("CloudSync006 test end.");
}

/* *
 * @tc.name: CloudSync007
 * @tc.desc: Test the invalid param of the CloudSync API
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(CloudDataTest, CloudSync007, TestSize.Level1)
{
    auto [state, proxy] = CloudManager::GetInstance().GetCloudService();
    ASSERT_EQ(state == CloudService::SUCCESS && proxy != nullptr, true);
    auto progress = [](DistributedRdb::Details &&) {};
    int32_t syncMode = 4; // 4 is native_first
    uint32_t seqNum = 21;
    auto ret = proxy->CloudSync("", TEST_STORE_ID, { syncMode, seqNum }, progress);  // bundleName is empty
    EXPECT_EQ(ret, CloudService::INVALID_ARGUMENT);
    LOG_INFO("CloudSync007 test end.");
}

/* *
 * @tc.name: CloudSync008
 * @tc.desc: Test the invalid param of the CloudSync API
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(CloudDataTest, CloudSync008, TestSize.Level1)
{
    auto [state, proxy] = CloudManager::GetInstance().GetCloudService();
    ASSERT_EQ(state == CloudService::SUCCESS && proxy != nullptr, true);
    auto progress = [](DistributedRdb::Details &&) {};
    int32_t syncMode = 4; // 4 is native_first
    uint32_t seqNum = 22;
    auto ret = proxy->CloudSync(TEST_BUNDLE_NAME, "", { syncMode, seqNum }, progress);  // storeId is empty
    EXPECT_EQ(ret, CloudService::INVALID_ARGUMENT);
    LOG_INFO("CloudSync008 test end.");
}

/* *
 * @tc.name: CloudSync009
 * @tc.desc: Test the invalid param of the CloudSync API
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(CloudDataTest, CloudSync009, TestSize.Level1)
{
    auto [state, proxy] = CloudManager::GetInstance().GetCloudService();
    ASSERT_EQ(state == CloudService::SUCCESS && proxy != nullptr, true);
    auto progress = [](DistributedRdb::Details &&) {};
    int32_t syncMode = 10;
    uint32_t seqNum = 22;
    auto ret = proxy->CloudSync(TEST_BUNDLE_NAME, TEST_STORE_ID, { syncMode, seqNum }, progress);  // invalid syncMode
    EXPECT_EQ(ret, CloudService::INVALID_ARGUMENT);
    LOG_INFO("CloudSync009 test end.");
}

/* *
 * @tc.name: InitNotifier001
 * @tc.desc: Test the InitNotifier API
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(CloudDataTest, InitNotifier001, TestSize.Level1)
{
    auto [state, proxy] = CloudManager::GetInstance().GetCloudService();
    ASSERT_EQ(state == CloudService::SUCCESS && proxy != nullptr, true);
    sptr<CloudNotifierStub> notifier = nullptr;
    auto status = proxy->InitNotifier(notifier);  // can not Marshalling a 'nullptr'
    EXPECT_EQ(status, CloudService::IPC_PARCEL_ERROR);
    LOG_INFO("InitNotifier001 test end.");
}

/* *
 * @tc.name: InitNotifier002
 * @tc.desc: Test the InitNotifier API
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(CloudDataTest, InitNotifier002, TestSize.Level1)
{
    auto [state, proxy] = CloudManager::GetInstance().GetCloudService();
    ASSERT_EQ(state == CloudService::SUCCESS && proxy != nullptr, true);
    sptr<CloudNotifierStub> notifier = new (std::nothrow) CloudNotifierStub(nullptr, nullptr);
    ExpectReply(CloudService::TRANS_INIT_NOTIFIER, CloudService::SUCCESS);
    auto status = proxy->InitNotifier(notifier);
    EXPECT_EQ(status, CloudService::SUCCESS);
    LOG_INFO("InitNotifier002 test end.");
}

/* *
 * @tc.name: MarshallingOptionTest
 * @tc.desc: Test the Marshalling interface
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(CloudDataTest, MarshallingOptionTest, TestSize.Level1)
{
    LOG_INFO("MarshallingOptionTest test in.");
    CloudData::CloudService::Option input;
    input.syncMode = 4;
    input.seqNum = 10;
    MessageParcel parcel;
    bool ret = ITypesUtil::Marshalling(input, parcel);
    EXPECT_TRUE(ret);

    CloudData::CloudService::Option output;
    ret = ITypesUtil::Unmarshalling(output, parcel);
    EXPECT_TRUE(ret);
    EXPECT_EQ(output.syncMode, input.syncMode);
    EXPECT_EQ(output.seqNum, input.seqNum);
    LOG_INFO("MarshallingOptionTest test end.");
}

/**
 * @tc.name: Subscribe_EmptyBundleInfos
 * @tc.desc: Test Subscribe with empty bundleInfos
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(CloudDataTest, Subscribe_EmptyBundleInfos, TestSize.Level1)
{
    auto [state, proxy] = CloudManager::GetInstance().GetCloudService();
    ASSERT_EQ(state == CloudService::SUCCESS && proxy != nullptr, true);
    std::vector<BundleInfo> bundleInfos;
    auto observer = std::make_shared<MockSyncInfoObserver>();
    auto status = proxy->Subscribe(CloudSubscribeType::SYNC_INFO_CHANGED, bundleInfos, observer);
    EXPECT_EQ(status, CloudService::INVALID_ARGUMENT_V20);
}

/**
 * @tc.name: Subscribe_NullObserver
 * @tc.desc: Test Subscribe with null observer
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(CloudDataTest, Subscribe_NullObserver, TestSize.Level1)
{
    auto [state, proxy] = CloudManager::GetInstance().GetCloudService();
    ASSERT_EQ(state == CloudService::SUCCESS && proxy != nullptr, true);
    std::vector<BundleInfo> bundleInfos = {{TEST_BUNDLE_NAME, TEST_STORE_ID}};
    auto status = proxy->Subscribe(CloudSubscribeType::SYNC_INFO_CHANGED, bundleInfos, nullptr);
    EXPECT_EQ(status, CloudService::INVALID_ARGUMENT_V20);
}

/**
 * @tc.name: Subscribe_SystemPermission
 * @tc.desc: Test Subscribe with system permission
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(CloudDataTest, Subscribe_SystemPermission, TestSize.Level1)
{
    auto [state, proxy] = CloudManager::GetInstance().GetCloudService();
    ASSERT_EQ(state == CloudService::SUCCESS && proxy != nullptr, true);
    std::vector<BundleInfo> bundleInfos = {{TEST_BUNDLE_NAME, TEST_STORE_ID}};
    auto observer = std::make_shared<MockSyncInfoObserver>();
    ExpectReply(CloudService::TRANS_SUBSCRIBE, CloudService::SUCCESS);
    auto status = proxy->Subscribe(CloudSubscribeType::SYNC_INFO_CHANGED, bundleInfos, observer);
    EXPECT_EQ(status, CloudService::SUCCESS);
    ExpectReply(CloudService::TRANS_UNSUBSCRIBE, CloudService::SUCCESS);
    status = proxy->Unsubscribe(CloudSubscribeType::SYNC_INFO_CHANGED, bundleInfos, observer);
    EXPECT_EQ(status, CloudService::SUCCESS);
}

/**
 * @tc.name: Subscribe_NormalHapPermission
 * @tc.desc: Test Subscribe with normal hap permission
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(CloudDataTest, Subscribe_NormalHapPermission, TestSize.Level1)
{
    auto [state, proxy] = CloudManager::GetInstance().GetCloudService();
    ASSERT_EQ(state == CloudService::SUCCESS && proxy != nullptr, true);
    std::vector<BundleInfo> bundleInfos = {{TEST_BUNDLE_NAME, TEST_STORE_ID}};
    auto observer = std::make_shared<MockSyncInfoObserver>();
    ExpectReply(CloudService::TRANS_SUBSCRIBE, CloudService::PERMISSION_DENIED);
    auto status = proxy->Subscribe(CloudSubscribeType::SYNC_INFO_CHANGED, bundleInfos, observer);
    EXPECT_EQ(status, CloudService::PERMISSION_DENIED);
    auto cloudProxy = std::static_pointer_cast<CloudServiceProxy>(proxy);
    EXPECT_TRUE(cloudProxy->ExportSubObservers().Empty());
}

/**
 * @tc.name: Subscribe_NoConfigPermission
 * @tc.desc: Test Subscribe without config permission
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(CloudDataTest, Subscribe_NoConfigPermission, TestSize.Level1)
{
    auto [state, proxy] = CloudManager::GetInstance().GetCloudService();
    ASSERT_EQ(state == CloudService::SUCCESS && proxy != nullptr, true);
    std::vector<BundleInfo> bundleInfos = {{TEST_BUNDLE_NAME, TEST_STORE_ID}};
    auto observer = std::make_shared<MockSyncInfoObserver>();
    ExpectReply(CloudService::TRANS_SUBSCRIBE, CloudService::CLOUD_CONFIG_PERMISSION_DENIED);
    auto status = proxy->Subscribe(CloudSubscribeType::SYNC_INFO_CHANGED, bundleInfos, observer);
    EXPECT_EQ(status, CloudService::CLOUD_CONFIG_PERMISSION_DENIED);
    auto cloudProxy = std::static_pointer_cast<CloudServiceProxy>(proxy);
    EXPECT_TRUE(cloudProxy->ExportSubObservers().Empty());
}

/**
 * @tc.name: Unsubscribe_NullObserver
 * @tc.desc: Test Unsubscribe with null observer
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(CloudDataTest, Unsubscribe_NullObserver, TestSize.Level1)
{
    auto [state, proxy] = CloudManager::GetInstance().GetCloudService();
    ASSERT_EQ(state == CloudService::SUCCESS && proxy != nullptr, true);
    std::vector<BundleInfo> bundleInfos = {{TEST_BUNDLE_NAME, TEST_STORE_ID}};
    auto status = proxy->Unsubscribe(CloudSubscribeType::SYNC_INFO_CHANGED, bundleInfos, nullptr);
    EXPECT_EQ(status, CloudService::SUCCESS);
}

/**
 * @tc.name: Unsubscribe_RemoveObserver
 * @tc.desc: Test Unsubscribe with observer to remove
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(CloudDataTest, Unsubscribe_RemoveObserver, TestSize.Level1)
{
    auto [state, proxy] = CloudManager::GetInstance().GetCloudService();
    ASSERT_EQ(state == CloudService::SUCCESS && proxy != nullptr, true);
    std::vector<BundleInfo> bundleInfos = {{TEST_BUNDLE_NAME, TEST_STORE_ID}};
    auto observer = std::make_shared<MockSyncInfoObserver>();
    ExpectReply(CloudService::TRANS_SUBSCRIBE, CloudService::SUCCESS);
    ASSERT_EQ(proxy->Subscribe(CloudSubscribeType::SYNC_INFO_CHANGED, bundleInfos, observer), CloudService::SUCCESS);
    ExpectReply(CloudService::TRANS_UNSUBSCRIBE, CloudService::SUCCESS);
    auto status = proxy->Unsubscribe(CloudSubscribeType::SYNC_INFO_CHANGED, bundleInfos, observer);
    EXPECT_EQ(status, CloudService::SUCCESS);
}

/**
 * @tc.name: Unsubscribe_SystemPermission
 * @tc.desc: Test Unsubscribe with system permission
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(CloudDataTest, Unsubscribe_SystemPermission, TestSize.Level1)
{
    auto [state, proxy] = CloudManager::GetInstance().GetCloudService();
    ASSERT_EQ(state == CloudService::SUCCESS && proxy != nullptr, true);
    std::vector<BundleInfo> bundleInfos = {{TEST_BUNDLE_NAME, TEST_STORE_ID}};
    auto observer = std::make_shared<MockSyncInfoObserver>();
    auto status = proxy->Unsubscribe(CloudSubscribeType::SYNC_INFO_CHANGED, bundleInfos, observer);
    EXPECT_EQ(status, CloudService::SUCCESS);
}

/**
 * @tc.name: Unsubscribe_NormalHapPermission
 * @tc.desc: Test Unsubscribe with normal hap permission
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(CloudDataTest, Unsubscribe_NormalHapPermission, TestSize.Level1)
{
    auto [state, proxy] = CloudManager::GetInstance().GetCloudService();
    ASSERT_EQ(state == CloudService::SUCCESS && proxy != nullptr, true);
    std::vector<BundleInfo> bundleInfos = {{TEST_BUNDLE_NAME, TEST_STORE_ID}};
    auto observer = std::make_shared<MockSyncInfoObserver>();
    ExpectReply(CloudService::TRANS_SUBSCRIBE, CloudService::SUCCESS);
    ASSERT_EQ(proxy->Subscribe(CloudSubscribeType::SYNC_INFO_CHANGED, bundleInfos, observer), CloudService::SUCCESS);

    ExpectReply(CloudService::TRANS_UNSUBSCRIBE, CloudService::PERMISSION_DENIED);
    auto status = proxy->Unsubscribe(CloudSubscribeType::SYNC_INFO_CHANGED, bundleInfos, observer);
    EXPECT_EQ(status, CloudService::PERMISSION_DENIED);
}

/**
 * @tc.name: QueryLastSyncInfoBatch_SystemPermission
 * @tc.desc: Test QueryLastSyncInfoBatch with system permission
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(CloudDataTest, QueryLastSyncInfoBatch_SystemPermission, TestSize.Level1)
{
    auto [state, proxy] = CloudManager::GetInstance().GetCloudService();
    ASSERT_EQ(state == CloudService::SUCCESS && proxy != nullptr, true);
    std::vector<BundleInfo> bundleInfos = {{TEST_BUNDLE_NAME, TEST_STORE_ID}};
    ExpectReply(CloudService::TRANS_QUERY_LAST_SYNC_INFO_BATCH, CloudService::ERROR);
    auto [status, results] = proxy->QueryLastSyncInfoBatch(TEST_ACCOUNT_ID, bundleInfos);
    EXPECT_EQ(status, CloudService::ERROR);
    EXPECT_TRUE(results.empty());
}

/**
 * @tc.name: ImportSubObservers_WithObservers
 * @tc.desc: Test ImportSubObservers with observers
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(CloudDataTest, ImportSubObservers_WithObservers, TestSize.Level1)
{
    auto [state, proxy] = CloudManager::GetInstance().GetCloudService();
    ASSERT_EQ(state == CloudService::SUCCESS && proxy != nullptr, true);
    auto cloudServiceProxy = std::static_pointer_cast<CloudServiceProxy>(proxy);
    ASSERT_NE(cloudServiceProxy, nullptr);
    CloudServiceProxy::SubObservers observers;
    std::map<std::string, std::list<CloudServiceProxy::SubObserverParam>> storeMap;
    storeMap[TEST_STORE_ID].push_back({std::make_shared<MockSyncInfoObserver>()});
    observers.Insert(TEST_BUNDLE_NAME, storeMap);
    ExpectReply(CloudService::TRANS_SUBSCRIBE, CloudService::SUCCESS);
    cloudServiceProxy->ImportSubObservers(observers);
    auto exportedObservers = cloudServiceProxy->ExportSubObservers();
    EXPECT_FALSE(exportedObservers.Empty());
}

/**
 * @tc.name: OnSyncInfoNotify_EmptyData
 * @tc.desc: Test OnSyncInfoNotify with empty data
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(CloudDataTest, OnSyncInfoNotify_EmptyData, TestSize.Level1)
{
    auto [state, proxy] = CloudManager::GetInstance().GetCloudService();
    ASSERT_EQ(state == CloudService::SUCCESS && proxy != nullptr, true);
    auto cloudServiceProxy = std::static_pointer_cast<CloudServiceProxy>(proxy);
    ASSERT_NE(cloudServiceProxy, nullptr);

    auto observer = std::make_shared<MockSyncInfoObserver>();
    std::vector<BundleInfo> bundleInfos = {{TEST_BUNDLE_NAME, TEST_STORE_ID}};
    ExpectReply(CloudService::TRANS_SUBSCRIBE, CloudService::SUCCESS);
    proxy->Subscribe(CloudSubscribeType::SYNC_INFO_CHANGED, bundleInfos, observer);

    BatchQueryLastResults emptyData;
    cloudServiceProxy->OnSyncInfoNotify(emptyData);
    EXPECT_FALSE(observer->IsCalled());
}

/**
 * @tc.name: OnSyncInfoNotify_NoMatchingBundle
 * @tc.desc: Test OnSyncInfoNotify when bundle not found in subObservers
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(CloudDataTest, OnSyncInfoNotify_NoMatchingBundle, TestSize.Level1)
{
    auto [state, proxy] = CloudManager::GetInstance().GetCloudService();
    ASSERT_EQ(state == CloudService::SUCCESS && proxy != nullptr, true);
    auto cloudServiceProxy = std::static_pointer_cast<CloudServiceProxy>(proxy);
    ASSERT_NE(cloudServiceProxy, nullptr);

    auto observer = std::make_shared<MockSyncInfoObserver>();
    std::vector<BundleInfo> bundleInfos = {{TEST_BUNDLE_NAME, TEST_STORE_ID}};
    ExpectReply(CloudService::TRANS_SUBSCRIBE, CloudService::SUCCESS);
    proxy->Subscribe(CloudSubscribeType::SYNC_INFO_CHANGED, bundleInfos, observer);

    BatchQueryLastResults data;
    QueryLastResults storeResults;
    CloudSyncInfo syncInfo;
    syncInfo.code = 0;
    syncInfo.syncStatus = SyncStatus::FINISHED;
    storeResults[TEST_STORE_ID] = syncInfo;
    data["non_existent_bundle"] = storeResults;

    cloudServiceProxy->OnSyncInfoNotify(data);
    EXPECT_FALSE(observer->IsCalled());
}

/**
 * @tc.name: OnSyncInfoNotify_WithMatchingBundle
 * @tc.desc: Test OnSyncInfoNotify when bundle found in subObservers
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(CloudDataTest, OnSyncInfoNotify_WithMatchingBundle, TestSize.Level1)
{
    auto [state, proxy] = CloudManager::GetInstance().GetCloudService();
    ASSERT_EQ(state == CloudService::SUCCESS && proxy != nullptr, true);
    auto cloudServiceProxy = std::static_pointer_cast<CloudServiceProxy>(proxy);
    ASSERT_NE(cloudServiceProxy, nullptr);

    auto observer = std::make_shared<MockSyncInfoObserver>();
    std::vector<BundleInfo> bundleInfos = {{TEST_BUNDLE_NAME, TEST_STORE_ID}};
    ExpectReply(CloudService::TRANS_SUBSCRIBE, CloudService::SUCCESS);
    proxy->Subscribe(CloudSubscribeType::SYNC_INFO_CHANGED, bundleInfos, observer);

    BatchQueryLastResults data;
    QueryLastResults storeResults;
    CloudSyncInfo syncInfo;
    syncInfo.code = 0;
    syncInfo.syncStatus = SyncStatus::FINISHED;
    storeResults[TEST_STORE_ID] = syncInfo;
    data[TEST_BUNDLE_NAME] = storeResults;

    cloudServiceProxy->OnSyncInfoNotify(data);
    EXPECT_TRUE(observer->IsCalled());
}

/**
 * @tc.name: OnSyncInfoNotify_MultipleObservers
 * @tc.desc: Test OnSyncInfoNotify with multiple observers
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(CloudDataTest, OnSyncInfoNotify_MultipleObservers, TestSize.Level1)
{
    auto [state, proxy] = CloudManager::GetInstance().GetCloudService();
    ASSERT_EQ(state == CloudService::SUCCESS && proxy != nullptr, true);
    auto cloudServiceProxy = std::static_pointer_cast<CloudServiceProxy>(proxy);
    ASSERT_NE(cloudServiceProxy, nullptr);

    auto observer1 = std::make_shared<MockSyncInfoObserver>();
    auto observer2 = std::make_shared<MockSyncInfoObserver>();
    std::vector<BundleInfo> bundleInfos = {{TEST_BUNDLE_NAME, TEST_STORE_ID}};
    ExpectReply(CloudService::TRANS_SUBSCRIBE, CloudService::SUCCESS);
    proxy->Subscribe(CloudSubscribeType::SYNC_INFO_CHANGED, bundleInfos, observer1);
    ExpectReply(CloudService::TRANS_SUBSCRIBE, CloudService::SUCCESS);
    proxy->Subscribe(CloudSubscribeType::SYNC_INFO_CHANGED, bundleInfos, observer2);

    BatchQueryLastResults data;
    QueryLastResults storeResults;
    CloudSyncInfo syncInfo;
    syncInfo.code = 0;
    syncInfo.syncStatus = SyncStatus::FINISHED;
    storeResults[TEST_STORE_ID] = syncInfo;
    data[TEST_BUNDLE_NAME] = storeResults;

    cloudServiceProxy->OnSyncInfoNotify(data);
    EXPECT_TRUE(observer1->IsCalled());
    EXPECT_TRUE(observer2->IsCalled());
}

/**
 * @tc.name: CollectObserverData_StoreIdNotFound
 * @tc.desc: Test CollectObserverData when storeId not found in storeMap
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(CloudDataTest, CollectObserverData_StoreIdNotFound, TestSize.Level1)
{
    auto [state, proxy] = CloudManager::GetInstance().GetCloudService();
    ASSERT_EQ(state == CloudService::SUCCESS && proxy != nullptr, true);
    auto cloudServiceProxy = std::static_pointer_cast<CloudServiceProxy>(proxy);
    ASSERT_NE(cloudServiceProxy, nullptr);

    auto observer = std::make_shared<MockSyncInfoObserver>();
    std::vector<BundleInfo> bundleInfos = {{TEST_BUNDLE_NAME, "store_001"}};
    ExpectReply(CloudService::TRANS_SUBSCRIBE, CloudService::SUCCESS);
    proxy->Subscribe(CloudSubscribeType::SYNC_INFO_CHANGED, bundleInfos, observer);

    std::map<std::string, CloudSyncInfo> storeResults;
    CloudSyncInfo syncInfo;
    syncInfo.code = 0;
    storeResults["different_store"] = syncInfo;

    auto subObservers = cloudServiceProxy->ExportSubObservers();
    std::map<std::string, std::list<CloudServiceProxy::SubObserverParam>> storeMap;
    subObservers.ComputeIfPresent(TEST_BUNDLE_NAME, [&storeMap](const auto &, auto &map) {
        storeMap = map;
        return true;
    });

    std::map<std::shared_ptr<ISyncInfoObserver>, BatchQueryLastResults> observerData;
    cloudServiceProxy->CollectObserverData(TEST_BUNDLE_NAME, storeResults, storeMap, observerData);
    EXPECT_TRUE(observerData.empty());
}

/**
 * @tc.name: CollectObserverData_StoreIdFound
 * @tc.desc: Test CollectObserverData when storeId found in storeMap
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(CloudDataTest, CollectObserverData_StoreIdFound, TestSize.Level1)
{
    auto [state, proxy] = CloudManager::GetInstance().GetCloudService();
    ASSERT_EQ(state == CloudService::SUCCESS && proxy != nullptr, true);
    auto cloudServiceProxy = std::static_pointer_cast<CloudServiceProxy>(proxy);
    ASSERT_NE(cloudServiceProxy, nullptr);

    auto observer = std::make_shared<MockSyncInfoObserver>();
    std::vector<BundleInfo> bundleInfos = {{TEST_BUNDLE_NAME, TEST_STORE_ID}};
    ExpectReply(CloudService::TRANS_SUBSCRIBE, CloudService::SUCCESS);
    proxy->Subscribe(CloudSubscribeType::SYNC_INFO_CHANGED, bundleInfos, observer);

    std::map<std::string, CloudSyncInfo> storeResults;
    CloudSyncInfo syncInfo;
    syncInfo.code = 0;
    syncInfo.syncStatus = SyncStatus::FINISHED;
    storeResults[TEST_STORE_ID] = syncInfo;

    auto subObservers = cloudServiceProxy->ExportSubObservers();
    std::map<std::string, std::list<CloudServiceProxy::SubObserverParam>> storeMap;
    subObservers.ComputeIfPresent(TEST_BUNDLE_NAME, [&storeMap](const auto &, auto &map) {
        storeMap = map;
        return true;
    });

    std::map<std::shared_ptr<ISyncInfoObserver>, BatchQueryLastResults> observerData;
    cloudServiceProxy->CollectObserverData(TEST_BUNDLE_NAME, storeResults, storeMap, observerData);
    EXPECT_FALSE(observerData.empty());
}

/**
 * @tc.name: CollectObserverData_WildcardStoreId
 * @tc.desc: Test CollectObserverData with wildcard storeId (empty string)
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(CloudDataTest, CollectObserverData_WildcardStoreId, TestSize.Level1)
{
    auto [state, proxy] = CloudManager::GetInstance().GetCloudService();
    ASSERT_EQ(state == CloudService::SUCCESS && proxy != nullptr, true);
    auto cloudServiceProxy = std::static_pointer_cast<CloudServiceProxy>(proxy);
    ASSERT_NE(cloudServiceProxy, nullptr);

    auto observer = std::make_shared<MockSyncInfoObserver>();
    std::vector<BundleInfo> bundleInfos = {{TEST_BUNDLE_NAME, ""}};
    ExpectReply(CloudService::TRANS_SUBSCRIBE, CloudService::SUCCESS);
    proxy->Subscribe(CloudSubscribeType::SYNC_INFO_CHANGED, bundleInfos, observer);

    std::map<std::string, CloudSyncInfo> storeResults;
    CloudSyncInfo syncInfo1;
    syncInfo1.code = 0;
    CloudSyncInfo syncInfo2;
    syncInfo2.code = 1;
    storeResults["store_001"] = syncInfo1;
    storeResults["store_002"] = syncInfo2;

    auto subObservers = cloudServiceProxy->ExportSubObservers();
    std::map<std::string, std::list<CloudServiceProxy::SubObserverParam>> storeMap;
    subObservers.ComputeIfPresent(TEST_BUNDLE_NAME, [&storeMap](const auto &, auto &map) {
        storeMap = map;
        return true;
    });

    std::map<std::shared_ptr<ISyncInfoObserver>, BatchQueryLastResults> observerData;
    cloudServiceProxy->CollectObserverData(TEST_BUNDLE_NAME, storeResults, storeMap, observerData);
    EXPECT_FALSE(observerData.empty());
}

/**
 * @tc.name: CollectObserverData_MultipleStores
 * @tc.desc: Test CollectObserverData with multiple stores matching
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(CloudDataTest, CollectObserverData_MultipleStores, TestSize.Level1)
{
    auto [state, proxy] = CloudManager::GetInstance().GetCloudService();
    ASSERT_EQ(state == CloudService::SUCCESS && proxy != nullptr, true);
    auto cloudServiceProxy = std::static_pointer_cast<CloudServiceProxy>(proxy);
    ASSERT_NE(cloudServiceProxy, nullptr);

    auto observer = std::make_shared<MockSyncInfoObserver>();
    std::vector<BundleInfo> bundleInfos = {
        {TEST_BUNDLE_NAME, "store_001"},
        {TEST_BUNDLE_NAME, "store_002"}
    };
    ExpectReply(CloudService::TRANS_SUBSCRIBE, CloudService::SUCCESS);
    proxy->Subscribe(CloudSubscribeType::SYNC_INFO_CHANGED, bundleInfos, observer);

    std::map<std::string, CloudSyncInfo> storeResults;
    CloudSyncInfo syncInfo1;
    syncInfo1.code = 0;
    CloudSyncInfo syncInfo2;
    syncInfo2.code = 1;
    storeResults["store_001"] = syncInfo1;
    storeResults["store_002"] = syncInfo2;
    storeResults["store_003"] = CloudSyncInfo{};

    auto subObservers = cloudServiceProxy->ExportSubObservers();
    std::map<std::string, std::list<CloudServiceProxy::SubObserverParam>> storeMap;
    subObservers.ComputeIfPresent(TEST_BUNDLE_NAME, [&storeMap](const auto &, auto &map) {
        storeMap = map;
        return true;
    });

    std::map<std::shared_ptr<ISyncInfoObserver>, BatchQueryLastResults> observerData;
    cloudServiceProxy->CollectObserverData(TEST_BUNDLE_NAME, storeResults, storeMap, observerData);
    EXPECT_FALSE(observerData.empty());
}

/**
 * @tc.name: OnSyncInfoNotifyInner_Success
 * @tc.desc: Test OnSyncInfoNotifyInner with valid data
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(CloudDataTest, OnSyncInfoNotifyInner_Success, TestSize.Level1)
{
    bool handlerCalled = false;
    BatchQueryLastResults receivedData;
    sptr<CloudNotifierStub> notifier = new CloudNotifierStub(
        [](uint32_t, Details &&) {},
        [&handlerCalled, &receivedData](const BatchQueryLastResults &data) {
            handlerCalled = true;
            receivedData = data;
        });

    MessageParcel data;
    MessageParcel reply;

    BatchQueryLastResults testData;
    QueryLastResults storeResults;
    CloudSyncInfo syncInfo;
    syncInfo.code = 0;
    syncInfo.syncStatus = SyncStatus::FINISHED;
    storeResults[TEST_STORE_ID] = syncInfo;
    testData[TEST_BUNDLE_NAME] = storeResults;

    ASSERT_TRUE(ITypesUtil::Marshal(data, testData));

    auto result = notifier->OnSyncInfoNotifyInner(data, reply);
    EXPECT_EQ(result, CloudService::SUCCESS);
    EXPECT_TRUE(handlerCalled);
}

/**
 * @tc.name: OnSyncInfoNotifyInner_UnmarshalFailed
 * @tc.desc: Test OnSyncInfoNotifyInner with invalid MessageParcel
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(CloudDataTest, OnSyncInfoNotifyInner_UnmarshalFailed, TestSize.Level1)
{
    sptr<CloudNotifierStub> notifier = new CloudNotifierStub(
        [](uint32_t, Details &&) {},
        [](const BatchQueryLastResults &) {});

    MessageParcel data;
    MessageParcel reply;

    auto result = notifier->OnSyncInfoNotifyInner(data, reply);
    EXPECT_EQ(result, CloudService::IPC_PARCEL_ERROR);
}

/**
 * @tc.name: OnSyncInfoNotifyInner_EmptyData
 * @tc.desc: Test OnSyncInfoNotifyInner with empty data
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(CloudDataTest, OnSyncInfoNotifyInner_EmptyData, TestSize.Level1)
{
    bool handlerCalled = false;
    sptr<CloudNotifierStub> notifier = new CloudNotifierStub(
        [](uint32_t, Details &&) {},
        [&handlerCalled](const BatchQueryLastResults &data) {
            handlerCalled = true;
            EXPECT_TRUE(data.empty());
        });

    MessageParcel data;
    MessageParcel reply;

    BatchQueryLastResults emptyData;
    ASSERT_TRUE(ITypesUtil::Marshal(data, emptyData));

    auto result = notifier->OnSyncInfoNotifyInner(data, reply);
    EXPECT_EQ(result, CloudService::SUCCESS);
    EXPECT_TRUE(handlerCalled);
}

/**
 * @tc.name: OnSyncInfoNotifyInner_NullHandler
 * @tc.desc: Test OnSyncInfoNotifyInner with null handler
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(CloudDataTest, OnSyncInfoNotifyInner_NullHandler, TestSize.Level1)
{
    sptr<CloudNotifierStub> notifier = new CloudNotifierStub(nullptr, nullptr);

    MessageParcel data;
    MessageParcel reply;

    BatchQueryLastResults testData;
    QueryLastResults storeResults;
    storeResults[TEST_STORE_ID] = CloudSyncInfo{};
    testData[TEST_BUNDLE_NAME] = storeResults;

    ASSERT_TRUE(ITypesUtil::Marshal(data, testData));

    auto result = notifier->OnSyncInfoNotifyInner(data, reply);
    EXPECT_EQ(result, CloudService::SUCCESS);
}

/**
 * @tc.name: OnRemoteDied_NullCloudService
 * @tc.desc: Test OnRemoteDied when cloudService_ is nullptr
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(CloudDataTest, OnRemoteDied_NullCloudService, TestSize.Level1)
{
    auto &manager = CloudManager::GetInstance();
    auto [state, proxy] = manager.GetCloudService();
    ASSERT_EQ(state == CloudService::SUCCESS && proxy != nullptr, true);

    auto cloudServiceProxy = std::static_pointer_cast<CloudServiceProxy>(proxy);
    ASSERT_NE(cloudServiceProxy, nullptr);

    auto observer = std::make_shared<MockSyncInfoObserver>();
    std::vector<BundleInfo> bundleInfos = {{TEST_BUNDLE_NAME, TEST_STORE_ID}};
    ExpectReply(CloudService::TRANS_SUBSCRIBE, CloudService::SUCCESS);
    proxy->Subscribe(CloudSubscribeType::SYNC_INFO_CHANGED, bundleInfos, observer);

    auto exportedObservers = cloudServiceProxy->ExportSubObservers();
    EXPECT_FALSE(exportedObservers.Empty());
}

/**
 * @tc.name: OnRemoteDied_ExportAndImportObservers
 * @tc.desc: Test observer export and import during OnRemoteDied
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(CloudDataTest, OnRemoteDied_ExportAndImportObservers, TestSize.Level1)
{
    auto [state, proxy] = CloudManager::GetInstance().GetCloudService();
    ASSERT_EQ(state == CloudService::SUCCESS && proxy != nullptr, true);
    auto cloudServiceProxy = std::static_pointer_cast<CloudServiceProxy>(proxy);
    ASSERT_NE(cloudServiceProxy, nullptr);

    auto observer = std::make_shared<MockSyncInfoObserver>();
    std::vector<BundleInfo> bundleInfos = {{TEST_BUNDLE_NAME, TEST_STORE_ID}};
    ExpectReply(CloudService::TRANS_SUBSCRIBE, CloudService::SUCCESS);
    proxy->Subscribe(CloudSubscribeType::SYNC_INFO_CHANGED, bundleInfos, observer);

    auto exportedObservers = cloudServiceProxy->ExportSubObservers();
    EXPECT_FALSE(exportedObservers.Empty());

    auto newProxy = std::static_pointer_cast<CloudServiceProxy>(
        CloudManager::GetInstance().GetCloudService().second);
    ASSERT_NE(newProxy, nullptr);
    ExpectReply(CloudService::TRANS_SUBSCRIBE, CloudService::SUCCESS);
    newProxy->ImportSubObservers(exportedObservers);

    auto reExportedObservers = newProxy->ExportSubObservers();
    EXPECT_FALSE(reExportedObservers.Empty());
}

/**
 * @tc.name: OnSyncInfoNotify_ObserverDataVerification
 * @tc.desc: Test OnSyncInfoNotify verifies correct data passed to observer
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(CloudDataTest, OnSyncInfoNotify_ObserverDataVerification, TestSize.Level1)
{
    auto [state, proxy] = CloudManager::GetInstance().GetCloudService();
    ASSERT_EQ(state == CloudService::SUCCESS && proxy != nullptr, true);
    auto cloudServiceProxy = std::static_pointer_cast<CloudServiceProxy>(proxy);
    ASSERT_NE(cloudServiceProxy, nullptr);

    auto observer = std::make_shared<MockSyncInfoObserver>();
    std::vector<BundleInfo> bundleInfos = {{TEST_BUNDLE_NAME, TEST_STORE_ID}};
    ExpectReply(CloudService::TRANS_SUBSCRIBE, CloudService::SUCCESS);
    proxy->Subscribe(CloudSubscribeType::SYNC_INFO_CHANGED, bundleInfos, observer);

    BatchQueryLastResults data;
    QueryLastResults storeResults;
    CloudSyncInfo syncInfo;
    syncInfo.code = 100;
    syncInfo.syncStatus = SyncStatus::FINISHED;
    syncInfo.startTime = 1000;
    syncInfo.finishTime = 2000;
    storeResults[TEST_STORE_ID] = syncInfo;
    data[TEST_BUNDLE_NAME] = storeResults;

    cloudServiceProxy->OnSyncInfoNotify(data);
    EXPECT_TRUE(observer->IsCalled());

    const auto &receivedData = observer->GetData();
    EXPECT_EQ(receivedData.size(), 1u);
    EXPECT_EQ(receivedData.count(TEST_BUNDLE_NAME), 1u);
    EXPECT_EQ(receivedData.at(TEST_BUNDLE_NAME).at(TEST_STORE_ID).code, 100);
    EXPECT_EQ(receivedData.at(TEST_BUNDLE_NAME).at(TEST_STORE_ID).syncStatus, SyncStatus::FINISHED);
}

/**
 * @tc.name: OnSyncInfoNotify_ObserverDataVerification
 * @tc.desc: Test OnSyncInfoNotify verifies correct data passed to observer
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(CloudDataTest, OnSyncInfoNotify_ObserverDataVerification001, TestSize.Level1)
{
    auto [state, proxy] = CloudManager::GetInstance().GetCloudService();
    ASSERT_EQ(state == CloudService::SUCCESS && proxy != nullptr, true);
    auto cloudServiceProxy = std::static_pointer_cast<CloudServiceProxy>(proxy);
    ASSERT_NE(cloudServiceProxy, nullptr);
 
    auto observer = std::make_shared<MockSyncInfoObserver>();
    ExpectReply(CloudService::TRANS_SUBSCRIBE_CLOUD_SYNC_TRIGGER, CloudService::SUCCESS);
    proxy->SubscribeCloudSyncTrigger(observer);
 
    BatchQueryLastResults data;
    QueryLastResults storeResults;
    int32_t triggerMode = 1;
 
    cloudServiceProxy->OnCloudSyncTrigger(triggerMode);
    EXPECT_TRUE(observer->IsCalled());
    EXPECT_EQ(observer->GetTriggerMode(), triggerMode);
    ExpectReply(CloudService::TRANS_UNSUBSCRIBE_CLOUD_SYNC_TRIGGER, CloudService::SUCCESS);
    proxy->UnSubscribeCloudSyncTrigger(observer);
}

/**
 * @tc.name: CloudSync010
 * @tc.desc: Test CloudSync with empty storeId and system permission
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(CloudDataTest, CloudSync010, TestSize.Level1)
{
    auto [state, proxy] = CloudManager::GetInstance().GetCloudService();
    ASSERT_EQ(state == CloudService::SUCCESS && proxy != nullptr, true);
    auto progress = [](DistributedRdb::Details &&) {};
    int32_t syncMode = 4;
    uint32_t seqNum = 23;
    BundleInfo bundleInfo = { TEST_BUNDLE_NAME, "" };
    ExpectReply(CloudService::TRANS_CLOUD_SYNC, CloudService::SUCCESS);
    auto status = proxy->CloudSync(bundleInfo, { syncMode, seqNum }, progress);
    EXPECT_NE(status, CloudService::INVALID_ARGUMENT_V20);
    LOG_INFO("CloudSync010 test end.");
}

/**
 * @tc.name: CloudSync011
 * @tc.desc: Test CloudSync with empty storeId and no config permission
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(CloudDataTest, CloudSync011, TestSize.Level1)
{
    auto [state, proxy] = CloudManager::GetInstance().GetCloudService();
    ASSERT_EQ(state == CloudService::SUCCESS && proxy != nullptr, true);
    auto progress = [](DistributedRdb::Details &&) {};
    int32_t syncMode = 4;
    uint32_t seqNum = 24;
    BundleInfo bundleInfo = { TEST_BUNDLE_NAME, "" };
    ExpectReply(CloudService::TRANS_CLOUD_SYNC, CloudService::CLOUD_CONFIG_PERMISSION_DENIED);
    auto status = proxy->CloudSync(bundleInfo, { syncMode, seqNum }, progress);
    EXPECT_EQ(status, CloudService::CLOUD_CONFIG_PERMISSION_DENIED);
    LOG_INFO("CloudSync011 test end.");
}

/**
 * @tc.name: Subscribe_EmptyStoreId
 * @tc.desc: Test Subscribe with empty storeId in BundleInfo
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(CloudDataTest, Subscribe_EmptyStoreId, TestSize.Level1)
{
    auto [state, proxy] = CloudManager::GetInstance().GetCloudService();
    ASSERT_EQ(state == CloudService::SUCCESS && proxy != nullptr, true);
    auto observer = std::make_shared<MockSyncInfoObserver>();
    std::vector<BundleInfo> bundleInfos = {{TEST_BUNDLE_NAME, ""}};
    ExpectReply(CloudService::TRANS_SUBSCRIBE, CloudService::SUCCESS);
    auto status = proxy->Subscribe(CloudSubscribeType::SYNC_INFO_CHANGED, bundleInfos, observer);
    EXPECT_EQ(status, CloudService::SUCCESS);
    ExpectReply(CloudService::TRANS_UNSUBSCRIBE, CloudService::SUCCESS);
    status = proxy->Unsubscribe(CloudSubscribeType::SYNC_INFO_CHANGED, bundleInfos, observer);
    EXPECT_EQ(status, CloudService::SUCCESS);
}

/**
 * @tc.name: Unsubscribe_EmptyStoreId
 * @tc.desc: Test Unsubscribe with empty storeId in BundleInfo
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(CloudDataTest, Unsubscribe_EmptyStoreId, TestSize.Level1)
{
    auto [state, proxy] = CloudManager::GetInstance().GetCloudService();
    ASSERT_EQ(state == CloudService::SUCCESS && proxy != nullptr, true);
    auto observer = std::make_shared<MockSyncInfoObserver>();
    std::vector<BundleInfo> bundleInfos = {{TEST_BUNDLE_NAME, ""}};
    ExpectReply(CloudService::TRANS_SUBSCRIBE, CloudService::SUCCESS);
    proxy->Subscribe(CloudSubscribeType::SYNC_INFO_CHANGED, bundleInfos, observer);
    ExpectReply(CloudService::TRANS_UNSUBSCRIBE, CloudService::SUCCESS);
    auto status = proxy->Unsubscribe(CloudSubscribeType::SYNC_INFO_CHANGED, bundleInfos, observer);
    EXPECT_EQ(status, CloudService::SUCCESS);
}

/**
 * @tc.name: OnSyncInfoNotify_EmptyStoreIdObserver
 * @tc.desc: Test OnSyncInfoNotify with observer subscribed with empty storeId
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(CloudDataTest, OnSyncInfoNotify_EmptyStoreIdObserver, TestSize.Level1)
{
    auto [state, proxy] = CloudManager::GetInstance().GetCloudService();
    ASSERT_EQ(state == CloudService::SUCCESS && proxy != nullptr, true);
    auto cloudServiceProxy = std::static_pointer_cast<CloudServiceProxy>(proxy);
    ASSERT_NE(cloudServiceProxy, nullptr);

    auto observer = std::make_shared<MockSyncInfoObserver>();
    std::vector<BundleInfo> bundleInfos = {{TEST_BUNDLE_NAME, ""}};
    ExpectReply(CloudService::TRANS_SUBSCRIBE, CloudService::SUCCESS);
    proxy->Subscribe(CloudSubscribeType::SYNC_INFO_CHANGED, bundleInfos, observer);

    BatchQueryLastResults data;
    QueryLastResults storeResults;
    CloudSyncInfo syncInfo;
    syncInfo.code = 0;
    syncInfo.syncStatus = SyncStatus::FINISHED;
    storeResults[TEST_STORE_ID] = syncInfo;
    data[TEST_BUNDLE_NAME] = storeResults;

    cloudServiceProxy->OnSyncInfoNotify(data);
    EXPECT_TRUE(observer->IsCalled());

    const auto &receivedData = observer->GetData();
    EXPECT_EQ(receivedData.size(), 1u);
    EXPECT_EQ(receivedData.count(TEST_BUNDLE_NAME), 1u);
}

/**
 * @tc.name: CloudSync_BundleInfo_001
 * @tc.desc: Test CloudSync with BundleInfo and null async callback
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(CloudDataTest, CloudSync_BundleInfo_001, TestSize.Level1)
{
    auto [state, proxy] = CloudManager::GetInstance().GetCloudService();
    ASSERT_EQ(state == CloudService::SUCCESS && proxy != nullptr, true);
    BundleInfo bundleInfo = { TEST_BUNDLE_NAME, TEST_STORE_ID };
    auto ret = proxy->CloudSync(bundleInfo, { 4, 200 }, nullptr);
    EXPECT_EQ(ret, CloudService::INVALID_ARGUMENT_V20);
    LOG_INFO("CloudSync_BundleInfo_001 test end.");
}

/**
 * @tc.name: CloudSync_BundleInfo_002
 * @tc.desc: Test CloudSync with BundleInfo and empty bundleName
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(CloudDataTest, CloudSync_BundleInfo_002, TestSize.Level1)
{
    auto [state, proxy] = CloudManager::GetInstance().GetCloudService();
    ASSERT_EQ(state == CloudService::SUCCESS && proxy != nullptr, true);
    auto progress = [](DistributedRdb::Details &&) {};
    BundleInfo bundleInfo = { "", TEST_STORE_ID };
    auto ret = proxy->CloudSync(bundleInfo, { 4, 201 }, progress);
    EXPECT_EQ(ret, CloudService::INVALID_ARGUMENT_V20);
    LOG_INFO("CloudSync_BundleInfo_002 test end.");
}

/**
 * @tc.name: CloudSync_BundleInfo_003
 * @tc.desc: Test CloudSync with BundleInfo and invalid syncMode
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(CloudDataTest, CloudSync_BundleInfo_003, TestSize.Level1)
{
    auto [state, proxy] = CloudManager::GetInstance().GetCloudService();
    ASSERT_EQ(state == CloudService::SUCCESS && proxy != nullptr, true);
    auto progress = [](DistributedRdb::Details &&) {};
    BundleInfo bundleInfo = { TEST_BUNDLE_NAME, TEST_STORE_ID };
    auto ret = proxy->CloudSync(bundleInfo, { 10, 202 }, progress);
    EXPECT_EQ(ret, CloudService::INVALID_ARGUMENT_V20);
    LOG_INFO("CloudSync_BundleInfo_003 test end.");
}

/**
 * @tc.name: CloudSync_BundleInfo_005
 * @tc.desc: Test CloudSync with BundleInfo and non-empty storeId
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(CloudDataTest, CloudSync_BundleInfo_005, TestSize.Level1)
{
    auto [state, proxy] = CloudManager::GetInstance().GetCloudService();
    ASSERT_EQ(state == CloudService::SUCCESS && proxy != nullptr, true);
    auto progress = [](DistributedRdb::Details &&) {};
    BundleInfo bundleInfo = { TEST_BUNDLE_NAME, TEST_STORE_ID };
    ExpectReply(CloudService::TRANS_CLOUD_SYNC, CloudService::SUCCESS);
    auto status = proxy->CloudSync(bundleInfo, { 4, 204 }, progress);
    EXPECT_EQ(status, CloudService::SUCCESS);
    LOG_INFO("CloudSync_BundleInfo_005 test end.");
}

/**
 * @tc.name: CloudSync_BundleInfo_006
 * @tc.desc: Test CloudSync with BundleInfo and normal hap permission
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(CloudDataTest, CloudSync_BundleInfo_006, TestSize.Level1)
{
    auto [state, proxy] = CloudManager::GetInstance().GetCloudService();
    ASSERT_EQ(state == CloudService::SUCCESS && proxy != nullptr, true);
    auto progress = [](DistributedRdb::Details &&) {};
    BundleInfo bundleInfo = { TEST_BUNDLE_NAME, TEST_STORE_ID };
    ExpectReply(CloudService::TRANS_CLOUD_SYNC, CloudService::PERMISSION_DENIED);
    auto status = proxy->CloudSync(bundleInfo, { 4, 205 }, progress);
    EXPECT_EQ(status, CloudService::PERMISSION_DENIED);
    LOG_INFO("CloudSync_BundleInfo_006 test end.");
}
/**
 * @tc.name: QueryStatistics_ParcelRoundTrip
 * @tc.desc: Distinct request fields and a nonempty reply exercise the real proxy codec.
 * @tc.type: FUNC
 * @tc.author: agent
 */
HWTEST_F(CloudDataTest, QueryStatistics_ParcelRoundTrip, TestSize.Level0)
{
    EXPECT_CALL(*remote_, SendRequest(CloudService::TRANS_QUERY_STATISTICS, testing::_, testing::_, testing::_))
        .WillOnce(testing::Invoke([](uint32_t, MessageParcel &request, MessageParcel &reply, MessageOption &) {
            EXPECT_EQ(request.ReadInterfaceToken(), CloudServiceProxy::GetDescriptor());
            std::string account;
            std::string bundle;
            std::string store;
            EXPECT_TRUE(ITypesUtil::Unmarshal(request, account, bundle, store));
            EXPECT_EQ(account, "account-roundtrip");
            EXPECT_EQ(bundle, "bundle-roundtrip");
            EXPECT_EQ(store, "store-roundtrip");
            // A one-entry map containing one statistic, encoded using the server wire format.
            EXPECT_TRUE(ITypesUtil::Marshal(reply, int32_t(CloudService::SUCCESS), int32_t{1},
                std::string("store-roundtrip"), int32_t{1}, std::string("table-roundtrip"), 11, 22, 33));
            return 0;
        }));

    auto [status, infos] = proxy_->QueryStatistics("account-roundtrip", "bundle-roundtrip", "store-roundtrip");

    ASSERT_EQ(status, CloudService::SUCCESS);
    ASSERT_EQ(infos.size(), 1u);
    ASSERT_EQ(infos.count("store-roundtrip"), 1u);
    const auto &statistics = infos.at("store-roundtrip");
    ASSERT_EQ(statistics.size(), 1u);
    EXPECT_EQ(statistics[0].table, "table-roundtrip");
    EXPECT_EQ(statistics[0].inserted, 11);
    EXPECT_EQ(statistics[0].updated, 22);
    EXPECT_EQ(statistics[0].normal, 33);
}
}

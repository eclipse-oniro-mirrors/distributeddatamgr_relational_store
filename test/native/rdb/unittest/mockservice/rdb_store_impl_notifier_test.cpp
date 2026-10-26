/**
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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "block_data.h"
#include "common.h"
#include "connection_mock.h"
#include "delay_notify.h"
#include "rdb_errno.h"
#include "rdb_store_impl.h"
#include "task_executor.h"

using namespace testing;
using namespace testing::ext;
using namespace OHOS;
using namespace OHOS::NativeRdb;

namespace {
constexpr int32_t waitSeconds = 5;
constexpr int32_t registrationThreads = 8;
constexpr auto blockedReadTimeout = std::chrono::milliseconds(100);

class RdbStoreImplNotifierTest : public Test {
public:
    void SetUp() override
    {
        RdbStoreConfig config(RDB_TEST_PATH + "rdb_store_impl_notifier_test.db");
        store_ = std::make_shared<RdbStoreImpl>(config);
        connection_ = std::make_shared<NiceMock<MockConnection>>();
        ON_CALL(*connection_, IsWriter()).WillByDefault(Return(true));
        pool_ = std::make_shared<ConnectionPool>(store_->configHolder_, store_->config_);
        auto [errCode, node] = pool_->writers_.Initialize(
            [connection = connection_]() { return std::make_pair(E_OK, connection); }, 1, waitSeconds, false);
        ASSERT_EQ(E_OK, errCode);
        store_->connectionPool_ = pool_;
    }

    void TearDown() override
    {
        store_.reset();
        pool_.reset();
        connection_.reset();
    }

protected:
    // GetKnowledgeSchemaHelper precedes GetExecutor inside InitDelayNotifier.
    bool WaitForInitializationStart()
    {
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(waitSeconds);
        while (std::chrono::steady_clock::now() < deadline) {
            {
                std::lock_guard<std::mutex> lock(store_->helperMutex_);
                if (store_->knowledgeSchemaHelper_ != nullptr) {
                    return true;
                }
            }
            std::this_thread::yield();
        }
        return false;
    }

    std::vector<int32_t> RegisterConcurrently()
    {
        auto start = std::make_shared<BlockData<bool>>(waitSeconds, false);
        auto ready = std::make_shared<BlockData<bool>>(waitSeconds, false);
        auto readyCount = std::make_shared<std::atomic<int32_t>>(0);
        auto results = std::make_shared<std::vector<int32_t>>(registrationThreads, E_ERROR);
        std::vector<std::thread> threads;
        for (int32_t i = 0; i < registrationThreads; ++i) {
            threads.emplace_back([store = store_, start, ready, readyCount, results, i]() {
                if (readyCount->fetch_add(1) + 1 == registrationThreads) {
                    ready->SetValue(true);
                }
                if (start->GetValue()) {
                    (*results)[i] = store->RegisterDataChangeCallback();
                }
            });
        }
        bool allReady = ready->GetValue();
        // Always release and join workers, including readiness timeout failures.
        start->SetValue(true);
        for (auto &thread : threads) {
            thread.join();
        }
        EXPECT_TRUE(allReady);
        return *results;
    }

    std::shared_ptr<RdbStoreImpl> store_;
    std::shared_ptr<MockConnection> connection_;
    std::shared_ptr<ConnectionPool> pool_;
};

/**
 * @tc.name: RegisterDataChangeCallback_001
 * @tc.desc: Concurrent callbacks deliver changes to the same fully initialized notifier
 * @tc.type: FUNC
 * @tc.require: issue2777
 */
HWTEST_F(RdbStoreImplNotifierTest, RegisterDataChangeCallback_001, TestSize.Level2)
{
    // Arrange: retain each callback so its target can be verified after registration.
    std::mutex callbacksMutex;
    std::vector<Connection::Notifier> callbacks;
    EXPECT_CALL(*connection_, SubscribeTableChanges(_))
        .Times(registrationThreads)
        .WillRepeatedly([&](const Connection::Notifier &callback) {
            std::lock_guard<std::mutex> lock(callbacksMutex);
            callbacks.push_back(callback);
            return E_OK;
        });

    // Act: release all registering threads together.
    for (auto result : RegisterConcurrently()) {
        EXPECT_EQ(E_OK, result) << "Concurrent callback registration failed";
    }

    // Assert: every captured callback must update the published notifier.
    ASSERT_EQ(registrationThreads, callbacks.size()) << "A registering thread did not supply a callback";
    auto notifier = store_->GetDelayNotifier();
    ASSERT_NE(nullptr, notifier) << "The notifier was not published";
    EXPECT_NE(nullptr, notifier->pool_) << "The notifier executor was not initialized";
    EXPECT_TRUE(notifier->task_) << "The notification task was not initialized";
    // Disable scheduling before invoking callbacks to isolate delivery from service notifications.
    notifier->pool_.reset();
    for (size_t i = 0; i < callbacks.size(); ++i) {
        ASSERT_TRUE(callbacks[i]) << "Empty callback at index " << i;
        auto table = "table_" + std::to_string(i);
        DistributedRdb::RdbChangedData changes;
        changes.tableData[table].isP2pSyncDataChange = true;
        callbacks[i](changes);
        EXPECT_EQ(1u, notifier->changedData_.tableData.count(table)) << "Callback target differs at index " << i;
    }
}

/**
 * @tc.name: InitDelayNotifier_001
 * @tc.desc: Readers wait for complete initialization and repeated initialization reuses the notifier
 * @tc.type: FUNC
 * @tc.require: issue2777
 */
HWTEST_F(RdbStoreImplNotifierTest, InitDelayNotifier_001, TestSize.Level2)
{
    EXPECT_EQ(nullptr, store_->GetDelayNotifier());
    auto &executor = TaskExecutor::GetInstance();
    // Pause initialization before SetExecutorPool/SetTask without adding a production test hook.
    std::unique_lock<std::mutex> executorLock(executor.mutex_);
    std::thread initializer([store = store_]() { store->InitDelayNotifier(); });
    EXPECT_TRUE(WaitForInitializationStart());
    auto readerStarted = std::make_shared<BlockData<bool>>(waitSeconds, false);
    std::packaged_task<std::shared_ptr<DelayNotify>()> read([store = store_, readerStarted]() {
        readerStarted->SetValue(true);
        return store->GetDelayNotifier();
    });
    auto result = read.get_future();
    std::thread reader(std::move(read));
    EXPECT_TRUE(readerStarted->GetValue());
    EXPECT_EQ(std::future_status::timeout, result.wait_for(blockedReadTimeout));
    executorLock.unlock();
    initializer.join();
    reader.join();

    auto notifier = result.get();
    ASSERT_NE(nullptr, notifier);
    EXPECT_EQ(executor.GetExecutor(), notifier->pool_);
    EXPECT_TRUE(notifier->task_);
    store_->InitDelayNotifier();
    EXPECT_EQ(notifier, store_->GetDelayNotifier());
}

/**
 * @tc.name: GetDelayNotifier_001
 * @tc.desc: A writer holder can read the notifier while registration waits for that writer
 * @tc.type: FUNC
 * @tc.require: issue2777
 */
HWTEST_F(RdbStoreImplNotifierTest, GetDelayNotifier_001, TestSize.Level2)
{
    auto writer = pool_->AcquireConnection(false);
    ASSERT_NE(nullptr, writer);
    EXPECT_CALL(*connection_, SubscribeTableChanges(_)).Times(1).WillOnce(Return(E_OK));
    std::packaged_task<int32_t()> registerCallback([store = store_]() { return store->RegisterDataChangeCallback(); });
    auto registered = registerCallback.get_future();
    std::thread registering(std::move(registerCallback));
    EXPECT_TRUE(WaitForInitializationStart());
    std::packaged_task<std::shared_ptr<DelayNotify>()> read([store = store_]() { return store->GetDelayNotifier(); });
    auto result = read.get_future();
    std::thread reader(std::move(read));
    auto readStatus = result.wait_for(std::chrono::seconds(waitSeconds));
    // Release the writer even when the read times out, so both workers can exit.
    writer.reset();
    registering.join();
    reader.join();

    EXPECT_EQ(std::future_status::ready, readStatus);
    EXPECT_EQ(E_OK, registered.get());
    auto notifier = result.get();
    ASSERT_NE(nullptr, notifier);
    EXPECT_EQ(notifier, store_->GetDelayNotifier());
}
} // namespace

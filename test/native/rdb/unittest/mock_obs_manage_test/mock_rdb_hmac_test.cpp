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
#include <gtest/gtest.h>

#include "mock_hmac.h"
#include "rdb_security_manager.h"

using namespace testing::ext;
using namespace OHOS::NativeRdb;

class MockRdbHmacTest : public testing::Test {
public:
    void SetUp() override
    {
        ::Test::GetHmacState() = {};
    }

    void TearDown() override
    {
        ::Test::GetHmacState() = {};
    }
};

/**
 * @brief Preserve the zero checksum fallback and release contexts on every MAC failure.
 * @tc.name: Rdb_Hmac_BackendFailures_001
 * @tc.type: RELI
 */
HWTEST_F(MockRdbHmacTest, Rdb_Hmac_BackendFailures_001, TestSize.Level3)
{
    for (const auto failure : { ::Test::HmacFailure::ALLOCATE, ::Test::HmacFailure::INIT, ::Test::HmacFailure::UPDATE,
             ::Test::HmacFailure::FINAL, ::Test::HmacFailure::LENGTH }) {
        // Arrange: each iteration injects a separate failure into a local manager.
        ::Test::GetHmacState() = {};
        ::Test::GetHmacState().failure = failure;
        RdbSecurityManager manager;
        std::vector<char> data = { 'a' };
        // Act.
        const auto result = manager.GenerateHMAC(data);
        // Assert: the mock writes nonzero output, which must never escape on failure.
        EXPECT_EQ(result, std::vector<char>(manager.HMAC_SIZE, 0)) << "stage=" << static_cast<int>(failure);
        const auto &state = ::Test::GetHmacState();
        const uint32_t allocated = failure == ::Test::HmacFailure::ALLOCATE ? 0U : 1U;
        const uint32_t updated = allocated != 0 && failure != ::Test::HmacFailure::INIT ? 1U : 0U;
        const uint32_t finalized = updated != 0 && failure != ::Test::HmacFailure::UPDATE ? 1U : 0U;
        EXPECT_EQ(state.created, allocated) << "allocation stage=" << static_cast<int>(failure);
        EXPECT_EQ(state.freed, allocated) << "release stage=" << static_cast<int>(failure);
        EXPECT_EQ(state.initCalls, allocated) << "initialization stage=" << static_cast<int>(failure);
        EXPECT_EQ(state.updateCalls, updated) << "update stage=" << static_cast<int>(failure);
        EXPECT_EQ(state.finalCalls, finalized) << "finalization stage=" << static_cast<int>(failure);
    }
}

/**
 * @brief Supply an empty key and release the successful per-call MAC context.
 * @tc.name: Rdb_Hmac_EmptyKeyLifecycle_001
 * @tc.type: FUNC
 */
HWTEST_F(MockRdbHmacTest, Rdb_Hmac_EmptyKeyLifecycle_001, TestSize.Level1)
{
    // Arrange.
    RdbSecurityManager manager;
    std::vector<char> data;
    const std::vector<uint8_t> bytes = { 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xab, 0xac };
    // Act.
    const auto result = manager.GenerateHMAC(data);
    // Assert.
    EXPECT_EQ(result, std::vector<char>(bytes.begin(), bytes.end())) << "truncate successful digest";
    EXPECT_EQ(::Test::GetHmacState().keyLength, 0U) << "placeholder byte is not part of the key";
    EXPECT_EQ(::Test::GetHmacState().created, 1U) << "one context per call";
    EXPECT_EQ(::Test::GetHmacState().freed, 1U) << "context released before return";
}

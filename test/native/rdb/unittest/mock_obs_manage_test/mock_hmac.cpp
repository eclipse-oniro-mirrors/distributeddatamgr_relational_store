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
#include "mock_hmac.h"

#include <algorithm>

#include "crypt_eal_mac.h"
#include "crypt_errno.h"

namespace Test {
HmacState &GetHmacState()
{
    static HmacState state;
    return state;
}
} // namespace Test

namespace {
constexpr uint32_t SHA256_SIZE = 32;
}

using Test::GetHmacState;
using Test::HmacFailure;
extern "C" {
CRYPT_EAL_MacCtx *CRYPT_EAL_MacNewCtx(CRYPT_MAC_AlgId id)
{
    auto &state = GetHmacState();
    if (id != CRYPT_MAC_HMAC_SHA256 || state.failure == HmacFailure::ALLOCATE) {
        return nullptr;
    }
    ++state.created;
    return reinterpret_cast<CRYPT_EAL_MacCtx *>(&state);
}

void CRYPT_EAL_MacFreeCtx(CRYPT_EAL_MacCtx *ctx)
{
    if (ctx != nullptr) {
        ++GetHmacState().freed;
    }
}

int32_t CRYPT_EAL_MacInit(CRYPT_EAL_MacCtx *ctx, const uint8_t *key, uint32_t len)
{
    auto &state = GetHmacState();
    ++state.initCalls;
    state.keyLength = len;
    return ctx != nullptr && key != nullptr && len == 0 && state.failure != HmacFailure::INIT ? CRYPT_SUCCESS
                                                                                              : CRYPT_NULL_INPUT;
}

int32_t CRYPT_EAL_MacUpdate(CRYPT_EAL_MacCtx *ctx, const uint8_t *data, uint32_t len)
{
    ++GetHmacState().updateCalls;
    return ctx != nullptr && (data != nullptr || len == 0) && GetHmacState().failure != HmacFailure::UPDATE
               ? CRYPT_SUCCESS
               : CRYPT_NULL_INPUT;
}

int32_t CRYPT_EAL_MacFinal(CRYPT_EAL_MacCtx *ctx, uint8_t *output, uint32_t *len)
{
    auto &state = GetHmacState();
    ++state.finalCalls;
    if (ctx == nullptr || output == nullptr || len == nullptr || *len < SHA256_SIZE) {
        return CRYPT_NULL_INPUT;
    }
    std::fill_n(output, SHA256_SIZE, 0xa5);
    if (state.failure == HmacFailure::FINAL) {
        return CRYPT_NULL_INPUT;
    }
    *len = state.failure == HmacFailure::LENGTH ? 0 : SHA256_SIZE;
    return CRYPT_SUCCESS;
}
} // extern "C"

/**
 * Copyright (c) 2022-2026 Huawei Device Co., Ltd.
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
#include "rdb_hmac.h"

#include <limits>
#include <memory>

#include "crypt_eal_mac.h"
#include "crypt_errno.h"

namespace OHOS::NativeRdb {
std::pair<std::string, std::vector<char>> GenerateHmacSha256(const std::vector<char> &data, uint32_t outputSize)
{
    if (outputSize == 0 || outputSize > HMAC_SHA256_SIZE) {
        return { "invalid hmac size=" + std::to_string(outputSize), {} };
    }
    std::vector<char> result(HMAC_SHA256_SIZE, 0);
    uint32_t length = result.size();
    std::unique_ptr<CRYPT_EAL_MacCtx, decltype(&CRYPT_EAL_MacFreeCtx)> ctx(
        CRYPT_EAL_MacNewCtx(CRYPT_MAC_HMAC_SHA256), CRYPT_EAL_MacFreeCtx);
    const uint8_t emptyKey = 0;
    auto ret = ctx == nullptr ? CRYPT_MEM_ALLOC_FAIL : CRYPT_EAL_MacInit(ctx.get(), &emptyKey, 0);
    if (ret == CRYPT_SUCCESS) {
        ret = data.size() > std::numeric_limits<uint32_t>::max()
                  ? CRYPT_INVALID_ARG
                  : CRYPT_EAL_MacUpdate(
                      ctx.get(), reinterpret_cast<const uint8_t *>(data.data()), static_cast<uint32_t>(data.size()));
    }
    if (ret == CRYPT_SUCCESS) {
        ret = CRYPT_EAL_MacFinal(ctx.get(), reinterpret_cast<uint8_t *>(result.data()), &length);
    }
    if (ret != CRYPT_SUCCESS || length != HMAC_SHA256_SIZE) {
        result.assign(outputSize, 0);
        return { "hmac generate failed, ret=" + std::to_string(ret) + ", len=" + std::to_string(length),
            std::move(result) };
    }
    result.resize(outputSize);
    return { "", std::move(result) };
}
} // namespace OHOS::NativeRdb

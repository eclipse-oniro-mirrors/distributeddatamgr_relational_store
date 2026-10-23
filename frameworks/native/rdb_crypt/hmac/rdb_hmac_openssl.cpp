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

#include <cstddef>
#include <openssl/hmac.h>

namespace OHOS::NativeRdb {
std::pair<std::string, std::vector<char>> GenerateHmacSha256(const std::vector<char> &data, uint32_t outputSize)
{
    if (outputSize == 0 || outputSize > HMAC_SHA256_SIZE) {
        return { "invalid hmac size=" + std::to_string(outputSize), {} };
    }
    std::vector<char> result(HMAC_SHA256_SIZE, 0);
    unsigned int hmacLen = 0;
    std::string key = "";
    HMAC(EVP_sha256(), key.c_str(), static_cast<int>(key.length()),
        reinterpret_cast<const unsigned char *>(data.data()), static_cast<int>(data.size()),
        reinterpret_cast<uint8_t *>(result.data()), &hmacLen);
    if (hmacLen == 0) {
        result.assign(outputSize, 0);
        return { "hmac generate failed" + std::to_string(hmacLen), std::move(result) };
    }
    result.resize(outputSize);
    return { "", std::move(result) };
}
} // namespace OHOS::NativeRdb

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
#ifndef RDB_HMAC_H
#define RDB_HMAC_H

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace OHOS::NativeRdb {
constexpr uint32_t HMAC_SHA256_SIZE = 32;

// Return outputSize bytes of an empty-key HMAC-SHA256 with an empty error on success,
// or an error and outputSize zero bytes on calculation failure.
// outputSize must be in [1, HMAC_SHA256_SIZE]; otherwise return an error and an empty vector.
std::pair<std::string, std::vector<char>> GenerateHmacSha256(const std::vector<char> &data, uint32_t outputSize);
} // namespace OHOS::NativeRdb
#endif // RDB_HMAC_H

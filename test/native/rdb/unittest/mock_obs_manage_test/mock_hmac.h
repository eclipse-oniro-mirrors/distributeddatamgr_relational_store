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
#ifndef MOCK_RDB_HMAC_H
#define MOCK_RDB_HMAC_H

#include <cstdint>

namespace Test {
enum class HmacFailure { NONE, ALLOCATE, INIT, UPDATE, FINAL, LENGTH };
struct HmacState {
    HmacFailure failure = HmacFailure::NONE;
    uint32_t created = 0;
    uint32_t freed = 0;
    uint32_t keyLength = 1;
};
HmacState &GetHmacState();
} // namespace Test
#endif // MOCK_RDB_HMAC_H

/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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
#ifndef RELATIONAL_STORE_CRYPT_H
#define RELATIONAL_STORE_CRYPT_H

#include <cstdint>
#include <string>
#include <vector>

#include "rdb_security_manager.h"

namespace OHOS {
namespace NativeRdb {
struct RDBCryptoFault {
    int32_t code;
    std::string message;
};
struct RDBCryptoParam {
    std::vector<uint8_t> rootAlias;
    std::vector<uint8_t> nonceValue;
    std::vector<uint8_t> KeyValue;
    RDBCryptoParam() = default;
    ~RDBCryptoParam()
    {
        nonceValue.assign(nonceValue.size(), 0);
        KeyValue.assign(KeyValue.size(), 0);
    }
};
} // namespace OHOS::NativeRdb
} // OHOS

#endif // RELATIONAL_STORE_CRYPT_H
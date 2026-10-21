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
#define LOG_TAG "RdbSecurityManager"
#include <cstddef>
#include <openssl/hmac.h>

#include <string>

#include "logger.h"
#include "rdb_errno.h"
#include "rdb_fault_hiview_reporter.h"
#include "rdb_security_manager.h"

namespace OHOS::NativeRdb {
using OHOS::Rdb::LogLabel;
using Reportor = RdbFaultHiViewReporter;

std::vector<char> RdbSecurityManager::GenerateHMAC(std::vector<char> &data)
{
    unsigned char hmacResult[EVP_MAX_MD_SIZE] = { 0 };
    unsigned int hmacLen = 0;
    std::string key = "";
    std::vector<char> result;
    result.resize(HMAC_SIZE, 0);

    HMAC(EVP_sha256(), key.c_str(), static_cast<int>(key.length()),
        reinterpret_cast<const unsigned char *>(data.data()), static_cast<int>(data.size()), hmacResult, &hmacLen);
    if (hmacLen == 0) {
        LOG_ERROR("hmac generate failed");
        Reportor::ReportFault(RdbFaultEvent(RdbFaultType::FT_HMAC_FILE, E_DFX_HMAC_KEY_FAIL, GetBundleName(),
            "hmac generate failed" + std::to_string(hmacLen)));
        return result;
    }
    for (unsigned int i = 0; i < HMAC_SIZE && i < hmacLen; ++i) {
        result[i] = static_cast<char>(hmacResult[i]);
    }
    return result;
}
} // namespace OHOS::NativeRdb

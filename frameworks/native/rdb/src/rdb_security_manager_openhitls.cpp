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
#include <algorithm>
#include <array>
#include <limits>
#include <memory>

#include "crypt_eal_mac.h"
#include "crypt_errno.h"
#include "logger.h"
#include "rdb_errno.h"
#include "rdb_fault_hiview_reporter.h"
#include "rdb_security_manager.h"

namespace OHOS::NativeRdb {
using OHOS::Rdb::LogLabel;
using Reportor = RdbFaultHiViewReporter;

std::vector<char> RdbSecurityManager::GenerateHMAC(std::vector<char> &data)
{
    constexpr uint32_t SHA256_SIZE = 32;
    std::vector<char> result(HMAC_SIZE, 0);
    std::array<uint8_t, SHA256_SIZE> digest{};
    uint32_t length = digest.size();
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
        ret = CRYPT_EAL_MacFinal(ctx.get(), digest.data(), &length);
    }
    if (ret != E_OK || length != digest.size()) {
        LOG_ERROR("GenerateHMAC failed, ret:%{public}d, len:%{public}u", ret, length);
        Reportor::ReportFault(RdbFaultEvent(RdbFaultType::FT_HMAC_FILE, E_DFX_HMAC_KEY_FAIL, GetBundleName(),
            "hmac generate failed " + std::to_string(ret)));
        return result;
    }
    std::copy_n(digest.begin(), HMAC_SIZE, result.begin());
    return result;
}
} // namespace OHOS::NativeRdb

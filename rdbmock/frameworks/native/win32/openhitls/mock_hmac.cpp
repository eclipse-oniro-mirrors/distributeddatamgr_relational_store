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
#include "crypt_eal_mac.h"
#include "crypt_errno.h"

extern "C" {
// Preserve the unavailable-MAC behavior of the former Windows/macOS OpenSSL mock.
CRYPT_EAL_MacCtx *CRYPT_EAL_MacNewCtx(CRYPT_MAC_AlgId)
{
    return nullptr;
}

void CRYPT_EAL_MacFreeCtx(CRYPT_EAL_MacCtx *)
{
}

int32_t CRYPT_EAL_MacInit(CRYPT_EAL_MacCtx *, const uint8_t *, uint32_t)
{
    return CRYPT_NOT_SUPPORT;
}

int32_t CRYPT_EAL_MacUpdate(CRYPT_EAL_MacCtx *, const uint8_t *, uint32_t)
{
    return CRYPT_NOT_SUPPORT;
}

int32_t CRYPT_EAL_MacFinal(CRYPT_EAL_MacCtx *, uint8_t *, uint32_t *)
{
    return CRYPT_NOT_SUPPORT;
}
} // extern "C"

/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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

#ifndef OHOS_COMMONTYPE_ERRNO_H
#define OHOS_COMMONTYPE_ERRNO_H

namespace OHOS {
namespace CommonType {

/**
* @brief The error code in the correct case.
*/
constexpr int E_OK = 0;

/**
* @brief The base code of the exception error code.
*/
constexpr int E_BASE = 14800000;

/**
* @brief The error code for the object type is invalid.
*/
constexpr int E_INVALID_OBJECT_TYPE = (E_BASE + 20);
} // namespace CommonType
} // namespace OHOS

#endif

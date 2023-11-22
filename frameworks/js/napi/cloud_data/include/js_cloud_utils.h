/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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
#ifndef CLOUD_DATA_JS_CLOUD_SHARE_H
#define CLOUD_DATA_JS_CLOUD_SHARE_H

#include "js_utils.h"
#include "cloud_types.h"
#include "common_types.h"
#include "rdb_predicates.h"
#include "result_set.h"

namespace OHOS::AppDataMgrJsKit {
namespace JSUtils {
using Participant = OHOS::CloudData::Participant;
using Privilege = OHOS::CloudData::Privilege;
using RdbPredicates = OHOS::NativeRdb::RdbPredicates;
using ResultSet = OHOS::NativeRdb::ResultSet;

template<>
int32_t Convert2Value(napi_env env, napi_value input, Participant &output);

template<>
int32_t Convert2Value(napi_env env, napi_value input, Privilege &output);

template<>
int32_t Convert2Value(napi_env env, napi_value input, std::shared_ptr<RdbPredicates> &output);

template<>
napi_value Convert2JSValue(napi_env env, const Participant &value);

template<>
napi_value Convert2JSValue(napi_env env, const Privilege &value);

template<>
napi_value Convert2JSValue(napi_env env, const CommonTypes::TsString &value);

template<>
napi_value Convert2JSValue(napi_env env, const std::shared_ptr<ResultSet> &value);
}; // namespace JSUtils
} // namespace OHOS::AppDataMgrJsKit
#endif // CLOUD_DATA_JS_CLOUD_SHARE_H

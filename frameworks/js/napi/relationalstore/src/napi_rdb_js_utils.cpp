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

#include "napi_rdb_js_utils.h"
#include "result_set.h"
#include "js_logger.h"
namespace OHOS::AppDataMgrJsKit {
namespace JSUtils {
using namespace NativeRdb;
template<>
int32_t Convert2Value(napi_env env, napi_value jsValue, Asset &output)
{
    napi_valuetype type;
    napi_status status = napi_typeof(env, jsValue, &type);
    if (status != napi_ok || type != napi_object) {
        LOG_DEBUG("napi_typeof failed status = %{public}d type = %{public}d", status, type);
        return napi_invalid_arg;
    }

    NAPI_CALL_BASE(env, Convert2ValueExt(env, GetNamedProperty(env, jsValue, "version"), output.version),
        napi_invalid_arg);
    NAPI_CALL_BASE(env, GetProperty(env, jsValue, output, name), napi_invalid_arg);
    NAPI_CALL_BASE(env, GetProperty(env, jsValue, output, uri), napi_invalid_arg);
    NAPI_CALL_BASE(env, GetProperty(env, jsValue, output, createTime), napi_invalid_arg);
    NAPI_CALL_BASE(env, GetProperty(env, jsValue, output, modifyTime), napi_invalid_arg);
    NAPI_CALL_BASE(env, GetProperty(env, jsValue, output, size), napi_invalid_arg);
    NAPI_CALL_BASE(env, GetProperty(env, jsValue, output, hash), napi_invalid_arg);
    return napi_ok;
}

template<>
napi_value Convert2JSValue(napi_env env, const Asset &value)
{
    napi_value object;
    NAPI_CALL_BASE(env, napi_create_object(env, &object), object);
    NAPI_CALL_BASE(env, AddJsProperty(env, object, value, version), object);
    NAPI_CALL_BASE(env, AddJsProperty(env, object, value, name), object);
    NAPI_CALL_BASE(env, AddJsProperty(env, object, value, uri), object);
    NAPI_CALL_BASE(env, AddJsProperty(env, object, value, createTime), object);
    NAPI_CALL_BASE(env, AddJsProperty(env, object, value, modifyTime), object);
    NAPI_CALL_BASE(env, AddJsProperty(env, object, value, size), object);
    NAPI_CALL_BASE(env, AddJsProperty(env, object, value, hash), object);
    return object;
}

template<>
napi_value Convert2JSValue(napi_env env, const RowEntity &rowEntity)
{
    napi_value ret;
    NAPI_CALL(env, napi_create_object(env, &ret));
    auto &values = rowEntity.Get();
    for (auto const &[key, object] : values) {
        napi_value value = JSUtils::Convert2JSValue(env, object.value);
        NAPI_CALL(env, napi_set_named_property(env, ret, key.c_str(), value));
    }
    return ret;
}
}; // namespace JSUtils
} // namespace OHOS::AppDataMgrJsKit
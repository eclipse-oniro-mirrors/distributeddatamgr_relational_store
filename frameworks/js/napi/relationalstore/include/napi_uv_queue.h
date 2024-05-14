/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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

#ifndef NAPI_UV_QUEUE_H
#define NAPI_UV_QUEUE_H

#include <functional>

#include "napi/native_api.h"
#include "napi/native_common.h"
#include "napi/native_node_api.h"
#include "uv.h"

namespace OHOS::RelationalStoreJsKit {
class NapiUvQueue {
    using NapiArgsGenerator = std::function<void(napi_env env, int &argc, napi_value *argv)>;
    using NapiCallbackGetter = std::function<napi_value(napi_env env)>;

public:
    NapiUvQueue(napi_env env);

    virtual ~NapiUvQueue();

    void CallFunction(NapiCallbackGetter getter, NapiArgsGenerator genArgs = NapiArgsGenerator());

    napi_env GetEnv();

private:
    static void Work(uv_work_t* work, int uvStatus);
    struct UvEntry {
        napi_env env;
        NapiCallbackGetter callback;
        NapiArgsGenerator args;
    };
    napi_env env_ = nullptr;
    uv_loop_s *loop_ = nullptr;

    static constexpr int MAX_CALLBACK_ARG_NUM = 6;
};
} // namespace OHOS::RelationalStoreJsKit
#endif
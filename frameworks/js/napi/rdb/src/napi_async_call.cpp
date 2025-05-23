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
#define LOG_TAG "AsyncCall"
#include "napi_async_call.h"

#include "logger.h"
#include "napi_rdb_trace.h"

namespace OHOS {
namespace AppDataMgrJsKit {
using namespace OHOS::Rdb;

void BaseContext::SetAction(
    napi_env env, napi_callback_info info, InputAction input, ExecuteAction exec, OutputAction output)
{
    env_ = env;
    size_t argc = MAX_INPUT_COUNT;
    napi_value self = nullptr;
    napi_value argv[MAX_INPUT_COUNT] = { nullptr };
    NAPI_CALL_RETURN_VOID(env, napi_get_cb_info(env, info, &argc, argv, &self, nullptr));
    napi_valuetype valueType = napi_undefined;
    if (argc > 0) {
        napi_typeof(env, argv[argc - 1], &valueType);
        if (valueType == napi_function) {
            LOG_DEBUG("AsyncCall set callback");
            NAPI_CALL_RETURN_VOID(env, napi_create_reference(env, argv[argc - 1], 1, &callback_));
            argc = argc - 1;
        }
    }
    // int -->input_(env, argc, argv, self)
    int status = input(env, argc, argv, self);

    // if input return is not ok, then napi_throw_error context error
    RDB_NAPI_ASSERT_RETURN_VOID_FROMV9(env, status == OK, error, apiversion);
    output_ = std::move(output);
    exec_ = std::move(exec);
    napi_create_reference(env, self, 1, &self_);
}

void BaseContext::SetError(std::shared_ptr<Error> err)
{
    error = err;
}

BaseContext::~BaseContext()
{
    if (env_ == nullptr) {
        return;
    }
    if (work_ != nullptr) {
        napi_delete_async_work(env_, work_);
    }
    if (callback_ != nullptr) {
        napi_delete_reference(env_, callback_);
    }
    napi_delete_reference(env_, self_);
    env_ = nullptr;
}

void AsyncCall::SetBusinessError(napi_env env, napi_value *businessError, std::shared_ptr<Error> error, int apiversion)
{
    LOG_DEBUG("SetBusinessError enter");
    napi_value code = nullptr;
    napi_value msg = nullptr;
    napi_create_object(env, businessError);
    if (apiversion < APIVERSION_V9) {
        napi_create_string_utf8(env, "async error.", NAPI_AUTO_LENGTH, &msg);
        napi_set_named_property(env, *businessError, "message", msg);
        return;
    }
    // if error is not inner error
    if (error != nullptr && error->GetCode() != E_INNER_ERROR) {
        napi_create_int32(env, error->GetCode(), &code);
        napi_create_string_utf8(env, error->GetMessage().c_str(), NAPI_AUTO_LENGTH, &msg);
        napi_set_named_property(env, *businessError, "code", code);
        napi_set_named_property(env, *businessError, "message", msg);
    }
}

napi_value AsyncCall::Call(napi_env env, std::shared_ptr<BaseContext> context)
{
    napi_value promise = nullptr;
    if (context->callback_ == nullptr) {
        napi_create_promise(env, &context->defer_, &promise);
    } else {
        napi_get_undefined(env, &promise);
    }
    context->keep_ = context;
    napi_value resource = nullptr;
    napi_create_string_utf8(env, "RdbAsyncCall", NAPI_AUTO_LENGTH, &resource);
    // create async work, execute function is OnExecute, complete function is OnComplete
    napi_create_async_work(env, nullptr, resource, AsyncCall::OnExecute, AsyncCall::OnComplete,
        reinterpret_cast<void *>(context.get()), &context->work_);
    // add async work to execute queue
    auto status = napi_queue_async_work_with_qos(env, context->work_, napi_qos_user_initiated);
    if (status != napi_ok) {
        napi_get_undefined(env, &promise);
    }
    return promise;
}

void AsyncCall::OnExecute(napi_env env, void *data)
{
    DISTRIBUTED_DATA_HITRACE(std::string(__FUNCTION__));
    BaseContext *context = reinterpret_cast<BaseContext *>(data);
    if (context && context->exec_) {
        context->execStatus = context->exec_();
        context->exec_ = nullptr;
    }
}

void AsyncCall::OnComplete(napi_env env, napi_status status, void *data)
{
    DISTRIBUTED_DATA_HITRACE(std::string(__FUNCTION__));
    BaseContext *context = reinterpret_cast<BaseContext *>(data);
    napi_value output = nullptr;
    int outStatus = ERR;
    // if async execute status is not napi_ok then un-execute out function
    if (context == nullptr) {
        return;
    }
    if ((context->execStatus == OK) && context->output_) {
        outStatus = context->output_(env, output);
    }
    context->output_ = nullptr;
    napi_value result[ARG_BUTT] = { 0 };
    // if out function status is ok then async renturn output data, else return error.
    if (outStatus == OK) {
        napi_get_undefined(env, &result[ARG_ERROR]);
        if (output != nullptr) {
            result[ARG_DATA] = output;
        } else {
            napi_get_undefined(env, &result[ARG_DATA]);
        }
    } else {
        napi_value businessError = nullptr;
        SetBusinessError(env, &businessError, context->error, context->apiversion);
        result[ARG_ERROR] = businessError;
        napi_get_undefined(env, &result[ARG_DATA]);
    }
    if (context->defer_ != nullptr) {
        // promise
        if (status == napi_ok && outStatus == OK) {
            napi_resolve_deferred(env, context->defer_, result[ARG_DATA]);
        } else {
            napi_reject_deferred(env, context->defer_, result[ARG_ERROR]);
        }
    } else {
        // callback
        napi_value callback = nullptr;
        napi_get_reference_value(env, context->callback_, &callback);
        napi_value returnValue = nullptr;
        napi_call_function(env, nullptr, callback, ARG_BUTT, result, &returnValue);
    }
    context->keep_.reset();
}
} // namespace AppDataMgrJsKit
} // namespace OHOS

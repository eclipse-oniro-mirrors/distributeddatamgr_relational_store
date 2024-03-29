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
#define LOG_TAG "NapiQueue"
#include "napi_queue.h"

#include "logger.h"

namespace OHOS::CloudData {
using namespace OHOS::Rdb;

ContextBase::~ContextBase()
{
    LOG_DEBUG("no memory leak after callback or promise[resolved/rejected]");
    if (env != nullptr) {
        if (callbackRef != nullptr) {
            auto status = napi_delete_reference(env, callbackRef);
            LOG_DEBUG("status:%{public}d", status);
        }
        if (selfRef != nullptr) {
            auto status = napi_delete_reference(env, selfRef);
            LOG_DEBUG("status:%{public}d", status);
        }
        env = nullptr;
    }
}

void ContextBase::GetCbInfo(napi_env envi, napi_callback_info info, NapiCbInfoParser parse, bool sync)
{
    env = envi;
    size_t argc = ARGC_MAX;
    napi_value argv[ARGC_MAX] = { nullptr };
    status = napi_get_cb_info(env, info, &argc, argv, &self, nullptr);
    ASSERT_STATUS(this, "napi_get_cb_info failed!");
    ASSERT_ARGS(this, argc <= ARGC_MAX, "too many arguments!");
    ASSERT_ARGS(this, self != nullptr, "no JavaScript this argument!");
    if (!sync) {
        napi_create_reference(env, self, 1, &selfRef);
    }
    status = napi_unwrap(env, self, &native);
    ASSERT_STATUS(this, "self unwrap failed!");

    if (!sync && (argc > 0)) {
        // get the last arguments :: <callback>
        size_t index = argc - 1;
        napi_valuetype type = napi_undefined;
        napi_status tyst = napi_typeof(env, argv[index], &type);
        if ((tyst == napi_ok) && (type == napi_function)) {
            status = napi_create_reference(env, argv[index], 1, &callbackRef);
            ASSERT_STATUS(this, "ref callback failed!");
            argc = index;
            LOG_DEBUG("async callback, no promise");
        } else {
            LOG_DEBUG("no callback, async pormose");
        }
    }

    if (parse) {
        parse(argc, argv);
    } else {
        ASSERT_ARGS(this, argc == 0, "required no arguments!");
    }
}

napi_value NapiQueue::AsyncWork(napi_env env, std::shared_ptr<ContextBase> ctxt, const std::string &name,
    NapiAsyncExecute execute, NapiAsyncComplete complete)
{
    LOG_DEBUG("name=%{public}s", name.c_str());
    AsyncContext *aCtx = new (std::nothrow) AsyncContext;
    if (aCtx == nullptr) {
        return nullptr;
    }
    aCtx->env = env;
    aCtx->ctx = std::move(ctxt);
    aCtx->execute = std::move(execute);
    aCtx->complete = std::move(complete);
    napi_value promise = nullptr;
    if (aCtx->ctx->callbackRef == nullptr) {
        napi_create_promise(env, &aCtx->deferred, &promise);
        LOG_DEBUG("create deferred promise");
    } else {
        napi_get_undefined(env, &promise);
    }

    napi_value resource = nullptr;
    napi_create_string_utf8(env, name.c_str(), NAPI_AUTO_LENGTH, &resource);
    napi_create_async_work(
        env, nullptr, resource,
        [](napi_env env, void *data) {
            ASSERT_VOID(data != nullptr, "napi_async_execute_callback nullptr");
            auto actx = static_cast<AsyncContext *>(data);
            ASSERT_VOID(actx->ctx != nullptr, "napi_async_execute_callback nullptr");
            LOG_DEBUG("napi_async_execute_callback ctxt->status=%{public}d", actx->ctx->status);
            if (actx->execute && actx->ctx->status == napi_ok) {
                actx->execute();
            }
        },
        [](napi_env env, napi_status status, void *data) {
            ASSERT_VOID(data != nullptr, "napi_async_complete_callback nullptr");
            auto actx = static_cast<AsyncContext *>(data);
            ASSERT_VOID(actx->ctx != nullptr, "napi_async_complete_callback nullptr");
            LOG_DEBUG("napi_async_complete_callback status=%{public}d, ctxt->status=%{public}d",
                status, actx->ctx->status);
            if ((status != napi_ok) && (actx->ctx->status == napi_ok)) {
                actx->ctx->status = status;
            }
            napi_value output = nullptr;
            if ((actx->complete) && (status == napi_ok) && (actx->ctx->status == napi_ok)) {
                actx->complete(output);
            }
            GenerateOutput(*actx, output);
            delete actx;
        },
        reinterpret_cast<void *>(aCtx), &aCtx->work);
    auto status = napi_queue_async_work_with_qos(env, aCtx->work, napi_qos_user_initiated);
    if (status != napi_ok) {
        napi_get_undefined(env, &promise);
        delete aCtx;
    }
    return promise;
}

void NapiQueue::GenerateOutput(AsyncContext &ctx, napi_value output)
{
    napi_value result[RESULT_ALL] = { nullptr };
    if (ctx.ctx->status == napi_ok) {
        napi_get_undefined(ctx.env, &result[RESULT_ERROR]);
        if (output == nullptr) {
            napi_get_undefined(ctx.env, &output);
        }
        result[RESULT_DATA] = output;
    } else {
        napi_value message = nullptr;
        napi_value errorCode = nullptr;
        if (ctx.ctx->jsCode != 0 && ctx.ctx->jsCode != -1) {
            napi_create_string_utf8(ctx.env, std::to_string(ctx.ctx->jsCode).c_str(), NAPI_AUTO_LENGTH, &errorCode);
        }
        if (ctx.ctx->jsCode == -1) {
            std::string jscode = "";
            napi_create_string_utf8(ctx.env, jscode.c_str(), NAPI_AUTO_LENGTH, &errorCode);
        }
        napi_create_string_utf8(ctx.env, ctx.ctx->error.c_str(), NAPI_AUTO_LENGTH, &message);
        napi_create_error(ctx.env, errorCode, message, &result[RESULT_ERROR]);
        napi_get_undefined(ctx.env, &result[RESULT_DATA]);
    }
    if (ctx.deferred != nullptr) {
        if (ctx.ctx->status == napi_ok) {
            LOG_DEBUG("deferred promise resolved");
            napi_resolve_deferred(ctx.env, ctx.deferred, result[RESULT_DATA]);
        } else {
            LOG_DEBUG("deferred promise rejected");
            napi_reject_deferred(ctx.env, ctx.deferred, result[RESULT_ERROR]);
        }
    } else {
        napi_value callback = nullptr;
        napi_get_reference_value(ctx.env, ctx.ctx->callbackRef, &callback);
        napi_value callbackResult = nullptr;
        LOG_DEBUG("call callback function");
        napi_call_function(ctx.env, nullptr, callback, RESULT_ALL, result, &callbackResult);
    }
}
} // namespace OHOS::CloudData

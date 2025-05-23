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
#define LOG_TAG "NapiRdbStoreHelper"
#include "napi_rdb_store_helper.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "logger.h"
#include "napi_async_call.h"
#include "napi_rdb_error.h"
#include "napi_rdb_js_utils.h"
#include "napi_rdb_store.h"
#include "napi_rdb_trace.h"
#include "rdb_errno.h"
#include "rdb_open_callback.h"
#include "rdb_store_config.h"
#include "unistd.h"

using namespace OHOS::Rdb;
using namespace OHOS::NativeRdb;
using namespace OHOS::AppDataMgrJsKit;
using namespace OHOS::AppDataMgrJsKit::JSUtils;

namespace OHOS {
namespace RelationalStoreJsKit {
using ContextParam = AppDataMgrJsKit::JSUtils::ContextParam;

class DefaultOpenCallback : public RdbOpenCallback {
public:
    int OnCreate(RdbStore &rdbStore) override
    {
        return E_OK;
    }
    int OnUpgrade(RdbStore &rdbStore, int oldVersion, int newVersion) override
    {
        return E_OK;
    }
};

napi_value GetRdbStore(napi_env env, napi_callback_info info)
{
    struct DeleteContext : public ContextBase {
        ContextParam param;
        RdbConfig config;
        std::shared_ptr<RdbStore> proxy;
    };
    auto context = std::make_shared<DeleteContext>();
    auto input = [context, info](napi_env env, size_t argc, napi_value *argv, napi_value self) {
        CHECK_RETURN_SET_E(argc == 2, std::make_shared<ParamNumError>("2 or 3"));
        int errCode = Convert2Value(env, argv[0], context->param);
        CHECK_RETURN_SET_E(OK == errCode, std::make_shared<ParamError>("Illegal context."));

        errCode = Convert2Value(env, argv[1], context->config);
        CHECK_RETURN_SET_E(OK == errCode, std::make_shared<ParamError>("Illegal StoreConfig or name."));

        CHECK_RETURN_SET_E(context->config.cryptoParam.IsValid(), std::make_shared<ParamError>("Illegal CryptoParam."));
        CHECK_RETURN_SET_E(context->config.tokenizer >= NONE_TOKENIZER && context->config.tokenizer < TOKENIZER_END,
            std::make_shared<ParamError>("Illegal tokenizer."));
        CHECK_RETURN_SET_E(RdbHelper::IsSupportedTokenizer(context->config.tokenizer),
            std::make_shared<InnerError>(NativeRdb::E_NOT_SUPPORT));
        if (!context->config.persist) {
            CHECK_RETURN_SET_E(context->config.rootDir.empty(),
                std::make_shared<InnerError>(NativeRdb::E_NOT_SUPPORT));
            return;
        }
        auto [code, err] = GetRealPath(env, argv[0], context->config, context->param);
        if (!context->config.rootDir.empty()) {
            context->config.isReadOnly = true;
        }
        CHECK_RETURN_SET_E(OK == code, err);
    };
    auto exec = [context]() -> int {
        int errCode = OK;
        DefaultOpenCallback callback;
        context->proxy =
            RdbHelper::GetRdbStore(GetRdbStoreConfig(context->config, context->param), -1, callback, errCode);
        // If the API version is less than 14, throw E_INVALID_ARGS.
        if (errCode == E_INVALID_SECRET_KEY && JSUtils::GetHapVersion() < 14) {
            errCode = E_INVALID_ARGS;
        }
        return errCode;
    };
    auto output = [context](napi_env env, napi_value &result) {
        result = RdbStoreProxy::NewInstance(env, context->proxy, context->param.isSystemApp);
        CHECK_RETURN_SET_E(result != nullptr, std::make_shared<InnerError>(E_ERROR));
    };
    context->SetAction(env, info, input, exec, output);

    CHECK_RETURN_NULL(context->error == nullptr || context->error->GetCode() == OK);
    return ASYNC_CALL(env, context);
}

napi_value DeleteRdbStore(napi_env env, napi_callback_info info)
{
    struct DeleteContext : public ContextBase {
        ContextParam param;
        RdbConfig config;
        bool onlyPath = false;
    };
    auto context = std::make_shared<DeleteContext>();
    auto input = [context](napi_env env, size_t argc, napi_value *argv, napi_value self) {
        CHECK_RETURN_SET_E(argc == 2, std::make_shared<ParamNumError>("2 or 3"));
        int errCode = Convert2Value(env, argv[0], context->param);
        CHECK_RETURN_SET_E(OK == errCode, std::make_shared<ParamError>("Illegal context."));

        if (IsNapiString(env, argv[1])) {
            context->onlyPath = true;
            errCode = Convert2Value(env, argv[1], context->config.name);
            CHECK_RETURN_SET_E(OK == errCode, std::make_shared<ParamError>("Illegal path."));
        } else {
            errCode = Convert2Value(env, argv[1], context->config);
            CHECK_RETURN_SET_E(OK == errCode, std::make_shared<ParamError>("Illegal StoreConfig or name."));
        }

        auto [code, err] = GetRealPath(env, argv[0], context->config, context->param);
        if (!context->config.rootDir.empty()) {
            context->config.isReadOnly = true;
        }
        CHECK_RETURN_SET_E(OK == code, err);
    };
    auto exec = [context]() -> int {
        RdbStoreConfig storeConfig = GetRdbStoreConfig(context->config, context->param);
        if (context->onlyPath) {
            storeConfig.SetDBType(DB_SQLITE);
            int errCodeSqlite = RdbHelper::DeleteRdbStore(storeConfig, false);
            storeConfig.SetDBType(DB_VECTOR);
            int errCodeVector = RdbHelper::DeleteRdbStore(storeConfig, false);
            return (errCodeSqlite == E_OK && errCodeVector == E_OK) ? E_OK : E_REMOVE_FILE;
        }
        return RdbHelper::DeleteRdbStore(storeConfig, false);
    };
    auto output = [context](napi_env env, napi_value &result) {
        napi_status status = napi_create_int64(env, OK, &result);
        CHECK_RETURN_SET_E(status == napi_ok, std::make_shared<InnerError>(E_ERROR));
    };
    context->SetAction(env, info, input, exec, output);

    CHECK_RETURN_NULL(context->error == nullptr || context->error->GetCode() == OK);
    return ASYNC_CALL(env, context);
}

napi_value IsVectorSupported(napi_env env, napi_callback_info info)
{
    bool result = RdbHelper::IsSupportArkDataDb();
    return JSUtils::Convert2JSValue(env, result);
}

napi_value IsTokenizerSupported(napi_env env, napi_callback_info info)
{
    size_t argc = 1;
    napi_value argv[1]{};
    napi_value self = nullptr;
    int32_t status = napi_get_cb_info(env, info, &argc, argv, &self, nullptr);
    RDB_NAPI_ASSERT(env, argc == 1, std::make_shared<ParamNumError>("1"));
    int32_t tokenizer = static_cast<int32_t>(Tokenizer::NONE_TOKENIZER);
    status = Convert2ValueExt(env, argv[0], tokenizer);
    RDB_NAPI_ASSERT(env, status == napi_ok && tokenizer >= NONE_TOKENIZER && tokenizer < TOKENIZER_END,
        std::make_shared<ParamError>("tokenizer", "a TOKENIZER."));
    bool result = RdbHelper::IsSupportedTokenizer(static_cast<Tokenizer>(tokenizer));
    return JSUtils::Convert2JSValue(env, result);
}

napi_value InitRdbHelper(napi_env env, napi_value exports)
{
    napi_property_descriptor properties[] = {
        DECLARE_NAPI_FUNCTION_WITH_DATA("getRdbStore", GetRdbStore, ASYNC),
        DECLARE_NAPI_FUNCTION_WITH_DATA("getRdbStoreSync", GetRdbStore, SYNC),
        DECLARE_NAPI_FUNCTION_WITH_DATA("deleteRdbStore", DeleteRdbStore, ASYNC),
        DECLARE_NAPI_FUNCTION_WITH_DATA("deleteRdbStoreSync", DeleteRdbStore, SYNC),
        DECLARE_NAPI_FUNCTION_WITH_DATA("isVectorSupported", IsVectorSupported, SYNC),
        DECLARE_NAPI_FUNCTION_WITH_DATA("isTokenizerSupported", IsTokenizerSupported, SYNC),
    };
    NAPI_CALL(env, napi_define_properties(env, exports, sizeof(properties) / sizeof(*properties), properties));
    return exports;
}
} // namespace RelationalStoreJsKit
} // namespace OHOS

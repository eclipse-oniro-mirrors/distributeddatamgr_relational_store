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

#include "napi_rdb_store.h"

#include <cinttypes>
#include <string>
#include <vector>
#include <algorithm>

#include "js_utils.h"
#include "logger.h"
#include "napi_async_call.h"
#include "napi_rdb_error.h"
#include "napi_rdb_predicates.h"
#include "napi_rdb_trace.h"
#include "napi_result_set.h"
#include "rdb_errno.h"
#include "securec.h"

using namespace OHOS::Rdb;
using namespace OHOS::NativeRdb;
using namespace OHOS::AppDataMgrJsKit;

namespace OHOS {
namespace RelationalStoreJsKit {
struct RdbStoreContext : public Context {
    std::string device;
    std::string tableName;
    std::vector<std::string> tablesNames;
    std::string whereClause;
    std::string sql;
    RdbPredicatesProxy *predicatesProxy;
    std::vector<std::string> columns;
    ValuesBucket valuesBucket;
    std::vector<ValuesBucket> valuesBuckets;
    std::map<std::string, ValueObject> numberMaps;
    std::vector<ValueObject> bindArgs;
    int64_t int64Output;
    int intOutput;
    std::vector<uint8_t> newKey;
    std::shared_ptr<ResultSet> resultSet;
    std::string aliasName;
    std::string pathName;
    std::string srcName;
    std::string columnName;
    int32_t enumArg;
    int32_t distributedType;
    int32_t syncMode;
    uint64_t cursor = UINT64_MAX;
    DistributedRdb::DistributedConfig distributedConfig;
    napi_ref asyncHolder = nullptr;
    NativeRdb::ConflictResolution conflictResolution;
    DistributedRdb::SyncResult syncResult;
    std::shared_ptr<RdbPredicates> rdbPredicates = nullptr;
    std::vector<NativeRdb::RdbStore::PRIKey> keys;
    std::map<RdbStore::PRIKey, RdbStore::Date> modifyTime;

    RdbStoreContext()
        : predicatesProxy(nullptr), int64Output(0), intOutput(0), enumArg(-1),
          distributedType(DistributedRdb::DistributedTableType::DISTRIBUTED_DEVICE),
          syncMode(DistributedRdb::SyncMode::PUSH),
          conflictResolution(ConflictResolution::ON_CONFLICT_NONE)
    {
    }
    virtual ~RdbStoreContext()
    {
    }
};

static __thread napi_ref constructor_ = nullptr;

RdbStoreProxy::RdbStoreProxy()
{
}

RdbStoreProxy::~RdbStoreProxy()
{
    LOG_DEBUG("RdbStoreProxy destructor");
}

bool RdbStoreProxy::IsSystemAppCalled()
{
    return isSystemAppCalled_;
}

bool IsNapiTypeString(napi_env env, size_t argc, napi_value *argv, size_t arg)
{
    if (arg >= argc) {
        return false;
    }
    napi_valuetype type = napi_undefined;
    NAPI_CALL_BASE(env, napi_typeof(env, argv[arg], &type), false);
    return type == napi_string;
}

void RdbStoreProxy::Init(napi_env env, napi_value exports)
{
    napi_property_descriptor descriptors[] = {
        DECLARE_NAPI_FUNCTION_WITH_DATA("delete", Delete, ASYNC),
        DECLARE_NAPI_FUNCTION_WITH_DATA("deleteSync", Delete, SYNC),
        DECLARE_NAPI_FUNCTION_WITH_DATA("update", Update, ASYNC),
        DECLARE_NAPI_FUNCTION_WITH_DATA("updateSync", Update, SYNC),
        DECLARE_NAPI_FUNCTION_WITH_DATA("insert", Insert, ASYNC),
        DECLARE_NAPI_FUNCTION_WITH_DATA("insertSync", Insert, SYNC),
        DECLARE_NAPI_FUNCTION_WITH_DATA("batchInsert", BatchInsert, ASYNC),
        DECLARE_NAPI_FUNCTION_WITH_DATA("batchInsertSync", BatchInsert, SYNC),
        DECLARE_NAPI_FUNCTION_WITH_DATA("querySql", QuerySql, ASYNC),
        DECLARE_NAPI_FUNCTION_WITH_DATA("querySqlSync", QuerySql, SYNC),
        DECLARE_NAPI_FUNCTION_WITH_DATA("query", Query, ASYNC),
        DECLARE_NAPI_FUNCTION_WITH_DATA("querySync", Query, SYNC),
        DECLARE_NAPI_FUNCTION_WITH_DATA("executeSql", ExecuteSql, ASYNC),
        DECLARE_NAPI_FUNCTION_WITH_DATA("executeSqlSync", ExecuteSql, SYNC),
        DECLARE_NAPI_FUNCTION_WITH_DATA("replace", Replace, ASYNC),
        DECLARE_NAPI_FUNCTION_WITH_DATA("replaceSync", Replace, SYNC),
        DECLARE_NAPI_FUNCTION_WITH_DATA("queryByStep", QueryByStep, ASYNC),
        DECLARE_NAPI_FUNCTION_WITH_DATA("queryByStepSync", QueryByStep, SYNC),
        DECLARE_NAPI_FUNCTION("backup", Backup),
        DECLARE_NAPI_FUNCTION("count", Count),
        DECLARE_NAPI_FUNCTION("addAttach", Attach),
        DECLARE_NAPI_FUNCTION("beginTransaction", BeginTransaction),
        DECLARE_NAPI_FUNCTION("rollBack", RollBack),
        DECLARE_NAPI_FUNCTION("commit", Commit),
        DECLARE_NAPI_FUNCTION("restore", Restore),
        DECLARE_NAPI_GETTER_SETTER("version", GetVersion, SetVersion),
        DECLARE_NAPI_GETTER("isInTransaction", IsInTransaction),
        DECLARE_NAPI_GETTER("isOpen", IsOpen),
        DECLARE_NAPI_GETTER("path", GetPath),
        DECLARE_NAPI_GETTER("isReadOnly", IsReadOnly),
        DECLARE_NAPI_GETTER("isMemoryRdb", IsMemoryRdb),
    };
    napi_value cons = nullptr;
    NAPI_CALL_RETURN_VOID(env, napi_define_class(env, "RdbStore", NAPI_AUTO_LENGTH, Initialize, nullptr,
                                   sizeof(descriptors) / sizeof(napi_property_descriptor), descriptors, &cons));
    NAPI_CALL_RETURN_VOID(env, napi_create_reference(env, cons, 1, &constructor_));

    LOG_DEBUG("Init RdbStoreProxy end");
}

napi_value RdbStoreProxy::Initialize(napi_env env, napi_callback_info info)
{
    napi_value self = nullptr;
    NAPI_CALL(env, napi_get_cb_info(env, info, NULL, NULL, &self, nullptr));
    auto finalize = [](napi_env env, void *data, void *hint) {
        RdbStoreProxy *proxy = reinterpret_cast<RdbStoreProxy *>(data);
        delete proxy;
    };
    auto *proxy = new (std::nothrow) RdbStoreProxy();
    if (proxy == nullptr) {
        return nullptr;
    }
    napi_status status = napi_wrap(env, self, proxy, finalize, nullptr, nullptr);
    if (status != napi_ok) {
        LOG_ERROR("RdbStoreProxy::Initialize napi_wrap failed! code:%{public}d!", status);
        finalize(env, proxy, nullptr);
        return nullptr;
    }
    return self;
}

napi_value RdbStoreProxy::NewInstance(napi_env env, std::shared_ptr<NativeRdb::RdbStore> value, bool isSystemAppCalled)
{
    if (value == nullptr) {
        LOG_ERROR("value is nullptr ? %{public}d", (value == nullptr));
        return nullptr;
    }
    napi_value cons = nullptr;
    napi_status status = napi_get_reference_value(env, constructor_, &cons);
    if (status != napi_ok) {
        LOG_ERROR("RdbStoreProxy::NewInstance get constructor failed! code:%{public}d!", status);
        return nullptr;
    }

    napi_value instance = nullptr;
    status = napi_new_instance(env, cons, 0, nullptr, &instance);
    if (status != napi_ok) {
        LOG_ERROR("RdbStoreProxy::NewInstance napi_new_instance failed! code:%{public}d!", status);
        return nullptr;
    }

    RdbStoreProxy *proxy = nullptr;
    status = napi_unwrap(env, instance, reinterpret_cast<void **>(&proxy));
    if (proxy == nullptr) {
        LOG_ERROR("RdbStoreProxy::NewInstance native instance is nullptr! code:%{public}d!", status);
        return instance;
    }
    proxy->queue_ = std::make_shared<AppDataMgrJsKit::UvQueue>(env);
    proxy->rdbStore_ = std::move(value);
    proxy->isSystemAppCalled_ = isSystemAppCalled;
    return instance;
}

RdbStoreProxy *GetNativeInstance(napi_env env, napi_value self)
{
    RdbStoreProxy *proxy = nullptr;
    napi_status status = napi_unwrap(env, self, reinterpret_cast<void **>(&proxy));
    if (proxy == nullptr) {
        LOG_ERROR("RdbStoreProxy::GetNativePredicates native instance is nullptr! code:%{public}d!", status);
        return nullptr;
    }
    return proxy;
}

int ParserThis(const napi_env &env, const napi_value &self, std::shared_ptr<RdbStoreContext> context)
{
    RdbStoreProxy *obj = GetNativeInstance(env, self);
    CHECK_RETURN_SET(obj && obj->rdbStore_, std::make_shared<ParamError>("RdbStore", "nullptr."));
    context->boundObj = obj;
    return OK;
}

int ParseTableName(const napi_env env, const napi_value arg, std::shared_ptr<RdbStoreContext> context)
{
    context->tableName = JSUtils::Convert2String(env, arg);
    CHECK_RETURN_SET(!context->tableName.empty(), std::make_shared<ParamError>("table", "not empty"));
    return OK;
}

int ParseCursor(const napi_env env, const napi_value arg, std::shared_ptr<RdbStoreContext> context)
{
    double cursor = 0;
    auto status = JSUtils::Convert2Value(env, arg, cursor);
    CHECK_RETURN_SET(status == napi_ok && cursor > 0, std::make_shared<ParamError>("cursor", "not invalid cursor"));
    context->cursor = static_cast<uint64_t>(cursor);
    return OK;
}

int ParseColumnName(const napi_env env, const napi_value arg, std::shared_ptr<RdbStoreContext> context)
{
    context->columnName = JSUtils::Convert2String(env, arg);
    CHECK_RETURN_SET(!context->columnName.empty(), std::make_shared<ParamError>("columnName", "not string"));
    return OK;
}

int ParsePrimaryKey(const napi_env env, const napi_value arg, std::shared_ptr<RdbStoreContext> context)
{
    JSUtils::Convert2Value(env, arg, context->keys);
    CHECK_RETURN_SET(!context->keys.empty(), std::make_shared<ParamError>("PRIKey", "not number or string"));
    return OK;
}

int ParseDevice(const napi_env env, const napi_value arg, std::shared_ptr<RdbStoreContext> context)
{
    context->device = JSUtils::Convert2String(env, arg);
    CHECK_RETURN_SET(!context->device.empty(), std::make_shared<ParamError>("device", "not empty"));
    return OK;
}

int ParseTablesName(const napi_env env, const napi_value arg, std::shared_ptr<RdbStoreContext> context)
{
    int32_t ret = JSUtils::Convert2Value(env, arg, context->tablesNames);
    CHECK_RETURN_SET(ret == napi_ok, std::make_shared<ParamError>("tablesNames", "not empty"));
    return OK;
}

int ParseSyncModeArg(const napi_env env, const napi_value arg, std::shared_ptr<RdbStoreContext> context)
{
    napi_valuetype type = napi_undefined;
    napi_typeof(env, arg, &type);
    CHECK_RETURN_SET(type == napi_number, std::make_shared<ParamError>("mode", "a SyncMode Type."));
    napi_status status = napi_get_value_int32(env, arg, &context->enumArg);
    CHECK_RETURN_SET(status == napi_ok, std::make_shared<ParamError>("mode", "a SyncMode Type."));
    bool checked = context->enumArg == 0 || context->enumArg == 1;
    CHECK_RETURN_SET(checked, std::make_shared<ParamError>("mode", "a SyncMode of device."));
    return OK;
}

int ParseDistributedTypeArg(const napi_env &env, size_t argc, napi_value * argv,
    std::shared_ptr<RdbStoreContext> context)
{
    context->distributedType = DistributedRdb::DISTRIBUTED_DEVICE;
    if (argc > 1) {
        auto status = JSUtils::Convert2ValueExt(env, argv[1], context->distributedType);
        bool checked = status == napi_ok && context->distributedType >= DistributedRdb::DISTRIBUTED_DEVICE
                       && context->distributedType <= DistributedRdb::DISTRIBUTED_CLOUD;
        CHECK_RETURN_SET(JSUtils::IsNull(env, argv[1]) || checked,
            std::make_shared<ParamError>("distributedType", "a DistributedType"));
    }
    return OK;
}

int ParseDistributedConfigArg(const napi_env &env, size_t argc, napi_value * argv,
    std::shared_ptr<RdbStoreContext> context)
{
    context->distributedConfig = { false };
    if (argc > 2) {
        auto status = JSUtils::Convert2Value(env, argv[2], context->distributedConfig);
        bool checked = status == napi_ok || JSUtils::IsNull(env, argv[2]);
        CHECK_RETURN_SET(checked, std::make_shared<ParamError>("distributedConfig", "a DistributedConfig type"));
    }
    return OK;
}

int ParseCloudSyncModeArg(const napi_env env, const napi_value arg, std::shared_ptr<RdbStoreContext> context)
{
    auto status = JSUtils::Convert2ValueExt(env, arg, context->syncMode);
    bool checked = (status == napi_ok && context->syncMode >= DistributedRdb::TIME_FIRST
                    && context->syncMode <= DistributedRdb::CLOUD_FIRST);
    CHECK_RETURN_SET(checked, std::make_shared<ParamError>("mode", "a SyncMode of cloud."));
    return OK;
}

int ParseCallback(const napi_env env, const napi_value arg, std::shared_ptr<RdbStoreContext> context)
{
    napi_valuetype valueType = napi_undefined;
    napi_status status = napi_typeof(env, arg, &valueType);
    CHECK_RETURN_SET((status == napi_ok && valueType == napi_function),
        std::make_shared<ParamError>("callback", "a function."));
    NAPI_CALL_BASE(env, napi_create_reference(env, arg, 1, &context->callback_), ERR);
    return OK;
}

int ParseCloudSyncCallback(const napi_env env, const napi_value arg, std::shared_ptr<RdbStoreContext> context)
{
    napi_valuetype valueType = napi_undefined;
    napi_typeof(env, arg, &valueType);
    CHECK_RETURN_SET(valueType == napi_function, std::make_shared<ParamError>("progress", "a callback type"));
    NAPI_CALL_BASE(env, napi_create_reference(env, arg, 1, &context->asyncHolder), ERR);
    return OK;
}

int ParsePredicates(const napi_env env, const napi_value arg, std::shared_ptr<RdbStoreContext> context)
{
    auto status = napi_unwrap(env, arg, reinterpret_cast<void **>(&context->predicatesProxy));
    CHECK_RETURN_SET(status == napi_ok && context->predicatesProxy != nullptr,
        std::make_shared<ParamError>("predicates", "an RdbPredicates."));
    context->tableName = context->predicatesProxy->GetPredicates()->GetTableName();
    context->rdbPredicates = context->predicatesProxy->GetPredicates();
    return OK;
}

int ParseDataSharePredicates(const napi_env env, const napi_value arg, std::shared_ptr<RdbStoreContext> context)
{
    return OK;
}

int ParseSrcName(const napi_env env, const napi_value arg, std::shared_ptr<RdbStoreContext> context)
{
    context->srcName = JSUtils::Convert2String(env, arg);
    CHECK_RETURN_SET(!context->srcName.empty(), std::make_shared<ParamError>("srcName", "not empty"));
    return OK;
}

int ParseColumns(const napi_env env, const napi_value arg, std::shared_ptr<RdbStoreContext> context)
{
    napi_valuetype type = napi_undefined;
    napi_typeof(env, arg, &type);
    if (type == napi_undefined || type == napi_null) {
        return OK;
    }
    int32_t ret = JSUtils::Convert2Value(env, arg, context->columns);
    CHECK_RETURN_SET(ret == napi_ok, std::make_shared<ParamError>("columns", "a string array"));
    return OK;
}

int ParseAlias(const napi_env env, const napi_value arg, std::shared_ptr<RdbStoreContext> context)
{
    context->aliasName = JSUtils::Convert2String(env, arg);
    CHECK_RETURN_SET(!context->aliasName.empty(), std::make_shared<ParamError>("aliasName", "not empty"));
    return OK;
}

int ParsePath(const napi_env env, const napi_value arg, std::shared_ptr<RdbStoreContext> context)
{
    context->pathName = JSUtils::Convert2String(env, arg);
    CHECK_RETURN_SET(!context->pathName.empty(), std::make_shared<ParamError>("pathName", "not empty"));
    return OK;
}

int ParseBindArgs(const napi_env env, const napi_value arg, std::shared_ptr<RdbStoreContext> context)
{
    context->bindArgs.clear();
    napi_valuetype type = napi_undefined;
    napi_typeof(env, arg, &type);
    if (type == napi_undefined || type == napi_null) {
        return OK;
    }
    bool isArray = false;
    napi_status status = napi_is_array(env, arg, &isArray);
    CHECK_RETURN_SET(status == napi_ok && isArray, std::make_shared<ParamError>("values", "a BindArgs array."));

    uint32_t arrLen = 0;
    status = napi_get_array_length(env, arg, &arrLen);
    CHECK_RETURN_SET(status == napi_ok, std::make_shared<ParamError>("values", "not empty."));
    for (size_t i = 0; i < arrLen; ++i) {
        napi_value element = nullptr;
        napi_get_element(env, arg, i, &element);
        ValueObject valueObject;
        int32_t ret = JSUtils::Convert2Value(env, element, valueObject.value);
        CHECK_RETURN_SET(ret == OK, std::make_shared<ParamError>(std::to_string(i), "ValueObject"));
        context->bindArgs.push_back(std::move(valueObject));
    }
    return OK;
}

int ParseSql(const napi_env env, const napi_value arg, std::shared_ptr<RdbStoreContext> context)
{
    context->sql = JSUtils::Convert2String(env, arg);
    CHECK_RETURN_SET(!context->sql.empty(), std::make_shared<ParamError>("sql", "not empty"));
    return OK;
}

int ParseValuesBucket(const napi_env env, const napi_value arg, std::shared_ptr<RdbStoreContext> context)
{
    napi_value keys = nullptr;
    napi_get_all_property_names(env, arg, napi_key_own_only,
        static_cast<napi_key_filter>(napi_key_enumerable | napi_key_skip_symbols),
        napi_key_numbers_to_strings, &keys);
    uint32_t arrLen = 0;
    napi_status status = napi_get_array_length(env, keys, &arrLen);
    CHECK_RETURN_SET(status == napi_ok, std::make_shared<ParamError>("values", "a ValuesBucket."));

    for (size_t i = 0; i < arrLen; ++i) {
        napi_value key = nullptr;
        status = napi_get_element(env, keys, i, &key);
        CHECK_RETURN_SET(status == napi_ok, std::make_shared<ParamError>("values", "a ValuesBucket."));
        std::string keyStr = JSUtils::Convert2String(env, key);
        napi_value value = nullptr;
        napi_get_property(env, arg, key, &value);
        ValueObject valueObject;
        int32_t ret = JSUtils::Convert2Value(env, value, valueObject.value);
        if (ret == napi_ok) {
            context->valuesBucket.Put(keyStr, valueObject);
        } else if (ret != napi_generic_failure) {
            CHECK_RETURN_SET(false, std::make_shared<ParamError>("The value type of " + keyStr, "valid."));
        }
    }
    return OK;
}

int ParseValuesBuckets(const napi_env env, const napi_value arg, std::shared_ptr<RdbStoreContext> context)
{
    bool isArray = false;
    napi_is_array(env, arg, &isArray);
    CHECK_RETURN_SET(isArray, std::make_shared<ParamError>("values", "a ValuesBucket array."));

    uint32_t arrLen = 0;
    napi_status status = napi_get_array_length(env, arg, &arrLen);
    CHECK_RETURN_SET(status == napi_ok, std::make_shared<ParamError>("values", "get array length."));

    for (uint32_t i = 0; i < arrLen; ++i) {
        napi_value obj = nullptr;
        status = napi_get_element(env, arg, i, &obj);
        CHECK_RETURN_SET(status == napi_ok, std::make_shared<ParamError>("values", "get element."));

        ParseValuesBucket(env, obj, context);
        context->valuesBuckets.push_back(context->valuesBucket);
        context->valuesBucket.Clear();
    }
    return OK;
}

int ParseConflictResolution(const napi_env env, const napi_value arg, std::shared_ptr<RdbStoreContext> context)
{
    int32_t conflictResolution = 0;
    napi_get_value_int32(env, arg, &conflictResolution);
    int min = static_cast<int32_t>(NativeRdb::ConflictResolution::ON_CONFLICT_NONE);
    int max = static_cast<int32_t>(NativeRdb::ConflictResolution::ON_CONFLICT_REPLACE);
    bool checked = (conflictResolution >= min) && (conflictResolution <= max);
    CHECK_RETURN_SET(checked, std::make_shared<ParamError>("conflictResolution", "a ConflictResolution."));
    context->conflictResolution = static_cast<NativeRdb::ConflictResolution>(conflictResolution);
    return OK;
}

napi_value RdbStoreProxy::Insert(napi_env env, napi_callback_info info)
{
    auto context = std::make_shared<RdbStoreContext>();
    auto input = [context](napi_env env, size_t argc, napi_value *argv, napi_value self) {
        CHECK_RETURN_SET_E(argc == 2 || argc == 3, std::make_shared<ParamNumError>("2 to 4"));
        CHECK_RETURN(OK == ParserThis(env, self, context));
        CHECK_RETURN(OK == ParseTableName(env, argv[0], context));
        CHECK_RETURN(OK == ParseValuesBucket(env, argv[1], context));
        if (argc == 3) {
            CHECK_RETURN(OK == ParseConflictResolution(env, argv[2], context));
        }
    };
    auto exec = [context]() -> int {
        RdbStoreProxy *obj = reinterpret_cast<RdbStoreProxy *>(context->boundObj);
        CHECK_RETURN_ERR(obj != nullptr && obj->rdbStore_ != nullptr);
        return obj->rdbStore_->InsertWithConflictResolution(context->int64Output, context->tableName,
            context->valuesBucket, context->conflictResolution);
    };
    auto output = [context](napi_env env, napi_value &result) {
        napi_status status = napi_create_int64(env, context->int64Output, &result);
        CHECK_RETURN_SET_E(status == napi_ok, std::make_shared<InnerError>(E_ERROR));
    };
    context->SetAction(env, info, input, exec, output);

    CHECK_RETURN_NULL(!(context->error) || context->error->GetCode() == OK);
    return AsyncCall::Call(env, context);
}

napi_value RdbStoreProxy::BatchInsert(napi_env env, napi_callback_info info)
{
    auto context = std::make_shared<RdbStoreContext>();
    auto input = [context](napi_env env, size_t argc, napi_value *argv, napi_value self) {
        CHECK_RETURN_SET_E(argc == 2, std::make_shared<ParamNumError>("2 or 3"));
        CHECK_RETURN(OK == ParserThis(env, self, context));
        CHECK_RETURN(OK == ParseTableName(env, argv[0], context));
        CHECK_RETURN(OK == ParseValuesBuckets(env, argv[1], context));
    };
    auto exec = [context]() -> int {
        RdbStoreProxy *obj = reinterpret_cast<RdbStoreProxy *>(context->boundObj);
        CHECK_RETURN_ERR(obj != nullptr && obj->rdbStore_ != nullptr);
        return obj->rdbStore_->BatchInsert(context->int64Output, context->tableName, context->valuesBuckets);
    };
    auto output = [context](napi_env env, napi_value &result) {
        napi_status status = napi_create_int64(env, context->int64Output, &result);
        CHECK_RETURN_SET_E(status == napi_ok, std::make_shared<InnerError>(E_ERROR));
    };
    context->SetAction(env, info, input, exec, output);

    CHECK_RETURN_NULL(context->error == nullptr || context->error->GetCode() == OK);
    return AsyncCall::Call(env, context);
}

napi_value RdbStoreProxy::Delete(napi_env env, napi_callback_info info)
{
    auto context = std::make_shared<RdbStoreContext>();
    auto input = [context](napi_env env, size_t argc, napi_value *argv, napi_value self) {
        CHECK_RETURN_SET_E(argc == 1 || argc == 2, std::make_shared<ParamNumError>("1 to 3"));
        CHECK_RETURN(OK == ParserThis(env, self, context));
        if (argc == 2) {
            CHECK_RETURN(OK == ParseTableName(env, argv[0], context));
            CHECK_RETURN(OK == ParseDataSharePredicates(env, argv[1], context));
        } else {
            CHECK_RETURN(OK == ParsePredicates(env, argv[0], context));
        }
    };
    auto exec = [context]() -> int {
        RdbStoreProxy *obj = reinterpret_cast<RdbStoreProxy *>(context->boundObj);
        CHECK_RETURN_ERR(obj != nullptr && obj->rdbStore_ != nullptr);
        CHECK_RETURN_ERR(context->rdbPredicates != nullptr);
        return obj->rdbStore_->Delete(context->intOutput, *(context->rdbPredicates));
    };
    auto output = [context](napi_env env, napi_value &result) {
        napi_status status = napi_create_int64(env, context->intOutput, &result);
        CHECK_RETURN_SET_E(status == napi_ok, std::make_shared<InnerError>(E_ERROR));
    };
    context->SetAction(env, info, input, exec, output);

    CHECK_RETURN_NULL(context->error == nullptr || context->error->GetCode() == OK);
    return AsyncCall::Call(env, context);
}

napi_value RdbStoreProxy::Update(napi_env env, napi_callback_info info)
{
    auto context = std::make_shared<RdbStoreContext>();
    auto input = [context](napi_env env, size_t argc, napi_value *argv, napi_value self) {
        CHECK_RETURN(OK == ParserThis(env, self, context));
        if (IsNapiTypeString(env, argc, argv, 0)) {
            CHECK_RETURN_SET_E(argc == 3 || argc == 4, std::make_shared<ParamNumError>("2 to 5"));
            CHECK_RETURN(OK == ParseTableName(env, argv[0], context));
            CHECK_RETURN(OK == ParseValuesBucket(env, argv[1], context));
            CHECK_RETURN(OK == ParseDataSharePredicates(env, argv[2], context));
            if (argc == 4) {
                CHECK_RETURN(OK == ParseConflictResolution(env, argv[3], context));
            }
        } else {
            CHECK_RETURN_SET_E(argc == 2 || argc == 3, std::make_shared<ParamNumError>("2 to 5"));
            CHECK_RETURN(OK == ParseValuesBucket(env, argv[0], context));
            CHECK_RETURN(OK == ParsePredicates(env, argv[1], context));
            if (argc == 3) {
                CHECK_RETURN(OK == ParseConflictResolution(env, argv[2], context));
            }
        }
    };
    auto exec = [context]() -> int {
        RdbStoreProxy *obj = reinterpret_cast<RdbStoreProxy *>(context->boundObj);
        CHECK_RETURN_ERR(obj != nullptr && obj->rdbStore_ != nullptr);
        CHECK_RETURN_ERR(context->rdbPredicates != nullptr);
        return obj->rdbStore_->UpdateWithConflictResolution(context->intOutput, context->tableName,
            context->valuesBucket, context->rdbPredicates->GetWhereClause(), context->rdbPredicates->GetBindArgs(),
            context->conflictResolution);
    };
    auto output = [context](napi_env env, napi_value &result) {
        napi_status status = napi_create_int64(env, context->intOutput, &result);
        CHECK_RETURN_SET_E(status == napi_ok, std::make_shared<InnerError>(E_ERROR));
    };
    context->SetAction(env, info, input, exec, output);

    CHECK_RETURN_NULL(context->error == nullptr || context->error->GetCode() == OK);
    return AsyncCall::Call(env, context);
}

napi_value RdbStoreProxy::Query(napi_env env, napi_callback_info info)
{
    auto context = std::make_shared<RdbStoreContext>();
    auto input = [context](napi_env env, size_t argc, napi_value *argv, napi_value self) {
        CHECK_RETURN(OK == ParserThis(env, self, context));
        if (IsNapiTypeString(env, argc, argv, 0)) {
            CHECK_RETURN_SET_E(argc == 2 || argc == 3, std::make_shared<ParamNumError>("1 to 4"));
            CHECK_RETURN(OK == ParseTableName(env, argv[0], context));
            CHECK_RETURN(OK == ParseDataSharePredicates(env, argv[1], context));
            if (argc == 3) {
                CHECK_RETURN(OK == ParseColumns(env, argv[2], context));
            }
        } else {
            CHECK_RETURN_SET_E(argc == 1 || argc == 2, std::make_shared<ParamNumError>("1 to 4"));
            CHECK_RETURN(OK == ParsePredicates(env, argv[0], context));
            if (argc == 2) {
                CHECK_RETURN(OK == ParseColumns(env, argv[1], context));
            }
        }
    };
    auto exec = [context]() -> int {
        RdbStoreProxy *obj = reinterpret_cast<RdbStoreProxy *>(context->boundObj);
        CHECK_RETURN_ERR(obj != nullptr && obj->rdbStore_ != nullptr);
        CHECK_RETURN_ERR(context->rdbPredicates != nullptr);
        context->resultSet = obj->rdbStore_->Query(*(context->rdbPredicates), context->columns);
        return (context->resultSet != nullptr) ? E_OK : E_ERROR;
    };
    auto output = [context](napi_env env, napi_value &result) {
        result = ResultSetProxy::NewInstance(env, context->resultSet);
        CHECK_RETURN_SET_E(result != nullptr, std::make_shared<InnerError>(E_ERROR));
    };
    context->SetAction(env, info, input, exec, output);

    CHECK_RETURN_NULL(context->error == nullptr || context->error->GetCode() == OK);
    return AsyncCall::Call(env, context);
}

napi_value RdbStoreProxy::QuerySql(napi_env env, napi_callback_info info)
{
    DISTRIBUTED_DATA_HITRACE(std::string(__FUNCTION__));
    auto context = std::make_shared<RdbStoreContext>();
    auto input = [context](napi_env env, size_t argc, napi_value *argv, napi_value self) {
        CHECK_RETURN_SET_E(argc == 1 || argc == 2, std::make_shared<ParamNumError>("1 to 3"));
        CHECK_RETURN(OK == ParserThis(env, self, context));
        CHECK_RETURN(OK == ParseSql(env, argv[0], context));
        if (argc == 2) {
            CHECK_RETURN(OK == ParseBindArgs(env, argv[1], context));
        }
    };
    auto exec = [context]() -> int {
        RdbStoreProxy *obj = reinterpret_cast<RdbStoreProxy *>(context->boundObj);
        context->resultSet = obj->rdbStore_->QueryByStep(context->sql, context->bindArgs);
        return (context->resultSet != nullptr) ? E_OK : E_ERROR;
    };
    auto output = [context](napi_env env, napi_value &result) {
        result = ResultSetProxy::NewInstance(env, context->resultSet);
        CHECK_RETURN_SET_E(result != nullptr, std::make_shared<InnerError>(E_ERROR));
    };
    context->SetAction(env, info, input, exec, output);

    CHECK_RETURN_NULL(context->error == nullptr || context->error->GetCode() == OK);
    return AsyncCall::Call(env, context);
}

napi_value RdbStoreProxy::ExecuteSql(napi_env env, napi_callback_info info)
{
    auto context = std::make_shared<RdbStoreContext>();
    auto input = [context](napi_env env, size_t argc, napi_value *argv, napi_value self) {
        CHECK_RETURN_SET_E(argc == 1 || argc == 2, std::make_shared<ParamNumError>("1 to 3"));
        CHECK_RETURN(OK == ParserThis(env, self, context));
        CHECK_RETURN(OK == ParseSql(env, argv[0], context));
        if (argc == 2) {
            CHECK_RETURN(OK == ParseBindArgs(env, argv[1], context));
        }
    };
    auto exec = [context]() -> int {
        RdbStoreProxy *obj = reinterpret_cast<RdbStoreProxy *>(context->boundObj);
        CHECK_RETURN_ERR(obj != nullptr && obj->rdbStore_ != nullptr);
        return obj->rdbStore_->ExecuteSql(context->sql, context->bindArgs);
    };
    auto output = [context](napi_env env, napi_value &result) {
        napi_status status = napi_get_undefined(env, &result);
        CHECK_RETURN_SET_E(status == napi_ok, std::make_shared<InnerError>(E_ERROR));
    };
    context->SetAction(env, info, input, exec, output);

    CHECK_RETURN_NULL(context->error == nullptr || context->error->GetCode() == OK);
    return AsyncCall::Call(env, context);
}

napi_value RdbStoreProxy::Count(napi_env env, napi_callback_info info)
{
    auto context = std::make_shared<RdbStoreContext>();
    auto input = [context](napi_env env, size_t argc, napi_value *argv, napi_value self) {
        CHECK_RETURN_SET_E(argc == 1, std::make_shared<ParamNumError>("1 or 2"));
        CHECK_RETURN(OK == ParserThis(env, self, context));
        CHECK_RETURN(OK == ParsePredicates(env, argv[0], context));
    };
    auto exec = [context]() -> int {
        RdbStoreProxy *obj = reinterpret_cast<RdbStoreProxy *>(context->boundObj);
        CHECK_RETURN_ERR(obj != nullptr && obj->rdbStore_ != nullptr);
        CHECK_RETURN_ERR(context->predicatesProxy != nullptr && context->predicatesProxy->GetPredicates() != nullptr);
        return obj->rdbStore_->Count(context->int64Output, *(context->predicatesProxy->GetPredicates()));
    };
    auto output = [context](napi_env env, napi_value &result) {
        napi_status status = napi_create_int64(env, context->int64Output, &result);
        CHECK_RETURN_SET_E(status == napi_ok, std::make_shared<InnerError>(E_ERROR));
    };
    context->SetAction(env, info, input, exec, output);

    CHECK_RETURN_NULL(context->error == nullptr || context->error->GetCode() == OK);
    return AsyncCall::Call(env, context);
}

napi_value RdbStoreProxy::Replace(napi_env env, napi_callback_info info)
{
    auto context = std::make_shared<RdbStoreContext>();
    auto input = [context](napi_env env, size_t argc, napi_value *argv, napi_value self) {
        CHECK_RETURN_SET_E(argc == 2, std::make_shared<ParamNumError>("2 or 3"));
        CHECK_RETURN(OK == ParserThis(env, self, context));
        CHECK_RETURN(OK == ParseTableName(env, argv[0], context));
        CHECK_RETURN(OK == ParseValuesBucket(env, argv[1], context));
    };
    auto exec = [context]() -> int {
        RdbStoreProxy *obj = reinterpret_cast<RdbStoreProxy *>(context->boundObj);
        CHECK_RETURN_ERR(obj != nullptr && obj->rdbStore_ != nullptr);
        return obj->rdbStore_->Replace(context->int64Output, context->tableName, context->valuesBucket);
    };
    auto output = [context](napi_env env, napi_value &result) {
        napi_status status = napi_create_int64(env, context->int64Output, &result);
        CHECK_RETURN_SET_E(status == napi_ok, std::make_shared<InnerError>(E_ERROR));
    };
    context->SetAction(env, info, input, exec, output);

    CHECK_RETURN_NULL(context->error == nullptr || context->error->GetCode() == OK);
    return AsyncCall::Call(env, context);
}

napi_value RdbStoreProxy::Backup(napi_env env, napi_callback_info info)
{
    auto context = std::make_shared<RdbStoreContext>();
    auto input = [context](napi_env env, size_t argc, napi_value *argv, napi_value self) {
        CHECK_RETURN_SET_E(argc == 1, std::make_shared<ParamNumError>("1 or 2"));
        CHECK_RETURN(OK == ParserThis(env, self, context));
        CHECK_RETURN(OK == ParseTableName(env, argv[0], context));
    };
    auto exec = [context]() -> int {
        RdbStoreProxy *obj = reinterpret_cast<RdbStoreProxy *>(context->boundObj);
        CHECK_RETURN_ERR(obj != nullptr && obj->rdbStore_ != nullptr);
        return obj->rdbStore_->Backup(context->tableName, context->newKey);
    };
    auto output = [context](napi_env env, napi_value &result) {
        napi_status status = napi_get_undefined(env, &result);
        CHECK_RETURN_SET_E(status == napi_ok, std::make_shared<InnerError>(E_ERROR));
    };
    context->SetAction(env, info, input, exec, output);

    CHECK_RETURN_NULL(context->error == nullptr || context->error->GetCode() == OK);
    return AsyncCall::Call(env, context);
}

napi_value RdbStoreProxy::Attach(napi_env env, napi_callback_info info)
{
    auto context = std::make_shared<RdbStoreContext>();
    auto input = [context](napi_env env, size_t argc, napi_value *argv, napi_value self) {
        CHECK_RETURN_SET_E(argc == 3, std::make_shared<ParamNumError>("3 or 4"));
        CHECK_RETURN(OK == ParserThis(env, self, context));
        CHECK_RETURN(OK == ParseAlias(env, argv[0], context));
        CHECK_RETURN(OK == ParsePath(env, argv[1], context));
    };
    auto exec = [context]() -> int {
        RdbStoreProxy *obj = reinterpret_cast<RdbStoreProxy *>(context->boundObj);
        CHECK_RETURN_ERR(obj != nullptr && obj->rdbStore_ != nullptr);
        return obj->rdbStore_->Attach(context->aliasName, context->pathName, context->newKey);
    };
    auto output = [context](napi_env env, napi_value &result) {
        napi_status status = napi_get_undefined(env, &result);
        CHECK_RETURN_SET_E(status == napi_ok, std::make_shared<InnerError>(E_ERROR));
    };
    context->SetAction(env, info, input, exec, output);

    CHECK_RETURN_NULL(context->error == nullptr || context->error->GetCode() == OK);
    return AsyncCall::Call(env, context);
}

napi_value RdbStoreProxy::IsReadOnly(napi_env env, napi_callback_info info)
{
    napi_value thisObj = nullptr;
    napi_get_cb_info(env, info, nullptr, nullptr, &thisObj, nullptr);
    RdbStoreProxy *rdbStoreProxy = GetNativeInstance(env, thisObj);
    RDB_NAPI_ASSERT(env, rdbStoreProxy && rdbStoreProxy->rdbStore_, std::make_shared<ParamError>("RdbStore", "valid"));
    bool out = rdbStoreProxy->rdbStore_->IsReadOnly();
    LOG_DEBUG("RdbStoreProxy::IsReadOnly out is : %{public}d", out);
    return JSUtils::Convert2JSValue(env, out);
}

napi_value RdbStoreProxy::IsMemoryRdb(napi_env env, napi_callback_info info)
{
    napi_value thisObj = nullptr;
    napi_get_cb_info(env, info, nullptr, nullptr, &thisObj, nullptr);
    RdbStoreProxy *rdbStoreProxy = GetNativeInstance(env, thisObj);
    RDB_NAPI_ASSERT(env, rdbStoreProxy && rdbStoreProxy->rdbStore_, std::make_shared<ParamError>("RdbStore", "valid"));
    bool out = rdbStoreProxy->rdbStore_->IsMemoryRdb();
    LOG_DEBUG("RdbStoreProxy::IsMemoryRdb out is : %{public}d", out);
    return JSUtils::Convert2JSValue(env, out);
}

napi_value RdbStoreProxy::GetPath(napi_env env, napi_callback_info info)
{
    napi_value thisObj = nullptr;
    napi_get_cb_info(env, info, nullptr, nullptr, &thisObj, nullptr);
    RdbStoreProxy *rdbStoreProxy = GetNativeInstance(env, thisObj);
    RDB_NAPI_ASSERT(env, rdbStoreProxy && rdbStoreProxy->rdbStore_, std::make_shared<ParamError>("RdbStore", "valid"));
    std::string path = rdbStoreProxy->rdbStore_->GetPath();
    LOG_DEBUG("RdbStoreProxy::GetPath path is empty ? %{public}d", path.empty());
    return JSUtils::Convert2JSValue(env, path);
}

napi_value RdbStoreProxy::BeginTransaction(napi_env env, napi_callback_info info)
{
    napi_value thisObj = nullptr;
    NAPI_CALL(env, napi_get_cb_info(env, info, nullptr, nullptr, &thisObj, nullptr));
    RdbStoreProxy *rdbStoreProxy = GetNativeInstance(env, thisObj);
    RDB_NAPI_ASSERT(env, rdbStoreProxy && rdbStoreProxy->rdbStore_, std::make_shared<ParamError>("RdbStore", "valid"));
    int errCode = rdbStoreProxy->rdbStore_->BeginTransaction();
    RDB_NAPI_ASSERT(env, errCode == E_OK, std::make_shared<InnerError>(errCode));
    LOG_DEBUG("RdbStoreProxy::BeginTransaction end, errCode is:%{public}d", errCode);
    return nullptr;
}

napi_value RdbStoreProxy::RollBack(napi_env env, napi_callback_info info)
{
    napi_value thisObj = nullptr;
    NAPI_CALL(env, napi_get_cb_info(env, info, nullptr, nullptr, &thisObj, nullptr));
    RdbStoreProxy *rdbStoreProxy = GetNativeInstance(env, thisObj);
    RDB_NAPI_ASSERT(env, rdbStoreProxy && rdbStoreProxy->rdbStore_, std::make_shared<ParamError>("RdbStore", "valid"));
    int errCode = rdbStoreProxy->rdbStore_->RollBack();
    NAPI_ASSERT(env, errCode == E_OK, "call RollBack failed");
    LOG_DEBUG("RdbStoreProxy::RollBack end, errCode is:%{public}d", errCode);
    return nullptr;
}

napi_value RdbStoreProxy::Commit(napi_env env, napi_callback_info info)
{
    napi_value thisObj = nullptr;
    NAPI_CALL(env, napi_get_cb_info(env, info, nullptr, nullptr, &thisObj, nullptr));
    RdbStoreProxy *rdbStoreProxy = GetNativeInstance(env, thisObj);
    RDB_NAPI_ASSERT(env, rdbStoreProxy && rdbStoreProxy->rdbStore_, std::make_shared<ParamError>("RdbStore", "valid"));
    int errCode = rdbStoreProxy->rdbStore_->Commit();
    NAPI_ASSERT(env, errCode == E_OK, "call Commit failed");
    LOG_DEBUG("RdbStoreProxy::Commit end, errCode is:%{public}d", errCode);
    return nullptr;
}

napi_value RdbStoreProxy::QueryByStep(napi_env env, napi_callback_info info)
{
    DISTRIBUTED_DATA_HITRACE(std::string(__FUNCTION__));
    LOG_DEBUG("RdbStoreProxy::QueryByStep start");
    auto context = std::make_shared<RdbStoreContext>();
    auto input = [context](napi_env env, size_t argc, napi_value *argv, napi_value self) {
        CHECK_RETURN_SET_E(argc == 1 || argc == 2, std::make_shared<ParamNumError>("2 or 3"));
        CHECK_RETURN(OK == ParserThis(env, self, context));
        CHECK_RETURN(OK == ParseSql(env, argv[0], context));
        if (argc == 2) {
            CHECK_RETURN(OK == ParseColumns(env, argv[1], context));
        }
    };
    auto exec = [context]() -> int {
        LOG_DEBUG("RdbStoreProxy::QueryByStep Async");
        RdbStoreProxy *obj = reinterpret_cast<RdbStoreProxy *>(context->boundObj);
        CHECK_RETURN_ERR(obj != nullptr && obj->rdbStore_ != nullptr);
        context->resultSet = obj->rdbStore_->QueryByStep(context->sql, context->columns);
        LOG_ERROR("RdbStoreProxy::QueryByStep is nullptr ? %{public}d ", context->resultSet == nullptr);
        return (context->resultSet != nullptr) ? E_OK : E_ERROR;
    };
    auto output = [context](napi_env env, napi_value &result) {
        result = ResultSetProxy::NewInstance(env, context->resultSet);
        CHECK_RETURN_SET_E(result != nullptr, std::make_shared<InnerError>(E_ERROR));
        LOG_DEBUG("RdbStoreProxy::QueryByStep end");
    };
    context->SetAction(env, info, input, exec, output);

    CHECK_RETURN_NULL(context->error == nullptr || context->error->GetCode() == OK);
    return AsyncCall::Call(env, context);
}

napi_value RdbStoreProxy::IsInTransaction(napi_env env, napi_callback_info info)
{
    napi_value thisObj = nullptr;
    napi_get_cb_info(env, info, nullptr, nullptr, &thisObj, nullptr);
    RdbStoreProxy *rdbStoreProxy = GetNativeInstance(env, thisObj);
    RDB_NAPI_ASSERT(env, rdbStoreProxy && rdbStoreProxy->rdbStore_, std::make_shared<ParamError>("RdbStore", "valid"));
    bool out = rdbStoreProxy->rdbStore_->IsInTransaction();
    LOG_DEBUG("RdbStoreProxy::IsInTransaction out is : %{public}d", out);
    return JSUtils::Convert2JSValue(env, out);
}

napi_value RdbStoreProxy::IsOpen(napi_env env, napi_callback_info info)
{
    napi_value thisObj = nullptr;
    napi_get_cb_info(env, info, nullptr, nullptr, &thisObj, nullptr);
    RdbStoreProxy *rdbStoreProxy = GetNativeInstance(env, thisObj);
    RDB_NAPI_ASSERT(env, rdbStoreProxy && rdbStoreProxy->rdbStore_, std::make_shared<ParamError>("RdbStore", "valid"));
    bool out = rdbStoreProxy->rdbStore_->IsOpen();
    LOG_DEBUG("RdbStoreProxy::IsOpen out is : %{public}d", out);
    return JSUtils::Convert2JSValue(env, out);
}

napi_value RdbStoreProxy::GetVersion(napi_env env, napi_callback_info info)
{
    napi_value thisObj = nullptr;
    napi_get_cb_info(env, info, nullptr, nullptr, &thisObj, nullptr);
    RdbStoreProxy *rdbStoreProxy = GetNativeInstance(env, thisObj);
    RDB_NAPI_ASSERT(env, rdbStoreProxy && rdbStoreProxy->rdbStore_, std::make_shared<ParamError>("RdbStore", "valid"));
    int32_t version = 0;
    int out = rdbStoreProxy->rdbStore_->GetVersion(version);
    LOG_DEBUG("RdbStoreProxy::GetVersion out is : %{public}d", out);
    return JSUtils::Convert2JSValue(env, version);
}

napi_value RdbStoreProxy::SetVersion(napi_env env, napi_callback_info info)
{
    napi_value thiz = nullptr;
    size_t argc = 1;
    napi_value args[1] = { 0 };
    napi_get_cb_info(env, info, &argc, args, &thiz, nullptr);
    RdbStoreProxy *rdbStoreProxy = GetNativeInstance(env, thiz);
    RDB_NAPI_ASSERT(env, rdbStoreProxy && rdbStoreProxy->rdbStore_, std::make_shared<ParamError>("RdbStore", "valid"));
    int32_t version = 0;
    napi_get_value_int32(env, args[0], &version);
    RDB_NAPI_ASSERT(env, version > 0, std::make_shared<ParamError>("version", "> 0"));
    int out = rdbStoreProxy->rdbStore_->SetVersion(version);
    LOG_DEBUG("RdbStoreProxy::SetVersion out is : %{public}d", out);
    return nullptr;
}

napi_value RdbStoreProxy::Restore(napi_env env, napi_callback_info info)
{
    LOG_DEBUG("RdbStoreProxy::Restore start");
    auto context = std::make_shared<RdbStoreContext>();
    auto input = [context](napi_env env, size_t argc, napi_value *argv, napi_value self) {
        CHECK_RETURN_SET_E(argc == 1, std::make_shared<ParamNumError>("1 or 2"));
        CHECK_RETURN(OK == ParserThis(env, self, context));
        CHECK_RETURN(OK == ParseSrcName(env, argv[0], context));
    };
    auto exec = [context]() -> int {
        LOG_DEBUG("RdbStoreProxy::Restore Async");
        RdbStoreProxy *obj = reinterpret_cast<RdbStoreProxy *>(context->boundObj);
        CHECK_RETURN_ERR(obj != nullptr && obj->rdbStore_ != nullptr);
        return obj->rdbStore_->Restore(context->srcName, context->newKey);
    };
    auto output = [context](napi_env env, napi_value &result) {
        napi_status status = napi_get_undefined(env, &result);
        CHECK_RETURN_SET_E(status == napi_ok, std::make_shared<InnerError>(E_ERROR));
        LOG_DEBUG("RdbStoreProxy::Restore end");
    };
    context->SetAction(env, info, input, exec, output);

    CHECK_RETURN_NULL(context->error == nullptr || context->error->GetCode() == OK);
    return AsyncCall::Call(env, context);
}
} // namespace RelationalStoreJsKit
} // namespace OHOS
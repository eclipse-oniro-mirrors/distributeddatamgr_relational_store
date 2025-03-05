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

#ifndef RDB_JSKIT_NAPI_RDB_STORE_H
#define RDB_JSKIT_NAPI_RDB_STORE_H

#include <list>
#include <mutex>

#include "js_proxy.h"
#include "js_uv_queue.h"
#include "napi/native_api.h"
#include "napi/native_common.h"
#include "napi/native_node_api.h"
#include "napi_rdb_error.h"
#include "rdb_helper.h"
#include "rdb_store.h"

namespace OHOS {
namespace RelationalStoreJsKit {
using Descriptor = std::function<std::vector<napi_property_descriptor>(void)>;
class RdbStoreProxy : public JSProxy::JSProxy<NativeRdb::RdbStore> {
public:
    static void Init(napi_env env, napi_value exports);
    static napi_value NewInstance(napi_env env, std::shared_ptr<NativeRdb::RdbStore> value, bool isSystemAppCalled,
        const std::string &bundleName);
    RdbStoreProxy();
    ~RdbStoreProxy();
    RdbStoreProxy(std::shared_ptr<NativeRdb::RdbStore> rdbStore);
    RdbStoreProxy &operator=(std::shared_ptr<NativeRdb::RdbStore> rdbStore);
    bool IsSystemAppCalled();
    std::string GetBundleName();

private:
    static Descriptor GetDescriptors();
    static void AddSyncFunctions(std::vector<napi_property_descriptor> &properties);
    static napi_value Initialize(napi_env env, napi_callback_info info);
    static napi_value Delete(napi_env env, napi_callback_info info);
    static napi_value Update(napi_env env, napi_callback_info info);
    static napi_value Insert(napi_env env, napi_callback_info info);
    static napi_value BatchInsert(napi_env env, napi_callback_info info);
    static napi_value BatchInsertWithConflictResolution(napi_env env, napi_callback_info info);
    static napi_value Query(napi_env env, napi_callback_info info);
    static napi_value QuerySql(napi_env env, napi_callback_info info);
    static napi_value ExecuteSql(napi_env env, napi_callback_info info);
    static napi_value Execute(napi_env env, napi_callback_info info);
    static napi_value Backup(napi_env env, napi_callback_info info);
    static napi_value Replace(napi_env env, napi_callback_info info);
    static napi_value Attach(napi_env env, napi_callback_info info);
    static napi_value Detach(napi_env env, napi_callback_info info);
    static napi_value GetPath(napi_env env, napi_callback_info info);
    static napi_value BeginTransaction(napi_env env, napi_callback_info info);
    static napi_value BeginTrans(napi_env env, napi_callback_info info);
    static napi_value RollBack(napi_env env, napi_callback_info info);
    static napi_value RollBackByTxId(napi_env env, napi_callback_info info);
    static napi_value Commit(napi_env env, napi_callback_info info);
    static napi_value QueryByStep(napi_env env, napi_callback_info info);
    static napi_value IsInTransaction(napi_env env, napi_callback_info info);
    static napi_value IsOpen(napi_env env, napi_callback_info info);
    static napi_value GetVersion(napi_env env, napi_callback_info info);
    static napi_value GetRebuilt(napi_env env, napi_callback_info info);
    static napi_value SetVersion(napi_env env, napi_callback_info info);
    static napi_value Restore(napi_env env, napi_callback_info info);
    static napi_value Close(napi_env env, napi_callback_info info);
    static napi_value CreateTransaction(napi_env env, napi_callback_info info);
    void UnregisterAll();
    int32_t dbType = NativeRdb::DB_SQLITE;
    std::mutex mutex_;
    bool isSystemAppCalled_ = false;
    std::shared_ptr<AppDataMgrJsKit::UvQueue> queue_;
    std::string bundleName_;
    
    static constexpr int WAIT_TIME_DEFAULT = 2;
    static constexpr int WAIT_TIME_LIMIT = 300;
};
} // namespace RelationalStoreJsKit
} // namespace OHOS

#endif // RDB_JSKIT_NAPI_RDB_STORE_H

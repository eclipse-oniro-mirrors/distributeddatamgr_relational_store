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

#ifndef NATIVE_RDB_STORE_MANAGER_H
#define NATIVE_RDB_STORE_MANAGER_H

#include <memory>
#include <mutex>
#include <string>
#include <utility>

#include "lru_bucket.h"
#include "rdb_open_callback.h"
#include "rdb_store_config.h"
#include "rdb_store_impl.h"

namespace OHOS {
namespace NativeRdb {
class RdbStoreManager {
public:
    static RdbStoreManager &GetInstance();
    RdbStoreManager();
    virtual ~RdbStoreManager();
    std::shared_ptr<RdbStore> GetRdbStore(const RdbStoreConfig &config,
        int &errCode, int version, RdbOpenCallback &openCallback);
    void Clear();
    bool Remove(const std::string &path);
    bool Delete(const std::string &path);
    int SetSecurityLabel(const RdbStoreConfig &config);

private:
    static constexpr uint32_t BUCKET_MAX_SIZE = 4;
    using Param = DistributedRdb::RdbSyncerParam;
    int ProcessOpenCallback(RdbStore &rdbStore,
        const RdbStoreConfig &config, int version, RdbOpenCallback &openCallback);
    bool IsConfigInvalidChanged(const std::string &path, const RdbStoreConfig &config);
    int32_t GetParamFromService(DistributedRdb::RdbSyncerParam &param);
    Param GetSyncParam(const RdbStoreConfig &config);
    std::string bundleName_;
    std::mutex mutex_;
    std::map<std::string, std::weak_ptr<RdbStoreImpl>> storeCache_;
    LRUBucket<std::string, Param> configCache_;
};
} // namespace NativeRdb
} // namespace OHOS
#endif

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

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <utility>

#include "lru_bucket.h"
#include "rdb_open_callback.h"
#include "rdb_store_config.h"

namespace OHOS {
namespace NativeRdb {
class RdbStoreImpl;
class RdbStoreManager {
public:
    static RdbStoreManager &GetInstance();
    RdbStoreManager();
    virtual ~RdbStoreManager();
    std::shared_ptr<RdbStore> GetRdbStore(
        const RdbStoreConfig &config, int &errCode, int version, RdbOpenCallback &openCallback);
    void Clear();
    void Init();
    bool Destroy();
    bool Remove(const std::string &path, bool shouldClose);
    bool Delete(const RdbStoreConfig &config, bool shouldClose);

private:
    using Param = DistributedRdb::RdbSyncerParam;
    using Info = DistributedRdb::RdbDebugInfo;
    using DebugInfos = std::map<std::string, RdbStoreManager::Info>;
    using DfxInfo = DistributedRdb::RdbDfxInfo;
    bool IsConfigInvalidChanged(const std::string &path, RdbStoreConfig &config);
    int32_t GetParamFromService(DistributedRdb::RdbSyncerParam &param);
    static bool IsPermitted(const DistributedRdb::RdbSyncerParam &param);
    static int32_t CheckConfig(const RdbStoreConfig &config);
    static Param GetSyncParam(const RdbStoreConfig &config);
    static int32_t Collector(const RdbStoreConfig &config, DebugInfos &debugInfos, DfxInfo &dfxInfo);
    std::shared_ptr<RdbStoreImpl> GetStoreFromCache(const std::string &path,
        const RdbStoreConfig &config, int &errCode);

    static constexpr uint32_t BUCKET_MAX_SIZE = 4;
    static const bool regCollector_;
    std::mutex mutex_;
    std::map<std::string, std::weak_ptr<RdbStoreImpl>> storeCache_;
    LRUBucket<std::string, Param> configCache_;
};
} // namespace NativeRdb
} // namespace OHOS
#endif

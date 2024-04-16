/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
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

#ifndef OHOS_DISTRIBUTED_DATA_RELATIONAL_STORE_FRAMEWORKS_NATIVE_RDB_INCLUDE_CONNECTION_H
#define OHOS_DISTRIBUTED_DATA_RELATIONAL_STORE_FRAMEWORKS_NATIVE_RDB_INCLUDE_CONNECTION_H
#include <cstdint>
#include <functional>
#include <memory>
#include <set>
#include <string>
#include <utility>

#include "statement.h"
namespace OHOS::NativeRdb {
class RdbStoreConfig;
class Statement;
class Connection {
public:
    using Notifier = std::function<void(const std::set<std::string> &tables)>;
    using Creator = std::pair<int, std::shared_ptr<Connection>> (*)(const RdbStoreConfig& config, bool isWriter);
    static std::pair<int, std::shared_ptr<Connection>> Create(const RdbStoreConfig &config, bool isWriter);
    static int32_t RegisterCreator(int32_t dbType, Creator creator);

    int32_t SetId(int32_t id);
    int32_t GetId() const;
    virtual ~Connection() = default;
    virtual int32_t OnInitialize() = 0;
    virtual std::pair<int, std::shared_ptr<Statement>> CreateStatement(
        const std::string &sql, std::shared_ptr<Connection> conn) = 0;
    virtual int32_t GetDBType() const = 0;
    virtual bool IsWriter() const = 0;
    virtual int ReSetKey(const RdbStoreConfig &config) = 0;
    virtual int TryCheckPoint() = 0;
    virtual int LimitWalSize() = 0;
    virtual int ConfigLocale(const std::string &localeStr) = 0;
    virtual int CleanDirtyData(const std::string &table, uint64_t cursor) = 0;
    virtual int SubscribeTableChanges(const Notifier &notifier) = 0;
    virtual int GetMaxVariable() const = 0;
    virtual int32_t GetJournalMode() = 0;
private:
    int32_t id_ = 0;
};
}
#endif // OHOS_DISTRIBUTED_DATA_RELATIONAL_STORE_FRAMEWORKS_NATIVE_RDB_INCLUDE_CONNECTION_H
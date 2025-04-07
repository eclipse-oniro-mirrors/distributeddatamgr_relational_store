/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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

#ifndef DISTRIBUTED_RDB_KNOWLEDGE_SCHEMA_HELPER_H
#define DISTRIBUTED_RDB_KNOWLEDGE_SCHEMA_HELPER_H

#include <memory>
#include <vector>
#include <shared_mutex>
#include <string>

#include "nlohmann/json.hpp"
#include "rdb_store_config.h"
#include "rdb_types.h"

namespace OHOS::NativeRdb {
class KnowledgeSchemaHelper : public std::enable_shared_from_this<KnowledgeSchemaHelper> {
public:
    KnowledgeSchemaHelper() = default;
    ~KnowledgeSchemaHelper();

    void Init(const RdbStoreConfig &config, const DistributedRdb::RdbKnowledgeSchema &schema);
    std::vector<nlohmann::json> ParseSchema(const std::vector<std::string> &schemaStr);
    std::pair<int, DistributedRdb::RdbKnowledgeSchema> GetRdbKnowledgeSchema(const std::string &dbName);
    void DonateKnowledgeData();
private:
    void LoadKnowledgeLib();
    void LoadKnowledgeSchemaManager(void *handle);
    DistributedRdb::RdbKnowledgeSchema ParseRdbKnowledgeSchema(const nlohmann::json &json,
        const std::string &dbName);
    bool IsLoadLib() const;

    mutable std::shared_mutex libMutex_;
    bool isLoadKnowledgeLib_ = false;
    IKnowledgeSchemaManager *schemaManager_ = nullptr;
    void *dlHandle_ = nullptr;
};
}
#endif // DISTRIBUTED_RDB_KNOWLEDGE_SCHEMA_HELPER_H

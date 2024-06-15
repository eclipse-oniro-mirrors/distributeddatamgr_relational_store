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
#include "connection.h"

#include "rdb_common.h"
#include "rdb_errno.h"
#include "rdb_store_config.h"
namespace OHOS::NativeRdb {
static Connection::Creator g_creators[DB_BUTT] = { nullptr, nullptr };
static Connection::Repairer g_repairers[DB_BUTT] = { nullptr, nullptr };
static Connection::Deleter g_fileDeleter[DB_BUTT] = { nullptr, nullptr };
std::pair<int, std::shared_ptr<Connection>> Connection::Create(const RdbStoreConfig &config, bool isWriter)
{
    auto dbType = config.GetDBType();
    if (dbType < static_cast<int32_t>(DB_SQLITE) || dbType >= static_cast<int32_t>(DB_BUTT)) {
        return { E_INVALID_ARGS, nullptr };
    }
    auto creator = g_creators[dbType];
    if (creator == nullptr) {
        return { E_ERROR, nullptr };
    }

    return creator(config, isWriter);
}

int32_t Connection::Repair(const RdbStoreConfig &config)
{
    auto dbType = config.GetDBType();
    if (dbType < static_cast<int32_t>(DB_SQLITE) || dbType >= static_cast<int32_t>(DB_BUTT)) {
        return E_INVALID_ARGS;
    }
    auto repairer = g_repairers[dbType];
    if (repairer == nullptr) {
        return E_ERROR;
    }

    return repairer(config);
}

void Connection::DeleteDbFile(const RdbStoreConfig &config)
{
    auto dbType = config.GetDBType();
    if (dbType < static_cast<int32_t>(DB_SQLITE) || dbType >= static_cast<int32_t>(DB_BUTT)) {
        return;
    }
    auto deleter = g_fileDeleter[dbType];
    if (deleter == nullptr) {
        return;
    }

    deleter(config);
}

int32_t Connection::RegisterCreator(int32_t dbType, Creator creator)
{
    if (dbType < static_cast<int32_t>(DB_SQLITE) || dbType >= static_cast<int32_t>(DB_BUTT)) {
        return E_INVALID_ARGS;
    }
    if (g_creators[dbType] != nullptr) {
        return E_OK;
    }

    g_creators[dbType] = creator;
    return E_OK;
}

int32_t Connection::RegisterRepairer(int32_t dbType, Repairer repairer)
{
    if (dbType < static_cast<int32_t>(DB_SQLITE) || dbType >= static_cast<int32_t>(DB_BUTT)) {
        return E_INVALID_ARGS;
    }
    if (g_repairers[dbType] != nullptr) {
        return E_OK;
    }

    g_repairers[dbType] = repairer;
    return E_OK;
}

int32_t Connection::RegisterFileDeleter(int32_t dbType, Deleter deleter)
{
    if (dbType < static_cast<int32_t>(DB_SQLITE) || dbType >= static_cast<int32_t>(DB_BUTT)) {
        return E_INVALID_ARGS;
    }
    if (g_fileDeleter[dbType] != nullptr) {
        return E_OK;
    }

    g_fileDeleter[dbType] = deleter;
    return E_OK;
}

int Connection::SetId(int id)
{
    id_ = id;
    return id_;
}

int Connection::GetId() const
{
    return id_;
}
} // namespace OHOS::NativeRdb

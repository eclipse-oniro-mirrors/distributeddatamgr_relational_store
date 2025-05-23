/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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

#ifndef NATIVE_RDB_RDB_OPEN_CALLBACK_H
#define NATIVE_RDB_RDB_OPEN_CALLBACK_H

#include "rdb_store.h"

namespace OHOS {
namespace NativeRdb {

/**
 * The RdbOpenCallback class of RDB.
 */
class API_EXPORT RdbOpenCallback {
public:
    /**
     * @brief Called when the database associate whit this RdbStore is created with the first time.
     *
     * This is where the creation of tables and insert the initial data of tables should happen.
     *
     * @param rdbStore Indicates the RdbStore object.
     */
    virtual int OnCreate(RdbStore &rdbStore) = 0;

    /**
     * @brief Called when the database associate whit this RdbStore needs to be upgrade.
     *
     * @param rdbStore Indicates the RdbStore object.
     * @param currentVersion Indicates the old database version.
     * @param targetVersion Indicates the new database version.
     */
    virtual int OnUpgrade(RdbStore &rdbStore, int currentVersion, int targetVersion) = 0;

    /**
     * @brief Called when the database associate whit this RdbStore needs to be downgrade.
     *
     * @param rdbStore Indicates the RdbStore object.
     * @param currentVersion Indicates the old database version.
     * @param targetVersion Indicates the new database version.
     */
    virtual int OnDowngrade(RdbStore &rdbStore, int currentVersion, int targetVersion)
    {
        return 0;
    }

    /**
     * @brief Called when the RdbStore has been opened.
     *
     * @param rdbStore Indicates the RdbStore object.
     */
    virtual int OnOpen(RdbStore &rdbStore)
    {
        return 0;
    }

    /**
     * @brief Called when the RdbStore has been corrupted
     *
     * @param databaseFile Indicates the RdbStore file name.
     */
    virtual int onCorruption(std::string databaseFile)
    {
        return 0;
    }

    /**
     * @brief Virtual destructor to ensure proper cleanup of derived class objects.
     */
    virtual ~RdbOpenCallback() = default;
};

} // namespace NativeRdb
} // namespace OHOS
#endif

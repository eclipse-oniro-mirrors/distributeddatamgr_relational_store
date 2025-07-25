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

#ifndef NATIVE_RDB_SQL_UTILS_H
#define NATIVE_RDB_SQL_UTILS_H
#include <string>
#include <vector>
#include "abs_rdb_predicates.h"
#include "rdb_common.h"
#include "rdb_predicates.h"
#include "values_bucket.h"
#include "value_object.h"

namespace OHOS {
namespace NativeRdb {
struct SqlInfo {
    std::string sql;
    std::vector<NativeRdb::ValueObject> args;
};

using Values = std::vector<ValueObject>;
using Row = ValuesBucket;
using Rows = std::vector<Row>;
using Fields = std::vector<std::string>;
using Resolution = ConflictResolution;

class API_EXPORT RdbSqlUtils {
public:
    /**
     * @brief create data base directory.
     */
    static int CreateDirectory(const std::string &databaseDir);

    /**
     * @brief get custom data base path.
     */
    static std::pair<std::string, int> GetDefaultDatabasePath(
        const std::string &baseDir, const std::string &name, const std::string &customDir = "");

    /**
     * @brief get custom rootDir data base path.
     */
    static std::pair<std::string, int> GetCustomDatabasePath(const std::string &rootDir, const std::string &name,
        const std::string &customDir);

    /**
     * @brief get default data base path.
     */
    static std::string GetDefaultDatabasePath(const std::string &baseDir, const std::string &name, int &errorCode);

    /**
     * @brief build query sql string.
     */
    static std::string BuildQueryString(const AbsRdbPredicates &predicates, const std::vector<std::string> &columns);
    
    static std::pair<int, SqlInfo> GetInsertSqlInfo(const std::string &table, const Row &row, Resolution resolution);
    static std::pair<int, SqlInfo> GetUpdateSqlInfo(const AbsRdbPredicates &predicates, const Row &row,
        Resolution resolution, const std::vector<std::string> &returningFields = {});
    static std::pair<int, SqlInfo> GetDeleteSqlInfo(
        const AbsRdbPredicates &predicates, const std::vector<std::string> &returningFields = {});
    static std::pair<int, SqlInfo> GetQuerySqlInfo(const AbsRdbPredicates &predicates, const Fields &columns);
};
} // namespace NativeRdb
} // namespace OHOS

#endif

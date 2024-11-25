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

#ifndef OHOS_DISTRIBUTED_DATA_RELATIONAL_STORE_FRAMEWORKS_NATIVE_RDB_DATA_ABILITY_ADAPTER_SRC_RESULT_SET_UTILS_H
#define OHOS_DISTRIBUTED_DATA_RELATIONAL_STORE_FRAMEWORKS_NATIVE_RDB_DATA_ABILITY_ADAPTER_SRC_RESULT_SET_UTILS_H
#include <memory>

#include "abs_shared_result_set.h"
#include "basic/result_set.h"

namespace OHOS::RdbDataAbilityAdapter {
class ResultSetUtils : public NativeRdb::AbsSharedResultSet {
    using DSResultSet = DataShare::ResultSet;

public:
    ResultSetUtils(std::shared_ptr<DSResultSet> dbResultSet);
    int GetColumnCount(int &count) override;
    int GetColumnType(int columnIndex, NativeRdb::ColumnType &columnType) override;
    int GetRowCount(int &count) override;
    int GetRowIndex(int &position) const override;
    int GoTo(int offset) override;
    int GoToRow(int position) override;
    int GoToFirstRow() override;
    int GoToLastRow() override;
    int GoToNextRow() override;
    int GoToPreviousRow() override;
    int IsEnded(bool &result) override;
    int IsStarted(bool &result) const override;
    int IsAtFirstRow(bool &result) const override;
    int IsAtLastRow(bool &result) override;
    int Close() override;

protected:
    std::pair<int, std::vector<std::string>> GetColumnNames() override;

private:
    std::shared_ptr<DSResultSet> resultSet_;
};
} // namespace OHOS::RdbDataAbilityAdapter
#endif // OHOS_DISTRIBUTED_DATA_RELATIONAL_STORE_FRAMEWORKS_NATIVE_RDB_DATA_ABILITY_ADAPTER_SRC_RESULT_SET_UTILS_H

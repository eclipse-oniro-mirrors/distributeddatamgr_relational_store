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

#ifndef NATIVE_RDB_STEP_RESULT_SET_H
#define NATIVE_RDB_STEP_RESULT_SET_H

#include <memory>
#include <shared_mutex>
#include <thread>
#include <vector>

#include "abs_result_set.h"
#include "connection.h"
#include "connection_pool.h"
#include "statement.h"

namespace OHOS {
namespace NativeRdb {
class StepResultSet : public AbsResultSet {
public:
    using Values = std::vector<ValueObject>;
    using Conn = std::shared_ptr<Connection>;
    using Time = std::chrono::steady_clock::time_point;
    StepResultSet(Time start, Conn conn, const std::string &sql, const Values &args, bool preCount, bool safe = false);
    ~StepResultSet() override;
    int GetColumnType(int columnIndex, ColumnType &columnType) override;
    int GoToRow(int position) override;
    int GoToNextRow() override;
    int GetSize(int columnIndex, size_t &size) override;
    int Get(int32_t col, ValueObject &value) override;
    int Close() override;
    int GetRowCount(int &count) override;

protected:
    std::pair<int, std::vector<std::string>> GetColumnNames() override;

private:
    template<typename T>
    int GetValue(int32_t col, T &value);
    std::pair<int, ValueObject> GetValueObject(int32_t col, size_t index);
    std::shared_ptr<Statement> GetStatement();
    int Reset();
    int PrepareStep();

    // Max times of retrying step query
    static const int STEP_QUERY_RETRY_MAX_TIMES = 50;
    // Interval of retrying step query in millisecond
    static const int STEP_QUERY_RETRY_INTERVAL = 1000;
    static const int EMPTY_ROW_COUNT = 0;

    bool isSupportCountRow_ = true;
    std::shared_ptr<Statement> statement_;
    std::shared_ptr<Connection> conn_;

    std::string sql_;
    std::vector<ValueObject> args_;
};
} // namespace NativeRdb
} // namespace OHOS
#endif

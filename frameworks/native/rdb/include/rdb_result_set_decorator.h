/*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
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

#ifndef NATIVE_RDB_RDB_RESULT_SET_DECORATOR_H
#define NATIVE_RDB_RDB_RESULT_SET_DECORATOR_H

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "rdb_errno.h"
#include "rdb_fault_injector.h"
#include "result_set.h"

namespace OHOS {
namespace NativeRdb {

// Result-set decorator of the RDB fault-injection mock layer - one template
// for both result-set bases. Query methods wrap the result sets they return
// whenever the database path has an injection entry, so resultSetRead faults
// surface on the read interface callers actually use: instantiate with
// ResultSet for the step result sets and with AbsSharedResultSet for the
// SharedBlock-based family. A hit returns E_SQLITE_CORRUPT without
// delegating; otherwise the call delegates to the inner result set
// unchanged. The read family deliberately excludes Close (BR-8): closing
// must stay available so callers can release resources while fault
// injection is active. Internal helpers of the AbsSharedResultSet family
// (SetBlock/Finalize/...) are inherited and not intercepted.
template <typename Inner>
class RdbResultSetDecorator final : public Inner {
public:
    RdbResultSetDecorator(std::shared_ptr<Inner> inner, std::shared_ptr<RdbFaultInjector> injector)
        : innerHolder_(std::move(inner)), inner_(*innerHolder_), injector_(std::move(injector))
    {
    }
    ~RdbResultSetDecorator() override = default;

    int GetAllColumnNames(std::vector<std::string> &columnNames) override
    {
        RDB_FAULT_INJECT_HIT(injector_, RESULT_SET_READ, "GetAllColumnNames", E_SQLITE_CORRUPT)
        return inner_.GetAllColumnNames(columnNames);
    }

    std::pair<int, std::vector<std::string>> GetWholeColumnNames() override
    {
        RDB_FAULT_INJECT_HIT(injector_, RESULT_SET_READ, "GetWholeColumnNames", { E_SQLITE_CORRUPT, {} })
        return inner_.GetWholeColumnNames();
    }

    int GetColumnCount(int &count) override
    {
        RDB_FAULT_INJECT_HIT(injector_, RESULT_SET_READ, "GetColumnCount", E_SQLITE_CORRUPT)
        return inner_.GetColumnCount(count);
    }

    int GetColumnType(int columnIndex, ColumnType &columnType) override
    {
        RDB_FAULT_INJECT_HIT(injector_, RESULT_SET_READ, "GetColumnType", E_SQLITE_CORRUPT)
        return inner_.GetColumnType(columnIndex, columnType);
    }

    int GetColumnIndex(const std::string &columnName, int &columnIndex) override
    {
        RDB_FAULT_INJECT_HIT(injector_, RESULT_SET_READ, "GetColumnIndex", E_SQLITE_CORRUPT)
        return inner_.GetColumnIndex(columnName, columnIndex);
    }

    int GetColumnName(int columnIndex, std::string &columnName) override
    {
        RDB_FAULT_INJECT_HIT(injector_, RESULT_SET_READ, "GetColumnName", E_SQLITE_CORRUPT)
        return inner_.GetColumnName(columnIndex, columnName);
    }

    int GetRowCount(int &count) override
    {
        RDB_FAULT_INJECT_HIT(injector_, RESULT_SET_READ, "GetRowCount", E_SQLITE_CORRUPT)
        return inner_.GetRowCount(count);
    }

    int GetRowIndex(int &position) const override
    {
        RDB_FAULT_INJECT_HIT(injector_, RESULT_SET_READ, "GetRowIndex", E_SQLITE_CORRUPT)
        return inner_.GetRowIndex(position);
    }

    int GoTo(int offset) override
    {
        RDB_FAULT_INJECT_HIT(injector_, RESULT_SET_READ, "GoTo", E_SQLITE_CORRUPT)
        return inner_.GoTo(offset);
    }

    int GoToRow(int position) override
    {
        RDB_FAULT_INJECT_HIT(injector_, RESULT_SET_READ, "GoToRow", E_SQLITE_CORRUPT)
        return inner_.GoToRow(position);
    }

    int GoToFirstRow() override
    {
        RDB_FAULT_INJECT_HIT(injector_, RESULT_SET_READ, "GoToFirstRow", E_SQLITE_CORRUPT)
        return inner_.GoToFirstRow();
    }

    int GoToLastRow() override
    {
        RDB_FAULT_INJECT_HIT(injector_, RESULT_SET_READ, "GoToLastRow", E_SQLITE_CORRUPT)
        return inner_.GoToLastRow();
    }

    int GoToNextRow() override
    {
        RDB_FAULT_INJECT_HIT(injector_, RESULT_SET_READ, "GoToNextRow", E_SQLITE_CORRUPT)
        return inner_.GoToNextRow();
    }

    int GoToPreviousRow() override
    {
        RDB_FAULT_INJECT_HIT(injector_, RESULT_SET_READ, "GoToPreviousRow", E_SQLITE_CORRUPT)
        return inner_.GoToPreviousRow();
    }

    int IsEnded(bool &result) override
    {
        RDB_FAULT_INJECT_HIT(injector_, RESULT_SET_READ, "IsEnded", E_SQLITE_CORRUPT)
        return inner_.IsEnded(result);
    }

    int IsStarted(bool &result) const override
    {
        RDB_FAULT_INJECT_HIT(injector_, RESULT_SET_READ, "IsStarted", E_SQLITE_CORRUPT)
        return inner_.IsStarted(result);
    }

    int IsAtFirstRow(bool &result) const override
    {
        RDB_FAULT_INJECT_HIT(injector_, RESULT_SET_READ, "IsAtFirstRow", E_SQLITE_CORRUPT)
        return inner_.IsAtFirstRow(result);
    }

    int IsAtLastRow(bool &result) override
    {
        RDB_FAULT_INJECT_HIT(injector_, RESULT_SET_READ, "IsAtLastRow", E_SQLITE_CORRUPT)
        return inner_.IsAtLastRow(result);
    }

    int GetBlob(int columnIndex, std::vector<uint8_t> &blob) override
    {
        RDB_FAULT_INJECT_HIT(injector_, RESULT_SET_READ, "GetBlob", E_SQLITE_CORRUPT)
        return inner_.GetBlob(columnIndex, blob);
    }

    int GetString(int columnIndex, std::string &value) override
    {
        RDB_FAULT_INJECT_HIT(injector_, RESULT_SET_READ, "GetString", E_SQLITE_CORRUPT)
        return inner_.GetString(columnIndex, value);
    }

    int GetInt(int columnIndex, int &value) override
    {
        RDB_FAULT_INJECT_HIT(injector_, RESULT_SET_READ, "GetInt", E_SQLITE_CORRUPT)
        return inner_.GetInt(columnIndex, value);
    }

    int GetLong(int columnIndex, int64_t &value) override
    {
        RDB_FAULT_INJECT_HIT(injector_, RESULT_SET_READ, "GetLong", E_SQLITE_CORRUPT)
        return inner_.GetLong(columnIndex, value);
    }

    int GetDouble(int columnIndex, double &value) override
    {
        RDB_FAULT_INJECT_HIT(injector_, RESULT_SET_READ, "GetDouble", E_SQLITE_CORRUPT)
        return inner_.GetDouble(columnIndex, value);
    }

    int IsColumnNull(int columnIndex, bool &isNull) override
    {
        RDB_FAULT_INJECT_HIT(injector_, RESULT_SET_READ, "IsColumnNull", E_SQLITE_CORRUPT)
        return inner_.IsColumnNull(columnIndex, isNull);
    }

    bool IsClosed() const override
    {
        RDB_FAULT_INJECT_HIT(injector_, RESULT_SET_READ, "IsClosed", true)
        return inner_.IsClosed();
    }

    int Close() override
    {
        RDB_FAULT_INJECT_METHOD(injector_, "Close", E_SQLITE_CORRUPT)
        return inner_.Close();
    }

    int GetAsset(int32_t col, ValueObject::Asset &value) override
    {
        RDB_FAULT_INJECT_HIT(injector_, RESULT_SET_READ, "GetAsset", E_SQLITE_CORRUPT)
        return inner_.GetAsset(col, value);
    }

    int GetAssets(int32_t col, ValueObject::Assets &value) override
    {
        RDB_FAULT_INJECT_HIT(injector_, RESULT_SET_READ, "GetAssets", E_SQLITE_CORRUPT)
        return inner_.GetAssets(col, value);
    }

    int GetFloat32Array(int32_t index, ValueObject::FloatVector &vecs) override
    {
        RDB_FAULT_INJECT_HIT(injector_, RESULT_SET_READ, "GetFloat32Array", E_SQLITE_CORRUPT)
        return inner_.GetFloat32Array(index, vecs);
    }

    int Get(int32_t col, ValueObject &value) override
    {
        RDB_FAULT_INJECT_HIT(injector_, RESULT_SET_READ, "Get", E_SQLITE_CORRUPT)
        return inner_.Get(col, value);
    }

    int GetRow(RowEntity &rowEntity) override
    {
        RDB_FAULT_INJECT_HIT(injector_, RESULT_SET_READ, "GetRow", E_SQLITE_CORRUPT)
        return inner_.GetRow(rowEntity);
    }

    std::pair<int, std::vector<ValueObject>> GetRowData() override
    {
        RDB_FAULT_INJECT_HIT(injector_, RESULT_SET_READ, "GetRowData", { E_SQLITE_CORRUPT, {} })
        return inner_.GetRowData();
    }

    std::pair<int, std::vector<std::vector<ValueObject>>> GetRowsData(int32_t maxCount, int32_t position) override
    {
        RDB_FAULT_INJECT_HIT(injector_, RESULT_SET_READ, "GetRowsData", { E_SQLITE_CORRUPT, {} })
        return inner_.GetRowsData(maxCount, position);
    }

    int GetSize(int columnIndex, size_t &size) override
    {
        RDB_FAULT_INJECT_HIT(injector_, RESULT_SET_READ, "GetSize", E_SQLITE_CORRUPT)
        return inner_.GetSize(columnIndex, size);
    }

private:
    // The wrapped result set, holder plus reference following the
    // configHolder_/config_ convention in rdb_store_impl.h.
    std::shared_ptr<Inner> innerHolder_;
    Inner &inner_;
    // The store's fault injector (see rdb_fault_injector.h), consumed only
    // through the RDB_FAULT_INJECT_* macros.
    std::shared_ptr<RdbFaultInjector> injector_;
};

} // namespace NativeRdb
} // namespace OHOS

#endif // NATIVE_RDB_RDB_RESULT_SET_DECORATOR_H

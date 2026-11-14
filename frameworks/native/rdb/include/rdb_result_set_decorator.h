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
        : innerHolder_(std::move(inner)), inner_(*innerHolder_),
          injectorHolder_(std::move(injector)), injector_(*injectorHolder_)
    {
    }
    ~RdbResultSetDecorator() override = default;

    int GetAllColumnNames(std::vector<std::string> &columnNames) override
    {
        if (injector_.Hit("GetAllColumnNames", RdbFaultOp::RESULT_SET_READ)) {
            return E_SQLITE_CORRUPT;
        }
        return inner_.GetAllColumnNames(columnNames);
    }

    std::pair<int, std::vector<std::string>> GetWholeColumnNames() override
    {
        if (injector_.Hit("GetWholeColumnNames", RdbFaultOp::RESULT_SET_READ)) {
            return { E_SQLITE_CORRUPT, {} };
        }
        return inner_.GetWholeColumnNames();
    }

    int GetColumnCount(int &count) override
    {
        if (injector_.Hit("GetColumnCount", RdbFaultOp::RESULT_SET_READ)) {
            return E_SQLITE_CORRUPT;
        }
        return inner_.GetColumnCount(count);
    }

    int GetColumnType(int columnIndex, ColumnType &columnType) override
    {
        if (injector_.Hit("GetColumnType", RdbFaultOp::RESULT_SET_READ)) {
            return E_SQLITE_CORRUPT;
        }
        return inner_.GetColumnType(columnIndex, columnType);
    }

    int GetColumnIndex(const std::string &columnName, int &columnIndex) override
    {
        if (injector_.Hit("GetColumnIndex", RdbFaultOp::RESULT_SET_READ)) {
            return E_SQLITE_CORRUPT;
        }
        return inner_.GetColumnIndex(columnName, columnIndex);
    }

    int GetColumnName(int columnIndex, std::string &columnName) override
    {
        if (injector_.Hit("GetColumnName", RdbFaultOp::RESULT_SET_READ)) {
            return E_SQLITE_CORRUPT;
        }
        return inner_.GetColumnName(columnIndex, columnName);
    }

    int GetRowCount(int &count) override
    {
        if (injector_.Hit("GetRowCount", RdbFaultOp::RESULT_SET_READ)) {
            return E_SQLITE_CORRUPT;
        }
        return inner_.GetRowCount(count);
    }

    int GetRowIndex(int &position) const override
    {
        if (injector_.Hit("GetRowIndex", RdbFaultOp::RESULT_SET_READ)) {
            return E_SQLITE_CORRUPT;
        }
        return inner_.GetRowIndex(position);
    }

    int GoTo(int offset) override
    {
        if (injector_.Hit("GoTo", RdbFaultOp::RESULT_SET_READ)) {
            return E_SQLITE_CORRUPT;
        }
        return inner_.GoTo(offset);
    }

    int GoToRow(int position) override
    {
        if (injector_.Hit("GoToRow", RdbFaultOp::RESULT_SET_READ)) {
            return E_SQLITE_CORRUPT;
        }
        return inner_.GoToRow(position);
    }

    int GoToFirstRow() override
    {
        if (injector_.Hit("GoToFirstRow", RdbFaultOp::RESULT_SET_READ)) {
            return E_SQLITE_CORRUPT;
        }
        return inner_.GoToFirstRow();
    }

    int GoToLastRow() override
    {
        if (injector_.Hit("GoToLastRow", RdbFaultOp::RESULT_SET_READ)) {
            return E_SQLITE_CORRUPT;
        }
        return inner_.GoToLastRow();
    }

    int GoToNextRow() override
    {
        if (injector_.Hit("GoToNextRow", RdbFaultOp::RESULT_SET_READ)) {
            return E_SQLITE_CORRUPT;
        }
        return inner_.GoToNextRow();
    }

    int GoToPreviousRow() override
    {
        if (injector_.Hit("GoToPreviousRow", RdbFaultOp::RESULT_SET_READ)) {
            return E_SQLITE_CORRUPT;
        }
        return inner_.GoToPreviousRow();
    }

    int IsEnded(bool &result) override
    {
        if (injector_.Hit("IsEnded", RdbFaultOp::RESULT_SET_READ)) {
            return E_SQLITE_CORRUPT;
        }
        return inner_.IsEnded(result);
    }

    int IsStarted(bool &result) const override
    {
        if (injector_.Hit("IsStarted", RdbFaultOp::RESULT_SET_READ)) {
            return E_SQLITE_CORRUPT;
        }
        return inner_.IsStarted(result);
    }

    int IsAtFirstRow(bool &result) const override
    {
        if (injector_.Hit("IsAtFirstRow", RdbFaultOp::RESULT_SET_READ)) {
            return E_SQLITE_CORRUPT;
        }
        return inner_.IsAtFirstRow(result);
    }

    int IsAtLastRow(bool &result) override
    {
        if (injector_.Hit("IsAtLastRow", RdbFaultOp::RESULT_SET_READ)) {
            return E_SQLITE_CORRUPT;
        }
        return inner_.IsAtLastRow(result);
    }

    int GetBlob(int columnIndex, std::vector<uint8_t> &blob) override
    {
        if (injector_.Hit("GetBlob", RdbFaultOp::RESULT_SET_READ)) {
            return E_SQLITE_CORRUPT;
        }
        return inner_.GetBlob(columnIndex, blob);
    }

    int GetString(int columnIndex, std::string &value) override
    {
        if (injector_.Hit("GetString", RdbFaultOp::RESULT_SET_READ)) {
            return E_SQLITE_CORRUPT;
        }
        return inner_.GetString(columnIndex, value);
    }

    int GetInt(int columnIndex, int &value) override
    {
        if (injector_.Hit("GetInt", RdbFaultOp::RESULT_SET_READ)) {
            return E_SQLITE_CORRUPT;
        }
        return inner_.GetInt(columnIndex, value);
    }

    int GetLong(int columnIndex, int64_t &value) override
    {
        if (injector_.Hit("GetLong", RdbFaultOp::RESULT_SET_READ)) {
            return E_SQLITE_CORRUPT;
        }
        return inner_.GetLong(columnIndex, value);
    }

    int GetDouble(int columnIndex, double &value) override
    {
        if (injector_.Hit("GetDouble", RdbFaultOp::RESULT_SET_READ)) {
            return E_SQLITE_CORRUPT;
        }
        return inner_.GetDouble(columnIndex, value);
    }

    int IsColumnNull(int columnIndex, bool &isNull) override
    {
        if (injector_.Hit("IsColumnNull", RdbFaultOp::RESULT_SET_READ)) {
            return E_SQLITE_CORRUPT;
        }
        return inner_.IsColumnNull(columnIndex, isNull);
    }

    bool IsClosed() const override
    {
        if (injector_.Hit("IsClosed", RdbFaultOp::RESULT_SET_READ)) {
            return true;
        }
        return inner_.IsClosed();
    }

    int Close() override
    {
        if (injector_.CheckMethod("Close")) {
            return E_SQLITE_CORRUPT;
        }
        return inner_.Close();
    }

    int GetAsset(int32_t col, ValueObject::Asset &value) override
    {
        if (injector_.Hit("GetAsset", RdbFaultOp::RESULT_SET_READ)) {
            return E_SQLITE_CORRUPT;
        }
        return inner_.GetAsset(col, value);
    }

    int GetAssets(int32_t col, ValueObject::Assets &value) override
    {
        if (injector_.Hit("GetAssets", RdbFaultOp::RESULT_SET_READ)) {
            return E_SQLITE_CORRUPT;
        }
        return inner_.GetAssets(col, value);
    }

    int GetFloat32Array(int32_t index, ValueObject::FloatVector &vecs) override
    {
        if (injector_.Hit("GetFloat32Array", RdbFaultOp::RESULT_SET_READ)) {
            return E_SQLITE_CORRUPT;
        }
        return inner_.GetFloat32Array(index, vecs);
    }

    int Get(int32_t col, ValueObject &value) override
    {
        if (injector_.Hit("Get", RdbFaultOp::RESULT_SET_READ)) {
            return E_SQLITE_CORRUPT;
        }
        return inner_.Get(col, value);
    }

    int GetRow(RowEntity &rowEntity) override
    {
        if (injector_.Hit("GetRow", RdbFaultOp::RESULT_SET_READ)) {
            return E_SQLITE_CORRUPT;
        }
        return inner_.GetRow(rowEntity);
    }

    std::pair<int, std::vector<ValueObject>> GetRowData() override
    {
        if (injector_.Hit("GetRowData", RdbFaultOp::RESULT_SET_READ)) {
            return { E_SQLITE_CORRUPT, {} };
        }
        return inner_.GetRowData();
    }

    std::pair<int, std::vector<std::vector<ValueObject>>> GetRowsData(int32_t maxCount, int32_t position) override
    {
        if (injector_.Hit("GetRowsData", RdbFaultOp::RESULT_SET_READ)) {
            return { E_SQLITE_CORRUPT, {} };
        }
        return inner_.GetRowsData(maxCount, position);
    }

    int GetSize(int columnIndex, size_t &size) override
    {
        if (injector_.Hit("GetSize", RdbFaultOp::RESULT_SET_READ)) {
            return E_SQLITE_CORRUPT;
        }
        return inner_.GetSize(columnIndex, size);
    }

private:
    // Holder plus reference, following the configHolder_/config_ convention
    // in rdb_store_impl.h.
    std::shared_ptr<Inner> innerHolder_;
    Inner &inner_;
    std::shared_ptr<RdbFaultInjector> injectorHolder_;
    RdbFaultInjector &injector_;
};

} // namespace NativeRdb
} // namespace OHOS

#endif // NATIVE_RDB_RDB_RESULT_SET_DECORATOR_H

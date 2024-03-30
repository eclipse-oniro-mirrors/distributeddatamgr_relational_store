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

#ifndef NATIVE_RDB_ABS_RESULT_SET_H
#define NATIVE_RDB_ABS_RESULT_SET_H

#include <memory>
#include <string>
#include <vector>
#include <map>
#include <mutex>

#include "result_set.h"
#include "value_object.h"

namespace OHOS {
namespace NativeRdb {
class AbsResultSet : public ResultSet {
public:
    AbsResultSet();
    virtual ~AbsResultSet();
    int GetRowCount(int &count) override;
    int GetAllColumnNames(std::vector<std::string> &columnNames) override;
    int GetBlob(int columnIndex, std::vector<uint8_t> &blob) override;
    int GetString(int columnIndex, std::string &value) override;
    int GetInt(int columnIndex, int &value) override;
    int GetLong(int columnIndex, int64_t &value) override;
    int GetDouble(int columnIndex, double &value) override;
    int GetAsset(int32_t col, ValueObject::Asset &value) override;
    int GetAssets(int32_t col, ValueObject::Assets &value) override;
    int GetFloat32Array(int32_t index, ValueObject::FloatVector &vecs) override;
    int Get(int32_t col, ValueObject &value) override;
    int GetModifyTime(std::string &modifyTime) override;
    int IsColumnNull(int columnIndex, bool &isNull) override;
    int GetRow(RowEntity &rowEntity) override;
    int GoToRow(int position) override;
    int GetColumnType(int columnIndex, ColumnType &columnType) override;
    int GetRowIndex(int &position) const override;
    int GoTo(int offset) override;
    int GoToFirstRow() override;
    int GoToLastRow() override;
    int GoToNextRow() override;
    int GoToPreviousRow() override;
    int IsAtFirstRow(bool &result) const override;
    int IsAtLastRow(bool &result) override;
    int IsStarted(bool &result) const override;
    int IsEnded(bool &result) override;
    int GetColumnCount(int &count) override;
    int GetColumnIndex(const std::string &columnName, int &columnIndex) override;
    int GetColumnName(int columnIndex, std::string &columnName) override;
    bool IsClosed() const override;
    int Close() override;

protected:
    // The default position of the result set
    static const int INIT_POS = -1;

    // Indicates whether the result set is closed
    std::mutex columnMapLock_;
    int columnCount_ = -1;
    /*
     * The value can be in the range [-1 ~ n], where -1 represents the start flag position and N represents the data end
     * flag position, and [0, n-1] represents the real data index.
     */
    int rowPos_;
    std::map<std::string, int> columnMap_;
    bool isClosed_;
};
} // namespace NativeRdb
} // namespace OHOS

#endif
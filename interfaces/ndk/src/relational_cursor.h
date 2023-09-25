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

#ifndef RELATIONAL_CURSOR_IMPL_H
#define RELATIONAL_CURSOR_IMPL_H

#include "oh_cursor.h"
#include "result_set.h"
#include <memory>

namespace OHOS {
namespace RdbNdk {
constexpr int RDB_CURSOR_CID = 1234563; // The class id used to uniquely identify the OH_Cursor class.
class RelationalCursor : public OH_Cursor {
public:
    explicit RelationalCursor(std::shared_ptr<OHOS::NativeRdb::ResultSet> resultSet);
private:
    static int GetColumnCount(OH_Cursor *cursor, int *count);
    static int GetColumnType(OH_Cursor *cursor, int32_t columnIndex, OH_ColumnType *columnType);
    static int GetColumnIndex(OH_Cursor *cursor, const char *name, int *columnIndex);
    static int GetColumnName(OH_Cursor *cursor, int32_t columnIndex, char *name, int length);
    static int GetRowCount(OH_Cursor *cursor, int *count);
    static int GoToNextRow(OH_Cursor *cursor);
    static int GetSize(OH_Cursor *cursor, int32_t columnIndex, size_t *size);
    static int GetText(OH_Cursor *cursor, int32_t columnIndex, char *value, int length);
    static int GetInt64(OH_Cursor *cursor, int32_t columnIndex, int64_t *value);
    static int GetReal(OH_Cursor *cursor, int32_t columnIndex, double *value);
    static int GetBlob(OH_Cursor *cursor, int32_t columnIndex, unsigned char *value, int length);
    static int GetAsset(OH_Cursor *cursor, int32_t columnIndex, int assetClassId, OH_Asset *value);
    static int GetAssets(OH_Cursor *cursor, int32_t columnIndex, int assetClassId, OH_Asset *value, uint32_t *length);
    static int IsNull(OH_Cursor *cursor, int32_t columnIndex, bool *isNull);
    static int Destroy(OH_Cursor *cursor);
    static RelationalCursor *GetSelf(OH_Cursor *cursor);
    std::shared_ptr<OHOS::NativeRdb::ResultSet> resultSet_;
};
} // namespace RdbNdk
} // namespace OHOS
#endif // RELATIONAL_CURSOR_IMPL_H

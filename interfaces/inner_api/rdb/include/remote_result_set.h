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

#ifndef NATIVE_RDB_REMOTE_RESULT_SET_H
#define NATIVE_RDB_REMOTE_RESULT_SET_H

#include <string>
#include <vector>
#include "rdb_visibility.h"

namespace OHOS {
namespace NativeRdb {

/**
 * @brief Indicates the column type.
 *
 * Value returned by getColumnType(int)
 */
enum class ColumnType {
    /** Indicates the column type is NULL.*/
    TYPE_NULL = 0,
    /** Indicates the column type is INTEGER.*/
    TYPE_INTEGER,
    /** Indicates the column type is FLOAT.*/
    TYPE_FLOAT,
    /** Indicates the column type is STRING.*/
    TYPE_STRING,
    /** Indicates the column type is BLOB.*/
    TYPE_BLOB,
};

/**
 * The RemoteResultSet class of RDB.
 * Provides methods for accessing a database result set generated by remote query the database.
 */
class RDB_API_EXPORT RemoteResultSet {
public:
    /**
     * @brief The error CMD in the correct case.
     */
    enum {
        /** Indicates the current CMD is CMD_GET_ALL_COLUMN_NAMES.*/
        CMD_GET_ALL_COLUMN_NAMES,
        /** Indicates the current CMD is CMD_GET_COLUMN_COUNT.*/
        CMD_GET_COLUMN_COUNT,
        /** Indicates the current CMD is CMD_GET_COLUMN_TYPE.*/
        CMD_GET_COLUMN_TYPE,
        /** Indicates the current CMD is CMD_GET_COLUMN_INDEX.*/
        CMD_GET_COLUMN_INDEX,
        /** Indicates the current CMD is CMD_GET_COLUMN_NAME.*/
        CMD_GET_COLUMN_NAME,
        /** Indicates the current CMD is CMD_GET_ROW_COUNT.*/
        CMD_GET_ROW_COUNT,
        /** Indicates the current CMD is CMD_GET_ROW_INDEX.*/
        CMD_GET_ROW_INDEX,
        /** Indicates the current CMD is CMD_GO_TO.*/
        CMD_GO_TO,
        /** Indicates the current CMD is CMD_GO_TO_ROW.*/
        CMD_GO_TO_ROW,
        /** Indicates the current CMD is CMD_GO_TO_FIRST_ROW.*/
        CMD_GO_TO_FIRST_ROW,
        /** Indicates the current CMD is CMD_GO_TO_LAST_ROW.*/
        CMD_GO_TO_LAST_ROW,
        /** Indicates the current CMD is CMD_GO_TO_NEXT_ROW.*/
        CMD_GO_TO_NEXT_ROW,
        /** Indicates the current CMD is CMD_GO_TO_PREV_ROW.*/
        CMD_GO_TO_PREV_ROW,
        /** Indicates the current CMD is CMD_IS_ENDED_ROW.*/
        CMD_IS_ENDED_ROW,
        /** Indicates the current CMD is CMD_IS_STARTED_ROW.*/
        CMD_IS_STARTED_ROW,
        /** Indicates the current CMD is CMD_IS_AT_FIRST_ROW.*/
        CMD_IS_AT_FIRST_ROW,
        /** Indicates the current CMD is CMD_IS_AT_LAST_ROW.*/
        CMD_IS_AT_LAST_ROW,
        /** Indicates the current CMD is CMD_GET_BLOB.*/
        CMD_GET_BLOB,
        /** Indicates the current CMD is CMD_GET_STRING.*/
        CMD_GET_STRING,
        /** Indicates the current CMD is CMD_GET_INT.*/
        CMD_GET_INT,
        /** Indicates the current CMD is CMD_GET_LONG.*/
        CMD_GET_LONG,
        /** Indicates the current CMD is CMD_GET_DOUBLE.*/
        CMD_GET_DOUBLE,
        /** Indicates the current CMD is CMD_IS_COLUMN_NULL.*/
        CMD_IS_COLUMN_NULL,
        /** Indicates the current CMD is CMD_IS_CLOSED.*/
        CMD_IS_CLOSED,
        /** Indicates the current CMD is CMD_CLOSE.*/
        CMD_CLOSE,
        /** Indicates the current CMD is CMD_MAX.*/
        CMD_MAX
    };

    /**
     * @brief Destructor.
     */
    virtual ~RemoteResultSet() {}

    /**
     * @brief Obtains a string array holding the names of all of the columns in the result set.
     *
     * @return Returns the names of the columns contains in this query result.
     */
    virtual int GetAllColumnNames(std::vector<std::string> &columnNames) = 0;

    /**
     * @brief Obtains the total number of columns.
     *
     * @return Returns the number of columns
     */
    virtual int GetColumnCount(int &count) = 0;

    /**
     * @brief Obtains data type of the given column's value.
     *
     * @param columnIndex Indicates the zero-based index of the target column.
     * @return Returns column value type.
     */
    virtual int GetColumnType(int columnIndex, ColumnType &columnType) = 0;

    /**
     * @brief Obtains the zero-based index for the given column name.
     *
     * @param columnName Indicates the name of the column.
     * @return Returns the column index for the given column, or -1 if the column does not exist.
     */
    virtual int GetColumnIndex(const std::string &columnName, int &columnIndex) = 0;

    /**
     * @brief Obtains the column name at the given column index.
     *
     * @param columnIndex Indicates the zero-based index.
     * @return Returns the column name for the given index.
     */
    virtual int GetColumnName(int columnIndex, std::string &columnName) = 0;

    /**
     * @brief Obtains the numbers of rows in the result set.
     */
    virtual int GetRowCount(int &count) = 0;

    /**
     * @brief Obtains the current position of the cursor in the result set.
     *
     * The value is zero-based. When the result set is first returned the cursor
     * will be at position -1, which is before the first row.
     * After the last row is returned another call to next() will leave the cursor past
     * the last entry, at a position of count().
     *
     * @return Returns the current cursor position.
     */
    virtual int GetRowIndex(int &position) const = 0;

    /**
     * @brief Move the cursor a relative amount from current position. Positive offset move forward,
     * negative offset move backward.
     *
     * @param offset Indicates the offset to be applied from the current position.
     * @return Returns whether the requested move succeeded.
     */
    virtual int GoTo(int offset) = 0;

    /**
     * @brief Move the cursor to an absolute position.
     *
     * @param position Indicates the zero-based position to move to.
     * @return Returns whether the requested move succeeded.
     */
    virtual int GoToRow(int position) = 0;

    /**
     * @brief Move the cursor to the first row.
     *
     * @return Returns whether the requested move succeeded.
     */
    virtual int GoToFirstRow() = 0;

    /**
     * @brief Move the cursor to the last row.
     *
     * @return Returns whether the requested move succeeded.
     */
    virtual int GoToLastRow() = 0;

    /**
     * @brief Move the cursor to the next row.
     *
     * @return Returns whether the requested move succeeded.
     */
    virtual int GoToNextRow() = 0;

    /**
     * @brief Move the cursor to the previous row.
     *
     * @return Returns whether the requested move succeeded.
     */
    virtual int GoToPreviousRow() = 0;

    /**
     * @brief Obtains whether the cursor is pointing to the position after the last row.
     *
     * @return Returns whether the cursor is after the last row.
     */
    virtual int IsEnded(bool &result) = 0;

    /**
     * @brief Obtains whether the cursor is pointing to the position before the first row.
     *
     * @return Returns whether the cursor is before the first row.
     */
    virtual int IsStarted(bool &result) const = 0;

    /**
     * @brief Obtains whether the cursor is pointing to the first row.
     *
     * @return Returns whether the cursor is pointing at the first entry.
     */
    virtual int IsAtFirstRow(bool &result) const = 0;

    /**
     * @brief Obtains whether the cursor is pointing to the last row.
     *
     * @return Returns whether the cursor is pointing at the last entry.
     */
    virtual int IsAtLastRow(bool &result) = 0;

    /**
     * @brief Obtains the value of the requested column as a byte array.
     *
     * @param columnIndex Indicates the zero-based index of the target column.
     * @return Returns the value of the requested column as a byte array.
     */
    virtual int GetBlob(int columnIndex, std::vector<uint8_t> &blob) = 0;

    /**
     * @brief Obtains the value of the requested column as a String.
     *
     * @param columnIndex Indicates the zero-based index of the target column.
     * @return Returns the value of the requested column as a String.
     */
    virtual int GetString(int columnIndex, std::string &value) = 0;

    /**
     * @brief Obtains the value of the requested column as a int.
     *
     * @param columnIndex Indicates the zero-based index of the target column.
     * @return Returns the value of the requested column as a int.
     */
    virtual int GetInt(int columnIndex, int &value) = 0;

    /**
     * @brief Obtains the value of the requested column as a long.
     *
     * @param columnIndex Indicates the zero-based index of the target column.
     * @return Returns the value of the requested column as a long.
     */
    virtual int GetLong(int columnIndex, int64_t &value) = 0;

    /**
     * @brief Obtains the value of the requested column as a double.
     *
     * @param columnIndex Indicates the zero-based index of the target column.
     * @return Returns the value of the requested column as a double.
     */
    virtual int GetDouble(int columnIndex, double &value) = 0;

    /**
     * @brief Obtains Whether the value of the requested column is null.
     *
     * @param columnIndex Indicates the zero-based index of the target column.
     * @return Returns whether the column value is null.
     */
    virtual int IsColumnNull(int columnIndex, bool &isNull) = 0;

    /**
     * @brief Obtains Return true if the result set is closed.
     *
     * @return Returns true if the result set is closed.
     */
    virtual bool IsClosed() const = 0;

    /**
     * @brief Closes the result set, releasing all of its resources and making it completely invalid.
     */
    virtual int Close() = 0;
};

} // namespace NativeRdb
} // namespace OHOS
#endif

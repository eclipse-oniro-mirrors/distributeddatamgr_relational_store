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
#define LOG_TAG "SqliteStatement"
#include "sqlite_statement.h"

#include <cstdint>
#include <iomanip>
#include <memory>
#include <sstream>
#include <utility>

#include "connection_pool.h"
#include "logger.h"
#include "raw_data_parser.h"
#include "rdb_errno.h"
#include "rdb_sql_statistic.h"
#include "relational_store_client.h"
#include "remote_result_set.h"
#include "share_block.h"
#include "shared_block_serializer_info.h"
#include "sqlite3.h"
#include "sqlite3ext.h"
#include "sqlite_connection.h"
#include "sqlite_errno.h"
#include "sqlite_utils.h"

namespace OHOS {
namespace NativeRdb {
using namespace OHOS::Rdb;
using namespace std::chrono;
using SqlStatistic = DistributedRdb::SqlStatistic;
// Setting Data Precision
constexpr SqliteStatement::Action SqliteStatement::ACTIONS[ValueObject::TYPE_MAX];
SqliteStatement::SqliteStatement() : readOnly_(false), columnCount_(0), numParameters_(0), stmt_(nullptr), sql_("")
{
    seqId_ = SqlStatistic::GenerateId();
    SqlStatistic sqlStatistic("", SqlStatistic::Step::STEP_TOTAL_REF, seqId_);
}

SqliteStatement::~SqliteStatement()
{
    SqlStatistic sqlStatistic("", SqlStatistic::Step::STEP_TOTAL_RES, seqId_);
    Finalize();
    conn_ = nullptr;
}

int SqliteStatement::Prepare(sqlite3 *dbHandle, const std::string &newSql)
{
    if (sql_.compare(newSql) == 0) {
        return E_OK;
    }
    // prepare the new sqlite3_stmt
    sqlite3_stmt *stmt = nullptr;
    SqlStatistic sqlStatistic(newSql, SqlStatistic::Step::STEP_PREPARE, seqId_);
    int errCode = sqlite3_prepare_v2(dbHandle, newSql.c_str(), newSql.length(), &stmt, nullptr);
    if (errCode != SQLITE_OK) {
        if (stmt != nullptr) {
            sqlite3_finalize(stmt);
        }
        return SQLiteError::ErrNo(errCode);
    }
    Finalize(); // finalize the old
    sql_ = newSql;
    stmt_ = stmt;
    readOnly_ = (sqlite3_stmt_readonly(stmt_) != 0);
    columnCount_ = sqlite3_column_count(stmt_);
    types_ = std::vector<int32_t>(columnCount_, COLUMN_TYPE_INVALID);
    numParameters_ = sqlite3_bind_parameter_count(stmt_);
    return E_OK;
}

int SqliteStatement::BindArgs(const std::vector<ValueObject> &bindArgs)
{
    SqlStatistic sqlStatistic("", SqlStatistic::Step::STEP_PREPARE, seqId_);
    if (bound_) {
        sqlite3_reset(stmt_);
        sqlite3_clear_bindings(stmt_);
    }
    bound_ = true;
    int index = 1;
    for (auto &arg : bindArgs) {
        auto action = ACTIONS[arg.value.index()];
        if (action == nullptr) {
            LOG_ERROR("not support the type %{public}zu", arg.value.index());
            return E_INVALID_ARGS;
        }
        auto errCode = action(stmt_, index, arg.value);
        if (errCode != SQLITE_OK) {
            LOG_ERROR("Bind has error: %{public}d, sql: %{public}s, errno %{public}d", errCode, sql_.c_str(), errno);
            return SQLiteError::ErrNo(errCode);
        }
        index++;
    }

    return E_OK;
}

int SqliteStatement::IsValid(int index) const
{
    if (stmt_ == nullptr) {
        LOG_ERROR("statement already close.");
        return E_ALREADY_CLOSED;
    }

    if (index >= columnCount_ || index < 0) {
        LOG_ERROR("index (%{public}d) is out of range [0, %{public}d]", index, columnCount_ - 1);
        return E_COLUMN_OUT_RANGE;
    }
    return E_OK;
}

int SqliteStatement::Prepare(const std::string &sql)
{
    if (slave_) {
        int errCode = slave_->Prepare(sql);
        if (errCode != E_OK) {
            LOG_WARN("slave prepare Error:%{public}d", errCode);
        }
    }
    if (stmt_ == nullptr) {
        return E_ERROR;
    }
    auto db = sqlite3_db_handle(stmt_);
    return Prepare(db, sql);
}

int SqliteStatement::Bind(const std::vector<ValueObject> &args)
{
    if (slave_) {
        int errCode = slave_->Bind(args);
        if (errCode != E_OK) {
            LOG_ERROR("slave bind error:%{public}d", errCode);
        }
    }
    int count = static_cast<int>(args.size());
    std::vector<ValueObject> abindArgs;

    if (count == 0) {
        return E_OK;
    }
    // Obtains the bound parameter set.
    if ((numParameters_ != 0) && (count <= numParameters_)) {
        for (const auto &i : args) {
            abindArgs.push_back(i);
        }

        for (int i = count; i < numParameters_; i++) { // TD: when count <> numParameters
            ValueObject val;
            abindArgs.push_back(val);
        }
    }

    if (count > numParameters_) {
        LOG_ERROR("bind args count(%{public}d) > numParameters(%{public}d), sql: %{public}s", count, numParameters_,
            sql_.c_str());
        return E_INVALID_BIND_ARGS_COUNT;
    }

    return BindArgs(abindArgs);
}

int SqliteStatement::Step()
{
    SqlStatistic sqlStatistic("", SqlStatistic::Step::STEP_EXECUTE, seqId_);
    return SQLiteError::ErrNo(sqlite3_step(stmt_));
}

int SqliteStatement::Reset()
{
    if (stmt_ == nullptr) {
        return E_OK;
    }

    int errCode = sqlite3_reset(stmt_);
    if (errCode != SQLITE_OK) {
        LOG_ERROR("reset ret is %{public}d, errno is %{public}d", errCode, errno);
        return SQLiteError::ErrNo(errCode);
    }
    return E_OK;
}

int SqliteStatement::Finalize()
{
    if (stmt_ == nullptr) {
        return E_OK;
    }

    int errCode = sqlite3_finalize(stmt_);
    stmt_ = nullptr;
    sql_ = "";
    readOnly_ = false;
    columnCount_ = -1;
    numParameters_ = 0;
    types_ = std::vector<int32_t>();
    if (errCode != SQLITE_OK) {
        LOG_ERROR("finalize ret is %{public}d, errno is %{public}d", errCode, errno);
        return SQLiteError::ErrNo(errCode);
    }
    return E_OK;
}

int SqliteStatement::Execute(const std::vector<ValueObject> &args)
{
    if (slave_ && !ReadOnly()) {
        int errCode = slave_->Execute(args);
        if (errCode != E_OK) {
            LOG_ERROR("slave execute error:%{public}d", errCode);
        }
    }
    int count = static_cast<int>(args.size());
    if (count != numParameters_) {
        LOG_ERROR("bind args count(%{public}d) > numParameters(%{public}d), sql is %{public}s", count, numParameters_,
            sql_.c_str());
        return E_INVALID_BIND_ARGS_COUNT;
    }

    if (conn_ != nullptr) {
        if (!conn_->IsWriter() && !ReadOnly()) {
            return E_EXECUTE_WRITE_IN_READ_CONNECTION;
        }

        auto errCode = conn_->LimitWalSize();
        if (errCode != E_OK) {
            return errCode;
        }
    }

    auto errCode = BindArgs(args);
    if (errCode != E_OK) {
        return errCode;
    }
    errCode = Step();
    if (errCode != E_NO_MORE_ROWS && errCode != E_OK) {
        LOG_ERROR("sqlite3_step failed %{public}d, sql is %{public}s, errno %{public}d", errCode, sql_.c_str(), errno);
        return errCode;
    }
    return E_OK;
}

std::pair<int, ValueObject> SqliteStatement::ExecuteForValue(const std::vector<ValueObject> &args)
{
    auto errCode = Execute(args);
    if (errCode == E_OK) {
        return GetColumn(0);
    }
    return { errCode, ValueObject() };
}

int SqliteStatement::Changes() const
{
    if (stmt_ == nullptr) {
        return -1;
    }
    auto db = sqlite3_db_handle(stmt_);
    return sqlite3_changes(db);
}

int64_t SqliteStatement::LastInsertRowId() const
{
    if (stmt_ == nullptr) {
        return -1;
    }
    auto db = sqlite3_db_handle(stmt_);
    return sqlite3_last_insert_rowid(db);
}

int32_t SqliteStatement::GetColumnCount() const
{
    return columnCount_;
}

std::pair<int32_t, std::string> SqliteStatement::GetColumnName(int index) const
{
    int ret = IsValid(index);
    if (ret != E_OK) {
        return { ret, "" };
    }

    const char *name = sqlite3_column_name(stmt_, index);
    if (name == nullptr) {
        LOG_ERROR("column_name is null.");
        return { E_ERROR, "" };
    }
    return { E_OK, std::string(name) };
}

static int32_t Convert2ColumnType(int32_t type)
{
    switch (type) {
        case SQLITE_INTEGER:
            return int32_t(ColumnType::TYPE_INTEGER);
        case SQLITE_FLOAT:
            return int32_t(ColumnType::TYPE_FLOAT);
        case SQLITE_BLOB:
            return int32_t(ColumnType::TYPE_BLOB);
        case SQLITE_TEXT:
            return int32_t(ColumnType::TYPE_STRING);
        default:
            break;
    }
    return int32_t(ColumnType::TYPE_NULL);
}

std::pair<int32_t, int32_t> SqliteStatement::GetColumnType(int index) const
{
    int ret = IsValid(index);
    if (ret != E_OK) {
        return { ret, int32_t(ColumnType::TYPE_NULL) };
    }

    int type = sqlite3_column_type(stmt_, index);
    if (type != SQLITE_BLOB) {
        return { E_OK, Convert2ColumnType(type) };
    }

    if (types_[index] != COLUMN_TYPE_INVALID) {
        return { E_OK, types_[index] };
    }

    const char *decl = sqlite3_column_decltype(stmt_, index);
    if (decl == nullptr) {
        LOG_ERROR("invalid type %{public}d, errno %{public}d.", type, errno);
        return { E_ERROR, int32_t(ColumnType::TYPE_NULL) };
    }

    auto declType = SqliteUtils::StrToUpper(std::string(decl));
    if (declType == ValueObject::DeclType<ValueObject::Asset>()) {
        types_[index] = int32_t(ColumnType::TYPE_ASSET);
    } else if (declType == ValueObject::DeclType<ValueObject::Assets>()) {
        types_[index] = int32_t(ColumnType::TYPE_ASSETS);
    } else if (declType == ValueObject::DeclType<ValueObject::FloatVector>()) {
        types_[index] = int32_t(ColumnType::TYPE_FLOAT32_ARRAY);
    } else if (declType == ValueObject::DeclType<ValueObject::BigInt>()) {
        types_[index] = int32_t(ColumnType::TYPE_BIGINT);
    } else {
        types_[index] = int32_t(ColumnType::TYPE_BLOB);
    }

    return { E_OK, types_[index] };
}

std::pair<int32_t, size_t> SqliteStatement::GetSize(int index) const
{
    auto [errCode, type] = GetColumnType(index);
    if (errCode != E_OK) {
        return { errCode, 0 };
    }

    if (type == int32_t(ColumnType::TYPE_STRING) || type == int32_t(ColumnType::TYPE_BLOB) ||
        type == int32_t(ColumnType::TYPE_NULL)) {
        auto size = static_cast<size_t>(sqlite3_column_bytes(stmt_, index));
        return { E_OK, size };
    }
    return { E_INVALID_COLUMN_TYPE, 0 };
}

std::pair<int32_t, ValueObject> SqliteStatement::GetColumn(int index) const
{
    auto [errCode, type] = GetColumnType(index);
    if (errCode != E_OK) {
        return { errCode, ValueObject() };
    }

    switch (static_cast<ColumnType>(type)) {
        case ColumnType::TYPE_FLOAT:
            return { E_OK, ValueObject(sqlite3_column_double(stmt_, index)) };
        case ColumnType::TYPE_INTEGER:
            return { E_OK, ValueObject(static_cast<int64_t>(sqlite3_column_int64(stmt_, index))) };
        case ColumnType::TYPE_STRING: {
            int size = sqlite3_column_bytes(stmt_, index);
            auto text = reinterpret_cast<const char *>(sqlite3_column_text(stmt_, index));
            return { E_OK, ValueObject(text == nullptr ? std::string("") : std::string(text, size)) };
        }
        case ColumnType::TYPE_NULL:
            return { E_OK, ValueObject() };
        default:
            break;
    }
    return { E_OK, GetValueFromBlob(index, type) };
}

ValueObject SqliteStatement::GetValueFromBlob(int32_t index, int32_t type) const
{
    int size = sqlite3_column_bytes(stmt_, index);
    auto blob = static_cast<const uint8_t *>(sqlite3_column_blob(stmt_, index));
    if (blob == nullptr || size <= 0) {
        return ValueObject();
    }
    switch (static_cast<ColumnType>(type)) {
        case ColumnType::TYPE_ASSET: {
            Asset asset;
            RawDataParser::ParserRawData(blob, size, asset);
            return ValueObject(std::move(asset));
        }
        case ColumnType::TYPE_ASSETS: {
            Assets assets;
            RawDataParser::ParserRawData(blob, size, assets);
            return ValueObject(std::move(assets));
        }
        case ColumnType::TYPE_FLOAT32_ARRAY: {
            Floats floats;
            RawDataParser::ParserRawData(blob, size, floats);
            return ValueObject(std::move(floats));
        }
        case ColumnType::TYPE_BIGINT: {
            BigInt bigint;
            RawDataParser::ParserRawData(blob, size, bigint);
            return ValueObject(std::move(bigint));
        }
        default:
            break;
    }
    return ValueObject(std::vector<uint8_t>(blob, blob + size));
}

bool SqliteStatement::ReadOnly() const
{
    return readOnly_;
}

bool SqliteStatement::SupportBlockInfo() const
{
    auto db = sqlite3_db_handle(stmt_);
    return (sqlite3_db_config(db, SQLITE_USE_SHAREDBLOCK) == SQLITE_OK);
}

int32_t SqliteStatement::FillBlockInfo(SharedBlockInfo *info) const
{
    SqlStatistic sqlStatistic("", SqlStatistic::Step::STEP_EXECUTE, seqId_);
    if (info == nullptr) {
        return E_ERROR;
    }
    int32_t errCode = E_OK;
    if (SupportBlockInfo()) {
        errCode = FillSharedBlockOpt(info, stmt_);
    } else {
        errCode = FillSharedBlock(info, stmt_);
    }
    if (errCode != E_OK) {
        return errCode;
    }
    if (!ResetStatement(info, stmt_)) {
        LOG_ERROR("ResetStatement Failed.");
        return E_ERROR;
    }
    return E_OK;
}

int32_t SqliteStatement::BindNil(sqlite3_stmt *stat, int index, const ValueObject::Type &arg)
{
    return sqlite3_bind_null(stat, index);
}

int32_t SqliteStatement::BindInteger(sqlite3_stmt *stat, int index, const ValueObject::Type &arg)
{
    auto val = std::get_if<int64_t>(&arg);
    if (val == nullptr) {
        return SQLITE_MISMATCH;
    }
    return sqlite3_bind_int64(stat, index, *val);
}

int32_t SqliteStatement::BindDouble(sqlite3_stmt *stat, int index, const ValueObject::Type &arg)
{
    auto val = std::get_if<double>(&arg);
    if (val == nullptr) {
        return SQLITE_MISMATCH;
    }
    return sqlite3_bind_double(stat, index, *val);
}

int32_t SqliteStatement::BindText(sqlite3_stmt *stat, int index, const ValueObject::Type &arg)
{
    auto val = std::get_if<std::string>(&arg);
    if (val == nullptr) {
        return SQLITE_MISMATCH;
    }
    return sqlite3_bind_text(stat, index, val->c_str(), val->length(), SQLITE_TRANSIENT);
}

int32_t SqliteStatement::BindBool(sqlite3_stmt *stat, int index, const ValueObject::Type &arg)
{
    auto val = std::get_if<bool>(&arg);
    if (val == nullptr) {
        return SQLITE_MISMATCH;
    }
    return sqlite3_bind_int64(stat, index, *val ? 1 : 0);
}

int32_t SqliteStatement::BindBlob(sqlite3_stmt *stat, int index, const ValueObject::Type &arg)
{
    auto val = std::get_if<std::vector<uint8_t>>(&arg);
    if (val == nullptr) {
        return SQLITE_MISMATCH;
    }

    if (val->empty()) {
        return sqlite3_bind_zeroblob(stat, index, 0);
    }
    return sqlite3_bind_blob(stat, index, static_cast<const void *>((*val).data()), (*val).size(), SQLITE_TRANSIENT);
}

int32_t SqliteStatement::BindAsset(sqlite3_stmt *stat, int index, const ValueObject::Type &arg)
{
    auto val = std::get_if<Asset>(&arg);
    if (val == nullptr) {
        return SQLITE_MISMATCH;
    }
    auto rawData = RawDataParser::PackageRawData(*val);
    return sqlite3_bind_blob(stat, index, static_cast<const void *>(rawData.data()), rawData.size(), SQLITE_TRANSIENT);
}

int32_t SqliteStatement::BindAssets(sqlite3_stmt *stat, int index, const ValueObject::Type &arg)
{
    auto val = std::get_if<Assets>(&arg);
    if (val == nullptr) {
        return SQLITE_MISMATCH;
    }
    auto rawData = RawDataParser::PackageRawData(*val);
    return sqlite3_bind_blob(stat, index, static_cast<const void *>(rawData.data()), rawData.size(), SQLITE_TRANSIENT);
}

int32_t SqliteStatement::BindFloats(sqlite3_stmt *stat, int index, const ValueObject::Type &object)
{
    auto val = std::get_if<Floats>(&object);
    if (val == nullptr) {
        return SQLITE_MISMATCH;
    }
    auto rawData = RawDataParser::PackageRawData(*val);
    return sqlite3_bind_blob(stat, index, static_cast<const void *>(rawData.data()), rawData.size(), SQLITE_TRANSIENT);
}

int32_t SqliteStatement::BindBigInt(sqlite3_stmt *stat, int index, const ValueObject::Type &arg)
{
    auto val = std::get_if<BigInt>(&arg);
    if (val == nullptr) {
        return SQLITE_MISMATCH;
    }
    auto rawData = RawDataParser::PackageRawData(*val);
    return sqlite3_bind_blob(stat, index, static_cast<const void *>(rawData.data()), rawData.size(), SQLITE_TRANSIENT);
}

int SqliteStatement::ModifyLockStatus(const std::string &table, const std::vector<std::vector<uint8_t>> &hashKeys,
    bool isLock)
{
    ::DistributedDB::DBStatus ret;
    auto db = sqlite3_db_handle(stmt_);
    if (db == nullptr) {
        return E_ERROR;
    }
    if (isLock) {
        ret = Lock(table, hashKeys, db);
    } else {
        ret = UnLock(table, hashKeys, db);
    }
    if (ret == ::DistributedDB::DBStatus::OK) {
        return E_OK;
    }
    if (ret == ::DistributedDB::DBStatus::WAIT_COMPENSATED_SYNC) {
        return E_WAIT_COMPENSATED_SYNC;
    }
    if (ret == ::DistributedDB::DBStatus::NOT_FOUND) {
        return E_NO_ROW_IN_QUERY;
    }
    LOG_ERROR("Lock/Unlock failed, err is %{public}d.", ret);
    return E_ERROR;
}
} // namespace NativeRdb
} // namespace OHOS

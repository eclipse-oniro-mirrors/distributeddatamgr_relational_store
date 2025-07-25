/*
 * Copyright (c) 2021-2025 Huawei Device Co., Ltd.
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
#define LOG_TAG "ShareBlock"
#include "share_block.h"

#include <unistd.h>

#include <algorithm>

#include "logger.h"
#include "shared_block.h"
#include "shared_block_serializer_info.h"
#include "sqlite_errno.h"
#include "sqlite_utils.h"
#include "string_utils.h"
#include "value_object.h"

namespace OHOS {
namespace NativeRdb {
using namespace OHOS::Rdb;

const int ERROR_STATUS = -1;
const unsigned int SLEEP_TIME = 1000;
// move to the highest 32 bits of 64 bits number
const int RETRY_TIME = 50;
const int PRINT_RETRY_TIMES = 10;

int SeriAddRow(void *pCtx, int addedRows)
{
    auto *serializer = static_cast<SharedBlockSerializerInfo *>(pCtx);
    return serializer->AddRow(addedRows);
}

int SeriReset(void *pCtx, int startPos)
{
    auto *serializer = static_cast<SharedBlockSerializerInfo *>(pCtx);
    return serializer->Reset(startPos);
}

int SeriFinish(void *pCtx, int addedRows, int totalRows)
{
    auto *serializer = static_cast<SharedBlockSerializerInfo *>(pCtx);
    return serializer->Finish(addedRows, totalRows);
}

int SeriPutString(void *pCtx, int addedRows, int column, const char *text, int size)
{
    auto *serializer = static_cast<SharedBlockSerializerInfo *>(pCtx);
    return serializer->PutString(addedRows, column, text, size);
}

int SeriPutLong(void *pCtx, int addedRows, int column, sqlite3_int64 value)
{
    auto *serializer = static_cast<SharedBlockSerializerInfo *>(pCtx);
    return serializer->PutLong(addedRows, column, value);
}

int SeriPutDouble(void *pCtx, int addedRows, int column, double value)
{
    auto *serializer = static_cast<SharedBlockSerializerInfo *>(pCtx);
    return serializer->PutDouble(addedRows, column, value);
}

int SeriPutBlob(void *pCtx, int addedRows, int column, const void *blob, int len)
{
    auto *serializer = static_cast<SharedBlockSerializerInfo *>(pCtx);
    return serializer->PutBlob(addedRows, column, blob, len);
}

int SeriPutNull(void *pCtx, int addedRows, int column)
{
    auto *serializer = static_cast<SharedBlockSerializerInfo *>(pCtx);
    return serializer->PutNull(addedRows, column);
}

int SeriPutOther(void *pCtx, int addedRows, int column)
{
    auto *serializer = static_cast<SharedBlockSerializerInfo *>(pCtx);
    return serializer->PutOther(addedRows, column);
}

int SharedBlockSetColumnNum(AppDataFwk::SharedBlock *sharedBlock, int columnNum)
{
    int status = sharedBlock->SetColumnNum(columnNum);
    if (status != AppDataFwk::SharedBlock::SHARED_BLOCK_OK) {
        return ERROR_STATUS;
    }
    return status;
}

int FillSharedBlockOpt(SharedBlockInfo *info, sqlite3_stmt *stmt)
{
    SharedBlockSerializerInfo serializer(info->sharedBlock, stmt, info->columnNum, info->startPos);
    Sqlite3SharedBlockMethods sqliteBlock =
        (info->sharedBlock != nullptr)
            ? Sqlite3SharedBlockMethods{ 1, &serializer, info->isCountAllRows, info->startPos, info->requiredPos,
                  SeriAddRow, SeriReset, SeriFinish, SeriPutString, SeriPutLong, SeriPutDouble, SeriPutBlob,
                  SeriPutNull, SeriPutOther }
            : Sqlite3SharedBlockMethods{ 1, &serializer, true, 0, 0, DefAddRow, DefReset, DefFinish, DefPutString,
                  DefPutLong, DefPutDouble, DefPutBlob, DefPutNull, DefPutOther };

    auto db = sqlite3_db_handle(stmt);
    int cfgErr = sqlite3_db_config(db, SQLITE_DBCONFIG_SET_SHAREDBLOCK, stmt, &sqliteBlock);
    if (cfgErr != SQLITE_OK) {
        LOG_ERROR("set sqlite shared block methods error. err=%{public}d, errno=%{public}d", cfgErr, errno);
        return SQLiteError::ErrNo(cfgErr);
    }
    int retryCount = 0;
    int errCode = SQLITE_OK;
    while (true) {
        errCode = sqlite3_step(stmt);
        if (errCode == SQLITE_LOCKED || errCode == SQLITE_BUSY) {
            LOG_WARN("Database locked, retrying errCode=%{public}d, errno=%{public}d", errCode, errno);
            if (retryCount <= RETRY_TIME) {
                usleep(SLEEP_TIME);
                retryCount++;
                continue;
            }
        }
        break;
    }
    info->totalRows = serializer.GetTotalRows();
    info->startPos = serializer.GetStartPos();
    info->addedRows = serializer.GetAddedRows();
    info->isFull = serializer.IsFull();

    if (errCode == SQLITE_DONE || errCode == SQLITE_ROW) {
        errCode = SQLITE_OK;
    }

    cfgErr = sqlite3_db_config(db, SQLITE_DBCONFIG_SET_SHAREDBLOCK, stmt, nullptr);
    if (cfgErr != SQLITE_OK || (errCode != SQLITE_OK) || retryCount > PRINT_RETRY_TIMES) {
        LOG_ERROR("failed, cfgErr=%{public}d errCode=%{public}d retry=%{public}d", cfgErr, errCode, retryCount);
    }
    return SQLiteError::ErrNo(errCode);
}

int FillSharedBlock(SharedBlockInfo *info, sqlite3_stmt *stmt)
{
    int retryCount = 0;
    info->totalRows = info->addedRows = 0;
    bool isFull = false;
    bool hasException = false;
    auto fillRow = info->sharedBlock == nullptr ? DefFillRow : FillRow;
    while (!hasException && (!isFull || info->isCountAllRows)) {
        int err = sqlite3_step(stmt);
        if (err == SQLITE_ROW) {
            retryCount = 0;
            info->totalRows += 1;
            if (info->startPos >= info->totalRows || isFull) {
                continue;
            }
            fillRow(info, stmt);
            isFull = info->isFull;
            hasException = info->hasException;
        } else if (err == SQLITE_DONE) {
            LOG_WARN("Processed all rows.");
            break;
        } else if (err == SQLITE_LOCKED || err == SQLITE_BUSY) {
            LOG_WARN("Database locked, retrying");
            if (retryCount > RETRY_TIME) {
                LOG_ERROR("Bailing on database busy retry.");
                hasException = true;
                return E_DATABASE_BUSY;
            } else {
                usleep(SLEEP_TIME);
                retryCount++;
            }
        } else {
            hasException = true;
            return SQLiteError::ErrNo(err);
        }
    }
    return E_OK;
}

void FillRow(SharedBlockInfo *info, sqlite3_stmt *stmt)
{
    FillOneRowResult fillOneRowResult =
        FillOneRow(info->sharedBlock, stmt, info->columnNum, info->startPos, info->addedRows);
    if (fillOneRowResult == SHARED_BLOCK_IS_FULL && info->addedRows &&
        info->startPos + info->addedRows <= info->requiredPos) {
        info->sharedBlock->Clear();
        info->sharedBlock->SetColumnNum(info->columnNum);
        info->startPos += info->addedRows;
        info->addedRows = 0;
        fillOneRowResult = FillOneRow(info->sharedBlock, stmt, info->columnNum, info->startPos, info->addedRows);
    }

    if (fillOneRowResult == FILL_ONE_ROW_SUCESS) {
        info->addedRows += 1;
    } else if (fillOneRowResult == SHARED_BLOCK_IS_FULL) {
        info->isFull = true;
    } else {
        info->hasException = true;
    }
}

FillOneRowResult FillOneRow(
    AppDataFwk::SharedBlock *sharedBlock, sqlite3_stmt *statement, int numColumns, int startPos, int addedRows)
{
    int status = sharedBlock->AllocRow();
    if (status != AppDataFwk::SharedBlock::SHARED_BLOCK_OK) {
        LOG_ERROR("Failed allocating fieldDir at startPos %{public}d row %{public}d, error=%{public}d", startPos,
            addedRows, status);
        return SHARED_BLOCK_IS_FULL;
    }

    FillOneRowResult result = FILL_ONE_ROW_SUCESS;
    for (int i = 0; i < numColumns; i++) {
        int type = sqlite3_column_type(statement, i);
        if (type == SQLITE_TEXT) {
            // TEXT data
            result = FillOneRowOfString(sharedBlock, statement, startPos, addedRows, i);
        } else if (type == SQLITE_INTEGER) {
            // INTEGER data
            result = FillOneRowOfLong(sharedBlock, statement, startPos, addedRows, i);
        } else if (type == SQLITE_FLOAT) {
            // FLOAT data
            result = FillOneRowOfFloat(sharedBlock, statement, startPos, addedRows, i);
        } else if (type == SQLITE_BLOB) {
            // BLOB data
            result = FillOneRowOfBlob(sharedBlock, statement, startPos, addedRows, i);
        } else if (type == SQLITE_NULL) {
            // NULL field
            result = FillOneRowOfNull(sharedBlock, statement, startPos, addedRows, i);
        } else {
            // Unknown data
            LOG_ERROR("Unknown column type when filling database shared block.");
            result = FILL_ONE_ROW_FAIL;
            break;
        }

        if (result == SHARED_BLOCK_IS_FULL) {
            break;
        }
    }

    // Free the last row if if was not successfully copied.
    if (result != FILL_ONE_ROW_SUCESS) {
        sharedBlock->FreeLastRow();
    }
    return result;
}

FillOneRowResult FillOneRowOfString(
    AppDataFwk::SharedBlock *sharedBlock, sqlite3_stmt *statement, int startPos, int addedRows, int pos)
{
    const char *text = reinterpret_cast<const char *>(sqlite3_column_text(statement, pos));
    if (text == nullptr) {
        LOG_ERROR("Text is null.");
        return SHARED_BLOCK_IS_FULL;
    }

    auto sizeIncludingNull = sqlite3_column_bytes(statement, pos) + 1;
    int status = sharedBlock->PutString(addedRows, pos, text, sizeIncludingNull);
    if (status != AppDataFwk::SharedBlock::SHARED_BLOCK_OK) {
        LOG_ERROR("Failed allocating %{public}d bytes for text at %{public}d,%{public}d, error=%{public}d",
            sizeIncludingNull, startPos + addedRows, pos, status);
        return SHARED_BLOCK_IS_FULL;
    }

    return FILL_ONE_ROW_SUCESS;
}

FillOneRowResult FillOneRowOfLong(
    AppDataFwk::SharedBlock *sharedBlock, sqlite3_stmt *statement, int startPos, int addedRows, int pos)
{
    int64_t value = sqlite3_column_int64(statement, pos);
    int status = sharedBlock->PutLong(addedRows, pos, value);
    if (status != AppDataFwk::SharedBlock::SHARED_BLOCK_OK) {
        LOG_ERROR("Failed allocating space for a long in column %{public}d, error=%{public}d", pos, status);
        return SHARED_BLOCK_IS_FULL;
    }

    return FILL_ONE_ROW_SUCESS;
}

FillOneRowResult FillOneRowOfFloat(
    AppDataFwk::SharedBlock *sharedBlock, sqlite3_stmt *statement, int startPos, int addedRows, int pos)
{
    double value = sqlite3_column_double(statement, pos);
    int status = sharedBlock->PutDouble(addedRows, pos, value);
    if (status != AppDataFwk::SharedBlock::SHARED_BLOCK_OK) {
        LOG_ERROR("Failed allocating space for a double in column %{public}d, error=%{public}d", pos, status);
        return SHARED_BLOCK_IS_FULL;
    }

    return FILL_ONE_ROW_SUCESS;
}

FillOneRowResult FillOneRowOfBlob(
    AppDataFwk::SharedBlock *sharedBlock, sqlite3_stmt *statement, int startPos, int addedRows, int pos)
{
    auto action = &AppDataFwk::SharedBlock::PutBlob;
    auto *declType = sqlite3_column_decltype(statement, pos);
    if (declType != nullptr) {
        auto type = StringUtils::TruncateAfterFirstParen(SqliteUtils::StrToUpper(declType));
        action = (type == ValueObject::DeclType<ValueObject::Asset>())         ? &AppDataFwk::SharedBlock::PutAsset
                 : (type == ValueObject::DeclType<ValueObject::Assets>())      ? &AppDataFwk::SharedBlock::PutAssets
                 : (type == ValueObject::DeclType<ValueObject::FloatVector>()) ? &AppDataFwk::SharedBlock::PutFloats
                 : (type == ValueObject::DeclType<ValueObject::BigInt>())      ? &AppDataFwk::SharedBlock::PutBigInt
                                                                               : &AppDataFwk::SharedBlock::PutBlob;
    }

    const void *blob = sqlite3_column_blob(statement, pos);
    auto size = sqlite3_column_bytes(statement, pos);
    int status = (sharedBlock->*action)(addedRows, pos, blob, size);
    if (status != AppDataFwk::SharedBlock::SHARED_BLOCK_OK) {
        LOG_ERROR("Failed allocating %{public}d bytes for blob at %{public}d,%{public}d, error=%{public}d", size,
            startPos + addedRows, pos, status);
        return SHARED_BLOCK_IS_FULL;
    }

    return FILL_ONE_ROW_SUCESS;
}

FillOneRowResult FillOneRowOfNull(
    AppDataFwk::SharedBlock *sharedBlock, sqlite3_stmt *statement, int startPos, int addedRows, int pos)
{
    int status = sharedBlock->PutNull(addedRows, pos);
    if (status != AppDataFwk::SharedBlock::SHARED_BLOCK_OK) {
        LOG_ERROR("Failed allocating space for a null in column %{public}d, error=%{public}d", pos, status);
        return SHARED_BLOCK_IS_FULL;
    }
    return FILL_ONE_ROW_SUCESS;
}

bool ResetStatement(SharedBlockInfo *info, sqlite3_stmt *stmt)
{
    sqlite3_reset(stmt);
    if (info->startPos > info->totalRows) {
        LOG_ERROR("startPos %{public}d > actual rows %{public}d", info->startPos, info->totalRows);
    }

    if ((info->isFull && info->totalRows > 0 && info->addedRows == 0) && info->sharedBlock != nullptr) {
        LOG_WARN("over 2MB[%{public}d, %{public}d]", info->totalRows, info->addedRows);
        return false;
    }
    return true;
}

int DefAddRow(void *pCtx, int addedRows)
{
    return SQLITE_FULL;
}

int DefReset(void *pCtx, int startPos)
{
    return SQLITE_OK;
}

int DefFinish(void *pCtx, int addedRows, int totalRows)
{
    auto *serializer = static_cast<SharedBlockSerializerInfo *>(pCtx);
    return serializer->Finish(addedRows, totalRows);
}

int DefPutString(void *pCtx, int addedRows, int column, const char *text, int size)
{
    return SQLITE_FULL;
}

int DefPutLong(void *pCtx, int addedRows, int column, sqlite3_int64 value)
{
    return SQLITE_FULL;
}

int DefPutDouble(void *pCtx, int addedRows, int column, double value)
{
    return SQLITE_FULL;
}

int DefPutBlob(void *pCtx, int addedRows, int column, const void *blob, int len)
{
    return SQLITE_FULL;
}

int DefPutNull(void *pCtx, int addedRows, int column)
{
    return SQLITE_FULL;
}

int DefPutOther(void *pCtx, int addedRows, int column)
{
    return SQLITE_FULL;
}

void DefFillRow(SharedBlockInfo *info, sqlite3_stmt *stmt)
{
    info->isFull = true;
}
} // namespace NativeRdb
} // namespace OHOS
/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
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

#include "relational_store_ffi.h"
#include "relational_store_utils.h"
#include "napi_rdb_js_utils.h"
#include "rdb_errno.h"
#include "cj_lambda.h"
#include <cstdlib>
#include <cstdint>

using namespace OHOS::FFI;

namespace OHOS {
namespace Relational {
extern "C" {
    int64_t FfiOHOSRelationalStoreGetRdbStore(OHOS::AbilityRuntime::Context* context, StoreConfig config,
        int32_t *errCode)
    {
        return GetRdbStore(context, config, errCode);
    }

    void FfiOHOSRelationalStoreDeleteRdbStore(OHOS::AbilityRuntime::Context* context, const char* name,
        int32_t *errCode)
    {
        DeleteRdbStore(context, name, errCode);
    }

    void FfiOHOSRelationalStoreDeleteRdbStoreConfig(OHOS::AbilityRuntime::Context* context, StoreConfig config,
        int32_t *errCode)
    {
        DeleteRdbStoreConfig(context, config, errCode);
    }

    int64_t FfiOHOSRelationalStoreRdbPredicatesConstructor(const char* tableName)
    {
        auto nativeRdbPredicates = FFIData::Create<RdbPredicatesImpl>(tableName);
        if (nativeRdbPredicates == nullptr) {
            return -1;
        }
        return nativeRdbPredicates->GetID();
    }

    int32_t FfiOHOSRelationalStoreInDevices(int64_t id, const char** devicesArray, int64_t devicesSize)
    {
        auto nativeRdbPredicates = FFIData::GetData<RdbPredicatesImpl>(id);
        if (nativeRdbPredicates == nullptr) {
            return -1;
        }
        nativeRdbPredicates->InDevices(devicesArray, devicesSize);
        return 0;
    }

    int32_t FfiOHOSRelationalStoreInAllDevices(int64_t id)
    {
        auto nativeRdbPredicates = FFIData::GetData<RdbPredicatesImpl>(id);
        if (nativeRdbPredicates == nullptr) {
            return -1;
        }
        nativeRdbPredicates->InAllDevices();
        return 0;
    }

    int32_t FfiOHOSRelationalStoreBeginWrap(int64_t id)
    {
        auto nativeRdbPredicates = FFIData::GetData<RdbPredicatesImpl>(id);
        if (nativeRdbPredicates == nullptr) {
            return -1;
        }
        nativeRdbPredicates->BeginWrap();
        return 0;
    }

    int32_t FfiOHOSRelationalStoreEndWrap(int64_t id)
    {
        auto nativeRdbPredicates = FFIData::GetData<RdbPredicatesImpl>(id);
        if (nativeRdbPredicates == nullptr) {
            return -1;
        }
        nativeRdbPredicates->EndWrap();
        return 0;
    }

    int32_t FfiOHOSRelationalStoreOr(int64_t id)
    {
        auto nativeRdbPredicates = FFIData::GetData<RdbPredicatesImpl>(id);
        if (nativeRdbPredicates == nullptr) {
            return -1;
        }
        nativeRdbPredicates->Or();
        return 0;
    }

    int32_t FfiOHOSRelationalStoreAnd(int64_t id)
    {
        auto nativeRdbPredicates = FFIData::GetData<RdbPredicatesImpl>(id);
        if (nativeRdbPredicates == nullptr) {
            return -1;
        }
        nativeRdbPredicates->And();
        return 0;
    }

    int32_t FfiOHOSRelationalStoreContains(int64_t id, const char* field, const char* value)
    {
        auto nativeRdbPredicates = FFIData::GetData<RdbPredicatesImpl>(id);
        if (nativeRdbPredicates == nullptr) {
            return -1;
        }
        nativeRdbPredicates->Contains(field, value);
        return 0;
    }

    int32_t FfiOHOSRelationalStoreBeginsWith(int64_t id, const char* field, const char* value)
    {
        auto nativeRdbPredicates = FFIData::GetData<RdbPredicatesImpl>(id);
        if (nativeRdbPredicates == nullptr) {
            return -1;
        }
        nativeRdbPredicates->BeginsWith(field, value);
        return 0;
    }

    int32_t FfiOHOSRelationalStoreEndsWith(int64_t id, const char* field, const char* value)
    {
        auto nativeRdbPredicates = FFIData::GetData<RdbPredicatesImpl>(id);
        if (nativeRdbPredicates == nullptr) {
            return -1;
        }
        nativeRdbPredicates->EndsWith(field, value);
        return 0;
    }

    int32_t FfiOHOSRelationalStoreIsNull(int64_t id, const char* field)
    {
        auto nativeRdbPredicates = FFIData::GetData<RdbPredicatesImpl>(id);
        if (nativeRdbPredicates == nullptr) {
            return -1;
        }
        nativeRdbPredicates->IsNull(field);
        return 0;
    }
    
    int32_t FfiOHOSRelationalStoreIsNotNull(int64_t id, const char* field)
    {
        auto nativeRdbPredicates = FFIData::GetData<RdbPredicatesImpl>(id);
        if (nativeRdbPredicates == nullptr) {
            return -1;
        }
        nativeRdbPredicates->IsNotNull(field);
        return 0;
    }

    int32_t FfiOHOSRelationalStoreLike(int64_t id, const char* field, const char* value)
    {
        auto nativeRdbPredicates = FFIData::GetData<RdbPredicatesImpl>(id);
        if (nativeRdbPredicates == nullptr) {
            return -1;
        }
        nativeRdbPredicates->Like(field, value);
        return 0;
    }
    
    int32_t FfiOHOSRelationalStoreGlob(int64_t id, const char* field, const char* value)
    {
        auto nativeRdbPredicates = FFIData::GetData<RdbPredicatesImpl>(id);
        if (nativeRdbPredicates == nullptr) {
            return -1;
        }
        nativeRdbPredicates->Glob(field, value);
        return 0;
    }

    int32_t FfiOHOSRelationalStoreOrderByAsc(int64_t id, const char* field)
    {
        auto nativeRdbPredicates = FFIData::GetData<RdbPredicatesImpl>(id);
        if (nativeRdbPredicates == nullptr) {
            return -1;
        }
        nativeRdbPredicates->OrderByAsc(field);
        return 0;
    }

    int32_t FfiOHOSRelationalStoreOrderByDesc(int64_t id, const char* field)
    {
        auto nativeRdbPredicates = FFIData::GetData<RdbPredicatesImpl>(id);
        if (nativeRdbPredicates == nullptr) {
            return -1;
        }
        nativeRdbPredicates->OrderByDesc(field);
        return 0;
    }

    int32_t FfiOHOSRelationalStoreDistinct(int64_t id)
    {
        auto nativeRdbPredicates = FFIData::GetData<RdbPredicatesImpl>(id);
        if (nativeRdbPredicates == nullptr) {
            return -1;
        }
        nativeRdbPredicates->Distinct();
        return 0;
    }

    int32_t FfiOHOSRelationalStoreLimitAs(int64_t id, int32_t value)
    {
        auto nativeRdbPredicates = FFIData::GetData<RdbPredicatesImpl>(id);
        if (nativeRdbPredicates == nullptr) {
            return -1;
        }
        nativeRdbPredicates->LimitAs(value);
        return 0;
    }

    int32_t FfiOHOSRelationalStoreOffsetAs(int64_t id, int32_t rowOffset)
    {
        auto nativeRdbPredicates = FFIData::GetData<RdbPredicatesImpl>(id);
        if (nativeRdbPredicates == nullptr) {
            return -1;
        }
        nativeRdbPredicates->OffsetAs(rowOffset);
        return 0;
    }

    int32_t FfiOHOSRelationalStoreGroupBy(int64_t id, const char** fieldsArray, int64_t fieldsSize)
    {
        auto nativeRdbPredicates = FFIData::GetData<RdbPredicatesImpl>(id);
        if (nativeRdbPredicates == nullptr) {
            return -1;
        }
        nativeRdbPredicates->GroupBy(fieldsArray, fieldsSize);
        return 0;
    }

    int32_t FfiOHOSRelationalStoreIndexedBy(int64_t id, const char* field)
    {
        auto nativeRdbPredicates = FFIData::GetData<RdbPredicatesImpl>(id);
        if (nativeRdbPredicates == nullptr) {
            return -1;
        }
        nativeRdbPredicates->IndexedBy(field);
        return 0;
    }

    int32_t FfiOHOSRelationalStoreLessThanOrEqualTo(int64_t id, const char* field, ValueType value)
    {
        auto nativeRdbPredicates = FFIData::GetData<RdbPredicatesImpl>(id);
        if (nativeRdbPredicates == nullptr) {
            return -1;
        }
        nativeRdbPredicates->LessThanOrEqualTo(field, value);
        return 0;
    }

    int32_t FfiOHOSRelationalStoreEqualTo(int64_t id, const char* field, ValueType value)
    {
        auto nativeRdbPredicates = FFIData::GetData<RdbPredicatesImpl>(id);
        if (nativeRdbPredicates == nullptr) {
            return -1;
        }
        nativeRdbPredicates->EqualTo(field, value);
        return 0;
    }


    int32_t FfiOHOSRelationalStoreGreaterThanOrEqualTo(int64_t id, const char* field, ValueType value)
    {
        auto nativeRdbPredicates = FFIData::GetData<RdbPredicatesImpl>(id);
        if (nativeRdbPredicates == nullptr) {
            return -1;
        }
        nativeRdbPredicates->GreaterThanOrEqualTo(field, value);
        return 0;
    }

    int32_t FfiOHOSRelationalStoreGreaterThan(int64_t id, const char* field, ValueType value)
    {
        auto nativeRdbPredicates = FFIData::GetData<RdbPredicatesImpl>(id);
        if (nativeRdbPredicates == nullptr) {
            return -1;
        }
        nativeRdbPredicates->GreaterThan(field, value);
        return 0;
    }

    int32_t FfiOHOSRelationalStoreNotBetween(int64_t id, const char* field, ValueType lowValue, ValueType highValue)
    {
        auto nativeRdbPredicates = FFIData::GetData<RdbPredicatesImpl>(id);
        if (nativeRdbPredicates == nullptr) {
            return -1;
        }
        nativeRdbPredicates->NotBetween(field, lowValue, highValue);
        return 0;
    }

    int32_t FfiOHOSRelationalStoreLessThan(int64_t id, const char* field, ValueType value)
    {
        auto nativeRdbPredicates = FFIData::GetData<RdbPredicatesImpl>(id);
        if (nativeRdbPredicates == nullptr) {
            return -1;
        }
        nativeRdbPredicates->LessThan(field, value);
        return 0;
    }

    int32_t FfiOHOSRelationalStoreBetween(int64_t id, const char* field, ValueType lowValue, ValueType highValue)
    {
        auto nativeRdbPredicates = FFIData::GetData<RdbPredicatesImpl>(id);
        if (nativeRdbPredicates == nullptr) {
            return -1;
        }
        nativeRdbPredicates->Between(field, lowValue, highValue);
        return 0;
    }

    int32_t FfiOHOSRelationalStoreIn(int64_t id, const char* field, ValueType* values, int64_t valuesSize)
    {
        auto nativeRdbPredicates = FFIData::GetData<RdbPredicatesImpl>(id);
        if (nativeRdbPredicates == nullptr) {
            return -1;
        }
        nativeRdbPredicates->In(field, values, valuesSize);
        return 0;
    }

    int32_t FfiOHOSRelationalStoreNotIn(int64_t id, const char* field, ValueType* values, int64_t valuesSize)
    {
        auto nativeRdbPredicates = FFIData::GetData<RdbPredicatesImpl>(id);
        if (nativeRdbPredicates == nullptr) {
            return -1;
        }
        nativeRdbPredicates->NotIn(field, values, valuesSize);
        return 0;
    }

    int32_t FfiOHOSRelationalStoreNotEqualTo(int64_t id, const char* field, ValueType value)
    {
        auto nativeRdbPredicates = FFIData::GetData<RdbPredicatesImpl>(id);
        if (nativeRdbPredicates == nullptr) {
            return -1;
        }
        nativeRdbPredicates->NotEqualTo(field, value);
        return 0;
    }

    int64_t FfiOHOSRelationalStoreQuery(int64_t id, int64_t predicatesId, char** columns, int64_t columnsSize,
        int32_t *errCode)
    {
        auto nativeRdbStore = FFIData::GetData<RdbStoreImpl>(id);
        if (nativeRdbStore == nullptr) {
            *errCode = -1;
            return -1;
        }
        auto nativeRdbPredicates = FFIData::GetData<RdbPredicatesImpl>(predicatesId);
        if (nativeRdbPredicates == nullptr) {
            *errCode = -1;
            return -1;
        }
        auto resultSet = nativeRdbStore->Query(*nativeRdbPredicates, columns, columnsSize);
        if (resultSet == nullptr) {
            *errCode = RelationalStoreJsKit::E_INNER_ERROR;
            return -1;
        } else {
            *errCode = RelationalStoreJsKit::OK;
        }
        auto nativeResultSet = FFIData::Create<ResultSetImpl>(resultSet);
        if (nativeResultSet == nullptr) {
            *errCode = -1;
            return -1;
        }
        return nativeResultSet->GetID();
    }

    int64_t FfiOHOSRelationalStoreRemoteQuery(int64_t id, char* device, int64_t predicatesId, char** columns,
        int64_t columnsSize)
    {
        auto nativeRdbStore = FFIData::GetData<RdbStoreImpl>(id);
        if (nativeRdbStore == nullptr) {
            return -1;
        }
        auto nativeRdbPredicates = FFIData::GetData<RdbPredicatesImpl>(predicatesId);
        if (nativeRdbPredicates == nullptr) {
            return -1;
        }
        auto resultSet = nativeRdbStore->RemoteQuery(device, *nativeRdbPredicates, columns, columnsSize);
        if (resultSet == nullptr) {
            return -1;
        }
        auto nativeResultSet = FFIData::Create<ResultSetImpl>(resultSet);
        if (nativeResultSet == nullptr) {
            return -1;
        }
        return nativeResultSet->GetID();
    }

    int64_t FfiOHOSRelationalStoreDelete(int64_t id, int64_t predicatesId, int32_t *errCode)
    {
        auto nativeRdbStore = FFIData::GetData<RdbStoreImpl>(id);
        if (nativeRdbStore == nullptr) {
            *errCode = -1;
            return -1;
        }
        auto nativeRdbPredicates = FFIData::GetData<RdbPredicatesImpl>(predicatesId);
        if (nativeRdbPredicates == nullptr) {
            *errCode = -1;
            return -1;
        }
        return nativeRdbStore->Delete(*nativeRdbPredicates, errCode);
    }

    int32_t FfiOHOSRelationalStoreSetDistributedTables(int64_t id, char** tables, int64_t tablesSize)
    {
        auto nativeRdbStore = FFIData::GetData<RdbStoreImpl>(id);
        if (nativeRdbStore == nullptr) {
            return -1;
        }
        nativeRdbStore->SetDistributedTables(tables, tablesSize);
        return 0;
    }

    int32_t FfiOHOSRelationalStoreSetDistributedTablesType(int64_t id, char** tables, int64_t tablesSize, int32_t type)
    {
        auto nativeRdbStore = FFIData::GetData<RdbStoreImpl>(id);
        if (nativeRdbStore == nullptr) {
            return -1;
        }
        nativeRdbStore->SetDistributedTables(tables, tablesSize, type);
        return 0;
    }

    int32_t FfiOHOSRelationalStoreSetDistributedTablesConfig(int64_t id, char** tables, int64_t tablesSize,
        int32_t type, DistributedRdb::DistributedConfig &distributedConfig)
    {
        auto nativeRdbStore = FFIData::GetData<RdbStoreImpl>(id);
        if (nativeRdbStore == nullptr) {
            return -1;
        }
        nativeRdbStore->SetDistributedTables(tables, tablesSize, type, distributedConfig);
        return 0;
    }

    char* FfiOHOSRelationalStoreObtainDistributedTableName(int64_t id, const char* device, char* table)
    {
        auto nativeRdbStore = FFIData::GetData<RdbStoreImpl>(id);
        if (nativeRdbStore == nullptr) {
            return nullptr;
        }
        return nativeRdbStore->ObtainDistributedTableName(device, table);
    }

    int32_t FfiOHOSRelationalStoreBackUp(int64_t id, const char* destName)
    {
        auto nativeRdbStore = FFIData::GetData<RdbStoreImpl>(id);
        if (nativeRdbStore == nullptr) {
            return -1;
        }
        return nativeRdbStore->Backup(destName);
    }

    int32_t FfiOHOSRelationalStoreReStore(int64_t id, const char* srcName)
    {
        auto nativeRdbStore = FFIData::GetData<RdbStoreImpl>(id);
        if (nativeRdbStore == nullptr) {
            return -1;
        }
        return nativeRdbStore->Restore(srcName);
    }

    int32_t FfiOHOSRelationalStoreCommit(int64_t id)
    {
        auto nativeRdbStore = FFIData::GetData<RdbStoreImpl>(id);
        if (nativeRdbStore == nullptr) {
            return -1;
        }
        return nativeRdbStore->Commit();
    }

    int32_t FfiOHOSRelationalStoreRollBack(int64_t id)
    {
        auto nativeRdbStore = FFIData::GetData<RdbStoreImpl>(id);
        if (nativeRdbStore == nullptr) {
            return -1;
        }
        return nativeRdbStore->RollBack();
    }

    int32_t FfiOHOSRelationalStoreBeginTransaction(int64_t id)
    {
        auto nativeRdbStore = FFIData::GetData<RdbStoreImpl>(id);
        if (nativeRdbStore == nullptr) {
            return -1;
        }
        return nativeRdbStore->BeginTransaction();
    }

    int64_t FfiOHOSRelationalStoreInsert(int64_t id, const char* table, ValuesBucket valuesBucket, int32_t conflict,
        int32_t *errCode)
    {
        auto nativeRdbStore = FFIData::GetData<RdbStoreImpl>(id);
        if (nativeRdbStore == nullptr) {
            *errCode = -1;
            return -1;
        }
        return nativeRdbStore->Insert(table, valuesBucket, conflict, errCode);
    }

    int64_t FfiOHOSRelationalStoreUpdate(int64_t id, ValuesBucket valuesBucket, int64_t predicatesId,
        NativeRdb::ConflictResolution conflictResolution, int32_t *errCode)
    {
        auto nativeRdbStore = FFIData::GetData<RdbStoreImpl>(id);
        auto nativeRdbSPredicates = FFIData::GetData<RdbPredicatesImpl>(predicatesId);
        if (nativeRdbStore == nullptr || nativeRdbSPredicates == nullptr) {
            *errCode = -1;
            return -1;
        }
        return nativeRdbStore->Update(valuesBucket, *nativeRdbSPredicates, conflictResolution, errCode);
    }

    void FfiOHOSRelationalStoreExecuteSql(int64_t id, const char* sql, int32_t *errCode)
    {
        auto nativeRdbStore = FFIData::GetData<RdbStoreImpl>(id);
        if (nativeRdbStore == nullptr) {
            *errCode = -1;
            return;
        }
        nativeRdbStore->ExecuteSql(sql, errCode);
    }

    CArrSyncResult FfiOHOSRelationalStoreSync(int64_t id, int32_t mode, int64_t predicatesId, int32_t *errCode)
    {
        auto nativeRdbStore = FFIData::GetData<RdbStoreImpl>(id);
        auto nativeRdbPredicates = FFIData::GetData<RdbPredicatesImpl>(predicatesId);
        if (nativeRdbStore == nullptr || nativeRdbPredicates == nullptr) {
            *errCode = -1;
            return CArrSyncResult{nullptr, nullptr, 0};
        }
        return nativeRdbStore->Sync(mode, *nativeRdbPredicates);
    }

    CArrStr FfiOHOSRelationalStoreGetAllColumnNames(int64_t id)
    {
        auto nativeResultSet = FFIData::GetData<ResultSetImpl>(id);
        if (nativeResultSet == nullptr) {
            return CArrStr{nullptr, 0};
        }
        return nativeResultSet->GetAllColumnNames();
    }

    int32_t FfiOHOSRelationalStoreGetColumnCount(int64_t id, int32_t *errCode)
    {
        auto nativeResultSet = FFIData::GetData<ResultSetImpl>(id);
        if (nativeResultSet == nullptr) {
            *errCode = -1;
            return -1;
        }
        return nativeResultSet->GetColumnCount();
    }

    int32_t FfiOHOSRelationalStoreGetRowCount(int64_t id, int32_t *errCode)
    {
        auto nativeResultSet = FFIData::GetData<ResultSetImpl>(id);
        if (nativeResultSet == nullptr) {
            *errCode = -1;
            return -1;
        }
        return nativeResultSet->GetRowCount();
    }

    int32_t FfiOHOSRelationalStoreGetRowIndex(int64_t id, int32_t *errCode)
    {
        auto nativeResultSet = FFIData::GetData<ResultSetImpl>(id);
        if (nativeResultSet == nullptr) {
            *errCode = -1;
            return -1;
        }
        return nativeResultSet->GetRowIndex();
    }

    bool FfiOHOSRelationalStoreIsAtFirstRow(int64_t id, int32_t *errCode)
    {
        auto nativeResultSet = FFIData::GetData<ResultSetImpl>(id);
        if (nativeResultSet == nullptr) {
            *errCode = -1;
            return false;
        }
        *errCode = 0;
        return nativeResultSet->IsAtFirstRow();
    }

    bool FfiOHOSRelationalStoreIsAtLastRow(int64_t id, int32_t *errCode)
    {
        auto nativeResultSet = FFIData::GetData<ResultSetImpl>(id);
        if (nativeResultSet == nullptr) {
            *errCode = -1;
            return false;
        }
        *errCode = 0;
        return nativeResultSet->IsAtLastRow();
    }

    bool FfiOHOSRelationalStoreIsEnded(int64_t id, int32_t *errCode)
    {
        auto nativeResultSet = FFIData::GetData<ResultSetImpl>(id);
        if (nativeResultSet == nullptr) {
            *errCode = -1;
            return false;
        }
        *errCode = 0;
        return nativeResultSet->IsEnded();
    }

    bool FfiOHOSRelationalStoreIsStarted(int64_t id, int32_t *errCode)
    {
        auto nativeResultSet = FFIData::GetData<ResultSetImpl>(id);
        if (nativeResultSet == nullptr) {
            *errCode = -1;
            return false;
        }
        *errCode = 0;
        return nativeResultSet->IsStarted();
    }

    bool FfiOHOSRelationalStoreIsClosed(int64_t id, int32_t *errCode)
    {
        auto nativeResultSet = FFIData::GetData<ResultSetImpl>(id);
        if (nativeResultSet == nullptr) {
            *errCode = -1;
            return false;
        }
        *errCode = 0;
        return nativeResultSet->IsClosed();
    }


    double FfiOHOSRelationalStoreGetDouble(int64_t id, int32_t columnIndex, int32_t* rtnCode)
    {
        auto nativeResultSet = FFIData::GetData<ResultSetImpl>(id);
        if (nativeResultSet == nullptr) {
            *rtnCode = -1;
            return -1;
        }
        return nativeResultSet->GetDouble(columnIndex, rtnCode);
    }

    bool FfiOHOSRelationalStoreGoToRow(int64_t id, int32_t position, int32_t* rtnCode)
    {
        auto nativeResultSet = FFIData::GetData<ResultSetImpl>(id);
        if (nativeResultSet == nullptr) {
            *rtnCode = -1;
            return false;
        }
        return nativeResultSet->GoToRow(position, rtnCode);
    }

    bool FfiOHOSRelationalStoreGoToPreviousRow(int64_t id, int32_t* rtnCode)
    {
        auto nativeResultSet = FFIData::GetData<ResultSetImpl>(id);
        if (nativeResultSet == nullptr) {
            *rtnCode = -1;
            return false;
        }
        return nativeResultSet->GoToPreviousRow(rtnCode);
    }

    bool FfiOHOSRelationalStoreGoToLastRow(int64_t id, int32_t* rtnCode)
    {
        auto nativeResultSet = FFIData::GetData<ResultSetImpl>(id);
        if (nativeResultSet == nullptr) {
            *rtnCode = -1;
            return false;
        }
        return nativeResultSet->GoToLastRow(rtnCode);
    }

    char* FfiOHOSRelationalStoreGetColumnName(int64_t id, int32_t columnIndex, int32_t* rtnCode)
    {
        auto nativeResultSet = FFIData::GetData<ResultSetImpl>(id);
        if (nativeResultSet == nullptr) {
            *rtnCode = -1;
            return nullptr;
        }
        return nativeResultSet->GetColumnName(columnIndex, rtnCode);
    }

    bool FfiOHOSRelationalStoreIsColumnNull(int64_t id, int32_t columnIndex, int32_t* rtnCode)
    {
        auto nativeResultSet = FFIData::GetData<ResultSetImpl>(id);
        if (nativeResultSet == nullptr) {
            *rtnCode = -1;
            return false;
        }
        return nativeResultSet->IsColumnNull(columnIndex, rtnCode);
    }

    Asset FfiOHOSRelationalStoreGetAsset(int64_t id, int32_t columnIndex, int32_t* rtnCode)
    {
        auto nativeResultSet = FFIData::GetData<ResultSetImpl>(id);
        if (nativeResultSet == nullptr) {
            *rtnCode = -1;
            return Asset{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, 0};
        }
        return nativeResultSet->GetAsset(columnIndex, rtnCode);
    }

    int32_t FfiOHOSRelationalStoreClose(int64_t id)
    {
        auto nativeResultSet = FFIData::GetData<ResultSetImpl>(id);
        if (nativeResultSet == nullptr) {
            return -1;
        }
        return nativeResultSet->Close();
    }

    int32_t FfiOHOSRelationalStoreGetColumnIndex(int64_t id, char* columnName, int32_t* rtnCode)
    {
        auto nativeResultSet = FFIData::GetData<ResultSetImpl>(id);
        if (nativeResultSet == nullptr) {
            *rtnCode = -1;
            return -1;
        }
        return nativeResultSet->GetColumnIndex(columnName, rtnCode);
    }

    char* FfiOHOSRelationalStoreGetString(int64_t id, int32_t columnIndex, int32_t* rtnCode)
    {
        auto nativeResultSet = FFIData::GetData<ResultSetImpl>(id);
        if (nativeResultSet == nullptr) {
            *rtnCode = -1;
            return nullptr;
        }
        return nativeResultSet->GetString(columnIndex, rtnCode);
    }

    bool FfiOHOSRelationalStoreGoToFirstRow(int64_t id, int32_t* rtnCode)
    {
        auto nativeResultSet = FFIData::GetData<ResultSetImpl>(id);
        if (nativeResultSet == nullptr) {
            *rtnCode = -1;
            return false;
        }
        return nativeResultSet->GoToFirstRow(rtnCode);
    }

    int64_t FfiOHOSRelationalStoreGetLong(int64_t id, int32_t columnIndex, int32_t* rtnCode)
    {
        auto nativeResultSet = FFIData::GetData<ResultSetImpl>(id);
        if (nativeResultSet == nullptr) {
            *rtnCode = -1;
            return -1;
        }
        return nativeResultSet->GetLong(columnIndex, rtnCode);
    }

    bool FfiOHOSRelationalStoreGoToNextRow(int64_t id, int32_t* rtnCode)
    {
        auto nativeResultSet = FFIData::GetData<ResultSetImpl>(id);
        if (nativeResultSet == nullptr) {
            *rtnCode = -1;
            return false;
        }
        return nativeResultSet->GoToNextRow(rtnCode);
    }

    CArrUI8 FfiOHOSRelationalStoreGetBlob(int64_t id, int32_t columnIndex, int32_t* rtnCode)
    {
        auto nativeResultSet = FFIData::GetData<ResultSetImpl>(id);
        if (nativeResultSet == nullptr) {
            *rtnCode = -1;
            return CArrUI8{nullptr, 0};
        }
        return nativeResultSet->GetBlob(columnIndex, rtnCode);
    }

    bool FfiOHOSRelationalStoreGoTo(int64_t id, int32_t offset, int32_t* rtnCode)
    {
        auto nativeResultSet = FFIData::GetData<ResultSetImpl>(id);
        if (nativeResultSet == nullptr) {
            *rtnCode = -1;
            return false;
        }
        return nativeResultSet->GoTo(offset, rtnCode);
    }

    Assets FfiOHOSRelationalStoreGetAssets(int64_t id, int32_t columnIndex, int32_t* rtnCode)
    {
        auto nativeResultSet = FFIData::GetData<ResultSetImpl>(id);
        if (nativeResultSet == nullptr) {
            *rtnCode = -1;
            return Assets{nullptr, 0};
        }
        return nativeResultSet->GetAssets(columnIndex, rtnCode);
    }

    ValuesBucket FfiOHOSRelationalStoreGetRow(int64_t id, int32_t *errCode)
    {
        auto nativeResultSet = FFIData::GetData<ResultSetImpl>(id);
        if (nativeResultSet == nullptr) {
            *errCode = -1;
            return ValuesBucket{nullptr, nullptr, 0};
        }
        return nativeResultSet->GetRow(errCode);
    }

    int32_t FfiOHOSRelationalStoreCleanDirtyData(int64_t id, const char* tableName, uint64_t cursor)
    {
        auto nativeRdbStore = FFIData::GetData<RdbStoreImpl>(id);
        if (nativeRdbStore == nullptr) {
            return -1;
        }
        return nativeRdbStore->CleanDirtyData(tableName, cursor);
    }

    int32_t FfiOHOSRelationalStoreBatchInsert(int64_t id, const char* tableName, ValuesBucket* values,
        int64_t valuesSize, int64_t* insertNum)
    {
        auto nativeRdbStore = FFIData::GetData<RdbStoreImpl>(id);
        if (nativeRdbStore == nullptr) {
            return -1;
        }
        return nativeRdbStore->BatchInsert(*insertNum, tableName, values, valuesSize);
    }

    int64_t FfiOHOSRelationalStoreQuerySql(int64_t id, const char *sql, ValueType *bindArgs, int64_t size,
        int32_t* errCode)
    {
        auto nativeRdbStore = FFIData::GetData<RdbStoreImpl>(id);
        if (nativeRdbStore == nullptr) {
            *errCode = RelationalStoreJsKit::E_INNER_ERROR;
            return -1;
        }
        auto resultSet = nativeRdbStore->QuerySql(sql, bindArgs, size);
        if (resultSet == nullptr) {
            *errCode = RelationalStoreJsKit::E_INNER_ERROR;
            return -1;
        } else {
            *errCode = RelationalStoreJsKit::OK;
        }
        auto nativeResultSet = FFIData::Create<ResultSetImpl>(resultSet);
        if (nativeResultSet == nullptr) {
            *errCode = -1;
            return -1;
        }
        return nativeResultSet->GetID();
    }

    void FfiOHOSRelationalStoreExecuteSqlBindArgs(int64_t id, char* sql, ValueType* bindArgs, int64_t bindArgsSize,
        int32_t *errCode)
    {
        auto nativeRdbStore = FFIData::GetData<RdbStoreImpl>(id);
        if (nativeRdbStore == nullptr) {
            *errCode = -1;
            return;
        }
        nativeRdbStore->ExecuteSql(sql, bindArgs, bindArgsSize, errCode);
    }

    int32_t FfiOHOSRelationalStoreOn(int64_t id, const char *event, bool interProcess,
        void (*callback)(), void (*callbackRef)())
    {
        auto nativeRdbStore = FFIData::GetData<RdbStoreImpl>(id);
        if (nativeRdbStore == nullptr) {
            return -1;
        }
        auto onChange = [lambda = CJLambda::Create(callbackRef)]() -> void { lambda(); };
        return nativeRdbStore->RegisterObserver(event, interProcess, (std::function<void()>*)(callback), onChange);
    }

    int32_t FfiOHOSRelationalStoreOff(int64_t id, const char *event, bool interProcess, void (*callback)())
    {
        auto nativeRdbStore = FFIData::GetData<RdbStoreImpl>(id);
        if (nativeRdbStore == nullptr) {
            return -1;
        }
        return nativeRdbStore->UnRegisterObserver(event, interProcess, (std::function<void()>*)(callback));
    }

    int32_t FfiOHOSRelationalStoreOffAll(int64_t id, const char *event, bool interProcess)
    {
        auto nativeRdbStore = FFIData::GetData<RdbStoreImpl>(id);
        if (nativeRdbStore == nullptr) {
            return -1;
        }
        return nativeRdbStore->UnRegisterAllObserver(event, interProcess);
    }

    int32_t FfiOHOSRelationalStoreEmit(int64_t id, const char *event)
    {
        auto nativeRdbStore = FFIData::GetData<RdbStoreImpl>(id);
        if (nativeRdbStore == nullptr) {
            return -1;
        }
        return nativeRdbStore->Emit(event);
    }
}
}
}

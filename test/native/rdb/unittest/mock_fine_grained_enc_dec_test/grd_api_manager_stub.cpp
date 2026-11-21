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

// Drop-in replacement for frameworks/native/rd/src/grd_api_manager.cpp in the
// MockFineGrainedEncDecTest target. The test binary must never dlopen the real
// GaussPD library, so this stub reports ArkData as available and returns a fully
// populated GRD_APIInfo table built from in-process mock functions. Captured
// arguments/counters are exposed through plain free functions (declared with
// matching extern declarations in the test sources; no shared header is added).
#include "grd_api_manager.h"

#include <cstdint>
#include <string>

#include "grd_error.h"

namespace OHOS {
namespace NativeRdb {
namespace {
constexpr std::uintptr_t FAKE_DB_HANDLE_VALUE = 0x12345678U;

int32_t g_openCallCount = 0;
std::string g_lastConfigStr;
int32_t g_addPolicyCallCount = 0;
std::string g_lastAddPolicy;
int32_t g_removePolicyCallCount = 0;
std::string g_lastRemovePolicy;
int32_t g_screenLockStateCallCount = 0;
GRD_ScreenLockState g_lastScreenLockState = GRD_SCREEN_UNLOCK;
int32_t g_deviceHealthyStateCallCount = 0;
GRD_DeviceHealthyState g_lastDeviceHealthyState = GRD_DEVICE_UNROOT;

GRD_DB *FakeDbHandle()
{
    return reinterpret_cast<GRD_DB *>(static_cast<std::uintptr_t>(FAKE_DB_HANDLE_VALUE));
}

int32_t StubDBOpen(const char *dbPath, const char *configStr, uint32_t flags, GRD_DB **db)
{
    (void)dbPath;
    (void)flags;
    g_openCallCount += 1;
    g_lastConfigStr = (configStr == nullptr) ? "" : configStr;
    if (db != nullptr) {
        *db = FakeDbHandle();
    }
    return GRD_OK;
}

int32_t StubDBClose(GRD_DB *, uint32_t)
{
    return GRD_OK;
}

int32_t StubDBRepair(const char *, const char *)
{
    return GRD_OK;
}

int32_t StubDBSqlPrepare(GRD_DB *, const char *, uint32_t, GRD_SqlStmt **, const char **)
{
    return GRD_OK;
}

int32_t StubDBSqlReset(GRD_SqlStmt *)
{
    return GRD_OK;
}

int32_t StubDBSqlFinalize(GRD_SqlStmt *)
{
    return GRD_OK;
}

int32_t StubDBSqlBindBlob(GRD_SqlStmt *, uint32_t, const void *, int32_t, void (*)(void *))
{
    return GRD_OK;
}

int32_t StubDBSqlBindText(GRD_SqlStmt *, uint32_t, const void *, int32_t, void (*)(void *))
{
    return GRD_OK;
}

int32_t StubDBSqlBindInt(GRD_SqlStmt *, uint32_t, int32_t)
{
    return GRD_OK;
}

int32_t StubDBSqlBindInt64(GRD_SqlStmt *, uint32_t, int64_t)
{
    return GRD_OK;
}

int32_t StubDBSqlBindDouble(GRD_SqlStmt *, uint32_t, double)
{
    return GRD_OK;
}

int32_t StubDBSqlBindNull(GRD_SqlStmt *, uint32_t)
{
    return GRD_OK;
}

int32_t StubDBSqlBindFloatVector(GRD_SqlStmt *, uint32_t, const float *, uint32_t, void (*)(void *))
{
    return GRD_OK;
}

int32_t StubDBSqlStep(GRD_SqlStmt *)
{
    // GRD_NO_DATA is mapped to E_NO_MORE_ROWS by RdUtils::TransferGrdErrno, so
    // result-set traversal of the internal version query terminates normally.
    return GRD_NO_DATA;
}

uint32_t StubDBSqlColCnt(GRD_SqlStmt *)
{
    return 0;
}

GRD_DbDataTypeE StubDBSqlColType(GRD_SqlStmt *, uint32_t)
{
    return GRD_DB_DATATYPE_INTEGER;
}

uint32_t StubDBSqlColBytes(GRD_SqlStmt *, uint32_t)
{
    return 0;
}

char *StubDBSqlColName(GRD_SqlStmt *, uint32_t)
{
    return nullptr;
}

GRD_DbValueT StubDBSqlColValue(GRD_SqlStmt *, uint32_t)
{
    GRD_DbValueT value = {};
    return value;
}

const void *StubDBSqlColBlob(GRD_SqlStmt *, uint32_t)
{
    return nullptr;
}

const char *StubDBSqlColText(GRD_SqlStmt *, uint32_t)
{
    return nullptr;
}

int32_t StubDBSqlColInt(GRD_SqlStmt *, uint32_t)
{
    return 0;
}

int64_t StubDBSqlColInt64(GRD_SqlStmt *, uint32_t)
{
    return 1;
}

double StubDBSqlColDouble(GRD_SqlStmt *, uint32_t)
{
    return 0;
}

const float *StubDBSqlColumnFloatVector(GRD_SqlStmt *, uint32_t, uint32_t *dim)
{
    if (dim != nullptr) {
        *dim = 0;
    }
    return nullptr;
}

int32_t StubDBBackup(GRD_DB *, const char *, GRD_CipherInfoT *)
{
    return GRD_OK;
}

int32_t StubDBRestore(const char *, const char *, GRD_CipherInfoT *)
{
    return GRD_OK;
}

int32_t StubDBReKey(const char *, const char *, GRD_CipherInfoT *)
{
    return GRD_OK;
}

GRD_DbValueT StubDBGetConfig(GRD_DB *, GRD_ConfigTypeE)
{
    // The open flow reads the DB version through GRD_CONFIG_USER_VERSION; report
    // version 1 so GetRdbStore(version=1) takes the OnOpen branch.
    GRD_DbValueT value = {};
    value.type = GRD_DB_DATATYPE_INTEGER;
    value.value.longValue = 1;
    return value;
}

int32_t StubDBSetConfig(GRD_DB *, GRD_ConfigTypeE, GRD_DbValueT)
{
    return GRD_OK;
}

int32_t StubDBSqlRegistryThreadPool(GRD_DB *, GRD_DbThreadPoolT *)
{
    return GRD_OK;
}

int32_t StubDBSqlRegistryClusterAlgo(GRD_DB *, const char *, GRD_ClusterAlgoFunc)
{
    return GRD_OK;
}

int32_t StubDBSetScreenLockState(GRD_ScreenLockState state)
{
    g_screenLockStateCallCount += 1;
    g_lastScreenLockState = state;
    return GRD_OK;
}

int32_t StubDBSetDeviceHealthyState(GRD_DeviceHealthyState state)
{
    g_deviceHealthyStateCallCount += 1;
    g_lastDeviceHealthyState = state;
    return GRD_OK;
}

int32_t StubDBAddPolicy(GRD_DB *, const char *policy)
{
    g_addPolicyCallCount += 1;
    g_lastAddPolicy = (policy == nullptr) ? "" : policy;
    return GRD_OK;
}

int32_t StubDBRemovePolicy(GRD_DB *, const char *policyName)
{
    g_removePolicyCallCount += 1;
    g_lastRemovePolicy = (policyName == nullptr) ? "" : policyName;
    return GRD_OK;
}
} // namespace

// Capture accessors consumed by the tests through matching `extern` declarations.
int32_t GetStubOpenCallCount();
std::string GetStubLastConfigStr();
int32_t GetStubAddPolicyCallCount();
std::string GetStubLastAddPolicy();
int32_t GetStubRemovePolicyCallCount();
std::string GetStubLastRemovePolicy();
int32_t GetStubScreenLockStateCallCount();
GRD_ScreenLockState GetStubLastScreenLockState();
int32_t GetStubDeviceHealthyStateCallCount();
GRD_DeviceHealthyState GetStubLastDeviceHealthyState();
void ResetStubState();

bool IsUsingArkData()
{
    return true;
}

GRD_APIInfo GetApiInfoInstance()
{
    GRD_APIInfo apiInfo;
    apiInfo.DBOpenApi = StubDBOpen;
    apiInfo.DBCloseApi = StubDBClose;
    apiInfo.DBRepairApi = StubDBRepair;
    apiInfo.DBSqlPrepare = StubDBSqlPrepare;
    apiInfo.DBSqlReset = StubDBSqlReset;
    apiInfo.DBSqlFinalize = StubDBSqlFinalize;
    apiInfo.DBSqlBindBlob = StubDBSqlBindBlob;
    apiInfo.DBSqlBindText = StubDBSqlBindText;
    apiInfo.DBSqlBindInt = StubDBSqlBindInt;
    apiInfo.DBSqlBindInt64 = StubDBSqlBindInt64;
    apiInfo.DBSqlBindDouble = StubDBSqlBindDouble;
    apiInfo.DBSqlBindNull = StubDBSqlBindNull;
    apiInfo.DBSqlBindFloatVector = StubDBSqlBindFloatVector;
    apiInfo.DBSqlStep = StubDBSqlStep;
    apiInfo.DBSqlColCnt = StubDBSqlColCnt;
    apiInfo.DBSqlColType = StubDBSqlColType;
    apiInfo.DBSqlColBytes = StubDBSqlColBytes;
    apiInfo.DBSqlColName = StubDBSqlColName;
    apiInfo.DBSqlColValue = StubDBSqlColValue;
    apiInfo.DBSqlColBlob = StubDBSqlColBlob;
    apiInfo.DBSqlColText = StubDBSqlColText;
    apiInfo.DBSqlColInt = StubDBSqlColInt;
    apiInfo.DBSqlColInt64 = StubDBSqlColInt64;
    apiInfo.DBSqlColDouble = StubDBSqlColDouble;
    apiInfo.DBSqlColumnFloatVector = StubDBSqlColumnFloatVector;
    apiInfo.DBBackupApi = StubDBBackup;
    apiInfo.DBRestoreApi = StubDBRestore;
    apiInfo.DBReKeyApi = StubDBReKey;
    apiInfo.DBGetConfigApi = StubDBGetConfig;
    apiInfo.DBSetConfigApi = StubDBSetConfig;
    apiInfo.DBSqlRegistryThreadPool = StubDBSqlRegistryThreadPool;
    apiInfo.DBSqlRegistryClusterAlgo = StubDBSqlRegistryClusterAlgo;
    apiInfo.DBSetScreenLockStateApi = StubDBSetScreenLockState;
    apiInfo.DBSetDeviceHealthyStateApi = StubDBSetDeviceHealthyState;
    apiInfo.DBAddPolicyApi = StubDBAddPolicy;
    apiInfo.DBRemovePolicyApi = StubDBRemovePolicy;
    return apiInfo;
}

int32_t GetStubOpenCallCount()
{
    return g_openCallCount;
}

std::string GetStubLastConfigStr()
{
    return g_lastConfigStr;
}

int32_t GetStubAddPolicyCallCount()
{
    return g_addPolicyCallCount;
}

std::string GetStubLastAddPolicy()
{
    return g_lastAddPolicy;
}

int32_t GetStubRemovePolicyCallCount()
{
    return g_removePolicyCallCount;
}

std::string GetStubLastRemovePolicy()
{
    return g_lastRemovePolicy;
}

int32_t GetStubScreenLockStateCallCount()
{
    return g_screenLockStateCallCount;
}

GRD_ScreenLockState GetStubLastScreenLockState()
{
    return g_lastScreenLockState;
}

int32_t GetStubDeviceHealthyStateCallCount()
{
    return g_deviceHealthyStateCallCount;
}

GRD_DeviceHealthyState GetStubLastDeviceHealthyState()
{
    return g_lastDeviceHealthyState;
}

void ResetStubState()
{
    g_openCallCount = 0;
    g_lastConfigStr.clear();
    g_addPolicyCallCount = 0;
    g_lastAddPolicy.clear();
    g_removePolicyCallCount = 0;
    g_lastRemovePolicy.clear();
    g_screenLockStateCallCount = 0;
    g_lastScreenLockState = GRD_SCREEN_UNLOCK;
    g_deviceHealthyStateCallCount = 0;
    g_lastDeviceHealthyState = GRD_DEVICE_UNROOT;
}
} // namespace NativeRdb
} // namespace OHOS

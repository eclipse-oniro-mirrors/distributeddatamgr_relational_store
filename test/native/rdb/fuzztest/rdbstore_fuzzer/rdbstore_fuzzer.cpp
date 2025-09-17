/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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
#include <fuzzer/FuzzedDataProvider.h>

#include "rdbstore_fuzzer.h"

#include "rdb_errno.h"
#include "rdb_helper.h"
#include "rdb_open_callback.h"
#include "rdb_store.h"

using namespace OHOS;
using namespace OHOS::NativeRdb;

static const int MIN_BLOB_SIZE = 1;
static const int MAX_BLOB_SIZE = 20;
static const int MIN_ROWS_SIZE = 1;
static const int MAX_ROWS_SIZE = 20;
static const int MAX_STRING_LENGTH = 20;

namespace OHOS {
class RdbStoreFuzzTest {
public:
    static void SetUpTestCase(void);

    static bool InsertData(std::shared_ptr<RdbStore> store, FuzzedDataProvider &provider);
    static bool BatchInsertData(std::shared_ptr<RdbStore> store, FuzzedDataProvider &provider);

    static std::string DATABASE_NAME;
    static std::shared_ptr<RdbStore> store_;
};
std::shared_ptr<RdbStore> RdbStoreFuzzTest::store_ = nullptr;
std::string RdbStoreFuzzTest::DATABASE_NAME = "/data/test/rdbStoreFuzz.db";

class RdbTestOpenCallback : public RdbOpenCallback {
public:
    int OnCreate(RdbStore &store) override;
    int OnUpgrade(RdbStore &store, int oldVersion, int newVersion) override;
    static const std::string CREATE_TABLE_TEST;
};
const std::string RdbTestOpenCallback::CREATE_TABLE_TEST = "CREATE TABLE IF NOT EXISTS test "
                                                           "(id INTEGER PRIMARY KEY AUTOINCREMENT, "
                                                           "name TEXT NOT NULL, age INTEGER, salary REAL, "
                                                           "blobType BLOB)";

int RdbTestOpenCallback::OnCreate(RdbStore &store)
{
    return store.ExecuteSql(CREATE_TABLE_TEST);
}

int RdbTestOpenCallback::OnUpgrade(RdbStore &store, int oldVersion, int newVersion)
{
    return E_OK;
}

void RdbStoreFuzzTest::SetUpTestCase(void)
{
    int errCode = E_OK;
    RdbStoreConfig config(RdbStoreFuzzTest::DATABASE_NAME);
    RdbTestOpenCallback helper;
    RdbStoreFuzzTest::store_ = RdbHelper::GetRdbStore(config, 1, helper, errCode);
    if (store_ == nullptr || errCode != E_OK) {
        return;
    }
}

bool RdbStoreFuzzTest::InsertData(std::shared_ptr<RdbStore> store, FuzzedDataProvider &provider)
{
    if (store == nullptr) {
        return false;
    }
    
    std::string tableName = provider.ConsumeRandomLengthString(MAX_STRING_LENGTH);
    std::string valName = provider.ConsumeRandomLengthString(MAX_STRING_LENGTH);
    int valAge = provider.ConsumeIntegral<int>();
    double valSalary = provider.ConsumeFloatingPoint<double>();

    ValuesBucket values;
    values.PutString("name", valName);
    values.PutInt("age", valAge);
    values.PutDouble("salary", valSalary);

    size_t blobSize = provider.ConsumeIntegralInRange<size_t>(MIN_BLOB_SIZE, MAX_BLOB_SIZE);
    std::vector<uint8_t> blobData = provider.ConsumeBytes<uint8_t>(blobSize);
    values.PutBlob("blobType", blobData);

    int64_t id;
    return store->Insert(id, tableName, values);
}

bool RdbInsertFuzz(FuzzedDataProvider &provider)
{
    if (RdbStoreFuzzTest::store_ == nullptr) {
        return false;
    }
    bool result = true;
    if (!RdbStoreFuzzTest::InsertData(RdbStoreFuzzTest::store_, provider)) {
        result = false;
    }

    RdbStoreFuzzTest::store_->ExecuteSql("DELETE FROM test");
    return result;
}

bool RdbStoreFuzzTest::BatchInsertData(std::shared_ptr<RdbStore> store, FuzzedDataProvider &provider)
{
    if (store == nullptr) {
        return false;
    }
    
    std::string tableName = provider.ConsumeRandomLengthString(MAX_STRING_LENGTH);
    std::string valName = provider.ConsumeRandomLengthString(MAX_STRING_LENGTH);
    int valAge = provider.ConsumeIntegral<int>();
    double valSalary = provider.ConsumeFloatingPoint<double>();

    ValuesBuckets rows;
    ValuesBucket value;
    value.PutString("name", valName);
    value.PutInt("age", valAge);
    value.PutDouble("salary", valSalary);

    size_t blobSize = provider.ConsumeIntegralInRange<size_t>(MIN_BLOB_SIZE, MAX_BLOB_SIZE);
    std::vector<uint8_t> blobData = provider.ConsumeBytes<uint8_t>(blobSize);
    value.PutBlob("blobType", blobData);

    uint32_t loopTimes = provider.ConsumeIntegralInRange<uint32_t>(MIN_ROWS_SIZE, MAX_ROWS_SIZE);
    for (uint32_t i = 0; i < loopTimes; i++) {
        rows.Put(value);
    }
    auto [code, num] = store->BatchInsert(tableName, rows, ConflictResolution::ON_CONFLICT_NONE);
    return code == E_OK;
}

bool RdbBatchInsertFuzz(FuzzedDataProvider &provider)
{
    if (RdbStoreFuzzTest::store_ == nullptr) {
        return false;
    }
    bool result = true;
    if (!RdbStoreFuzzTest::BatchInsertData(RdbStoreFuzzTest::store_, provider)) {
        result = false;
    }

    RdbStoreFuzzTest::store_->ExecuteSql("DELETE FROM test");
    return result;
}

bool RdbDeleteFuzz(FuzzedDataProvider &provider)
{
    if (RdbStoreFuzzTest::store_ == nullptr) {
        return false;
    }
    bool result = true;
    int errCode = RdbStoreFuzzTest::InsertData(RdbStoreFuzzTest::store_, provider);
    if (errCode != E_OK) {
        result = false;
    }
    
    std::string tableName = provider.ConsumeRandomLengthString(MAX_STRING_LENGTH);
    std::string whereClause = provider.ConsumeRandomLengthString(MAX_STRING_LENGTH);

    int deletedRows;
    errCode = RdbStoreFuzzTest::store_->Delete(deletedRows, tableName, whereClause);
    if (errCode != E_OK) {
        result = false;
    }

    RdbStoreFuzzTest::store_->ExecuteSql("DELETE FROM test");
    return result;
}

bool RdbUpdateFuzz(FuzzedDataProvider &provider)
{
    if (RdbStoreFuzzTest::store_ == nullptr) {
        return false;
    }
    bool result = true;
    int errCode = RdbStoreFuzzTest::InsertData(RdbStoreFuzzTest::store_, provider);
    if (errCode != E_OK) {
        result = false;
    }
    
    std::string valName = provider.ConsumeRandomLengthString(MAX_STRING_LENGTH);
    std::string tableName = provider.ConsumeRandomLengthString(MAX_STRING_LENGTH);
    std::string whereClause = provider.ConsumeRandomLengthString(MAX_STRING_LENGTH);
    int valAge = provider.ConsumeIntegral<int>();
    double valSalary = provider.ConsumeFloatingPoint<double>();

    int changedRows;
    ValuesBucket values;
    values.PutString("name", valName);
    values.PutInt("age", valAge);
    values.PutDouble("salary", valSalary);

    size_t blobSize = provider.ConsumeIntegralInRange<size_t>(MIN_BLOB_SIZE, MAX_BLOB_SIZE);
    std::vector<uint8_t> blobData = provider.ConsumeBytes<uint8_t>(blobSize);
    values.PutBlob("blobType", blobData);

    errCode = RdbStoreFuzzTest::store_->Update(
        changedRows, tableName, values, whereClause, std::vector<std::string>{ valName });
    if (errCode != E_OK) {
        result = false;
    }
    RdbStoreFuzzTest::store_->ExecuteSql("DELETE FROM test");
    return result;
}

int RdbDoLockRowFuzz(FuzzedDataProvider &provider, bool isLock)
{
    if (RdbStoreFuzzTest::store_ == nullptr) {
        return false;
    }
    bool result = true;
    int errCode = RdbStoreFuzzTest::InsertData(RdbStoreFuzzTest::store_, provider);
    if (errCode != E_OK) {
        result = false;
    }
    
    std::string valName = provider.ConsumeRandomLengthString(MAX_STRING_LENGTH);
    std::string tableName = provider.ConsumeRandomLengthString(MAX_STRING_LENGTH);
    AbsRdbPredicates predicates(tableName);
    predicates.EqualTo("name", ValueObject(valName));
    errCode = RdbStoreFuzzTest::store_->ModifyLockStatus(predicates, isLock);
    if (errCode != E_OK) {
        result = false;
    }

    RdbStoreFuzzTest::store_->ExecuteSql("DELETE FROM test");
    return result;
}

bool RdbLockRowFuzz(FuzzedDataProvider &provider)
{
    return RdbDoLockRowFuzz(provider, true);
}

bool RdbUnlockRowFuzz(FuzzedDataProvider &provider)
{
    return RdbDoLockRowFuzz(provider, false);
}

void RdbSetLockedRowPredicates(AbsRdbPredicates &predicates)
{
    predicates.Clear();
    predicates.BeginWrap();
    predicates.EqualTo(AbsRdbPredicates::LOCK_STATUS, AbsRdbPredicates::LOCKED);
    predicates.Or();
    predicates.EqualTo(AbsRdbPredicates::LOCK_STATUS, AbsRdbPredicates::LOCK_CHANGED);
    predicates.EndWrap();
}

void RdbQueryLockedRowFuzz1(FuzzedDataProvider &provider)
{
    if (RdbStoreFuzzTest::store_ == nullptr) {
        return;
    }
    int errCode = RdbStoreFuzzTest::InsertData(RdbStoreFuzzTest::store_, provider);
    if (errCode != E_OK) {
        return;
    }
    
    std::string valName = provider.ConsumeRandomLengthString(MAX_STRING_LENGTH);
    std::string tableName = provider.ConsumeRandomLengthString(MAX_STRING_LENGTH);
    std::string vectorElem = provider.ConsumeRandomLengthString(MAX_STRING_LENGTH);
    AbsRdbPredicates predicates(tableName);
    RdbSetLockedRowPredicates(predicates);
    predicates.EqualTo("name", ValueObject(valName));
    RdbStoreFuzzTest::store_->QueryByStep(predicates, { vectorElem });

    RdbSetLockedRowPredicates(predicates);
    predicates.NotEqualTo("name", ValueObject(valName));
    RdbStoreFuzzTest::store_->QueryByStep(predicates, { vectorElem });

    RdbSetLockedRowPredicates(predicates);
    predicates.Contains("name", valName);
    RdbStoreFuzzTest::store_->QueryByStep(predicates, { vectorElem });

    RdbSetLockedRowPredicates(predicates);
    predicates.BeginsWith("name", valName);
    RdbStoreFuzzTest::store_->QueryByStep(predicates, { vectorElem });

    RdbSetLockedRowPredicates(predicates);
    predicates.EndsWith("name", valName);
    RdbStoreFuzzTest::store_->QueryByStep(predicates, { vectorElem });

    RdbSetLockedRowPredicates(predicates);
    predicates.Like("name", valName);
    RdbStoreFuzzTest::store_->QueryByStep(predicates, { vectorElem });

    RdbSetLockedRowPredicates(predicates);
    predicates.Glob("name", valName);
    RdbStoreFuzzTest::store_->QueryByStep(predicates, { vectorElem });
    RdbStoreFuzzTest::store_->ExecuteSql("DELETE FROM test");
}

void RdbQueryLockedRowFuzz2(FuzzedDataProvider &provider)
{
    if (RdbStoreFuzzTest::store_ == nullptr) {
        return;
    }
    int errCode = RdbStoreFuzzTest::InsertData(RdbStoreFuzzTest::store_, provider);
    if (errCode != E_OK) {
        return;
    }
    
    std::string valName = provider.ConsumeRandomLengthString(MAX_STRING_LENGTH);
    std::string tableName = provider.ConsumeRandomLengthString(MAX_STRING_LENGTH);
    ValueObject valAge(provider.ConsumeIntegral<int>());
    ValueObject valAgeChange(provider.ConsumeIntegral<int>());
    std::vector<std::string> bindArgs({ provider.ConsumeRandomLengthString(MAX_STRING_LENGTH) });
    std::vector<ValueObject> vectorElem({ provider.ConsumeRandomLengthString(MAX_STRING_LENGTH) });
    AbsRdbPredicates predicates(tableName);
    RdbSetLockedRowPredicates(predicates);
    predicates.Between("age", valAge, valAgeChange);
    RdbStoreFuzzTest::store_->QueryByStep(predicates, bindArgs);

    RdbSetLockedRowPredicates(predicates);
    predicates.NotBetween("age", valAge, valAgeChange);
    RdbStoreFuzzTest::store_->QueryByStep(predicates, bindArgs);

    RdbSetLockedRowPredicates(predicates);
    predicates.GreaterThan("age", valAge);
    RdbStoreFuzzTest::store_->QueryByStep(predicates, bindArgs);

    RdbSetLockedRowPredicates(predicates);
    predicates.LessThan("age", valAgeChange);
    RdbStoreFuzzTest::store_->QueryByStep(predicates, bindArgs);

    RdbSetLockedRowPredicates(predicates);
    predicates.GreaterThanOrEqualTo("age", valAge);
    RdbStoreFuzzTest::store_->QueryByStep(predicates, bindArgs);

    RdbSetLockedRowPredicates(predicates);
    predicates.LessThanOrEqualTo("age", valAgeChange);
    RdbStoreFuzzTest::store_->QueryByStep(predicates, bindArgs);

    RdbSetLockedRowPredicates(predicates);
    predicates.In("name", vectorElem);
    RdbStoreFuzzTest::store_->QueryByStep(predicates, bindArgs);

    RdbSetLockedRowPredicates(predicates);
    predicates.NotIn("name", vectorElem);
    RdbStoreFuzzTest::store_->QueryByStep(predicates, bindArgs);
    RdbStoreFuzzTest::store_->ExecuteSql("DELETE FROM test");
}

void RdbQueryFuzz1(FuzzedDataProvider &provider)
{
    if (RdbStoreFuzzTest::store_ == nullptr) {
        return;
    }
    int errCode = RdbStoreFuzzTest::InsertData(RdbStoreFuzzTest::store_, provider);
    if (errCode != E_OK) {
        return;
    }
    
    std::string valName = provider.ConsumeRandomLengthString(MAX_STRING_LENGTH);
    std::string tableName = provider.ConsumeRandomLengthString(MAX_STRING_LENGTH);
    std::string vectorElem = provider.ConsumeRandomLengthString(MAX_STRING_LENGTH);
    AbsRdbPredicates predicates(tableName);

    predicates.EqualTo("name", ValueObject(valName));
    RdbStoreFuzzTest::store_->Query(predicates, { vectorElem });

    predicates.Clear();
    predicates.NotEqualTo("name", ValueObject(valName));
    RdbStoreFuzzTest::store_->Query(predicates, { vectorElem });

    predicates.Clear();
    predicates.Contains("name", valName);
    RdbStoreFuzzTest::store_->Query(predicates, { vectorElem });

    predicates.Clear();
    predicates.BeginsWith("name", valName);
    RdbStoreFuzzTest::store_->Query(predicates, { vectorElem });

    predicates.Clear();
    predicates.EndsWith("name", valName);
    RdbStoreFuzzTest::store_->Query(predicates, { vectorElem });

    predicates.Clear();
    predicates.Like("name", valName);
    RdbStoreFuzzTest::store_->Query(predicates, { vectorElem });

    predicates.Clear();
    predicates.NotLike("name", valName);
    RdbStoreFuzzTest::store_->Query(predicates, { vectorElem });

    predicates.Clear();
    predicates.NotContains("name", valName);
    RdbStoreFuzzTest::store_->Query(predicates, { vectorElem });

    predicates.Clear();
    predicates.Glob("name", valName);
    RdbStoreFuzzTest::store_->Query(predicates, { vectorElem });
    RdbStoreFuzzTest::store_->ExecuteSql("DELETE FROM test");
}

void RdbQueryFuzz2(FuzzedDataProvider &provider)
{
    if (RdbStoreFuzzTest::store_ == nullptr) {
        return;
    }
    int errCode = RdbStoreFuzzTest::InsertData(RdbStoreFuzzTest::store_, provider);
    if (errCode != E_OK) {
        return;
    }
    
    std::string valName = provider.ConsumeRandomLengthString(MAX_STRING_LENGTH);
    std::string tableName = provider.ConsumeRandomLengthString(MAX_STRING_LENGTH);
    ValueObject valAge(provider.ConsumeIntegral<int>());
    ValueObject valAgeChange(provider.ConsumeIntegral<int>());
    std::vector<std::string> bindArgs({ provider.ConsumeRandomLengthString(MAX_STRING_LENGTH) });
    std::vector<ValueObject> vectorElem({ provider.ConsumeRandomLengthString(MAX_STRING_LENGTH) });

    AbsRdbPredicates predicates(tableName);

    predicates.Clear();
    predicates.Between("age", valAge, valAgeChange);
    RdbStoreFuzzTest::store_->Query(predicates, bindArgs);

    predicates.Clear();
    predicates.NotBetween("age", valAge, valAgeChange);
    RdbStoreFuzzTest::store_->Query(predicates, bindArgs);

    predicates.Clear();
    predicates.GreaterThan("age", valAge);
    RdbStoreFuzzTest::store_->Query(predicates, bindArgs);

    predicates.Clear();
    predicates.LessThan("age", valAgeChange);
    RdbStoreFuzzTest::store_->Query(predicates, bindArgs);

    predicates.Clear();
    predicates.GreaterThanOrEqualTo("age", valAge);
    RdbStoreFuzzTest::store_->Query(predicates, bindArgs);

    predicates.Clear();
    predicates.LessThanOrEqualTo("age", valAgeChange);
    RdbStoreFuzzTest::store_->Query(predicates, bindArgs);

    predicates.Clear();
    predicates.In("name", vectorElem);
    RdbStoreFuzzTest::store_->Query(predicates, bindArgs);

    predicates.Clear();
    predicates.NotIn("name", vectorElem);
    RdbStoreFuzzTest::store_->Query(predicates, bindArgs);
    RdbStoreFuzzTest::store_->ExecuteSql("DELETE FROM test");
}

void RdbInitKnowledgeSchemaFuzz(FuzzedDataProvider &provider)
{
    if (RdbStoreFuzzTest::store_ == nullptr) {
        return;
    }
    DistributedRdb::RdbKnowledgeTable table;
    DistributedRdb::RdbKnowledgeSchema schema;
    std::string dbName = provider.ConsumeRandomLengthString(MAX_STRING_LENGTH);
    std::string tableName = provider.ConsumeRandomLengthString(MAX_STRING_LENGTH);
    table.tableName = tableName;
    schema.dbName = dbName;
    schema.tables.push_back(table);
    std::shared_ptr<RdbStore> &store = RdbStoreFuzzTest::store_;
    store->InitKnowledgeSchema(schema);
}

int32_t ClusterAlgoByEvenNumberFuzz(ClstAlgoParaT *para)
{
    int *result = para->clusterResult;
    const int specialId = 985;
    for (uint32_t i = 0; i < para->newFeaturesNum; i++) {
        result[i] = specialId;
    }
    return 0;
}

bool RdbRegisterAlgoFuzz(FuzzedDataProvider &provider)
{
    if (RdbStoreFuzzTest::store_ == nullptr) {
        return false;
    }
    bool result = true;
    std::string clstAlgoName = provider.ConsumeRandomLengthString(MAX_STRING_LENGTH);
    int errCode =
        RdbStoreFuzzTest::store_->RegisterAlgo(clstAlgoName, ClusterAlgoByEvenNumberFuzz);
    if (errCode != E_OK) {
        result = false;
    }
    return result;
}
} // namespace OHOS

/* Fuzzer entry point */
extern "C" int LLVMFuzzerInitialize(int *argc, char ***argv)
{
    OHOS::RdbStoreFuzzTest::SetUpTestCase();
    return 0;
}

/* Fuzzer entry point */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    /* Run your code on data */
    FuzzedDataProvider provider(data, size);
    OHOS::RdbInsertFuzz(provider);
    OHOS::RdbDeleteFuzz(provider);
    OHOS::RdbUpdateFuzz(provider);
    OHOS::RdbQueryFuzz1(provider);
    OHOS::RdbQueryFuzz2(provider);
    OHOS::RdbLockRowFuzz(provider);
    OHOS::RdbUnlockRowFuzz(provider);
    OHOS::RdbQueryLockedRowFuzz1(provider);
    OHOS::RdbQueryLockedRowFuzz2(provider);
    OHOS::RdbInitKnowledgeSchemaFuzz(provider);
    OHOS::RdbRegisterAlgoFuzz(provider);
    OHOS::RdbStoreFuzzTest::TearDownTestCase();
    return 0;
}
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

#ifndef NATIVE_RDB_RDB_STORE_DECORATOR_H
#define NATIVE_RDB_RDB_STORE_DECORATOR_H

#include <memory>
#include <string>
#include <utility>

#include "rdb_fault_injector.h"
#include "rdb_store_impl.h"

namespace OHOS {
namespace NativeRdb {

// Decorated store - the default store object returned by RdbStoreManager.
// It derives from RdbStoreImpl so every method it does not override is
// inherited and works unchanged (Sync, Backup, observers, ...), and it
// overrides only the final sinks that the RdbStore base-class template
// methods funnel every public overload of a family into (see rdb_store.cpp).
// The role is twofold: each override carries the per-call instrumentation
// (HITRACE, stat reporting, SQL statistics, perf statistics - moved here
// from rdb_store_impl.cpp so the implementation keeps only the functional
// core), and it applies the fault-injection verdict carried by the
// per-store injector member: a hit returns E_SQLITE_CORRUPT without
// touching the database (still fully observable, since the instrumentation
// has already run), otherwise the call goes to the parent. Result sets
// returned by the query sinks are wrapped so resultSetRead faults surface
// on the read interface callers use, and transactions are constructed
// decorated with the same injector. When the injector is inactive (user
// builds link the no-op implementation) the decorator behaves exactly like
// the plain implementation.
// Inject-and-return guard for decorated sinks: if the injector hits, return
// the caller-supplied corruption value without executing the parent.
#define RDB_INJECT_GUARD(op, name, ...) \
    if (ShouldInject(RdbFaultOp::op, name)) { \
        return __VA_ARGS__; \
    }

class RdbStoreDecorator final : public RdbStoreImpl {
public:
    explicit RdbStoreDecorator(const RdbStoreConfig &config);

    std::pair<int, int64_t> Insert(const std::string &table, const Row &row, const InsertConfig &config) override;
    std::pair<int, int64_t> BatchInsert(const std::string &table, const ValuesBuckets &rows) override;
    std::pair<int32_t, Results> BatchInsert(
        const std::string &table, const RefRows &rows, const BatchInsertConfig &config) override;
    std::pair<int32_t, Results> Update(
        const Row &row, const AbsRdbPredicates &predicates, const UpdateConfig &config) override;
    std::pair<int32_t, Results> Delete(const AbsRdbPredicates &predicates, const DeleteConfig &config) override;
    std::shared_ptr<AbsSharedResultSet> QuerySql(
        const std::string &sql, const Values &args, const QueryConfig &config) override;
    std::shared_ptr<ResultSet> QueryByStep(
        const std::string &sql, const Values &args, const QueryOptions &options, const QueryConfig &config) override;
    int ExecuteSql(const std::string &sql, const Values &args) override;
    std::pair<int32_t, ValueObject> Execute(
        const std::string &sql, const Values &args, int64_t trxId, const ExecuteConfig &config) override;
    std::pair<int32_t, Results> ExecuteExt(const std::string &sql, const Values &args) override;
    int ExecuteAndGetLong(int64_t &outValue, const std::string &sql, const Values &args) override;
    int ExecuteAndGetString(std::string &outValue, const std::string &sql, const Values &args) override;
    int ExecuteForLastInsertedRowId(int64_t &outValue, const std::string &sql, const Values &args) override;
    int ExecuteForChangedRowCount(int64_t &outValue, const std::string &sql, const Values &args) override;
    int Count(int64_t &outValue, const AbsRdbPredicates &predicates) override;
    std::pair<int32_t, std::shared_ptr<Transaction>> CreateTransaction(int32_t type) override;
    int BeginTransaction() override;
    std::pair<int, int64_t> BeginTrans() override;
    int RollBack() override;
    int RollBack(int64_t trxId) override;
    int Commit() override;
    int Commit(int64_t trxId) override;

    int32_t Init(int version, RdbOpenCallback &openCallback, bool isNeedSetAcl = false,
        bool isSilentAccessible = false) override;

private:
    // Injection verdict for a hooked sink: applies only after the open
    // (Init) completed, so OnCreate/OnUpgrade callbacks and internal
    // statements never consume the budget.
    bool ShouldInject(RdbFaultOp op, const std::string &methodName) const;

    std::pair<int32_t, std::shared_ptr<Transaction>> CreateTransactionObj(
        int32_t type, std::shared_ptr<Connection> conn) override;

    std::string dbPath_;
    // Per-store injector: created in Init (after the base Init succeeds,
    // outside the manager lock) so the config file read never happens inside a
    // critical section. Passed to the transaction and result-set decorators
    // this store creates, so the budget state is shared by everything
    // serving the database. Holder plus reference, following the
    // configHolder_/config_ convention in rdb_store_impl.h.
    std::shared_ptr<RdbFaultInjector> injectorHolder_;
    RdbFaultInjector *injector_ = nullptr;
};

} // namespace NativeRdb
} // namespace OHOS

#endif // NATIVE_RDB_RDB_STORE_DECORATOR_H

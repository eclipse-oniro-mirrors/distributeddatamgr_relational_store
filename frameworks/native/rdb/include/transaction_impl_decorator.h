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

#ifndef NATIVE_RDB_TRANSACTION_IMPL_DECORATOR_H
#define NATIVE_RDB_TRANSACTION_IMPL_DECORATOR_H

#include <memory>
#include <string>
#include <utility>

#include "rdb_fault_injector.h"
#include "transaction_impl.h"

namespace OHOS {
namespace NativeRdb {

// Fault-injection decorator of the transaction implementation - the
// transaction object constructed by RdbStoreDecorator, which passes its
// injector member in at construction (see TransactionImplDecorator::Create).
// Each override applies the injection verdict (returning E_SQLITE_CORRUPT
// without touching the database) and otherwise delegates to the parent,
// which keeps the per-call instrumentation and the internal TransDB facade.
// The query sinks wrap the result sets they return so resultSetRead faults
// surface on the read interface callers use.
class TransactionImplDecorator final : public TransactionImpl {
public:
    TransactionImplDecorator(std::shared_ptr<Connection> connection, const std::string &path, bool auditEnabled,
        std::shared_ptr<RdbFaultInjector> injector);

    // Mirrors TransactionImpl::Create, but constructs this decorator with
    // the store's injector so budget state is shared with the store.
    static std::pair<int32_t, std::shared_ptr<Transaction>> Create(int32_t type,
        std::shared_ptr<Connection> connection, const std::string &path, bool auditEnabled,
        std::shared_ptr<RdbFaultInjector> injector);

    int32_t Commit() override;
    int32_t Rollback() override;
    int32_t Close() override;

    std::pair<int32_t, int64_t> Insert(
        const std::string &table, const Row &row, const InsertConfig &config = {}) override;
    std::pair<int32_t, int64_t> BatchInsert(const std::string &table, const Rows &rows) override;
    std::pair<int32_t, int64_t> BatchInsert(const std::string &table, const RefRows &rows) override;
    std::pair<int32_t, Results> BatchInsert(
        const std::string &table, const RefRows &rows, const BatchInsertConfig &config) override;
    std::pair<int32_t, Results> Update(
        const Row &row, const AbsRdbPredicates &predicates, const UpdateConfig &config = {}) override;
    std::pair<int32_t, Results> Delete(
        const AbsRdbPredicates &predicates, const DeleteConfig &config = {}) override;
    std::shared_ptr<ResultSet> QueryByStep(const std::string &sql, const Values &args,
        const QueryOptions &options, const QueryConfig &config = {}) override;
    std::shared_ptr<ResultSet> QueryByStep(const AbsRdbPredicates &predicates, const Fields &columns,
        const QueryOptions &options, const QueryConfig &config = {}) override;
    std::pair<int32_t, ValueObject> Execute(
        const std::string &sql, const Values &args, const ExecuteConfig &config = {}) override;
    std::pair<int32_t, Results> ExecuteExt(
        const std::string &sql, const Values &args, const ExecuteConfig &config = {}) override;

private:
    // Wraps a step result set when the database has an injection entry.
    std::shared_ptr<ResultSet> WrapIfConfigured(std::shared_ptr<ResultSet> resultSet) const;

    // Holder plus reference, following the configHolder_/config_ convention
    // in rdb_store_impl.h; the class only uses the reference.
    std::shared_ptr<RdbFaultInjector> injectorHolder_;
    RdbFaultInjector &injector_;
};

} // namespace NativeRdb
} // namespace OHOS

#endif // NATIVE_RDB_TRANSACTION_IMPL_DECORATOR_H

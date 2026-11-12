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

// Fault-injection decorator of the transaction implementation. Every
// override applies the injection verdict and otherwise calls the parent
// implementation, which keeps the per-call instrumentation and the internal
// TransDB facade.

#include "transaction_impl_decorator.h"

#include "logger.h"
#include "rdb_errno.h"
#include "rdb_result_set_decorator.h"

using namespace OHOS::Rdb;

namespace OHOS {
namespace NativeRdb {

TransactionImplDecorator::TransactionImplDecorator(std::shared_ptr<Connection> connection,
    const std::string &path, bool auditEnabled, std::shared_ptr<RdbFaultInjector> injector)
    : TransactionImpl(std::move(connection), path, auditEnabled), injectorHolder_(std::move(injector)),
      injector_(*injectorHolder_)
{
}

std::pair<int32_t, std::shared_ptr<Transaction>> TransactionImplDecorator::Create(int32_t type,
    std::shared_ptr<Connection> connection, const std::string &path, bool auditEnabled,
    std::shared_ptr<RdbFaultInjector> injector)
{
    // Fault-injection mock: the decorator is the transaction object of a
    // decorated store; it behaves exactly like the plain implementation
    // when the injector is inactive.
    auto trans = std::make_shared<TransactionImplDecorator>(
        std::move(connection), path, auditEnabled, std::move(injector));
    if (trans == nullptr) {
        return { E_ERROR, nullptr };
    }
    auto errorCode = trans->Begin(type);
    if (errorCode != E_OK) {
        LOG_ERROR("Transaction begin failed, errorCode:%{public}d", errorCode);
        return { errorCode, nullptr };
    }
    return { E_OK, trans };
}

int32_t TransactionImplDecorator::Commit()
{
    // Fault-injection mock: return the corruption error without touching the database.
    if (injector_.Hit("Commit", RdbFaultOp::TRANSACTION)) {
        return E_SQLITE_CORRUPT;
    }
    return TransactionImpl::Commit();
}

int32_t TransactionImplDecorator::Rollback()
{
    if (injector_.Hit("Rollback", RdbFaultOp::TRANSACTION)) {
        return E_SQLITE_CORRUPT;
    }
    return TransactionImpl::Rollback();
}

int32_t TransactionImplDecorator::Close()
{
    if (injector_.Hit("Close", RdbFaultOp::TRANSACTION)) {
        return E_SQLITE_CORRUPT;
    }
    return TransactionImpl::Close();
}

std::pair<int32_t, int64_t> TransactionImplDecorator::Insert(
    const std::string &table, const Row &row, const InsertConfig &config)
{
    if (injector_.Hit("Insert", RdbFaultOp::INSERT)) {
        return { E_SQLITE_CORRUPT, -1 };
    }
    return TransactionImpl::Insert(table, row, config);
}

std::pair<int32_t, int64_t> TransactionImplDecorator::BatchInsert(
    const std::string &table, const Rows &rows)
{
    if (injector_.Hit("BatchInsert", RdbFaultOp::INSERT)) {
        return { E_SQLITE_CORRUPT, -1 };
    }
    return TransactionImpl::BatchInsert(table, rows);
}

std::pair<int32_t, int64_t> TransactionImplDecorator::BatchInsert(
    const std::string &table, const RefRows &rows)
{
    if (injector_.Hit("BatchInsert", RdbFaultOp::INSERT)) {
        return { E_SQLITE_CORRUPT, -1 };
    }
    return TransactionImpl::BatchInsert(table, rows);
}

std::pair<int32_t, Results> TransactionImplDecorator::BatchInsert(
    const std::string &table, const RefRows &rows, const BatchInsertConfig &config)
{
    if (injector_.Hit("BatchInsert", RdbFaultOp::INSERT)) {
        return { E_SQLITE_CORRUPT, {} };
    }
    return TransactionImpl::BatchInsert(table, rows, config);
}

std::pair<int32_t, Results> TransactionImplDecorator::Update(
    const Row &row, const AbsRdbPredicates &predicates, const UpdateConfig &config)
{
    if (injector_.Hit("Update", RdbFaultOp::UPDATE)) {
        return { E_SQLITE_CORRUPT, {} };
    }
    return TransactionImpl::Update(row, predicates, config);
}

std::pair<int32_t, Results> TransactionImplDecorator::Delete(
    const AbsRdbPredicates &predicates, const DeleteConfig &config)
{
    if (injector_.Hit("Delete", RdbFaultOp::DELETE)) {
        return { E_SQLITE_CORRUPT, {} };
    }
    return TransactionImpl::Delete(predicates, config);
}

std::shared_ptr<ResultSet> TransactionImplDecorator::QueryByStep(const std::string &sql, const Values &args,
    const QueryOptions &options, const QueryConfig &config)
{
    if (injector_.Hit("QueryByStep", RdbFaultOp::QUERY)) {
        return nullptr;
    }
    return WrapIfConfigured(TransactionImpl::QueryByStep(sql, args, options, config));
}

std::shared_ptr<ResultSet> TransactionImplDecorator::QueryByStep(const AbsRdbPredicates &predicates,
    const Fields &columns, const QueryOptions &options, const QueryConfig &config)
{
    if (injector_.Hit("QueryByStep", RdbFaultOp::QUERY)) {
        return nullptr;
    }
    return WrapIfConfigured(TransactionImpl::QueryByStep(predicates, columns, options, config));
}

std::pair<int32_t, ValueObject> TransactionImplDecorator::Execute(
    const std::string &sql, const Values &args, const ExecuteConfig &config)
{
    if (injector_.Hit("Execute", RdbFaultOp::EXECUTE_SQL)) {
        return { E_SQLITE_CORRUPT, {} };
    }
    return TransactionImpl::Execute(sql, args, config);
}

std::pair<int32_t, Results> TransactionImplDecorator::ExecuteExt(
    const std::string &sql, const Values &args, const ExecuteConfig &config)
{
    if (injector_.Hit("ExecuteExt", RdbFaultOp::EXECUTE_SQL)) {
        return { E_SQLITE_CORRUPT, {} };
    }
    return TransactionImpl::ExecuteExt(sql, args, config);
}

std::shared_ptr<ResultSet> TransactionImplDecorator::WrapIfConfigured(
    std::shared_ptr<ResultSet> resultSet) const
{
    if (resultSet != nullptr && injector_.HasEntry()) {
        return std::make_shared<RdbResultSetDecorator<ResultSet>>(resultSet, injectorHolder_);
    }
    return resultSet;
}

} // namespace NativeRdb
} // namespace OHOS

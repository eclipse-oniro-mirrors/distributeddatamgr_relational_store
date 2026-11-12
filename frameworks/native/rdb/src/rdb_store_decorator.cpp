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

// Fault-injection decorator of the store layer - the default store object
// returned by RdbStoreManager. Every override mirrors exactly the
// instrumentation the sink method used to carry (HITRACE, stat reporting,
// SQL statistics, perf statistics - moved here from rdb_store_impl.cpp so
// the implementation keeps only the functional core), then decides from the
// injector state whether to return E_SQLITE_CORRUPT, and otherwise calls
// the parent implementation. Query sinks wrap the result sets they return
// so resultSetRead faults surface on the read interface callers use.

#include "rdb_store_decorator.h"

#include "rdb_errno.h"
#include "rdb_perfStat.h"
#include "rdb_result_set_decorator.h"
#include "rdb_sql_statistic.h"
#include "rdb_stat_reporter.h"
#include "rdb_trace.h"
#include "transaction_impl_decorator.h"

namespace OHOS {
namespace NativeRdb {
using DistributedRdb::PerfStat;
using DistributedRdb::SqlStatistic;

RdbStoreDecorator::RdbStoreDecorator(const RdbStoreConfig &config)
    : RdbStoreImpl(config), dbPath_(GetPath()), injectorHolder_(std::make_shared<RdbFaultInjector>(dbPath_)),
      injector_(*injectorHolder_)
{
}

bool RdbStoreDecorator::ShouldInject(RdbFaultOp op, const std::string &methodName) const
{
    return GetInitStatus() == E_OK && injector_.Hit(methodName, op);
}

std::pair<int, int64_t> RdbStoreDecorator::Insert(const std::string &table, const Row &row, const InsertConfig &config)
{
    DISTRIBUTED_DATA_HITRACE(std::string(__FUNCTION__));
    RdbStatReporter reportStat(RDB_PERF, INSERT, GetConfig(), GetReportFunc());
    SqlStatistic sqlStatistic("", SqlStatistic::Step::STEP_TOTAL);
    PerfStat perfStat(dbPath_, "", PerfStat::Step::STEP_TOTAL);
    // Fault-injection mock: return the corruption error without touching the database.
    if (ShouldInject(RdbFaultOp::INSERT, "Insert")) {
        return { E_SQLITE_CORRUPT, -1 };
    }
    return RdbStoreImpl::Insert(table, row, config);
}

std::pair<int, int64_t> RdbStoreDecorator::BatchInsert(const std::string &table, const ValuesBuckets &rows)
{
    DISTRIBUTED_DATA_HITRACE(std::string(__FUNCTION__));
    RdbStatReporter reportStat(RDB_PERF, BATCHINSERT, GetConfig(), GetReportFunc());
    SqlStatistic sqlStatistic("", SqlStatistic::Step::STEP_TOTAL);
    PerfStat perfStat(dbPath_, "", PerfStat::Step::STEP_TOTAL, 0, rows.RowSize());
    if (ShouldInject(RdbFaultOp::INSERT, "BatchInsert")) {
        return { E_SQLITE_CORRUPT, -1 };
    }
    return RdbStoreImpl::BatchInsert(table, rows);
}

std::pair<int32_t, Results> RdbStoreDecorator::BatchInsert(
    const std::string &table, const RefRows &rows, const BatchInsertConfig &config)
{
    DISTRIBUTED_DATA_HITRACE(std::string(__FUNCTION__));
    RdbStatReporter reportStat(RDB_PERF, BATCHINSERT, GetConfig(), GetReportFunc());
    SqlStatistic sqlStatistic("", SqlStatistic::Step::STEP_TOTAL);
    PerfStat perfStat(dbPath_, "", PerfStat::Step::STEP_TOTAL, 0, rows.RowSize());
    if (ShouldInject(RdbFaultOp::INSERT, "BatchInsert")) {
        return { E_SQLITE_CORRUPT, -1 };
    }
    return RdbStoreImpl::BatchInsert(table, rows, config);
}

std::pair<int32_t, Results> RdbStoreDecorator::Update(
    const Row &row, const AbsRdbPredicates &predicates, const UpdateConfig &config)
{
    DISTRIBUTED_DATA_HITRACE(std::string(__FUNCTION__));
    RdbStatReporter reportStat(RDB_PERF, UPDATE, GetConfig(), GetReportFunc());
    SqlStatistic sqlStatistic("", SqlStatistic::Step::STEP_TOTAL);
    PerfStat perfStat(dbPath_, "", PerfStat::Step::STEP_TOTAL);
    if (ShouldInject(RdbFaultOp::UPDATE, "Update")) {
        return { E_SQLITE_CORRUPT, -1 };
    }
    return RdbStoreImpl::Update(row, predicates, config);
}

std::pair<int32_t, Results> RdbStoreDecorator::Delete(const AbsRdbPredicates &predicates, const DeleteConfig &config)
{
    DISTRIBUTED_DATA_HITRACE(std::string(__FUNCTION__));
    RdbStatReporter reportStat(RDB_PERF, DELETE, GetConfig(), GetReportFunc());
    SqlStatistic sqlStatistic("", SqlStatistic::Step::STEP_TOTAL);
    PerfStat perfStat(dbPath_, "", PerfStat::Step::STEP_TOTAL);
    if (ShouldInject(RdbFaultOp::DELETE, "Delete")) {
        return { E_SQLITE_CORRUPT, -1 };
    }
    return RdbStoreImpl::Delete(predicates, config);
}

std::shared_ptr<AbsSharedResultSet> RdbStoreDecorator::QuerySql(
    const std::string &sql, const Values &args, const QueryConfig &config)
{
    DISTRIBUTED_DATA_HITRACE(std::string(__FUNCTION__));
    SqlStatistic sqlStatistic("", SqlStatistic::Step::STEP_TOTAL);
    PerfStat perfStat(dbPath_, "", PerfStat::Step::STEP_TOTAL);
    if (ShouldInject(RdbFaultOp::QUERY, "QuerySql")) {
        return nullptr;
    }
    auto resultSet = RdbStoreImpl::QuerySql(sql, args, config);
    // The AbsSharedResultSet family follows the parent's CROSS_PLATFORM
    // boundary; on the SDK variants the parent returns nullptr anyway,
    // so the wrap never fires there.
#if !defined(CROSS_PLATFORM)
    if (resultSet != nullptr && injector_.HasEntry()) {
        return std::make_shared<RdbResultSetDecorator<AbsSharedResultSet>>(resultSet, injectorHolder_);
    }
#endif
    return resultSet;
}

std::shared_ptr<ResultSet> RdbStoreDecorator::QueryByStep(
    const std::string &sql, const Values &args, const QueryOptions &options, const QueryConfig &config)
{
    DISTRIBUTED_DATA_HITRACE(std::string(__FUNCTION__));
    SqlStatistic sqlStatistic("", SqlStatistic::Step::STEP_TOTAL);
    PerfStat perfStat(dbPath_, "", PerfStat::Step::STEP_TOTAL);
    if (ShouldInject(RdbFaultOp::QUERY, "QueryByStep")) {
        return nullptr;
    }
    auto resultSet = RdbStoreImpl::QueryByStep(sql, args, options, config);
    if (resultSet != nullptr && injector_.HasEntry()) {
        return std::make_shared<RdbResultSetDecorator<ResultSet>>(resultSet, injectorHolder_);
    }
    return resultSet;
}

int RdbStoreDecorator::ExecuteSql(const std::string &sql, const Values &args)
{
    DISTRIBUTED_DATA_HITRACE(std::string(__FUNCTION__));
    RdbStatReporter reportStat(RDB_PERF, EXECUTESQL, GetConfig(), GetReportFunc());
    SqlStatistic sqlStatistic("", SqlStatistic::Step::STEP_TOTAL);
    PerfStat perfStat(dbPath_, "", PerfStat::Step::STEP_TOTAL);
    if (ShouldInject(RdbFaultOp::EXECUTE_SQL, "ExecuteSql")) {
        return E_SQLITE_CORRUPT;
    }
    return RdbStoreImpl::ExecuteSql(sql, args);
}

std::pair<int32_t, ValueObject> RdbStoreDecorator::Execute(
    const std::string &sql, const Values &args, int64_t trxId, const ExecuteConfig &config)
{
    RdbStatReporter reportStat(RDB_PERF, EXECUTE, GetConfig(), GetReportFunc());
    SqlStatistic sqlStatistic("", SqlStatistic::Step::STEP_TOTAL);
    PerfStat perfStat(dbPath_, "", PerfStat::Step::STEP_TOTAL);
    if (ShouldInject(RdbFaultOp::EXECUTE_SQL, "Execute")) {
        return { E_SQLITE_CORRUPT, ValueObject() };
    }
    return RdbStoreImpl::Execute(sql, args, trxId, config);
}

std::pair<int32_t, Results> RdbStoreDecorator::ExecuteExt(const std::string &sql, const Values &args)
{
    RdbStatReporter reportStat(RDB_PERF, EXECUTE, GetConfig(), GetReportFunc());
    SqlStatistic sqlStatistic("", SqlStatistic::Step::STEP_TOTAL);
    if (ShouldInject(RdbFaultOp::EXECUTE_SQL, "ExecuteExt")) {
        return { E_SQLITE_CORRUPT, -1 };
    }
    return RdbStoreImpl::ExecuteExt(sql, args);
}

int RdbStoreDecorator::ExecuteAndGetLong(int64_t &outValue, const std::string &sql, const Values &args)
{
    if (ShouldInject(RdbFaultOp::EXECUTE_SQL, "ExecuteAndGetLong")) {
        return E_SQLITE_CORRUPT;
    }
    return RdbStoreImpl::ExecuteAndGetLong(outValue, sql, args);
}

int RdbStoreDecorator::ExecuteAndGetString(std::string &outValue, const std::string &sql, const Values &args)
{
    if (ShouldInject(RdbFaultOp::EXECUTE_SQL, "ExecuteAndGetString")) {
        return E_SQLITE_CORRUPT;
    }
    return RdbStoreImpl::ExecuteAndGetString(outValue, sql, args);
}

int RdbStoreDecorator::ExecuteForLastInsertedRowId(int64_t &outValue, const std::string &sql, const Values &args)
{
    if (ShouldInject(RdbFaultOp::EXECUTE_SQL, "ExecuteForLastInsertedRowId")) {
        return E_SQLITE_CORRUPT;
    }
    return RdbStoreImpl::ExecuteForLastInsertedRowId(outValue, sql, args);
}

int RdbStoreDecorator::ExecuteForChangedRowCount(int64_t &outValue, const std::string &sql, const Values &args)
{
    if (ShouldInject(RdbFaultOp::EXECUTE_SQL, "ExecuteForChangedRowCount")) {
        return E_SQLITE_CORRUPT;
    }
    return RdbStoreImpl::ExecuteForChangedRowCount(outValue, sql, args);
}

int RdbStoreDecorator::Count(int64_t &outValue, const AbsRdbPredicates &predicates)
{
    if (ShouldInject(RdbFaultOp::QUERY, "Count")) {
        return E_SQLITE_CORRUPT;
    }
    return RdbStoreImpl::Count(outValue, predicates);
}

std::pair<int32_t, std::shared_ptr<Transaction>> RdbStoreDecorator::CreateTransaction(int32_t type)
{
    DISTRIBUTED_DATA_HITRACE(std::string(__FUNCTION__));
    PerfStat perfStat(dbPath_, "", PerfStat::Step::STEP_TOTAL);
    if (ShouldInject(RdbFaultOp::TRANSACTION, "CreateTransaction")) {
        return { E_SQLITE_CORRUPT, nullptr };
    }
    return RdbStoreImpl::CreateTransaction(type);
}

std::pair<int32_t, std::shared_ptr<Transaction>> RdbStoreDecorator::CreateTransactionObj(
    int32_t type, std::shared_ptr<Connection> conn)
{
    // Fault-injection mock: construct the decorated transaction with the
    // store's injector so the budget state is shared with the store.
    return TransactionImplDecorator::Create(
        type, std::move(conn), dbPath_, GetConfig().IsAuditEnabled(), injectorHolder_);
}

int RdbStoreDecorator::BeginTransaction()
{
    DISTRIBUTED_DATA_HITRACE(std::string(__FUNCTION__));
    RdbStatReporter reportStat(RDB_PERF, BEGINTRANSACTION, GetConfig(), GetReportFunc());
    if (ShouldInject(RdbFaultOp::TRANSACTION, "BeginTransaction")) {
        return E_SQLITE_CORRUPT;
    }
    return RdbStoreImpl::BeginTransaction();
}

std::pair<int, int64_t> RdbStoreDecorator::BeginTrans()
{
    DISTRIBUTED_DATA_HITRACE(std::string(__FUNCTION__));
    if (ShouldInject(RdbFaultOp::TRANSACTION, "BeginTrans")) {
        return { E_SQLITE_CORRUPT, 0 };
    }
    return RdbStoreImpl::BeginTrans();
}

int RdbStoreDecorator::RollBack()
{
    DISTRIBUTED_DATA_HITRACE(std::string(__FUNCTION__));
    RdbStatReporter reportStat(RDB_PERF, ROLLBACK, GetConfig(), GetReportFunc());
    if (ShouldInject(RdbFaultOp::TRANSACTION, "RollBack")) {
        return E_SQLITE_CORRUPT;
    }
    return RdbStoreImpl::RollBack();
}

int RdbStoreDecorator::RollBack(int64_t trxId)
{
    DISTRIBUTED_DATA_HITRACE(std::string(__FUNCTION__));
    if (ShouldInject(RdbFaultOp::TRANSACTION, "RollBack")) {
        return E_SQLITE_CORRUPT;
    }
    return RdbStoreImpl::RollBack(trxId);
}

int RdbStoreDecorator::Commit()
{
    DISTRIBUTED_DATA_HITRACE(std::string(__FUNCTION__));
    RdbStatReporter reportStat(RDB_PERF, COMMIT, GetConfig(), GetReportFunc());
    if (ShouldInject(RdbFaultOp::TRANSACTION, "Commit")) {
        return E_SQLITE_CORRUPT;
    }
    return RdbStoreImpl::Commit();
}

int RdbStoreDecorator::Commit(int64_t trxId)
{
    DISTRIBUTED_DATA_HITRACE(std::string(__FUNCTION__));
    if (ShouldInject(RdbFaultOp::TRANSACTION, "Commit")) {
        return E_SQLITE_CORRUPT;
    }
    return RdbStoreImpl::Commit(trxId);
}

} // namespace NativeRdb
} // namespace OHOS

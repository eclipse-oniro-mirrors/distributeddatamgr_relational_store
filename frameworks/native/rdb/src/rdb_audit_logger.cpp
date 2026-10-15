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

#define LOG_TAG "RdbAuditLogger"
#include "rdb_audit_logger.h"

#include <algorithm>
#include <sys/stat.h>

#include <cerrno>
#include <chrono>
#include <cinttypes>
#include <cstring>
#include <sstream>

#include "logger.h"
#include "rdb_audit_logger_manager.h"
#include "rdb_audit_utils.h"
#include "rdb_errno.h"
#include "rdb_db_info_manager.h"
#include "rdb_db_logger_manager.h"
#include "rdb_platform.h"
#include "rdb_time_utils.h"
#include "sqlite_utils.h"
#include "string_utils.h"
#include "task_executor.h"

namespace OHOS {
namespace NativeRdb {
using namespace OHOS::Rdb;

namespace {
constexpr const char *AUDIT_DIR_SA_ROOT = "/data/log/hiaudit/rdb";
constexpr const char *AUDIT_DIR_APP_ROOT = "/data/storage/el2/log";
constexpr const char *AUDIT_DIR_APP_SUB = "rdb";
constexpr int64_t THROTTLE_INTERVAL_MS = 60 * 1000; // 60 s
constexpr size_t THROTTLE_MAP_MAX_SIZE = 64;
constexpr int64_t FLUSH_THRESHOLD = 1000; // flush early when accumulated rows reach this
// Length of the "YYYY-" year prefix in "YYYY-MM-DD HH:MM:SS.mmm" (stripped by TsLog).
constexpr size_t YEAR_PREFIX_LEN = 5;

int64_t NowMs()
{
    auto dur = std::chrono::steady_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(dur).count();
}

bool IsUnderAuditRoot(const std::string &path)
{
    return path.rfind(AUDIT_DIR_SA_ROOT, 0) == 0 ||
           path.rfind(std::string(AUDIT_DIR_APP_ROOT) + "/" + AUDIT_DIR_APP_SUB, 0) == 0;
}

bool TryMkDir(const std::string &path)
{
    if (MkDir(path, AUDIT_DIR_MODE) != 0 && errno != EEXIST) {
        LOG_ERROR("ProbeAuditDir: mkdir failed, path=%{public}s, errno=%{public}d", path.c_str(), errno);
        return false;
    }
    return true;
}

// Create the full directory path, only creating segments under known audit roots
// (skipping system directories like /data, /data/log that this component does not own).
bool MkDirP(const std::string &path)
{
    if (path.empty()) {
        return false;
    }
    struct stat st;
    if (stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
        return true;
    }
    size_t pos = 0;
    while ((pos = path.find('/', pos + 1)) != std::string::npos) {
        std::string sub = path.substr(0, pos);
        struct stat subSt;
        if (stat(sub.c_str(), &subSt) == 0 && S_ISDIR(subSt.st_mode)) {
            continue;
        }
        if (IsUnderAuditRoot(sub) && !TryMkDir(sub)) {
            return false;
        }
    }
    return TryMkDir(path);
}

// Probe the shared audit root directory (SA root first, then app log root).
// Returns "" when neither root exists (audit disabled). Called once from the
// RdbAuditLoggerImpl constructor; the result is passed to both singleton managers.
std::string ProbeAuditDir()
{
    struct stat st;
    if (stat(AUDIT_DIR_SA_ROOT, &st) == 0 && S_ISDIR(st.st_mode)) {
        int32_t uid = static_cast<int32_t>(GetUid());
        std::string saDir = std::string(AUDIT_DIR_SA_ROOT) + "/" + std::to_string(uid) + "/";
        if (MkDirP(saDir)) {
            return saDir;
        }
    }
    if (stat(AUDIT_DIR_APP_ROOT, &st) == 0 && S_ISDIR(st.st_mode)) {
        std::string dir = std::string(AUDIT_DIR_APP_ROOT) + "/" + AUDIT_DIR_APP_SUB + "/";
        if (MkDirP(dir)) {
            return dir;
        }
    }
    LOG_INFO("ProbeAuditDir: no audit directory available, audit disabled");
    return "";
}

// hilog-style timestamp: "MM-DD HH:MM:SS.mmm" (strip the year prefix).
std::string TsLog()
{
    std::string ts = RdbTimeUtils::GetCurSysTimeWithMs();
    return ts.size() > YEAR_PREFIX_LEN ? ts.substr(YEAR_PREFIX_LEN) : ts;
}

// Append file info as space-separated key=value pairs (log style, not JSON).
// ACL text from getfacl contains newlines; collapse to ';' so each event
// stays on a single events.log line.
void WriteFileInfo(std::ostringstream &os, const char *label, const FileInfo &fi)
{
    std::string acl = fi.permission.acl;
    std::replace(acl.begin(), acl.end(), '\n', ';');
    os << " " << label << "_inode=" << fi.node
       << " " << label << "_size=" << fi.size
       << " " << label << "_mode=" << fi.permission.mode
       << " " << label << "_acl=" << acl
       << " " << label << "_mtime=" << fi.time.mtime;
}
} // namespace

std::unique_ptr<RdbAuditLogger> RdbAuditLogger::Create(bool auditEnabled)
{
    return auditEnabled ? std::make_unique<RdbAuditLoggerImpl>() : std::make_unique<RdbAuditLogger>();
}

RdbAuditLoggerImpl::RdbAuditLoggerImpl()
{
    if (RdbAuditLoggerManager::GetInstance().IsInitialized()) {
        enabled_ = true;
        return;
    }
    std::string auditDir = ProbeAuditDir();
    if (auditDir.empty()) {
        return;
    }
    RdbAuditLoggerManager::GetInstance().Init(auditDir);
    RdbDbLoggerManager::GetInstance().Init(auditDir);
    enabled_ = true;
}

void RdbAuditLoggerImpl::OnOpenOk(const std::string &dbPath, bool created)
{
    if (!IsActive()) {
        return;
    }
    auto executor = TaskExecutor::GetInstance().GetExecutor();
    if (executor == nullptr) {
        return;
    }
    // Collect file info synchronously to avoid TOCTOU with subsequent writes (AUD-5).
    std::string openLine = BuildOpenOkLine(dbPath);
    LastOpenDbInfo lastOpen = RdbDbInfoManager::GetInstance().BuildLastOpen(dbPath, created);
    executor->Execute([openLine = std::move(openLine), dbPath, lastOpen = std::move(lastOpen)]() {
        RdbAuditLoggerManager::GetInstance().AppendEventSync(openLine);
        RdbDbLoggerManager::GetInstance().RecordOpenSync(dbPath, lastOpen);
    });
}

void RdbAuditLoggerImpl::OnOpenFail(const std::string &dbPath, int rc, int osErrno)
{
    if (!IsActive()) {
        return;
    }
    auto executor = TaskExecutor::GetInstance().GetExecutor();
    if (executor == nullptr) {
        return;
    }
    executor->Execute([dbPath, rc, osErrno]() {
        RdbAuditLoggerManager::GetInstance().AppendEventSync(BuildOpenFailLine(dbPath, rc, osErrno));
    });
}

void RdbAuditLoggerImpl::OnIoError(
    const std::string &op, const std::string &file, int rc, int osErrno)
{
    if (!IsActive()) {
        return;
    }
    RdbDbLoggerManager::GetInstance().RecordIoError(op, file, rc, osErrno);
}

void RdbAuditLoggerImpl::OnSqlAudit(
    const std::string &dbPath, const std::string &op, const std::string &sql, int64_t rows)
{
    if (!IsActive()) {
        return;
    }
    std::string auditOp = op;
    std::string tbl;
    if (auditOp.empty()) {
        // DDL (DROP/TRUNCATE): parse op and table name from the SQL.
        auditOp = RdbAuditUtils::ParseDropTruncateOp(sql);
        if (auditOp.empty()) {
            return;
        }
        tbl = RdbAuditUtils::ParseDropTruncateTable(sql);
    } else {
        // CRUD ops: caller passes the table name via the sql parameter (AUD-9).
        tbl = sql;
    }
    bool alwaysLog = (auditOp == "DELETE" || auditOp == "DROP" || auditOp == "TRUNCATE" || auditOp == "BatchInsert");
    if (alwaysLog) {
        if (auditOp == "DELETE" && rows <= 0) {
            return;
        }
    } else if (rows <= 0) {
        return;
    }
    // Inside a transaction: buffer the event (aggregated by op+tbl); flush on
    // successful COMMIT, discard on ROLLBACK. Only non-transaction SQL is
    // written immediately / throttled.
    {
        std::lock_guard<std::mutex> lock(txnMutex_);
        if (inTransaction_) {
            txnBuffer_[{auditOp, tbl}] += rows;
            return;
        }
    }
    if (alwaysLog) {
        EmitSqlAudit(dbPath, auditOp, tbl, rows);
        return;
    }
    // Throttle stays sync (uses per-instance throttleMap_); only the write is async.
    // dbPath_ is set inside AccumulateOrFlush under mutex_ (AUD-8, destructor reads it under same lock).
    ThrottleDecision d = AccumulateOrFlush(dbPath, auditOp, tbl, rows);
    if (d.suppressed) {
        if (d.throttleStarted) {
            EmitThrottleLog(dbPath, auditOp, tbl, d.accumulated);
        }
        return;
    }
    if (d.flushRows > 0) {
        EmitSqlAudit(dbPath, auditOp, tbl, d.flushRows);
    }
}

void RdbAuditLoggerImpl::OnTransaction(
    const std::string &dbPath, const std::string &op, int rc, bool outer)
{
    if (!IsActive()) {
        return;
    }
    std::lock_guard<std::mutex> lock(txnMutex_);
    if (op == "BEGIN") {
        if (outer) {
            inTransaction_ = true;
            commitSucceeded_ = false;
            txnBuffer_.clear();
        }
        return;
    }
    if (op == "COMMIT") {
        if (outer) {
            commitSucceeded_ = (rc == E_OK);
            EmitCombinedTransaction(dbPath, "COMMIT", rc);
            txnBuffer_.clear();
            inTransaction_ = false;
        }
        return;
    }
    if (op == "ROLLBACK") {
        if (outer) {
            EmitCombinedTransaction(dbPath, "ROLLBACK", rc);
            txnBuffer_.clear();
            inTransaction_ = false;
        }
    }
}

void RdbAuditLoggerImpl::OnTransactionEvent(const std::string &dbPath, const std::string &op, int rc)
{
    if (!IsActive()) {
        return;
    }
    std::lock_guard<std::mutex> lock(txnMutex_);
    if (op == "BEGIN") {
        inTransaction_ = true;
        commitSucceeded_ = false;
        txnBuffer_.clear();
        return;
    }
    if (op == "COMMIT") {
        commitSucceeded_ = (rc == E_OK);
        EmitCombinedTransaction(dbPath, "COMMIT", rc);
        txnBuffer_.clear();
        inTransaction_ = false;
        return;
    }
    if (op == "ROLLBACK") {
        EmitCombinedTransaction(dbPath, "ROLLBACK", rc);
        txnBuffer_.clear();
        inTransaction_ = false;
    }
}

void RdbAuditLoggerImpl::EmitSqlAudit(
    const std::string &dbPath, const std::string &op, const std::string &tbl, int64_t rows)
{
    auto executor = TaskExecutor::GetInstance().GetExecutor();
    if (executor == nullptr) {
        return;
    }
    executor->Execute([dbPath, op, tbl, rows]() {
        RdbAuditLoggerManager::GetInstance().AppendEventSync(BuildSqlAuditLine(dbPath, op, tbl, rows));
    });
}

void RdbAuditLoggerImpl::EmitThrottleLog(
    const std::string &dbPath, const std::string &op, const std::string &tbl, int64_t accumulated)
{
    auto executor = TaskExecutor::GetInstance().GetExecutor();
    if (executor == nullptr) {
        return;
    }
    executor->Execute([dbPath, op, tbl, accumulated]() {
        RdbAuditLoggerManager::GetInstance().AppendEventSync(BuildThrottleLine(dbPath, op, tbl, accumulated));
    });
}

void RdbAuditLoggerImpl::EmitCombinedTransaction(
    const std::string &dbPath, const std::string &finalOp, int rc)
{
    // Must be called under txnMutex_ — reads txnBuffer_.
    std::string line = BuildCombinedTransactionLine(dbPath, finalOp, rc);
    auto executor = TaskExecutor::GetInstance().GetExecutor();
    if (executor == nullptr) {
        return;
    }
    executor->Execute([line = std::move(line)]() {
        RdbAuditLoggerManager::GetInstance().AppendEventSync(line);
    });
}

void RdbAuditLoggerImpl::OnPragma(
    const std::string &dbPath, const std::string &sql, int rc, const std::string &result)
{
    if (!IsActive()) {
        return;
    }
    RdbAuditLoggerManager::GetInstance().OnPragma(dbPath, sql, rc, result);
}

void RdbAuditLoggerImpl::OnCorrupt(
    const std::string &dbPath, int rc, int osErrno, const std::string &detail)
{
    if (!IsActive()) {
        return;
    }
    RdbDbLoggerManager::GetInstance().RecordCorrupt(dbPath, rc, osErrno, detail);
}

void RdbAuditLoggerImpl::OnDbDelete(const std::string &dbPath, const std::string &op)
{
    if (!IsActive()) {
        return;
    }
    auto executor = TaskExecutor::GetInstance().GetExecutor();
    if (executor == nullptr) {
        return;
    }
    // Collect file info synchronously before deletion takes effect (AUD-5).
    DeleteInfo del;
    del.files = RdbDbInfoManager::GetInstance().CollectDbFileInfo(dbPath);
    del.callerInfo = RdbDbInfoManager::GetInstance().CollectCaller();
    del.time = RdbTimeUtils::GetCurSysTimeWithMs();
    executor->Execute([dbPath, del = std::move(del)]() {
        RdbDbLoggerManager::GetInstance().WriteDeleteSync(dbPath, del);
    });
}

RdbAuditLoggerImpl::~RdbAuditLoggerImpl()
{
    // Transaction buffer: if still in a transaction and no successful commit
    // was recorded (commit failed or was never called), discard the buffered
    // events without writing. When a commit succeeded, the buffer was already
    // flushed by OnTransaction/OnTransactionEvent before clearing inTransaction_.
    {
        std::lock_guard<std::mutex> txnLock(txnMutex_);
        if (inTransaction_ && !commitSucceeded_) {
            txnBuffer_.clear();
            inTransaction_ = false;
        }
    }
    // Flush any accumulated throttle data that was never written (AUD-8).
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto &entry : throttleMap_) {
        if (entry.second.accumulatedRows > 0) {
            LOG_INFO("Audit throttle flush on destroy: op=%{public}s tbl=%{public}s rows=%{public}" PRId64,
                entry.second.op.c_str(), SqliteUtils::Anonymous(entry.second.tbl).c_str(),
                entry.second.accumulatedRows);
            EmitSqlAudit(dbPath_, entry.second.op, entry.second.tbl, entry.second.accumulatedRows);
        }
    }
}

RdbAuditLoggerImpl::ThrottleDecision RdbAuditLoggerImpl::AccumulateOrFlush(
    const std::string &dbPath, const std::string &op, const std::string &tbl, int64_t rows)
{
    std::lock_guard<std::mutex> lock(mutex_);
    dbPath_ = dbPath; // remember for destructor flush (AUD-8)
    std::string eventKey = "SQL_AUDIT:" + op + ":" + tbl;
    ThrottleDecision d;
    int64_t now = NowMs();
    auto it = throttleMap_.find(eventKey);
    if (it != throttleMap_.end() && (now - it->second.timestamp) < THROTTLE_INTERVAL_MS) {
        it->second.accumulatedRows += rows;
        d.accumulated = it->second.accumulatedRows;
        if (it->second.accumulatedRows >= FLUSH_THRESHOLD) {
            d.flushRows = it->second.accumulatedRows;
            LOG_INFO("Audit throttle early-flush: op=%{public}s tbl=%{public}s flushRows=%{public}" PRId64,
                op.c_str(), SqliteUtils::Anonymous(tbl).c_str(), d.flushRows);
            it->second = {now, 0, false, op, tbl};
            return d;
        }
        if (!it->second.throttleLogged) {
            it->second.throttleLogged = true;
            d.throttleStarted = true;
        }
        d.suppressed = true;
        LOG_INFO("Audit throttle accumulated: op=%{public}s tbl=%{public}s rows=%{public}" PRId64
            " accumulated=%{public}" PRId64,
            op.c_str(), SqliteUtils::Anonymous(tbl).c_str(), rows, d.accumulated);
        return d;
    }
    if (it != throttleMap_.end()) {
        // Window expired: flush accumulated rows from the previous window.
        d.flushRows = it->second.accumulatedRows;
        if (d.flushRows > 0) {
            LOG_INFO("Audit throttle window-expired flush: op=%{public}s tbl=%{public}s flushRows=%{public}" PRId64,
                op.c_str(), SqliteUtils::Anonymous(tbl).c_str(), d.flushRows);
        }
        it->second = {now, rows, false, op, tbl};
    } else {
        throttleMap_[eventKey] = {now, rows, false, op, tbl};
    }
    if (throttleMap_.size() > THROTTLE_MAP_MAX_SIZE) {
        for (auto mapIt = throttleMap_.begin(); mapIt != throttleMap_.end();) {
            if (mapIt->first != eventKey && (now - mapIt->second.timestamp) >= THROTTLE_INTERVAL_MS) {
                LOG_INFO("Audit throttle evict expired entry: op=%{public}s accumulated=%{public}" PRId64,
                    mapIt->second.op.c_str(), mapIt->second.accumulatedRows);
                mapIt = throttleMap_.erase(mapIt);
            } else {
                ++mapIt;
            }
        }
    }
    return d;
}

std::string RdbAuditLoggerImpl::BuildOpenOkLine(const std::string &dbPath)
{
    std::string ts = TsLog();
    auto caller = RdbDbInfoManager::GetInstance().CollectCaller();
    auto fileInfo = RdbDbInfoManager::GetInstance().CollectDbFileInfo(dbPath);
    auto slaveInfo = RdbDbInfoManager::GetInstance().CollectDbFileInfo(SqliteUtils::GetSlavePath(dbPath));
    std::string dbName = SqliteUtils::Anonymous(StringUtils::ExtractFileName(dbPath));
    std::ostringstream os;
    os << ts << " " << caller.pid << " " << caller.tid << " OPEN:"
       << " db=" << dbName << " proc=pid:" << caller.pid << ":tid:" << caller.tid;
    WriteFileInfo(os, "db", fileInfo.db);
    WriteFileInfo(os, "wal", fileInfo.wal);
    WriteFileInfo(os, "shm", fileInfo.shm);
    std::string dirAcl = fileInfo.parent.permission.acl;
    std::replace(dirAcl.begin(), dirAcl.end(), '\n', ';');
    os << " slave_db_inode=" << slaveInfo.db.node << " slave_wal_inode=" << slaveInfo.wal.node
       << " slave_shm_inode=" << slaveInfo.shm.node
       << " dir_mode=" << fileInfo.parent.permission.mode
       << " dir_acl=" << dirAcl;
    return os.str();
}

std::string RdbAuditLoggerImpl::BuildOpenFailLine(const std::string &dbPath, int rc, int osErrno)
{
    std::string ts = TsLog();
    auto caller = RdbDbInfoManager::GetInstance().CollectCaller();
    std::string dbName = SqliteUtils::Anonymous(StringUtils::ExtractFileName(dbPath));
    std::ostringstream os;
    os << ts << " " << caller.pid << " " << caller.tid << " OFAIL:"
       << " db=" << dbName << " proc=pid:" << caller.pid << ":tid:" << caller.tid
       << " rc=" << rc << " os_errno=" << osErrno
       << " path=" << SqliteUtils::Anonymous(dbPath);
    return os.str();
}

std::string RdbAuditLoggerImpl::BuildSqlAuditLine(
    const std::string &dbPath, const std::string &op, const std::string &tbl, int64_t rows)
{
    std::string ts = TsLog();
    auto caller = RdbDbInfoManager::GetInstance().CollectCaller();
    std::string dbName = SqliteUtils::Anonymous(StringUtils::ExtractFileName(dbPath));
    std::ostringstream os;
    os << ts << " " << caller.pid << " " << caller.tid << " SQL:"
       << " db=" << dbName << " op=" << op << " tbl=" << SqliteUtils::Anonymous(tbl)
       << " rows=" << rows << " caller=pid:" << caller.pid << ":tid:" << caller.tid;
    return os.str();
}

std::string RdbAuditLoggerImpl::BuildCombinedTransactionLine(
    const std::string &dbPath, const std::string &finalOp, int rc)
{
    // Must be called under txnMutex_ — reads txnBuffer_.
    std::string ts = TsLog();
    auto caller = RdbDbInfoManager::GetInstance().CollectCaller();
    std::string dbName = SqliteUtils::Anonymous(StringUtils::ExtractFileName(dbPath));
    std::ostringstream os;
    os << ts << " " << caller.pid << " " << caller.tid << " TRX:"
       << " db=" << dbName << " BEGIN";
    for (const auto &entry : txnBuffer_) {
        os << ">" << entry.first.first
           << "(tbl=" << SqliteUtils::Anonymous(entry.first.second)
           << ",rows=" << entry.second << ")";
    }
    os << ">" << finalOp << " rc=" << rc
       << " caller=pid:" << caller.pid << ":tid:" << caller.tid;
    return os.str();
}

std::string RdbAuditLoggerImpl::BuildThrottleLine(
    const std::string &dbPath, const std::string &op, const std::string &tbl, int64_t accumulated)
{
    std::string ts = TsLog();
    auto caller = RdbDbInfoManager::GetInstance().CollectCaller();
    std::string dbName = SqliteUtils::Anonymous(StringUtils::ExtractFileName(dbPath));
    std::ostringstream os;
    os << ts << " " << caller.pid << " " << caller.tid << " THROTTLE:"
       << " db=" << dbName << " op=" << op << " tbl=" << SqliteUtils::Anonymous(tbl)
       << " accumulated=" << accumulated << " caller=pid:" << caller.pid << ":tid:" << caller.tid;
    return os.str();
}

} // namespace NativeRdb
} // namespace OHOS

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

#ifndef RDB_AUDIT_LOGGER_H
#define RDB_AUDIT_LOGGER_H

#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>

namespace OHOS {
namespace NativeRdb {

// Audit logger interface. The base class is a no-op implementation: every On*
// method is an empty inline body, so callers that hold a base instance pay no
// I/O cost. RdbAuditLoggerImpl (below) overrides the methods to actually
// collect context and persist to events.log / audit.json via the singleton
// managers. RdbStoreImpl holds a pointer obtained from
// RdbAuditLogger::Create(config) and invokes On* directly — no
// IsAuditEnabled gating at the call site. When audit is disabled Create
// returns a base instance (all calls are no-ops); when enabled it returns an
// Impl instance whose constructor has already probed the audit root and
// initialized the singleton managers.
//
// Writes are best-effort and asynchronous: they never block the caller's
// database operations.
class RdbAuditLogger {
public:
    RdbAuditLogger() = default;
    virtual ~RdbAuditLogger() = default;
    RdbAuditLogger(const RdbAuditLogger &) = delete;
    RdbAuditLogger &operator=(const RdbAuditLogger &) = delete;

    // Factory: returns an RdbAuditLoggerImpl when auditEnabled is true,
    // otherwise a no-op base instance.
    static std::unique_ptr<RdbAuditLogger> Create(bool auditEnabled);

    virtual void OnOpenOk(const std::string &dbPath, bool created) {}
    virtual void OnOpenFail(const std::string &dbPath, int rc, int osErrno) {}
    virtual void OnIoError(const std::string &op, const std::string &file, int rc, int osErrno) {}
    virtual void OnSqlAudit(const std::string &dbPath, const std::string &op, const std::string &sql, int64_t rows) {}
    virtual void OnPragma(const std::string &dbPath, const std::string &sql, int rc, const std::string &result) {}
    virtual void OnCorrupt(const std::string &dbPath, int rc, int osErrno, const std::string &detail) {}
    virtual void OnDbDelete(const std::string &dbPath, const std::string &op) {}
    // op is one of "BEGIN" / "COMMIT" / "ROLLBACK"; rc is the statement result code.
    // outer=true marks the outermost transaction boundary (stack transitions to/from
    // empty). SQL events between BEGIN and the outermost COMMIT/ROLLBACK are buffered:
    // flushed to events.log only on successful outermost COMMIT, discarded on ROLLBACK.
    virtual void OnTransaction(const std::string &dbPath, const std::string &op, int rc, bool outer) {}
};

// Real audit implementation. The constructor probes the shared audit root
// and initializes the singleton managers (so On* needs no lazy init).
// Delegates all persistence to the RdbAuditLoggerManager / RdbDbLoggerManager
// singletons. Created only when config.IsAuditEnabled() is true (via
// RdbAuditLogger::Create).
class RdbAuditLoggerImpl : public RdbAuditLogger {
public:
    RdbAuditLoggerImpl();
    ~RdbAuditLoggerImpl() override; // AUD-8: flush pending throttle data
    RdbAuditLoggerImpl(const RdbAuditLoggerImpl &) = delete;
    RdbAuditLoggerImpl &operator=(const RdbAuditLoggerImpl &) = delete;

    void OnOpenOk(const std::string &dbPath, bool created) override;
    void OnOpenFail(const std::string &dbPath, int rc, int osErrno) override;
    void OnIoError(const std::string &op, const std::string &file, int rc, int osErrno) override;
    void OnSqlAudit(const std::string &dbPath, const std::string &op, const std::string &sql, int64_t rows) override;
    void OnPragma(const std::string &dbPath, const std::string &sql, int rc, const std::string &result) override;
    void OnCorrupt(const std::string &dbPath, int rc, int osErrno, const std::string &detail) override;
    void OnDbDelete(const std::string &dbPath, const std::string &op) override;
    void OnTransaction(const std::string &dbPath, const std::string &op, int rc, bool outer) override;

private:
    // Outcome of a throttle window lookup for INSERT/UPDATE (high-frequency
    // ops that accumulate rows within a 60s window before flushing).
    struct ThrottleDecision {
        bool suppressed = false;       // event accumulated (not flushed now)
        bool throttleStarted = false;  // first suppression in window - emit THROTTLE log
        int64_t flushRows = 0;         // accumulated rows to flush now
        int64_t accumulated = 0;       // total accumulated rows (for throttle log)
    };
    ThrottleDecision AccumulateOrFlush(
        const std::string &dbPath, const std::string &op, const std::string &tbl, int64_t rows);
    bool IsActive() const { return enabled_; }

    void EmitSqlAudit(const std::string &dbPath, const std::string &op, const std::string &tbl, int64_t rows);
    void EmitThrottleLog(const std::string &dbPath, const std::string &op, const std::string &tbl,
        int64_t accumulated);
    void EmitTransaction(const std::string &dbPath, const std::string &op, int rc);
    void FlushTxnBuffer(const std::string &dbPath);

    static std::string BuildOpenOkLine(const std::string &dbPath);
    static std::string BuildOpenFailLine(const std::string &dbPath, int rc, int osErrno);
    static std::string BuildSqlAuditLine(
        const std::string &dbPath, const std::string &op, const std::string &tbl, int64_t rows);
    static std::string BuildTransactionLine(const std::string &dbPath, const std::string &op, int rc);
    static std::string BuildThrottleLine(
        const std::string &dbPath, const std::string &op, const std::string &tbl, int64_t accumulated);

    bool enabled_ = false;
    std::string dbPath_; // remembered for destructor throttle flush (AUD-8)
    struct ThrottleEntry {
        int64_t timestamp = 0;
        int64_t accumulatedRows = 0;
        bool throttleLogged = false; // THROTTLE line already emitted for this window
        std::string op;  // for destructor flush (AUD-8)
        std::string tbl; // for destructor flush (AUD-8)
    };
    std::unordered_map<std::string, ThrottleEntry> throttleMap_;
    std::mutex mutex_;

    // Transaction buffering: SQL events inside a transaction are buffered (aggregated
    // by op+tbl) and only flushed to events.log on successful outermost COMMIT;
    // discarded on outermost ROLLBACK. BEGIN/ROLLBACK are never written; only the
    // successful COMMIT produces a TRX line (plus the flushed SQL).
    std::mutex txnMutex_;
    bool inTransaction_ = false;
    std::map<std::pair<std::string, std::string>, int64_t> txnBuffer_;
};

} // namespace NativeRdb
} // namespace OHOS

#endif // RDB_AUDIT_LOGGER_H

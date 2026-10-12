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

#include <fcntl.h>
#include <gtest/gtest.h>
#include <sys/stat.h>
#include <unistd.h>

#include <chrono>
#include <cstdint>
#include <cstring>
#include <string>
#include <thread>

#include "rdb_audit_logger.h"
#include "rdb_audit_logger_manager.h"
#include "rdb_audit_utils.h"
#include "rdb_db_info_manager.h"
#include "rdb_db_logger_manager.h"
#include "rdb_errno.h"
#include "rdb_helper.h"
#include "rdb_open_callback.h"
#include "rdb_predicates.h"
#include "rdb_store_config.h"
#include "rdb_types.h"
#include "sqlite_utils.h"
#include "string_utils.h"
#include "values_bucket.h"

using namespace testing::ext;
using namespace OHOS::NativeRdb;

namespace {
constexpr const char *TEST_BASE_DIR = "/data/test/rdb_audit";
constexpr size_t READ_BUF_SIZE = 4096;
constexpr int POLL_INTERVAL_MS = 1000;
const std::string::size_type NPOS = std::string::npos;
constexpr int DRAIN_OBSERVE_MS = 2000;
constexpr int WAIT_TIMEOUT_MS = 3000;
constexpr int RC_OK = 0;
constexpr int RC_ERR = 14;

bool MakeDirRecursive(const std::string &path, mode_t mode)
{
    if (path.empty()) {
        return false;
    }
    struct stat st;
    if (stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
        return true;
    }
    size_t pos = 1;
    while ((pos = path.find('/', pos)) != std::string::npos) {
        std::string sub = path.substr(0, pos);
        if (stat(sub.c_str(), &st) != 0) {
            mkdir(sub.c_str(), mode);
        }
        pos++;
    }
    if (stat(path.c_str(), &st) != 0) {
        return mkdir(path.c_str(), mode) == 0 || errno == EEXIST;
    }
    return true;
}

std::string ReadFileContent(const std::string &path)
{
    int fd = open(path.c_str(), O_RDONLY);
    if (fd < 0) {
        return "";
    }
    std::string content;
    char buf[READ_BUF_SIZE];
    ssize_t n = 0;
    while ((n = read(fd, buf, sizeof(buf))) > 0) {
        content.append(buf, static_cast<size_t>(n));
    }
    close(fd);
    return content;
}

size_t CountLines(const std::string &content)
{
    if (content.empty()) {
        return 0;
    }
    size_t count = 0;
    for (char c : content) {
        if (c == '\n') {
            count++;
        }
    }
    return count;
}

bool FileExists(const std::string &path)
{
    struct stat st;
    return stat(path.c_str(), &st) == 0;
}

std::string AuditDir()
{
    return std::string(TEST_BASE_DIR) + "/.audit/";
}

std::string DbPath()
{
    return std::string(TEST_BASE_DIR) + "/el2/database/test.db";
}

std::string DbDir()
{
    std::string path = DbPath();
    size_t pos = path.rfind('/');
    return (pos == std::string::npos) ? TEST_BASE_DIR : path.substr(0, pos);
}

std::string EventsLogPath()
{
    return AuditDir() + "events.log";
}

std::string AuditJsonPath()
{
    return AuditDir() + SqliteUtils::GetArea(DbPath()) +
        SqliteUtils::RemoveSuffix(StringUtils::ExtractFileName(DbPath())) + "_audit.json";
}

void DrainPendingAuditWrites()
{
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(WAIT_TIMEOUT_MS);
    auto observeEnd = std::chrono::steady_clock::now() + std::chrono::milliseconds(DRAIN_OBSERVE_MS);
    while (std::chrono::steady_clock::now() < deadline) {
        if (FileExists(AuditJsonPath())) {
            unlink(AuditJsonPath().c_str());
            observeEnd = std::chrono::steady_clock::now() + std::chrono::milliseconds(DRAIN_OBSERVE_MS);
        } else if (std::chrono::steady_clock::now() >= observeEnd) {
            return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(POLL_INTERVAL_MS));
    }
}

void SetupAudit()
{
    MakeDirRecursive(AuditDir(), AUDIT_DIR_MODE);
    RdbAuditLoggerManager::GetInstance().Init(AuditDir());
    RdbDbLoggerManager::GetInstance().Init(AuditDir());
    truncate(EventsLogPath().c_str(), 0);
    unlink(AuditJsonPath().c_str());
    DrainPendingAuditWrites();
}

size_t WaitEventLines(size_t expected, int timeoutMs = WAIT_TIMEOUT_MS)
{
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (std::chrono::steady_clock::now() < deadline) {
        if (CountLines(ReadFileContent(EventsLogPath())) >= expected) {
            return CountLines(ReadFileContent(EventsLogPath()));
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(POLL_INTERVAL_MS));
    }
    return CountLines(ReadFileContent(EventsLogPath()));
}

RdbStoreConfig MakeAuditConfig()
{
    RdbStoreConfig config(DbPath());
    config.SetBundleName("audit_test_app");
    config.SetAuditEnabled(true);
    config.SetIntegrityCheck(IntegrityCheck::FULL);
    return config;
}
} // namespace

class AuditOpenCallback : public RdbOpenCallback {
public:
    int OnCreate(RdbStore &store) override
    {
        return store.ExecuteSql("CREATE TABLE IF NOT EXISTS users (id INTEGER PRIMARY KEY, name TEXT)");
    }
    int OnUpgrade(RdbStore &store, int oldVersion, int newVersion) override
    {
        return E_OK;
    }
};

class RdbAuditTxnTest : public testing::Test {
public:
    static void SetUpTestCase(void)
    {
        RdbHelper::DeleteRdbStore(DbPath());
    }
    static void TearDownTestCase(void)
    {
        RdbHelper::DeleteRdbStore(DbPath());
    }

    void SetUp() override
    {
        RdbHelper::DeleteRdbStore(DbPath());
        MakeDirRecursive(TEST_BASE_DIR, AUDIT_DIR_MODE);
        MakeDirRecursive(DbDir(), AUDIT_DIR_MODE);
        SetupAudit();
    }

    void TearDown() override
    {
        RdbHelper::DeleteRdbStore(DbPath());
    }
};

// ===== Throttle (AccumulateOrFlush) =====

/**
 * @tc.name: RdbAuditTxn_OnThrottle_Basic_001
 * @tc.desc: 1st INSERT starts window (no emit); 2nd emits THROTTLE once; 3rd silent.
 * @tc.type: FUNC
 */
HWTEST_F(RdbAuditTxnTest, OnThrottle_Basic_001, TestSize.Level0)
{
    RdbAuditLoggerImpl logger;
    // 1st: starts 60s window, stored, no emit.
    logger.OnSqlAudit(DbPath(), "INSERT", "tblA", 1);
    EXPECT_EQ(CountLines(ReadFileContent(EventsLogPath())), 0);
    // 2nd: suppressed; throttle starts → one THROTTLE line.
    logger.OnSqlAudit(DbPath(), "INSERT", "tblA", 1);
    EXPECT_EQ(WaitEventLines(1), 1);
    EXPECT_NE(ReadFileContent(EventsLogPath()).find("THROTTLE:"), NPOS);
    // 3rd: suppressed; throttleLogged already true → no new line.
    logger.OnSqlAudit(DbPath(), "INSERT", "tblA", 1);
    EXPECT_EQ(CountLines(ReadFileContent(EventsLogPath())), 1);
}

/**
 * @tc.name: RdbAuditTxn_OnThrottle_EarlyFlush_002
 * @tc.desc: Accumulated rows >= FLUSH_THRESHOLD(1000) triggers early flush.
 * @tc.type: FUNC
 */
HWTEST_F(RdbAuditTxnTest, OnThrottle_EarlyFlush_002, TestSize.Level0)
{
    RdbAuditLoggerImpl logger;
    // 1st call: starts window (no emit).
    logger.OnSqlAudit(DbPath(), "INSERT", "tblA", 1);
    // 2nd call: suppressed, emits THROTTLE.
    logger.OnSqlAudit(DbPath(), "INSERT", "tblA", 1);
    // Calls 3..999: suppressed silently (accumulated = 2 + 997 = 999, below threshold).
    for (int i = 0; i < 997; ++i) {
        logger.OnSqlAudit(DbPath(), "INSERT", "tblA", 1);
    }
    EXPECT_EQ(CountLines(ReadFileContent(EventsLogPath())), 1); // only THROTTLE so far
    // 1000th call: accumulated = 999 + 1 = 1000 >= 1000 → early flush.
    logger.OnSqlAudit(DbPath(), "INSERT", "tblA", 1);
    EXPECT_EQ(WaitEventLines(2, 10000), 2);
    auto content = ReadFileContent(EventsLogPath());
    EXPECT_NE(content.find("THROTTLE:"), NPOS);
    EXPECT_NE(content.find("op=INSERT"), NPOS);
}

/**
 * @tc.name: RdbAuditTxn_OnThrottle_DestructorFlush_003
 * @tc.desc: ~RdbAuditLoggerImpl flushes pending throttle data that was never written.
 * @tc.type: FUNC
 */
HWTEST_F(RdbAuditTxnTest, OnThrottle_DestructorFlush_003, TestSize.Level0)
{
    {
        RdbAuditLoggerImpl logger;
        // 1st INSERT: starts window, stored, no emit.
        logger.OnSqlAudit(DbPath(), "INSERT", "tblA", 1);
        EXPECT_EQ(CountLines(ReadFileContent(EventsLogPath())), 0);
        // Logger goes out of scope → destructor flushes 1 pending row.
    }
    EXPECT_EQ(WaitEventLines(1), 1);
    EXPECT_NE(ReadFileContent(EventsLogPath()).find("op=INSERT"), NPOS);
}

/**
 * @tc.name: RdbAuditTxn_OnThrottle_MultiTable_004
 * @tc.desc: Different op+tbl combinations have independent throttle windows.
 * @tc.type: FUNC
 */
HWTEST_F(RdbAuditTxnTest, OnThrottle_MultiTable_004, TestSize.Level0)
{
    RdbAuditLoggerImpl logger;
    // 1st INSERT tblA: starts window, no emit.
    logger.OnSqlAudit(DbPath(), "INSERT", "tblA", 1);
    // 1st INSERT tblB: different key, starts own window, no emit.
    logger.OnSqlAudit(DbPath(), "INSERT", "tblB", 1);
    EXPECT_EQ(CountLines(ReadFileContent(EventsLogPath())), 0);
    // 2nd INSERT tblA: suppressed, THROTTLE for tblA.
    logger.OnSqlAudit(DbPath(), "INSERT", "tblA", 1);
    // 2nd INSERT tblB: suppressed, THROTTLE for tblB.
    logger.OnSqlAudit(DbPath(), "INSERT", "tblB", 1);
    EXPECT_EQ(WaitEventLines(2), 2);
    auto content = ReadFileContent(EventsLogPath());
    EXPECT_NE(content.find("tbl=tbl***"), NPOS);
}

// ===== OnTransaction (stack-based, buffer management) =====

/**
 * @tc.name: RdbAuditTxn_OnTransaction_Nested_005
 * @tc.desc: Inner BEGIN/COMMIT/ROLLBACK (outer=false) are no-ops; only outermost
 *          COMMIT emits one combined line (BEGIN>INSERT>COMMIT).
 * @tc.type: FUNC
 */
HWTEST_F(RdbAuditTxnTest, OnTransaction_Nested_005, TestSize.Level0)
{
    RdbAuditLoggerImpl logger;
    // Outer BEGIN: inTransaction_=true, buffer cleared.
    logger.OnTransaction(DbPath(), "BEGIN", RC_OK, true);
    // Inner BEGIN (outer=false): no-op.
    logger.OnTransaction(DbPath(), "BEGIN", RC_OK, false);
    // SQL: buffered.
    logger.OnSqlAudit(DbPath(), "INSERT", "tblA", 5);
    EXPECT_EQ(CountLines(ReadFileContent(EventsLogPath())), 0);
    // Inner COMMIT (outer=false): no-op, buffer NOT flushed.
    logger.OnTransaction(DbPath(), "COMMIT", RC_OK, false);
    EXPECT_EQ(CountLines(ReadFileContent(EventsLogPath())), 0);
    // Inner ROLLBACK (outer=false): no-op, buffer NOT discarded.
    logger.OnTransaction(DbPath(), "ROLLBACK", RC_OK, false);
    EXPECT_EQ(CountLines(ReadFileContent(EventsLogPath())), 0);
    // Outer COMMIT: one combined line.
    logger.OnTransaction(DbPath(), "COMMIT", RC_OK, true);
    EXPECT_EQ(WaitEventLines(1), 1);
    auto content = ReadFileContent(EventsLogPath());
    EXPECT_NE(content.find("TRX:"), NPOS);
    EXPECT_NE(content.find("INSERT("), NPOS);
    EXPECT_NE(content.find("rows=5"), NPOS);
    EXPECT_NE(content.find(">COMMIT"), NPOS);
    EXPECT_EQ(content.find("SQL:"), NPOS); // no separate SQL lines
}

/**
 * @tc.name: RdbAuditTxn_OnTransaction_Aggregation_006
 * @tc.desc: INSERT + UPDATE to same table are aggregated separately in txnBuffer_,
 *          emitted as one combined line on COMMIT.
 * @tc.type: FUNC
 */
HWTEST_F(RdbAuditTxnTest, OnTransaction_Aggregation_006, TestSize.Level0)
{
    RdbAuditLoggerImpl logger;
    logger.OnTransaction(DbPath(), "BEGIN", RC_OK, true);
    logger.OnSqlAudit(DbPath(), "INSERT", "tblA", 5);
    logger.OnSqlAudit(DbPath(), "UPDATE", "tblA", 3);
    logger.OnSqlAudit(DbPath(), "INSERT", "tblA", 2); // aggregated → INSERT rows=7
    EXPECT_EQ(CountLines(ReadFileContent(EventsLogPath())), 0);
    logger.OnTransaction(DbPath(), "COMMIT", RC_OK, true);
    EXPECT_EQ(WaitEventLines(1), 1); // one combined line
    auto content = ReadFileContent(EventsLogPath());
    EXPECT_NE(content.find("INSERT("), NPOS);
    EXPECT_NE(content.find("rows=7"), NPOS);
    EXPECT_NE(content.find("UPDATE("), NPOS);
    EXPECT_NE(content.find("rows=3"), NPOS);
    EXPECT_NE(content.find(">COMMIT"), NPOS);
    EXPECT_EQ(content.find("SQL:"), NPOS); // no separate SQL lines
}

/**
 * @tc.name: RdbAuditTxn_OnTransaction_RollbackDiscards_007
 * @tc.desc: ROLLBACK emits one combined line showing the discarded ops.
 * @tc.type: FUNC
 */
HWTEST_F(RdbAuditTxnTest, OnTransaction_RollbackDiscards_007, TestSize.Level0)
{
    RdbAuditLoggerImpl logger;
    logger.OnTransaction(DbPath(), "BEGIN", RC_OK, true);
    logger.OnSqlAudit(DbPath(), "INSERT", "tblA", 5);
    logger.OnSqlAudit(DbPath(), "UPDATE", "tblA", 3);
    EXPECT_EQ(CountLines(ReadFileContent(EventsLogPath())), 0);
    logger.OnTransaction(DbPath(), "ROLLBACK", RC_OK, true);
    EXPECT_EQ(WaitEventLines(1), 1); // one combined ROLLBACK line
    auto content = ReadFileContent(EventsLogPath());
    EXPECT_NE(content.find("INSERT("), NPOS);
    EXPECT_NE(content.find("UPDATE("), NPOS);
    EXPECT_NE(content.find(">ROLLBACK"), NPOS);
    // After ROLLBACK, inTransaction_ is false; new SQL goes through throttle.
    logger.OnSqlAudit(DbPath(), "INSERT", "tblB", 1);
    EXPECT_EQ(CountLines(ReadFileContent(EventsLogPath())), 1); // 1st in new window, no emit
}

/**
 * @tc.name: RdbAuditTxn_OnTransaction_Disabled_008
 * @tc.desc: No-op base RdbAuditLogger produces no output for any On* call.
 * @tc.type: FUNC
 */
HWTEST_F(RdbAuditTxnTest, OnTransaction_Disabled_008, TestSize.Level0)
{
    auto logger = RdbAuditLogger::Create(false);
    logger->OnTransaction(DbPath(), "BEGIN", RC_OK, true);
    logger->OnSqlAudit(DbPath(), "INSERT", "tblA", 1);
    logger->OnTransaction(DbPath(), "COMMIT", RC_OK, true);
    logger->OnTransactionEvent(DbPath(), "ROLLBACK", RC_ERR);
    logger->OnPragma(DbPath(), "PRAGMA integrity_check", RC_OK, "ok");
    EXPECT_EQ(CountLines(ReadFileContent(EventsLogPath())), 0);
}

// ===== Real DB integration =====

/**
 * @tc.name: RdbAuditTxn_RealDbTransaction_009
 * @tc.desc: Real DB: committed transaction emits combined line; rolled-back also emits.
 * @tc.type: FUNC
 */
HWTEST_F(RdbAuditTxnTest, RealDbTransaction_009, TestSize.Level0)
{
    AuditOpenCallback cb;
    int err = E_OK;
    auto store = RdbHelper::GetRdbStore(MakeAuditConfig(), 1, cb, err);
    ASSERT_NE(store, nullptr);
    // Committed transaction: INSERT buffered → combined COMMIT line.
    store->BeginTransaction();
    ValuesBucket b;
    b.Put("id", 1);
    b.Put("name", "a");
    store->Insert("users", b);
    store->Commit();
    // Rolled-back transaction: INSERT buffered → combined ROLLBACK line.
    store->BeginTransaction();
    ValuesBucket b2;
    b2.Put("id", 2);
    b2.Put("name", "b");
    store->Insert("users", b2);
    store->RollBack();
    // open (2) + combined COMMIT(1) + combined ROLLBACK(1) = 4.
    EXPECT_EQ(WaitEventLines(4), 4);
    auto content = ReadFileContent(EventsLogPath());
    EXPECT_NE(content.find("INSERT("), NPOS);
    EXPECT_NE(content.find(">COMMIT"), NPOS);
    EXPECT_NE(content.find(">ROLLBACK"), NPOS);
    EXPECT_EQ(content.find("SQL:"), NPOS); // no separate SQL lines
}

/**
 * @tc.name: RdbAuditTxn_RealDbCreateTransaction_010
 * @tc.desc: CreateTransaction path: SQL buffered, COMMIT/ROLLBACK emit combined lines.
 * @tc.type: FUNC
 */
HWTEST_F(RdbAuditTxnTest, RealDbCreateTransaction_010, TestSize.Level0)
{
    AuditOpenCallback cb;
    int err = E_OK;
    auto store = RdbHelper::GetRdbStore(MakeAuditConfig(), 1, cb, err);
    ASSERT_NE(store, nullptr);
    auto [ret, trans] = store->CreateTransaction(Transaction::EXCLUSIVE);
    ASSERT_EQ(ret, E_OK);
    ASSERT_NE(trans, nullptr);
    ValuesBucket b;
    b.Put("id", 1);
    b.Put("name", "a");
    trans->Insert("users", b);
    trans->Commit();
    auto [ret2, trans2] = store->CreateTransaction(Transaction::EXCLUSIVE);
    ASSERT_EQ(ret2, E_OK);
    ASSERT_NE(trans2, nullptr);
    ValuesBucket b2;
    b2.Put("id", 2);
    b2.Put("name", "b");
    trans2->Insert("users", b2);
    trans2->Rollback();
    // open(2) + trans conn PRG(1) + combined COMMIT(1) + combined ROLLBACK(1) = 5.
    EXPECT_EQ(WaitEventLines(5), 5);
    auto content = ReadFileContent(EventsLogPath());
    EXPECT_NE(content.find(">COMMIT"), NPOS);
    EXPECT_NE(content.find(">ROLLBACK"), NPOS);
    EXPECT_NE(content.find("INSERT("), NPOS);
    EXPECT_EQ(content.find("SQL:"), NPOS); // no separate SQL lines
}

// ===== OnTransactionEvent (connection-based, no buffer) =====

/**
 * @tc.name: RdbAuditTxn_OnTransactionEvent_CommitFail_011
 * @tc.desc: OnTransactionEvent BEGIN + ROLLBACK(error) emits one combined line with rc=err.
 * @tc.type: FUNC
 */
HWTEST_F(RdbAuditTxnTest, OnTransactionEvent_CommitFail_011, TestSize.Level0)
{
    RdbAuditLoggerImpl logger;
    // BEGIN: sets inTransaction_, no emit.
    logger.OnTransactionEvent(DbPath(), "BEGIN", RC_OK);
    EXPECT_EQ(CountLines(ReadFileContent(EventsLogPath())), 0);
    // ROLLBACK with error: one combined line (BEGIN>ROLLBACK rc=14).
    logger.OnTransactionEvent(DbPath(), "ROLLBACK", RC_ERR);
    EXPECT_EQ(WaitEventLines(1), 1);
    auto content = ReadFileContent(EventsLogPath());
    EXPECT_NE(content.find("TRX:"), NPOS);
    EXPECT_NE(content.find("BEGIN"), NPOS);
    EXPECT_NE(content.find("ROLLBACK"), NPOS);
    EXPECT_NE(content.find("rc=" + std::to_string(RC_ERR)), NPOS);
}

// ===== OnSqlAudit edge cases =====

/**
 * @tc.name: RdbAuditTxn_OnSqlAudit_DeleteZeroRows_012
 * @tc.desc: DELETE with rows=0 produces no audit line (alwaysLog but rows<=0 filter).
 * @tc.type: FUNC
 */
HWTEST_F(RdbAuditTxnTest, OnSqlAudit_DeleteZeroRows_012, TestSize.Level0)
{
    RdbAuditLoggerImpl logger;
    logger.OnSqlAudit(DbPath(), "DELETE", "tblA", 0);
    EXPECT_EQ(CountLines(ReadFileContent(EventsLogPath())), 0);
    // rows>0 should still write.
    logger.OnSqlAudit(DbPath(), "DELETE", "tblA", 1);
    EXPECT_EQ(WaitEventLines(1), 1);
    EXPECT_NE(ReadFileContent(EventsLogPath()).find("op=DELETE"), NPOS);
}

/**
 * @tc.name: RdbAuditTxn_OnSqlAudit_InsertZeroRows_013
 * @tc.desc: INSERT with rows=0 produces no audit line (non-alwaysLog, rows<=0 filter).
 * @tc.type: FUNC
 */
HWTEST_F(RdbAuditTxnTest, OnSqlAudit_InsertZeroRows_013, TestSize.Level0)
{
    RdbAuditLoggerImpl logger;
    logger.OnSqlAudit(DbPath(), "INSERT", "tblA", 0);
    EXPECT_EQ(CountLines(ReadFileContent(EventsLogPath())), 0);
    // rows>0 starts throttle window (no immediate emit).
    logger.OnSqlAudit(DbPath(), "INSERT", "tblA", 1);
    EXPECT_EQ(CountLines(ReadFileContent(EventsLogPath())), 0);
}

// ===== Rate limit (global, 60 lines/s) =====

/**
 * @tc.name: RdbAuditTxn_RateLimit_014
 * @tc.desc: At most 60 lines/s; excess replaced by one RATE_LIMIT summary line.
 * @tc.type: FUNC
 */
HWTEST_F(RdbAuditTxnTest, RateLimit_014, TestSize.Level0)
{
    truncate(EventsLogPath().c_str(), 0);
    auto &mgr = RdbAuditLoggerManager::GetInstance();
    // 100 sync writes in the same 1s window: 60 written, 40 dropped.
    for (int i = 0; i < 100; ++i) {
        mgr.AppendEventSync("{\"evt\":\"TEST\",\"i\":" + std::to_string(i) + "}");
    }
    EXPECT_EQ(CountLines(ReadFileContent(EventsLogPath())), 60);
    // Wait for window reset: next call emits one RATE_LIMIT summary + the line.
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    mgr.AppendEventSync("{\"evt\":\"AFTER\"}");
    auto content = ReadFileContent(EventsLogPath());
    EXPECT_EQ(CountLines(content), 62); // 60 + 1 RATE_LIMIT + 1 AFTER
    EXPECT_NE(content.find("RATE_LIMIT:"), NPOS);
    EXPECT_NE(content.find("dropped=40"), NPOS);
}

// ===== COMMIT failure audit (stack-based OnTransaction) =====

/**
 * @tc.name: RdbAuditTxn_OnTransaction_CommitFail_015
 * @tc.desc: OnTransaction BEGIN + COMMIT(error) emits one combined TRX line
 *          with buffered ops and rc=err, mirroring RollBack failure behavior.
 * @tc.type: FUNC
 */
HWTEST_F(RdbAuditTxnTest, OnTransaction_CommitFail_015, TestSize.Level0)
{
    RdbAuditLoggerImpl logger;
    // BEGIN: sets inTransaction_, clears buffer, no emit.
    logger.OnTransaction(DbPath(), "BEGIN", RC_OK, true);
    EXPECT_EQ(CountLines(ReadFileContent(EventsLogPath())), 0);
    // Buffer some SQL ops (aggregated, no immediate emit).
    logger.OnSqlAudit(DbPath(), "INSERT", "tblA", 3);
    logger.OnSqlAudit(DbPath(), "DELETE", "tblA", 2);
    EXPECT_EQ(CountLines(ReadFileContent(EventsLogPath())), 0);
    // COMMIT with error: one combined line (BEGIN>DELETE(...)>INSERT(...)>COMMIT rc=14).
    logger.OnTransaction(DbPath(), "COMMIT", RC_ERR, true);
    EXPECT_EQ(WaitEventLines(1), 1);
    auto content = ReadFileContent(EventsLogPath());
    EXPECT_NE(content.find("TRX:"), NPOS);
    EXPECT_NE(content.find("BEGIN"), NPOS);
    EXPECT_NE(content.find("INSERT("), NPOS);
    EXPECT_NE(content.find("DELETE("), NPOS);
    EXPECT_NE(content.find(">COMMIT"), NPOS);
    EXPECT_NE(content.find("rc=" + std::to_string(RC_ERR)), NPOS);
    EXPECT_EQ(content.find("SQL:"), NPOS); // no separate SQL lines
    // After COMMIT (even failed), inTransaction_ is false; new SQL goes through throttle.
    logger.OnSqlAudit(DbPath(), "INSERT", "tblB", 1);
    EXPECT_EQ(CountLines(ReadFileContent(EventsLogPath())), 1); // 1st in new window, no emit
}

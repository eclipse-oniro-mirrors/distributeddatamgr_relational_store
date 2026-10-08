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
constexpr size_t EVENT_LINES_OPEN_THROTTLE_OP = 4;

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

/**
 * @tc.name: RdbAudit_OnThrottle_028
 * @tc.desc: Throttled INSERT: 2nd call emits a THROTTLE line once, 3rd is silent
 * @tc.type: FUNC
 */
HWTEST_F(RdbAuditTxnTest, OnThrottle_028, TestSize.Level0)
{
    RdbAuditLoggerImpl logger;
    // 1st INSERT: starts the 60s window, stored, no emit, no THROTTLE.
    logger.OnSqlAudit(DbPath(), "INSERT", "", 1);
    EXPECT_EQ(CountLines(ReadFileContent(EventsLogPath())), 0);
    // 2nd INSERT: suppressed; throttle starts → one THROTTLE line.
    logger.OnSqlAudit(DbPath(), "INSERT", "", 1);
    EXPECT_EQ(WaitEventLines(1), 1);
    EXPECT_NE(ReadFileContent(EventsLogPath()).find("THROTTLE:"), NPOS);
    // 3rd INSERT: suppressed; throttleLogged already true → no new line.
    logger.OnSqlAudit(DbPath(), "INSERT", "", 1);
    EXPECT_EQ(CountLines(ReadFileContent(EventsLogPath())), 1);
}

/**
 * @tc.name: RdbAudit_RealDbTransaction_029
 * @tc.desc: Real DB: committed transaction's SQL is audited; rolled-back is not.
 * @tc.type: FUNC
 */
HWTEST_F(RdbAuditTxnTest, RealDbTransaction_029, TestSize.Level0)
{
    AuditOpenCallback cb;
    int err = E_OK;
    auto store = RdbHelper::GetRdbStore(MakeAuditConfig(), 1, cb, err);
    ASSERT_NE(store, nullptr);
    // Committed transaction: INSERT buffered → flushed on COMMIT.
    store->BeginTransaction();
    ValuesBucket b;
    b.Put("id", 1);
    b.Put("name", "a");
    store->Insert("users", b);
    store->Commit();
    // Rolled-back transaction: INSERT buffered → discarded on ROLLBACK.
    store->BeginTransaction();
    ValuesBucket b2;
    b2.Put("id", 2);
    b2.Put("name", "b");
    store->Insert("users", b2);
    store->RollBack();
    // open (2) + INSERT(1, flushed) + COMMIT(1) = 4. Rollback adds nothing.
    EXPECT_EQ(WaitEventLines(4), 4);
    auto content = ReadFileContent(EventsLogPath());
    EXPECT_NE(content.find("op=INSERT"), NPOS);
    EXPECT_NE(content.find("op=COMMIT"), NPOS);
    EXPECT_EQ(content.find("op=BEGIN"), NPOS);
    EXPECT_EQ(content.find("op=ROLLBACK"), NPOS);
}

/**
 * @tc.name: RdbAudit_RealDbCreateTransaction_030
 * @tc.desc: CreateTransaction path: BEGIN/COMMIT/ROLLBACK TRX lines emitted; SQL audited via throttle.
 * @tc.type: FUNC
 */
HWTEST_F(RdbAuditTxnTest, RealDbCreateTransaction_030, TestSize.Level0)
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
    // open(2) + BEGIN(1) + COMMIT(1) + BEGIN(1) + THROTTLE(1) + ROLLBACK(1) = 7.
    // 1st INSERT starts throttle window (no emit); 2nd INSERT emits THROTTLE.
    EXPECT_EQ(WaitEventLines(7), 7);
    auto content = ReadFileContent(EventsLogPath());
    EXPECT_NE(content.find("op=BEGIN"), NPOS);
    EXPECT_NE(content.find("op=COMMIT"), NPOS);
    EXPECT_NE(content.find("op=ROLLBACK"), NPOS);
}

/**
 * @tc.name: RdbAudit_RateLimit_031
 * @tc.desc: At most 60 lines/s; excess replaced by one RATE_LIMIT summary line
 * @tc.type: FUNC
 */
HWTEST_F(RdbAuditTxnTest, RateLimit_031, TestSize.Level0)
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

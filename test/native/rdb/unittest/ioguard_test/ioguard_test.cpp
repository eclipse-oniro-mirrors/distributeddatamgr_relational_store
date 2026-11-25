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
#define LOG_TAG "IoGuardTest"
#include <gtest/gtest.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <cstdio>
#include <memory>
#include <string>
#include <sys/wait.h>

#include "sqlite3sym.h"
#include "common.h"
#include "rdb_errno.h"
#include "rdb_helper.h"
#include "rdb_open_callback.h"
#include "rdb_store.h"
#include "rdb_store_config.h"
#include "values_bucket.h"
#include "abs_rdb_predicates.h"

#include "rdb_store_impl.h"
#include "connection_pool.h"
#include "sqlite_connection.h"
#include "logger.h"

using namespace testing::ext;
using namespace OHOS::NativeRdb;
using namespace OHOS::Rdb;

namespace {
constexpr const char *DB_NAME = "/data/test/ioguard_test.db";
constexpr const char *DB_A = "/data/test/ioguard_multi_a.db";
constexpr const char *DB_B = "/data/test/ioguard_multi_b.db";
constexpr const char *DB_READONLY = "/data/test/ioguard_readonly.db";
constexpr const char *DB_ENCRYPTED = "/data/test/ioguard_encrypted.db";

constexpr int UNIXFILE_FD_OFFSET = sizeof(void *) * 3;
constexpr int EXIT_CHILD_OPEN_FAIL = 100;
constexpr int EXIT_CHILD_FD_FAIL = 101;
constexpr int EXIT_CHILD_WRITE_FAIL = 102;
constexpr int EXIT_EXEC_FAIL = 127;
constexpr mode_t LOG_FILE_MODE = 0644;

int ExtractFdFromFile(sqlite3_file *pFile)
{
    if (pFile == nullptr) {
        return -1;
    }
    const char *base = reinterpret_cast<const char *>(pFile);
    return *reinterpret_cast<const int *>(base + UNIXFILE_FD_OFFSET);
}

int GetRdbDbFd(const std::shared_ptr<RdbStore> &store)
{
    if (!store) {
        return -1;
    }
    auto impl = std::static_pointer_cast<RdbStoreImpl>(store);
    auto pool = impl->GetPool();
    if (!pool) {
        return -1;
    }
    // Read connection: available for both read-write and read-only stores
    auto conn = pool->Acquire(true);
    if (!conn) {
        return -1;
    }
    auto sqliteConn = std::static_pointer_cast<SqliteConnection>(conn);
    if (!sqliteConn || !sqliteConn->dbHandle_) {
        return -1;
    }
    sqlite3_file *pFile = nullptr;
    int rc = sqlite3_file_control(sqliteConn->dbHandle_, "main", SQLITE_FCNTL_FILE_POINTER, &pFile);
    if (rc != SQLITE_OK || pFile == nullptr) {
        return -1;
    }
    return ExtractFdFromFile(pFile);
}

int GetRdbSlaveFd(const std::shared_ptr<RdbStore> &store)
{
    if (!store) {
        return -1;
    }
    auto impl = std::static_pointer_cast<RdbStoreImpl>(store);
    auto pool = impl->GetPool();
    if (!pool) {
        return -1;
    }
    auto conn = pool->Acquire(false);
    if (!conn) {
        return -1;
    }
    auto sqliteConn = std::static_pointer_cast<SqliteConnection>(conn);
    if (!sqliteConn || !sqliteConn->slaveConnection_) {
        return -1;
    }
    sqlite3 *slaveDb = sqliteConn->slaveConnection_->dbHandle_;
    if (!slaveDb) {
        return -1;
    }
    sqlite3_file *pFile = nullptr;
    int rc = sqlite3_file_control(slaveDb, "main", SQLITE_FCNTL_FILE_POINTER, &pFile);
    if (rc != SQLITE_OK || pFile == nullptr) {
        return -1;
    }
    return ExtractFdFromFile(pFile);
}

class TestOpenCallback : public RdbOpenCallback {
public:
    int OnCreate(RdbStore &s) override
    {
        std::string sql = "CREATE TABLE IF NOT EXISTS test_t(id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT NOT NULL)";
        return s.ExecuteSql(sql);
    }
    int OnUpgrade(RdbStore &s, int, int) override { return E_OK; }
};

std::shared_ptr<RdbStore> OpenStore(const char *dbPath, const char *bundle)
{
    RdbStoreConfig config(dbPath);
    config.SetBundleName(bundle);
    int errCode = E_OK;
    TestOpenCallback cb;
    return RdbHelper::GetRdbStore(config, 1, cb, errCode);
}

std::shared_ptr<RdbStore> OpenStoreHa(const char *dbPath, const char *bundle, HAMode mode)
{
    RdbStoreConfig config(dbPath);
    config.SetBundleName(bundle);
    config.SetHaMode(mode);
    int errCode = E_OK;
    TestOpenCallback cb;
    return RdbHelper::GetRdbStore(config, 1, cb, errCode);
}

std::shared_ptr<RdbStore> OpenStoreReadOnly(const char *dbPath, const char *bundle)
{
    RdbStoreConfig config(dbPath, StorageMode::MODE_DISK, true);
    config.SetBundleName(bundle);
    int errCode = E_OK;
    TestOpenCallback cb;
    return RdbHelper::GetRdbStore(config, 1, cb, errCode);
}

std::shared_ptr<RdbStore> OpenStoreEncrypted(const char *dbPath, const char *bundle)
{
    RdbStoreConfig config(dbPath);
    config.SetBundleName(bundle);
    RdbStoreConfig::CryptoParam cryptoParam;
    cryptoParam.encryptKey_ = std::vector<uint8_t>{ 1, 2, 3, 4, 5, 6 };
    config.SetCryptoParam(cryptoParam);
    int errCode = E_OK;
    TestOpenCallback cb;
    return RdbHelper::GetRdbStore(config, 1, cb, errCode);
}

void CrudBasic(const std::shared_ptr<RdbStore> &store, int id, const std::string &name)
{
    ASSERT_NE(store, nullptr);
    // Insert
    ValuesBucket v;
    v.PutInt("id", id);
    v.PutString("name", name);
    int64_t rowId = 0;
    EXPECT_EQ(store->Insert(rowId, "test_t", v), E_OK);

    // Query
    std::string sql = "SELECT name FROM test_t WHERE id = " + std::to_string(id);
    auto rs = store->QuerySql(sql);
    ASSERT_NE(rs, nullptr);
    EXPECT_EQ(rs->GoToFirstRow(), E_OK);
    std::string result;
    EXPECT_EQ(rs->GetString(0, result), E_OK);
    EXPECT_EQ(result, name) << "query name mismatch, id=" << id;

    // Update
    ValuesBucket upd;
    upd.PutString("name", "upd_" + name);
    AbsRdbPredicates predicates("test_t");
    predicates.EqualTo("id", std::to_string(id));
    int changedRows = 0;
    EXPECT_EQ(store->Update(changedRows, upd, predicates), E_OK);
    EXPECT_EQ(changedRows, 1) << "update should affect 1 row, id=" << id;

    // Delete
    int deletedRows = 0;
    EXPECT_EQ(store->Delete(deletedRows, predicates), E_OK);
    EXPECT_EQ(deletedRows, 1) << "delete should remove 1 row, id=" << id;
}

void AssertFdNotProtected(const std::shared_ptr<RdbStore> &store)
{
    int fd = GetRdbDbFd(store);
    ASSERT_GE(fd, 0);
    const char *data = "garbage";
    EXPECT_NE(write(fd, data, std::strlen(data)), -1) << "write should succeed (not protected)";
    EXPECT_NE(close(fd), -1) << "close should succeed (not protected)";
}

void AssertFdProtected(const std::shared_ptr<RdbStore> &store)
{
    int fd = GetRdbDbFd(store);
    ASSERT_GE(fd, 0);
    const char *data = "garbage";
    EXPECT_EQ(write(fd, data, std::strlen(data)), -1) << "write should fail (EPERM)";
    EXPECT_EQ(errno, EPERM);
    EXPECT_EQ(close(fd), -1) << "close should fail (EPERM)";
    EXPECT_EQ(errno, EPERM);
}

void RunChildTest(const char *envKey, const char *filter, const char *logPath)
{
    pid_t pid = fork();
    ASSERT_GE(pid, 0) << "fork failed, errno=" << errno;
    if (pid == 0) {
        setenv(envKey, "1", 1);
        int logFd = open(logPath, O_WRONLY | O_CREAT | O_TRUNC, LOG_FILE_MODE);
        if (logFd >= 0) {
            dup2(logFd, STDOUT_FILENO);
            dup2(logFd, STDERR_FILENO);
            close(logFd);
        }
        std::string filterArg = std::string("--gtest_filter=") + filter;
        execl("/data/test/IoguardTest", "IoguardTest", filterArg.c_str(), nullptr);
        _exit(EXIT_EXEC_FAIL);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    ASSERT_TRUE(WIFEXITED(status)) << "child did not exit normally";
    EXPECT_EQ(WEXITSTATUS(status), 0) << "child test failed, check log: " << logPath;
}

} // namespace

class IoguardTest : public testing::Test {
public:
    static void SetUpTestCase(void) { RdbHelper::DeleteRdbStore(DB_NAME); }
    static void TearDownTestCase(void) { RdbHelper::DeleteRdbStore(DB_NAME); }
    void SetUp(void)
    {
        RdbHelper::ClearCache();
        RdbHelper::DeleteRdbStore(DB_NAME);
    }
    void TearDown(void)
    {
        RdbHelper::ClearCache();
        RdbHelper::DeleteRdbStore(DB_NAME);
    }
};

// ===== Single-process single-db =====

/**
 * @brief Default state: no EnableIOGuard call. Open store, CRUD normal,
 *        fd write+close succeed (io_guard not active).
 */
HWTEST_F(IoguardTest, Rdb_IoGuard_Default_NotProtected_001, TestSize.Level0)
{
    auto store = OpenStore(DB_NAME, "com.ohos.ioguard.test");
    ASSERT_NE(store, nullptr);
    CrudBasic(store, 1, "hello");

    AssertFdNotProtected(store);
}

/**
 * @brief Open store, close store (release), then EnableIOGuard — latched, fails.
 *        SQLite process-level latch: once sqlite3_open has been called, sqlite3_config
 *        cannot be changed. Closing all connections does not reset the latch.
 *        Enable returns E_ERROR, reopen store CRUD normal, fd not protected.
 */
HWTEST_F(IoguardTest, Rdb_IoGuard_OpenClose_EnableFail_001, TestSize.Level0)
{
    auto store = OpenStore(DB_NAME, "com.ohos.ioguard.test");
    ASSERT_NE(store, nullptr);
    CrudBasic(store, 1, "first");
    store = nullptr;
    RdbHelper::ClearCache();

    EXPECT_EQ(RdbHelper::EnableIOGuard(), E_ERROR)
        << "enable should fail after sqlite initialized, even if all stores closed";

    auto store2 = OpenStore(DB_NAME, "com.ohos.ioguard.test");
    ASSERT_NE(store2, nullptr);
    CrudBasic(store2, 2, "second");
    AssertFdNotProtected(store2);
}

/**
 * @brief EnableIOGuard() then open store. io_guard is on.
 *        CRUD normal (authorized writes), direct fd write+close fail EPERM.
 *        Fork+exec child for clean latch state.
 */
HWTEST_F(IoguardTest, Rdb_IoGuard_Enable_Protected_001, TestSize.Level0)
{
    RunChildTest("IOGUARD_CHILD", "IoguardTest.Rdb_IoGuard_Protected_Child_001",
                 "/data/test/ioguard_protected_child.log");
}

HWTEST_F(IoguardTest, Rdb_IoGuard_Protected_Child_001, TestSize.Level0)
{
    if (getenv("IOGUARD_CHILD") == nullptr) {
        GTEST_SKIP() << "skipped in parent";
    }
    ASSERT_EQ(RdbHelper::EnableIOGuard(), E_OK);

    auto store = OpenStore(DB_NAME, "com.ohos.ioguard.test");
    ASSERT_NE(store, nullptr);
    CrudBasic(store, 2, "world");

    AssertFdProtected(store);
    CrudBasic(store, 3, "after_protected");
}

/**
 * @brief EnableIOGuard, open store, release store (nullptr + ClearCache), reopen same DB.
 *        Verify io_guard protection persists across store reopen — io_guard is a global
 *        sqlite3_config, not per-store state. Reopened store fd must still be protected.
 *        Fork+exec child for clean latch state.
 */
HWTEST_F(IoguardTest, Rdb_IoGuard_Reopen_StillProtected_001, TestSize.Level0)
{
    RunChildTest("IOGUARD_REOPEN_CHILD", "IoguardTest.Rdb_IoGuard_Reopen_Child_001",
                 "/data/test/ioguard_reopen_child.log");
}

HWTEST_F(IoguardTest, Rdb_IoGuard_Reopen_Child_001, TestSize.Level0)
{
    if (getenv("IOGUARD_REOPEN_CHILD") == nullptr) {
        GTEST_SKIP() << "skipped in parent";
    }
    RdbHelper::DeleteRdbStore(DB_NAME);
    RdbHelper::ClearCache();

    ASSERT_EQ(RdbHelper::EnableIOGuard(), E_OK);
    auto store = OpenStore(DB_NAME, "com.ohos.ioguard.reopen");
    ASSERT_NE(store, nullptr);
    CrudBasic(store, 1, "first");
    AssertFdProtected(store);

    store = nullptr;
    RdbHelper::ClearCache();

    auto store2 = OpenStore(DB_NAME, "com.ohos.ioguard.reopen");
    ASSERT_NE(store2, nullptr);
    CrudBasic(store2, 2, "second");
    AssertFdProtected(store2);
    CrudBasic(store2, 3, "after_reopen");
}

// ===== Dual-write =====

/**
 * @brief EnableIOGuard(), open dual-write store (MAIN_REPLICA).
 *        Main+slave CRUD normal, main+slave fd write+close fail EPERM.
 *        Fork+exec child for clean latch state.
 */
HWTEST_F(IoguardTest, Rdb_IoGuard_DoubleWrite_Protected_001, TestSize.Level0)
{
    RunChildTest("IOGUARD_DW_CHILD", "IoguardTest.Rdb_IoGuard_DoubleWrite_Child_001",
                 "/data/test/ioguard_dw_child.log");
}

HWTEST_F(IoguardTest, Rdb_IoGuard_DoubleWrite_Child_001, TestSize.Level0)
{
    if (getenv("IOGUARD_DW_CHILD") == nullptr) {
        GTEST_SKIP() << "skipped in parent";
    }
    ASSERT_EQ(RdbHelper::EnableIOGuard(), E_OK);

    auto store = OpenStoreHa(DB_NAME, "com.ohos.ioguard.dw", HAMode::MAIN_REPLICA);
    ASSERT_NE(store, nullptr);
    CrudBasic(store, 3, "dual");

    int mainFd = GetRdbDbFd(store);
    ASSERT_GE(mainFd, 0);
    const char *data = "garbage";
    EXPECT_EQ(write(mainFd, data, std::strlen(data)), -1);
    EXPECT_EQ(errno, EPERM);
    EXPECT_EQ(close(mainFd), -1);
    EXPECT_EQ(errno, EPERM);

    int slaveFd = GetRdbSlaveFd(store);
    ASSERT_GE(slaveFd, 0);
    EXPECT_EQ(write(slaveFd, data, std::strlen(data)), -1);
    EXPECT_EQ(errno, EPERM);
    EXPECT_EQ(close(slaveFd), -1);
    EXPECT_EQ(errno, EPERM);

    CrudBasic(store, 4, "after_protected");
}

// ===== Single-process multi-db =====

/**
 * @brief Open A, then EnableIOGuard() — latched, fails. Open B.
 *        Enable returns E_ERROR, A+B CRUD normal, A+B fd not protected.
 */
HWTEST_F(IoguardTest, Rdb_IoGuard_MultiDb_OpenA_EnableFail_OpenB_001, TestSize.Level0)
{
    RdbHelper::DeleteRdbStore(DB_A);
    RdbHelper::DeleteRdbStore(DB_B);

    auto storeA = OpenStore(DB_A, "com.ohos.ioguard.a");
    ASSERT_NE(storeA, nullptr);
    CrudBasic(storeA, 1, "aaa");

    EXPECT_EQ(RdbHelper::EnableIOGuard(), E_ERROR) << "enable should fail (latched)";

    auto storeB = OpenStore(DB_B, "com.ohos.ioguard.b");
    ASSERT_NE(storeB, nullptr);
    CrudBasic(storeB, 1, "bbb");

    AssertFdNotProtected(storeA);
    AssertFdNotProtected(storeB);

    RdbHelper::ClearCache();
    RdbHelper::DeleteRdbStore(DB_A);
    RdbHelper::DeleteRdbStore(DB_B);
}

/**
 * @brief EnableIOGuard(), open A, open B. Both guarded.
 *        Enable succeeds, A+B CRUD normal, A+B fd write+close EPERM.
 *        Fork+exec child for clean latch state.
 */
HWTEST_F(IoguardTest, Rdb_IoGuard_MultiDb_Enable_OpenAB_Protected_001, TestSize.Level0)
{
    RunChildTest("IOGUARD_MULTI_CHILD", "IoguardTest.Rdb_IoGuard_MultiDb_Enable_OpenAB_Child_001",
                 "/data/test/ioguard_multi_protected_child.log");
}

HWTEST_F(IoguardTest, Rdb_IoGuard_MultiDb_Enable_OpenAB_Child_001, TestSize.Level0)
{
    if (getenv("IOGUARD_MULTI_CHILD") == nullptr) {
        GTEST_SKIP() << "skipped in parent";
    }
    RdbHelper::DeleteRdbStore(DB_A);
    RdbHelper::DeleteRdbStore(DB_B);

    ASSERT_EQ(RdbHelper::EnableIOGuard(), E_OK);

    auto storeA = OpenStore(DB_A, "com.ohos.ioguard.a");
    ASSERT_NE(storeA, nullptr);
    CrudBasic(storeA, 1, "aaa");

    auto storeB = OpenStore(DB_B, "com.ohos.ioguard.b");
    ASSERT_NE(storeB, nullptr);
    CrudBasic(storeB, 1, "bbb");

    AssertFdProtected(storeA);
    AssertFdProtected(storeB);
    CrudBasic(storeA, 2, "after_protected");
    CrudBasic(storeB, 2, "after_protected");
}

/**
 * @brief EnableIOGuard(), open A, EnableIOGuard() again — latched, fails. Open B.
 *        Second enable returns E_ERROR (rdb reports), A+B CRUD normal, A+B fd protected.
 *        Fork+exec child for clean latch state.
 */
HWTEST_F(IoguardTest, Rdb_IoGuard_MultiDb_Enable_OpenA_ReEnableFail_OpenB_001, TestSize.Level0)
{
    RunChildTest("IOGUARD_MULTI_DISABLE_CHILD", "IoguardTest.Rdb_IoGuard_MultiDb_Disable_Child_001",
                 "/data/test/ioguard_multi_disable_child.log");
}

HWTEST_F(IoguardTest, Rdb_IoGuard_MultiDb_Disable_Child_001, TestSize.Level0)
{
    if (getenv("IOGUARD_MULTI_DISABLE_CHILD") == nullptr) {
        GTEST_SKIP() << "skipped in parent";
    }
    RdbHelper::DeleteRdbStore(DB_A);
    RdbHelper::DeleteRdbStore(DB_B);

    ASSERT_EQ(RdbHelper::EnableIOGuard(), E_OK);

    auto storeA = OpenStore(DB_A, "com.ohos.ioguard.a");
    ASSERT_NE(storeA, nullptr);
    CrudBasic(storeA, 1, "aaa");

    EXPECT_EQ(RdbHelper::EnableIOGuard(), E_ERROR)
        << "second enable should fail (latched), rdb must report error";

    auto storeB = OpenStore(DB_B, "com.ohos.ioguard.b");
    ASSERT_NE(storeB, nullptr);
    CrudBasic(storeB, 1, "bbb");

    AssertFdProtected(storeA);
    AssertFdProtected(storeB);
    CrudBasic(storeA, 2, "after_protected");
    CrudBasic(storeB, 2, "after_protected");
}

// ===== Multi-process single-db =====

/**
 * @brief Parent enable+open A, fork child open same A.
 *        Parent fd guarded (EPERM), child fd not guarded (fork resets io_guard).
 *        Fork+exec child for clean latch state (enable before open).
 */
HWTEST_F(IoguardTest, Rdb_IoGuard_MultiProcess_ForkChild_001, TestSize.Level0)
{
    RunChildTest("IOGUARD_MP_CHILD", "IoguardTest.Rdb_IoGuard_MultiProcess_Child_001",
                 "/data/test/ioguard_mp_child.log");
}

HWTEST_F(IoguardTest, Rdb_IoGuard_MultiProcess_Child_001, TestSize.Level0)
{
    if (getenv("IOGUARD_MP_CHILD") == nullptr) {
        GTEST_SKIP() << "skipped in parent";
    }
    RdbHelper::DeleteRdbStore(DB_NAME);
    RdbHelper::ClearCache();

    ASSERT_EQ(RdbHelper::EnableIOGuard(), E_OK);
    auto storeParent = OpenStore(DB_NAME, "com.ohos.ioguard.mp");
    ASSERT_NE(storeParent, nullptr);
    CrudBasic(storeParent, 1, "parent");

    AssertFdProtected(storeParent);
    CrudBasic(storeParent, 2, "after_protected");

    // Release store before fork: child would inherit open SQLite connections
    // holding WAL/shm locks, causing OpenStore in child to deadlock on lock acquisition.
    storeParent = nullptr;
    RdbHelper::ClearCache();

    pid_t pid = fork();
    ASSERT_GE(pid, 0) << "fork failed";
    if (pid == 0) {
        auto storeChild = OpenStore(DB_NAME, "com.ohos.ioguard.mp");
        if (!storeChild) {
            _exit(EXIT_CHILD_OPEN_FAIL);
        }
        int fd = GetRdbDbFd(storeChild);
        if (fd < 0) {
            _exit(EXIT_CHILD_FD_FAIL);
        }
        const char *childData = "child";
        if (write(fd, childData, std::strlen(childData)) == -1) {
            _exit(EXIT_CHILD_WRITE_FAIL);
        }
        _exit(0);
    }

    int status = 0;
    waitpid(pid, &status, 0);
    ASSERT_TRUE(WIFEXITED(status));
    int exitCode = WEXITSTATUS(status);
    EXPECT_EQ(exitCode, 0) << "child write should succeed after fork, exit=" << exitCode;
}

// ===== Read-only database =====

/**
 * @brief EnableIOGuard, reopen DB as read-only. Query works, query still works after fd check.
 *        Read-only fd is opened O_RDONLY — write already blocked by kernel (EBADF),
 *        close succeeds normally. IOGuard fd protection N/A for read-only stores.
 *        Core assertion: EnableIOGuard doesn't break read-only store Query functionality.
 *        Two child processes: child 1 seeds data, child 2 enables io_guard and reopens read-only.
 */
HWTEST_F(IoguardTest, Rdb_IoGuard_ReadOnly_Protected_001, TestSize.Level0)
{
    RunChildTest("IOGUARD_RO_SEED_CHILD", "IoguardTest.Rdb_IoGuard_ReadOnly_Seed_Child_001",
                 "/data/test/ioguard_ro_seed_child.log");
    RunChildTest("IOGUARD_RO_CHILD", "IoguardTest.Rdb_IoGuard_ReadOnly_Child_001",
                 "/data/test/ioguard_ro_child.log");
}

HWTEST_F(IoguardTest, Rdb_IoGuard_ReadOnly_Seed_Child_001, TestSize.Level0)
{
    if (getenv("IOGUARD_RO_SEED_CHILD") == nullptr) {
        GTEST_SKIP() << "skipped in parent";
    }
    RdbHelper::DeleteRdbStore(DB_READONLY);
    RdbHelper::ClearCache();

    auto store = OpenStore(DB_READONLY, "com.ohos.ioguard.ro");
    ASSERT_NE(store, nullptr);
    ValuesBucket v;
    v.PutInt("id", 1);
    v.PutString("name", "seed");
    int64_t rowId = 0;
    EXPECT_EQ(store->Insert(rowId, "test_t", v), E_OK);
}

HWTEST_F(IoguardTest, Rdb_IoGuard_ReadOnly_Child_001, TestSize.Level0)
{
    if (getenv("IOGUARD_RO_CHILD") == nullptr) {
        GTEST_SKIP() << "skipped in parent";
    }
    RdbHelper::ClearCache();

    ASSERT_EQ(RdbHelper::EnableIOGuard(), E_OK);
    auto store = OpenStoreReadOnly(DB_READONLY, "com.ohos.ioguard.ro");
    ASSERT_NE(store, nullptr);

    // Query works
    auto rs = store->QuerySql("SELECT name FROM test_t WHERE id = 1");
    ASSERT_NE(rs, nullptr);
    EXPECT_EQ(rs->GoToFirstRow(), E_OK);
    std::string name;
    EXPECT_EQ(rs->GetString(0, name), E_OK);
    EXPECT_EQ(name, "seed");

    // Read-only fd: write fails (-1, EBADF from kernel or EPERM from io_guard),
    // close fails with EPERM (io_guard protects all SQLite fds regardless of read/write mode)
    int fd = GetRdbDbFd(store);
    ASSERT_GE(fd, 0);
    const char *data = "garbage";
    EXPECT_EQ(write(fd, data, std::strlen(data)), -1) << "write should fail on read-only fd";
    EXPECT_EQ(close(fd), -1) << "close should fail (io_guard protects all fds)";
    EXPECT_EQ(errno, EPERM) << "close errno should be EPERM";

    // Query still works after fd operation
    auto rs2 = store->QuerySql("SELECT COUNT(*) FROM test_t");
    ASSERT_NE(rs2, nullptr);
    EXPECT_EQ(rs2->GoToFirstRow(), E_OK);
}

// ===== Encrypted database =====

/**
 * @brief EnableIOGuard, open encrypted DB (custom key). CRUD normal, fd protected, CRUD still works.
 *        Uses raw key (Path B) to avoid HUKS dependency.
 *        Fork+exec child for clean latch state.
 */
HWTEST_F(IoguardTest, Rdb_IoGuard_Encrypted_Protected_001, TestSize.Level0)
{
    RunChildTest("IOGUARD_ENC_CHILD", "IoguardTest.Rdb_IoGuard_Encrypted_Child_001",
                 "/data/test/ioguard_enc_child.log");
}

HWTEST_F(IoguardTest, Rdb_IoGuard_Encrypted_Child_001, TestSize.Level0)
{
    if (getenv("IOGUARD_ENC_CHILD") == nullptr) {
        GTEST_SKIP() << "skipped in parent";
    }
    RdbHelper::DeleteRdbStore(DB_ENCRYPTED);
    RdbHelper::ClearCache();

    ASSERT_EQ(RdbHelper::EnableIOGuard(), E_OK);
    auto store = OpenStoreEncrypted(DB_ENCRYPTED, "com.ohos.ioguard.enc");
    ASSERT_NE(store, nullptr);
    CrudBasic(store, 1, "encrypted");

    AssertFdProtected(store);
    CrudBasic(store, 2, "after_protected");
}

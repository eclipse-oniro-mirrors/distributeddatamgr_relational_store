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
#include <errno.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <chrono>
#include <fstream>

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

constexpr int UNIXFILE_FD_OFFSET = sizeof(void *) * 3;

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
    auto conn = pool->Acquire(false);
    if (!conn) {
        return -1;
    }
    auto sqliteConn = std::static_pointer_cast<SqliteConnection>(conn);
    if (!sqliteConn || !sqliteConn->dbHandle_) {
        return -1;
    }
    sqlite3_file *pFile = nullptr;
    if (sqlite3_file_control(sqliteConn->dbHandle_, "main", SQLITE_FCNTL_FILE_POINTER, &pFile) != SQLITE_OK || !pFile) {
        return -1;
    }
    int fd = -1;
    std::memcpy(&fd, reinterpret_cast<const char *>(pFile) + UNIXFILE_FD_OFFSET, sizeof(int));
    return fd;
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
    if (sqlite3_file_control(slaveDb, "main", SQLITE_FCNTL_FILE_POINTER, &pFile) != SQLITE_OK || !pFile) {
        return -1;
    }
    int fd = -1;
    std::memcpy(&fd, reinterpret_cast<const char *>(pFile) + UNIXFILE_FD_OFFSET, sizeof(int));
    return fd;
}

class TestOpenCallback : public RdbOpenCallback {
public:
    int OnCreate(RdbStore &s) override
    {
        return s.ExecuteSql("CREATE TABLE IF NOT EXISTS test_t(id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT NOT NULL)");
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

void CrudBasic(const std::shared_ptr<RdbStore> &store, int id, const std::string &name)
{
    ASSERT_NE(store, nullptr);
    ValuesBucket v;
    v.PutInt("id", id);
    v.PutString("name", name);
    int64_t rowId = 0;
    EXPECT_EQ(store->Insert(rowId, "test_t", v), E_OK);
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
        int logFd = open(logPath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (logFd >= 0) {
            dup2(logFd, STDOUT_FILENO);
            dup2(logFd, STDERR_FILENO);
            close(logFd);
        }
        std::string filterArg = std::string("--gtest_filter=") + filter;
        execl("/data/test/IoguardTest", "IoguardTest", filterArg.c_str(), nullptr);
        _exit(127);
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

    pid_t pid = fork();
    ASSERT_GE(pid, 0) << "fork failed";
    if (pid == 0) {
        RdbHelper::ClearCache();
        auto storeChild = OpenStore(DB_NAME, "com.ohos.ioguard.mp");
        if (!storeChild) {
            _exit(100);
        }
        int fd = GetRdbDbFd(storeChild);
        if (fd < 0) {
            _exit(101);
        }
        const char *data = "child";
        if (write(fd, data, std::strlen(data)) == -1) {
            _exit(102);
        }
        _exit(0);
    }

    int status = 0;
    waitpid(pid, &status, 0);
    ASSERT_TRUE(WIFEXITED(status));
    EXPECT_EQ(WEXITSTATUS(status), 0) << "child write should succeed after fork, exit=" << WEXITSTATUS(status);
}

// ===== Performance =====

// Perf helper: pre-load PRE_LOAD rows, then measure N operations of each CRUD type.
struct PerfResult {
    double insertAvg;
    double queryAvg;
    double updateAvg;
    double deleteAvg;
};

static PerfResult MeasureCrud(const std::shared_ptr<RdbStore> &store, int preLoad, int n)
{
    PerfResult r = {0, 0, 0, 0};
    for (int i = 0; i < preLoad; i++) {
        ValuesBucket v;
        v.PutInt("id", i);
        v.PutString("name", std::to_string(i));
        int64_t rowId = 0;
        store->Insert(rowId, "test_t", v);
    }

    auto t0 = std::chrono::steady_clock::now();
    for (int i = preLoad; i < preLoad + n; i++) {
        ValuesBucket v;
        v.PutInt("id", i);
        v.PutString("name", std::to_string(i));
        int64_t rowId = 0;
        store->Insert(rowId, "test_t", v);
    }
    auto t1 = std::chrono::steady_clock::now();
    r.insertAvg = (double)std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count() / n;

    auto t2 = std::chrono::steady_clock::now();
    for (int i = 0; i < n; i++) {
        int target = i % (preLoad + n);
        auto rs = store->QuerySql("SELECT id, name FROM test_t WHERE id = " + std::to_string(target));
        if (rs) {
            rs->GoToFirstRow();
        }
    }
    auto t3 = std::chrono::steady_clock::now();
    r.queryAvg = (double)std::chrono::duration_cast<std::chrono::microseconds>(t3 - t2).count() / n;

    auto t4 = std::chrono::steady_clock::now();
    for (int i = 0; i < n; i++) {
        int target = i % (preLoad + n);
        ValuesBucket v;
        v.PutString("name", "upd_" + std::to_string(i));
        AbsRdbPredicates predicates("test_t");
        predicates.EqualTo("id", std::to_string(target));
        int changedRows = 0;
        store->Update(changedRows, v, predicates);
    }
    auto t5 = std::chrono::steady_clock::now();
    r.updateAvg = (double)std::chrono::duration_cast<std::chrono::microseconds>(t5 - t4).count() / n;

    auto t6 = std::chrono::steady_clock::now();
    for (int i = 0; i < n; i++) {
        int target = preLoad + i;
        AbsRdbPredicates predicates("test_t");
        predicates.EqualTo("id", std::to_string(target));
        int deletedRows = 0;
        store->Delete(deletedRows, predicates);
    }
    auto t7 = std::chrono::steady_clock::now();
    r.deleteAvg = (double)std::chrono::duration_cast<std::chrono::microseconds>(t7 - t6).count() / n;

    return r;
}

constexpr int PERF_PRE_LOAD = 20000;
constexpr int PERF_N = 2000;
constexpr const char *PERF_RESULT_FILE = "/data/test/ioguard_perf_guarded.txt";

/**
 * @brief Baseline (parent) + guarded (fork+exec child), compare CRUD degradation.
 *        Parent runs baseline (no io_guard), child runs guarded (io_guard enabled).
 *        Child writes results to file; parent reads and asserts <= 1.1x.
 */
HWTEST_F(IoguardTest, Rdb_IoGuard_Perf_Degrade_001, TestSize.Level2)
{
    RdbHelper::DeleteRdbStore(DB_NAME);
    RdbHelper::ClearCache();

    // 1. Baseline: no io_guard (in parent — open triggers latch but parent doesn't need enable)
    auto store = OpenStore(DB_NAME, "com.ohos.ioguard.perf");
    ASSERT_NE(store, nullptr);
    auto base = MeasureCrud(store, PERF_PRE_LOAD, PERF_N);
    store = nullptr;
    RdbHelper::ClearCache();
    RdbHelper::DeleteRdbStore(DB_NAME);

    // 2. Guarded: fork+exec child (fresh process, latch=0, can enable)
    RunChildTest("IOGUARD_PERF_CHILD", "IoguardTest.Rdb_IoGuard_Perf_Child_001",
                 "/data/test/ioguard_perf_child.log");

    // 3. Read guarded results from file
    std::ifstream ifs(PERF_RESULT_FILE);
    ASSERT_TRUE(ifs.is_open()) << "cannot open guarded result file: " << PERF_RESULT_FILE;
    double gInsert, gQuery, gUpdate, gDelete;
    ifs >> gInsert >> gQuery >> gUpdate >> gDelete;
    ifs.close();

    // 4. Print comparison + assert
    LOG_INFO("perf: N=%{public}d, pre_load=%{public}d", PERF_N, PERF_PRE_LOAD);
    LOG_INFO("insert: baseline=%{public}.2fus, guarded=%{public}.2fus, ratio=%{public}.2f",
             base.insertAvg, gInsert, gInsert / base.insertAvg);
    LOG_INFO("query: baseline=%{public}.2fus, guarded=%{public}.2fus, ratio=%{public}.2f",
             base.queryAvg, gQuery, gQuery / base.queryAvg);
    LOG_INFO("update: baseline=%{public}.2fus, guarded=%{public}.2fus, ratio=%{public}.2f",
             base.updateAvg, gUpdate, gUpdate / base.updateAvg);
    LOG_INFO("delete: baseline=%{public}.2fus, guarded=%{public}.2fus, ratio=%{public}.2f",
             base.deleteAvg, gDelete, gDelete / base.deleteAvg);

    EXPECT_LE(gInsert / base.insertAvg, 1.1) << "insert degradation exceeds 1.1x";
    EXPECT_LE(gQuery / base.queryAvg, 1.1) << "query degradation exceeds 1.1x";
    EXPECT_LE(gUpdate / base.updateAvg, 1.1) << "update degradation exceeds 1.1x";
    EXPECT_LE(gDelete / base.deleteAvg, 1.1) << "delete degradation exceeds 1.1x";
}

/**
 * @brief Child: fresh process (latch=0). Enable io_guard, run CRUD, write results to file.
 */
HWTEST_F(IoguardTest, Rdb_IoGuard_Perf_Child_001, TestSize.Level2)
{
    if (getenv("IOGUARD_PERF_CHILD") == nullptr) {
        GTEST_SKIP() << "skipped in parent";
    }
    RdbHelper::DeleteRdbStore(DB_NAME);
    RdbHelper::ClearCache();

    ASSERT_EQ(RdbHelper::EnableIOGuard(), E_OK);
    auto store = OpenStore(DB_NAME, "com.ohos.ioguard.perf");
    ASSERT_NE(store, nullptr);
    auto guarded = MeasureCrud(store, PERF_PRE_LOAD, PERF_N);

    // Write results to file for parent to read
    std::ofstream ofs(PERF_RESULT_FILE);
    ASSERT_TRUE(ofs.is_open()) << "cannot open result file for writing: " << PERF_RESULT_FILE;
    ofs << guarded.insertAvg << " " << guarded.queryAvg << " "
        << guarded.updateAvg << " " << guarded.deleteAvg;
    ofs.close();
}

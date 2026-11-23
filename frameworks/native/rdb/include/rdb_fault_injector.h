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

#ifndef NATIVE_RDB_RDB_FAULT_INJECTOR_H
#define NATIVE_RDB_RDB_FAULT_INJECTOR_H

#include <atomic>
#include <memory>
#include <mutex>
#include <random>
#include <string>
#include <unordered_map>

#include "rdb_visibility.h"

namespace OHOS {
namespace NativeRdb {

// Operation families, matching the keys of the "operations" object in
// rdb_fault_inject_config.json one by one.
enum class RdbFaultOp {
    INSERT,
    UPDATE,
    DELETE,
    QUERY,
    EXECUTE_SQL,
    TRANSACTION,
    RESULT_SET_READ,
};

// Per-database verdict object deciding "inject or not" for the RDB
// fault-injection mock layer. It never fabricates error codes itself: a hit
// only tells the call site to return E_SQLITE_CORRUPT. One instance is
// bound to one database path and reads rdb_fault_inject_config.json once at
// construction. Root/debug builds compile the implementation
// (rdb_fault_injector.cpp) together with the rdb sources into
// libnative_rdb.z.so; this class is that implementation's plain interface
// and carries no build-variant shape of its own - the decorators hold it as
// a shared_ptr but reach every verdict only through the RDB_FAULT_INJECT_*
// macros below (plus CreateOnce for the per-store instance), and user
// builds reference it nowhere (the macros expand to nothing there, so no
// injector implementation is compiled and no injector object is ever
// created). The config file is parsed only through
// Serializable::Unmarshal (see rdb_fault_injector.cpp), and budget
// consumption happens under the per-entry lock so a missed probability
// sample does not consume budget.
class API_EXPORT RdbFaultInjector {
public:
    // Reads rdb_fault_inject_config.json from the database directory and
    // keeps the parsed snapshot for the bound path. A missing file or
    // invalid content leaves the injector unconfigured (verdicts all miss).
    explicit RdbFaultInjector(const std::string &dbPath);
    ~RdbFaultInjector() = default;

    // Serialized idempotent creation for one caller-held member. The store
    // manager serves a cache entry to a concurrent second opener while the
    // first open may still be running, and both Init calls must end up
    // sharing one injector (one config snapshot, one budget) instead of
    // racing two instances into the member. The scope of that sharing is
    // the member alone: creation is never keyed by path and never cached
    // across holders - every store (every database path) gets its own
    // instance. Serialization is per-store via the caller's once_flag (no
    // global lock): only concurrent opens of the same store block each
    // other, and calls after the first are a fast path. Reached only
    // through RDB_FAULT_INJECT_CREATE.
    static void CreateOnce(std::once_flag &flag, const std::string &dbPath, std::shared_ptr<RdbFaultInjector> &holder);

    // Combined verdict for hook sites: the per-method entry first, then the
    // operation family. Returns true when the call site should inject.
    bool Hit(const std::string &methodName, RdbFaultOp op);

    // Operation-family verdict; a hit consumes budget and emits the
    // distinguishable log (TAG: RDB_FAULT_INJECT, rate-limited).
    bool Check(RdbFaultOp op);

    // Per-method verdict (targets a single method by name, e.g. "Backup").
    bool CheckMethod(const std::string &methodName);

    // Open-path consuming verdict (a hit consumes budget): the store
    // manager refuses the open with E_SQLITE_CORRUPT.
    bool ConsumeOpen();

    // Whether any injection entry exists for the bound path (the snapshot
    // is fixed at construction). Query methods use it to decide whether to
    // wrap returned result sets; unconfigured databases are never wrapped.
    bool HasEntry() const;

private:
    RdbFaultInjector(const RdbFaultInjector &) = delete;
    RdbFaultInjector &operator=(const RdbFaultInjector &) = delete;

    struct InjectConfig {
        int64_t count = -1;    // -1 = unlimited; 0 = never inject
        int probability = 100; // 0-100, default 100 (always)
        std::atomic<int64_t> remaining{ 0 };
        std::mutex mutex;
        std::mt19937 rng{ std::random_device{}() };
    };

    struct PathEntry {
        std::shared_ptr<InjectConfig> openConfig;
        std::unordered_map<int, std::shared_ptr<InjectConfig>> opConfigs;
        std::unordered_map<std::string, std::shared_ptr<InjectConfig>> methodConfigs;

        bool IsEmpty() const
        {
            return openConfig == nullptr && opConfigs.empty() && methodConfigs.empty();
        }
    };

    // Parsed per-database config lives in rdb_fault_injector.cpp; the
    // Apply* helpers translate its fields into the runtime entry.
    void ApplyOpen(int64_t count, int32_t probability, PathEntry &entry) const;
    void ApplyOperation(const std::string &key, int64_t count, int32_t probability, PathEntry &entry) const;
    void ApplyMethod(const std::string &name, int64_t count, int32_t probability, PathEntry &entry) const;

    static bool TryConsume(InjectConfig &cfg);
    std::shared_ptr<PathEntry> LoadEntry() const;

    std::string dbPath_;
    // Immutable after construction: the ctor publishes it before the
    // instance is shared with any caller thread; verdict paths only read it.
    std::shared_ptr<PathEntry> entry_;
    bool enabled_ = false;
};


// The decorator-facing fault-injection surface; inj is the caller's
// std::shared_ptr<RdbFaultInjector>. RDB_FAULT_INJECT_CREATE brings the
// per-store instance to life at the start of the store's Init (idempotent
// and serialized, see RdbFaultInjector::CreateOnce);
// RDB_FAULT_INJECT_HIT decides by the per-method entry first, then the
// operation family; RDB_FAULT_INJECT_METHOD decides by the per-method entry
// only, for cleanup or file-level methods that belong to no operation
// family; RDB_FAULT_INJECT_WRAP re-seats a query result set with its
// decorated counterpart when the database has an injection entry (the
// expansion uses RdbResultSetDecorator, so the expansion site must include
// rdb_result_set_decorator.h). A null injector reads as a miss, never an
// error. Root builds expand the verdicts; user builds compile everything
// out, so call sites reference no injector symbols, the result-set
// decorator template is never instantiated and the shipped artifact carries
// no mock behavior at all. New injection points must use these macros -
// never call the injector verdict methods directly.
#ifdef RDB_FAULT_INJECTION_ENABLE
#define RDB_FAULT_INJECT_CREATE(flag, dbPath, inj) \
    RdbFaultInjector::CreateOnce(flag, dbPath, inj)
#define RDB_FAULT_INJECT_HIT(inj, op, name, ...) \
    if ((inj) != nullptr && (inj)->Hit(name, RdbFaultOp::op)) { \
        return __VA_ARGS__; \
    }
#define RDB_FAULT_INJECT_METHOD(inj, name, ret) \
    if ((inj) != nullptr && (inj)->CheckMethod(name)) { \
        return ret; \
    }
#define RDB_FAULT_INJECT_WRAP(resultSet, inj, Inner) \
    if ((resultSet) != nullptr && (inj) != nullptr && (inj)->HasEntry()) { \
        (resultSet) = std::make_shared<RdbResultSetDecorator<Inner>>((resultSet), (inj)); \
    }
#else
#define RDB_FAULT_INJECT_CREATE(flag, dbPath, inj)
#define RDB_FAULT_INJECT_HIT(inj, op, name, ...)
#define RDB_FAULT_INJECT_METHOD(inj, name, ret)
#define RDB_FAULT_INJECT_WRAP(resultSet, inj, Inner)
#endif

} // namespace NativeRdb
} // namespace OHOS

#endif // NATIVE_RDB_RDB_FAULT_INJECTOR_H

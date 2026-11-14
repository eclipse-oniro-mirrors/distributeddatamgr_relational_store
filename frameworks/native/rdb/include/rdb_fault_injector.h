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
// only tells the call site to return E_SQLITE_CORRUPT. RdbStoreDecorator
// owns one instance per store - constructed with the store's database path,
// which reads rdb_fault_inject_config.json once - and passes the instance to
// the transaction and result-set decorators it creates, so the budget state
// is shared by everything serving one database. The open path (store
// manager, sqlite connection) uses short-lived instances whose verdicts
// naturally reset per open cycle. The build wiring selects between the real
// implementation (rdb_fault_injector.cpp, root/debug builds) and the no-op
// implementation (rdb_fault_injector_noop.cpp, user builds), so user builds
// carry no behavior at all. The config file is parsed only through
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

} // namespace NativeRdb
} // namespace OHOS

#endif // NATIVE_RDB_RDB_FAULT_INJECTOR_H

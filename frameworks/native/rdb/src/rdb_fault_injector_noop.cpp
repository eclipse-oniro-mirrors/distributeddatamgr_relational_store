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

// No-op implementation of the RDB fault-injection mock layer for user
// (release) builds. The root build selects the real implementation in
// rdb_fault_injector.cpp via the rdb_fault_injection_enable switch declared
// in relational_store.gni; this translation unit keeps every hook site
// linkable while contributing no behavior, no config reads and no logs, so
// all verdicts are misses.

#include "rdb_fault_injector.h"

namespace OHOS {
namespace NativeRdb {

RdbFaultInjector::RdbFaultInjector(const std::string &dbPath)
{
    (void)dbPath;
}

bool RdbFaultInjector::Hit(const std::string &methodName, RdbFaultOp op)
{
    (void)methodName;
    (void)op;
    return false;
}

bool RdbFaultInjector::Check(RdbFaultOp op)
{
    (void)op;
    return false;
}

bool RdbFaultInjector::CheckMethod(const std::string &methodName)
{
    (void)methodName;
    return false;
}

bool RdbFaultInjector::ConsumeOpen()
{
    return false;
}

bool RdbFaultInjector::HasEntry() const
{
    return false;
}

} // namespace NativeRdb
} // namespace OHOS

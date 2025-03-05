/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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
#define LOG_TAG "RdbStatReporter"

#include "rdb_stat_reporter.h"

#include <fcntl.h>
#include <unistd.h>

#include <chrono>
#include <iomanip>
#include <limits>
#include <mutex>
#include <sstream>
#include <unordered_map>

#include "accesstoken_kit.h"
#include "connection.h"
#include "hisysevent_c.h"
#include "ipc_skeleton.h"
#include "logger.h"
#include "rdb_errno.h"
#include "rdb_manager_impl.h"
#include "rdb_time_utils.h"
#include "sqlite_global_config.h"
#include "sqlite_utils.h"
#include "task_executor.h"

namespace OHOS::NativeRdb {
using namespace OHOS::Rdb;
constexpr int32_t WAIT_TIME = 600;
constexpr int32_t TIMEOUT_FIRST = 1500;
constexpr int32_t TIMEOUT_SECOND = 5000;
constexpr int32_t TIMEOUT_THIRD = 10000;
using RdbMgr = DistributedRdb::RdbManagerImpl;

RdbStatReporter::RdbStatReporter(StatType statType, SubType subType, const RdbStoreConfig &config, ReportFunc func)
{
    startTime_ = std::chrono::steady_clock::now();
    statEvent_.statType = static_cast<uint32_t>(statType);
    statEvent_.bundleName = config.GetBundleName();
    statEvent_.storeName = SqliteUtils::Anonymous(config.GetName());
    statEvent_.subType = static_cast<uint32_t>(subType);
    reportFunc_ = func;
}

RdbStatReporter::~RdbStatReporter()
{
    if (reportFunc_ == nullptr) {
        return;
    }
    auto endTime = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime_).count();
    auto reportInterval = std::chrono::duration_cast<std::chrono::seconds>(endTime - reportTime_).count();
    if (duration >= TIMEOUT_FIRST && reportInterval >= WAIT_TIME) {
        statEvent_.costTime = GetTimeType(static_cast<uint32_t>(duration));
        auto pool = TaskExecutor::GetInstance().GetExecutor();
        if (pool == nullptr) {
            LOG_WARN("task pool err when RdbStatReporter");
        }
        pool->Execute(std::bind(reportFunc_, statEvent_));
        reportTime_ = std::chrono::steady_clock::now();
    }
}

TimeType RdbStatReporter::GetTimeType(uint32_t costTime)
{
    if (costTime >= TIMEOUT_FIRST && costTime < TIMEOUT_SECOND) {
        return TIME_LEVEL_FIRST;
    } else if (costTime >= TIMEOUT_SECOND && costTime < TIMEOUT_THIRD) {
        return TIME_LEVEL_SECOND;
    }
    return TIME_LEVEL_THIRD;
}

} // namespace OHOS::NativeRdb
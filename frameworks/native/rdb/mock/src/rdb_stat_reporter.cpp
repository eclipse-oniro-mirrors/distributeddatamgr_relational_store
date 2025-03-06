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

#include "rdb_stat_reporter.h"

namespace OHOS::NativeRdb {

RdbStatReporter::RdbStatReporter(
    StatType statType, SubType subType, const RdbStoreConfig &config, ReportFunc func)
{
}

RdbStatReporter::~RdbStatReporter()
{
}

TimeType RdbStatReporter::GetTimeType(uint32_t costTime)
{
    return TIME_LEVEL_FIRST;
}

void RdbStatReporter::setReportTime()
{
}

} // namespace OHOS::NativeRdb
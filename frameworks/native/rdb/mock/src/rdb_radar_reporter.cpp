/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
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

#include "rdb_radar_reporter.h"
#include "rdb_errno.h"

namespace OHOS::NativeRdb {


bool RdbRadar::hasHostPkg_ = false;
std::string RdbRadar::hostPkg_{ "" };

RdbRadar::RdbRadar(Scene scene, const char* funcName, std::string bundleName) : scene_(scene), funcName_(funcName)
{
}

RdbRadar::~RdbRadar()
{
}

RdbRadar& RdbRadar::operator=(int errCode)
{
    errCode_ = errCode;
    return *this;
}

RdbRadar::operator int() const
{
    return errCode_;
}

void RdbRadar::LocalReport(int bizSence, const char* funcName, int state, int errCode)
{
    (void)scene_;
    (void)funcName_;
}

void RdbRadar::GetHostPkgInfo(std::string bundleName)
{
}
}
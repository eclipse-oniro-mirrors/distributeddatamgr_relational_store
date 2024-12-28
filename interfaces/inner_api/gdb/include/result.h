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

#ifndef INTERFACE_GDB_RESULT_H
#define INTERFACE_GDB_RESULT_H

#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "edge.h"
#include "path.h"
#include "vertex.h"
#include "rdb_visibility.h"

namespace OHOS::DistributedDataAip {
using GraphValue = std::variant<std::monostate, int32_t, int64_t, double, std::string, std::vector<uint8_t>,
    std::shared_ptr<Vertex>, std::shared_ptr<Edge>, std::shared_ptr<Path>, std::nullptr_t>;

class API_EXPORT Result {
public:
    API_EXPORT virtual std::vector<std::unordered_map<std::string, GraphValue>> GetAllData() const = 0;
};
} // namespace OHOS::DistributedDataAip
#endif //INTERFACE_GDB_RESULT_H
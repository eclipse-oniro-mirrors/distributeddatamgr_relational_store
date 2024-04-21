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

#ifndef RELATIONAL_STORE_OBSERVER_H
#define RELATIONAL_STORE_OBSERVER_H

#include <string>
#include <variant>
#include <vector>

namespace OHOS {
namespace NativeRdb {
namespace DistributedDB {
using Type = std::variant<std::monostate, std::string, int64_t, double>;

enum ChangeType : uint32_t {
    OP_INSERT = 0,
    OP_UPDATE,
    OP_DELETE,
    OP_BUTT,
};

struct ChangedData {
    std::string tableName;
    std::vector<std::vector<Type>> primaryData[OP_BUTT];
};

class StoreObserver {
public:
    using StoreChangedInfo = std::vector<ChangedData>;
    virtual void OnChange(StoreChangedInfo &&data) {};
};
}
}
}

#endif //RELATIONAL_STORE_OBSERVER_H
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

#ifndef DISTRIBUTED_RDB_RDB_PREDICATES_MEMO_H
#define DISTRIBUTED_RDB_RDB_PREDICATES_MEMO_H

#include <algorithm>
#include <iterator>
#include <string>
#include <variant>
#include <vector>

namespace OHOS {
namespace DistributedRdb {
enum RdbPredicateOperator {
    EQUAL_TO,
    NOT_EQUAL_TO,
    AND,
    OR,
    ORDER_BY,
    LIMIT,
    BEGIN_GROUP,
    END_GROUP,
    IN,
    NOT_IN,
    CONTAIN,
    BEGIN_WITH,
    END_WITH,
    IS_NULL,
    IS_NOT_NULL,
    LIKE,
    GLOB,
    BETWEEN,
    NOT_BETWEEN,
    GREATER_THAN,
    GREATER_THAN_OR_EQUAL,
    LESS_THAN,
    LESS_THAN_OR_EQUAL,
    DISTINCT,
    INDEXED_BY,
    NOT_CONTAINS,
    NOT_LIKE,
    ASSETS_ONLY,
    NOT_GLOB,
    OPERATOR_MAX
};
using DataValue = std::variant<int64_t, std::string>;
struct RdbPredicateOperation {
    RdbPredicateOperator operator_;
    std::string field_;
    std::vector<DataValue> values_;
};

struct PredicatesMemo {
    inline void AddOperation(const RdbPredicateOperator op, const std::string &field, const std::string &value)
    {
        operations_.push_back({ op, field, { value } });
    }
    inline void AddOperation(const RdbPredicateOperator op, const std::string &field, const int64_t &value)
    {
        operations_.push_back({ op, field, { value } });
    }
    inline void AddOperation(
        const RdbPredicateOperator op, const std::string &field, const std::vector<std::string> &values)
    {
        std::vector<DataValue> values_;
        values_.reserve(values.size());
        std::copy(values.begin(), values.end(), std::back_inserter(values_));
        operations_.push_back({ op, field, values_ });
    }

    inline void AddOperation(
        const RdbPredicateOperator op, const std::string &field, const std::vector<int64_t> &values)
    {
        std::vector<DataValue> values_;
        values_.reserve(values.size());
        std::copy(values.begin(), values.end(), std::back_inserter(values_));
        operations_.push_back({ op, field, values_ });
    }

    std::vector<std::string> tables_;
    std::vector<std::string> devices_;
    std::vector<RdbPredicateOperation> operations_;
};
} // namespace DistributedRdb
} // namespace OHOS
#endif // DISTRIBUTED_RDB_RDB_PREDICATES_MEMO_H

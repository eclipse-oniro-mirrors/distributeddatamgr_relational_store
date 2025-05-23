/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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
#define LOG_TAG "RdbUtils"
#include "rdb_utils.h"

#include "logger.h"
#include "raw_data_parser.h"

using namespace OHOS::Rdb;
using namespace OHOS::RdbDataShareAdapter;
using namespace OHOS::DataShare;
using namespace OHOS::NativeRdb;

constexpr RdbUtils::OperateHandler RdbUtils::HANDLERS[LAST_TYPE];

ValuesBucket RdbUtils::ToValuesBucket(DataShareValuesBucket valuesBucket)
{
    std::map<std::string, ValueObject> valuesMap;
    for (auto &[key, dsValue] : valuesBucket.valuesMap) {
        ValueObject::Type value;
        RawDataParser::Convert(std::move(dsValue), value);
        valuesMap.insert(std::pair<std::string, ValueObject>(key, std::move(value)));
    }
    return ValuesBucket(valuesMap);
}

RdbPredicates RdbUtils::ToPredicates(const DataShareAbsPredicates &predicates, const std::string &table)
{
    RdbPredicates rdbPredicates(table);
    if (predicates.GetSettingMode() == QUERY_LANGUAGE) {
        rdbPredicates.SetWhereClause(predicates.GetWhereClause());
        rdbPredicates.SetWhereArgs(predicates.GetWhereArgs());
        rdbPredicates.SetOrder(predicates.GetOrder());
    }

    const auto &operations = predicates.GetOperationList();
    for (const auto &oper : operations) {
        if (oper.operation >= 0 && oper.operation < LAST_TYPE) {
            (*HANDLERS[oper.operation])(oper, rdbPredicates);
        }
    }
    return rdbPredicates;
}

OHOS::NativeRdb::ValueObject RdbUtils::ToValueObject(const DataSharePredicatesObject &predicatesObject)
{
    if (auto *val = std::get_if<int>(&predicatesObject.value)) {
        return ValueObject(*val);
    }
    ValueObject::Type value;
    RawDataParser::Convert(std::move(predicatesObject.value), value);
    return value;
}

std::shared_ptr<ResultSetBridge> RdbUtils::ToResultSetBridge(std::shared_ptr<ResultSet> resultSet)
{
    if (resultSet == nullptr) {
        LOG_ERROR("resultSet is null.");
        return nullptr;
    }
    return std::make_shared<RdbResultSetBridge>(resultSet);
}

void RdbUtils::NoSupport(const OperationItem &item, RdbPredicates &query)
{
    LOG_ERROR("invalid operation:%{public}d", item.operation);
}

void RdbUtils::EqualTo(const OperationItem &item, RdbPredicates &predicates)
{
    // 2 is the number of argument item.singleParams
    if (item.singleParams.size() < 2) {
        LOG_ERROR("SingleParams is missing elements, size is %{public}zu", item.singleParams.size());
        return;
    }
    predicates.EqualTo(item.GetSingle(0), ToValueObject(item.GetSingle(1)));
}

void RdbUtils::NotEqualTo(const OperationItem &item, RdbPredicates &predicates)
{
    // 2 is the number of argument item.singleParams
    if (item.singleParams.size() < 2) {
        LOG_ERROR("SingleParams is missing elements, size is %{public}zu", item.singleParams.size());
        return;
    }
    predicates.NotEqualTo(item.GetSingle(0), ToValueObject(item.GetSingle(1)));
}

void RdbUtils::GreaterThan(const OperationItem &item, RdbPredicates &predicates)
{
    // 2 is the number of argument item.singleParams
    if (item.singleParams.size() < 2) {
        LOG_ERROR("SingleParams is missing elements, size is %{public}zu", item.singleParams.size());
        return;
    }
    predicates.GreaterThan(item.GetSingle(0), ToValueObject(item.GetSingle(1)));
}

void RdbUtils::LessThan(const OperationItem &item, RdbPredicates &predicates)
{
    // 2 is the number of argument item.singleParams
    if (item.singleParams.size() < 2) {
        LOG_ERROR("SingleParams is missing elements, size is %{public}zu", item.singleParams.size());
        return;
    }
    predicates.LessThan(item.GetSingle(0), ToValueObject(item.GetSingle(1)));
}

void RdbUtils::GreaterThanOrEqualTo(const OperationItem &item, RdbPredicates &predicates)
{
    // 2 is the number of argument item.singleParams
    if (item.singleParams.size() < 2) {
        LOG_ERROR("SingleParams is missing elements, size is %{public}zu", item.singleParams.size());
        return;
    }
    predicates.GreaterThanOrEqualTo(item.GetSingle(0), ToValueObject(item.GetSingle(1)));
}

void RdbUtils::LessThanOrEqualTo(const OperationItem &item, RdbPredicates &predicates)
{
    // 2 is the number of argument item.singleParams
    if (item.singleParams.size() < 2) {
        LOG_ERROR("SingleParams is missing elements, size is %{public}zu", item.singleParams.size());
        return;
    }
    predicates.LessThanOrEqualTo(item.GetSingle(0), ToValueObject(item.GetSingle(1)));
}

void RdbUtils::And(const DataShare::OperationItem &item, RdbPredicates &predicates)
{
    predicates.And();
}

void RdbUtils::Or(const DataShare::OperationItem &item, RdbPredicates &predicates)
{
    predicates.Or();
}

void RdbUtils::IsNull(const OperationItem &item, RdbPredicates &predicates)
{
    if (item.singleParams.size() < 1) {
        LOG_ERROR("SingleParams is missing elements, size is %{public}zu", item.singleParams.size());
        return;
    }
    predicates.IsNull(item.GetSingle(0));
}

void RdbUtils::IsNotNull(const DataShare::OperationItem &item, RdbPredicates &predicates)
{
    if (item.singleParams.size() < 1) {
        LOG_ERROR("SingleParams is missing elements, size is %{public}zu", item.singleParams.size());
        return;
    }
    predicates.IsNotNull(item.GetSingle(0));
}

void RdbUtils::In(const DataShare::OperationItem &item, RdbPredicates &predicates)
{
    if (item.singleParams.size() < 1 || item.multiParams.size() < 1) {
        LOG_ERROR(
            "SingleParams size is %{public}zu, MultiParams size is %{public}zu",
            item.singleParams.size(), item.multiParams.size());
        return;
    }
    predicates.In(item.GetSingle(0), MutliValue(item.multiParams[0]));
}

void RdbUtils::NotIn(const DataShare::OperationItem &item, RdbPredicates &predicates)
{
    if (item.singleParams.size() < 1 || item.multiParams.size() < 1) {
        LOG_ERROR(
            "SingleParams size is %{public}zu, MultiParams size is %{public}zu",
            item.singleParams.size(), item.multiParams.size());
        return;
    }
    predicates.NotIn(item.GetSingle(0), MutliValue(item.multiParams[0]));
}

void RdbUtils::Like(const DataShare::OperationItem &item, RdbPredicates &predicates)
{
    // 2 is the number of argument item.singleParams
    if (item.singleParams.size() < 2) {
        LOG_ERROR("SingleParams is missing elements, size is %{public}zu", item.singleParams.size());
        return;
    }
    predicates.Like(item.GetSingle(0), ToValueObject(item.GetSingle(1)));
}

void RdbUtils::NotLike(const DataShare::OperationItem &item, RdbPredicates &predicates)
{
    // 2 is the number of argument item.singleParams
    if (item.singleParams.size() < 2) {
        LOG_ERROR("SingleParams is missing elements, size is %{public}zu", item.singleParams.size());
        return;
    }
    predicates.NotLike(item.GetSingle(0), ToValueObject(item.GetSingle(1)));
}

void RdbUtils::OrderByAsc(const DataShare::OperationItem &item, RdbPredicates &predicates)
{
    if (item.singleParams.size() < 1) {
        LOG_ERROR("SingleParams is missing elements, size is %{public}zu", item.singleParams.size());
        return;
    }
    predicates.OrderByAsc(item.GetSingle(0));
}

void RdbUtils::OrderByDesc(const DataShare::OperationItem &item, RdbPredicates &predicates)
{
    if (item.singleParams.size() < 1) {
        LOG_ERROR("SingleParams is missing elements, size is %{public}zu", item.singleParams.size());
        return;
    }
    predicates.OrderByDesc(item.GetSingle(0));
}

void RdbUtils::Limit(const DataShare::OperationItem &item, RdbPredicates &predicates)
{
    // 2 is the number of argument item.singleParams
    if (item.singleParams.size() < 2) {
        LOG_ERROR("SingleParams is missing elements, size is %{public}zu", item.singleParams.size());
        return;
    }
    predicates.Limit(item.GetSingle(0));
    predicates.Offset(item.GetSingle(1));
}

void RdbUtils::Offset(const DataShare::OperationItem &item, RdbPredicates &predicates)
{
    if (item.singleParams.size() < 1) {
        LOG_ERROR("SingleParams is missing elements, size is %{public}zu", item.singleParams.size());
        return;
    }
    predicates.Offset(item.GetSingle(0));
}

void RdbUtils::BeginWrap(const DataShare::OperationItem &item, RdbPredicates &predicates)
{
    predicates.BeginWrap();
}

void RdbUtils::EndWrap(const DataShare::OperationItem &item, RdbPredicates &predicates)
{
    predicates.EndWrap();
}

void RdbUtils::BeginsWith(const DataShare::OperationItem &item, RdbPredicates &predicates)
{
    // 2 is the number of argument item.singleParams
    if (item.singleParams.size() < 2) {
        LOG_ERROR("SingleParams is missing elements, size is %{public}zu", item.singleParams.size());
        return;
    }
    predicates.BeginsWith(item.GetSingle(0), ToValueObject(item.GetSingle(1)));
}

void RdbUtils::EndsWith(const DataShare::OperationItem &item, RdbPredicates &predicates)
{
    // 2 is the number of argument item.singleParams
    if (item.singleParams.size() < 2) {
        LOG_ERROR("SingleParams is missing elements, size is %{public}zu", item.singleParams.size());
        return;
    }
    predicates.EndsWith(item.GetSingle(0), ToValueObject(item.GetSingle(1)));
}

void RdbUtils::Distinct(const DataShare::OperationItem &item, RdbPredicates &predicates)
{
    predicates.Distinct();
}

void RdbUtils::GroupBy(const DataShare::OperationItem &item, RdbPredicates &predicates)
{
    if (item.multiParams.size() < 1) {
        LOG_ERROR("MultiParams is missing elements, size is %{public}zu", item.multiParams.size());
        return;
    }
    predicates.GroupBy(MutliValue(item.multiParams[0]));
}

void RdbUtils::IndexedBy(const DataShare::OperationItem &item, RdbPredicates &predicates)
{
    if (item.singleParams.size() < 1) {
        LOG_ERROR("SingleParams is missing elements, size is %{public}zu", item.singleParams.size());
        return;
    }
    predicates.IndexedBy(item.GetSingle(0));
}

void RdbUtils::Contains(const DataShare::OperationItem &item, RdbPredicates &predicates)
{
    // 2 is the number of argument item.singleParams
    if (item.singleParams.size() < 2) {
        LOG_ERROR("SingleParams is missing elements, size is %{public}zu", item.singleParams.size());
        return;
    }
    predicates.Contains(item.GetSingle(0), ToValueObject(item.GetSingle(1)));
}

void RdbUtils::NotContains(const DataShare::OperationItem &item, RdbPredicates &predicates)
{
    // 2 is the number of argument item.singleParams
    if (item.singleParams.size() < 2) {
        LOG_ERROR("SingleParams is missing elements, size is %{public}zu", item.singleParams.size());
        return;
    }
    predicates.NotContains(item.GetSingle(0), ToValueObject(item.GetSingle(1)));
}

void RdbUtils::Glob(const DataShare::OperationItem &item, RdbPredicates &predicates)
{
    // 2 is the number of argument item.singleParams
    if (item.singleParams.size() < 2) {
        LOG_ERROR("SingleParams is missing elements, size is %{public}zu", item.singleParams.size());
        return;
    }
    predicates.Glob(item.GetSingle(0), ToValueObject(item.GetSingle(1)));
}

void RdbUtils::Between(const DataShare::OperationItem &item, RdbPredicates &predicates)
{
    // 3 is the number of argument item.singleParams
    if (item.singleParams.size() < 3) {
        LOG_ERROR("SingleParams is missing elements, size is %{public}zu", item.singleParams.size());
        return;
    }
    // singleParams[2] is another param
    predicates.Between(item.GetSingle(0), ToValueObject(item.GetSingle(1)), ToValueObject(item.GetSingle(2)));
}

void RdbUtils::NotBetween(const DataShare::OperationItem &item, RdbPredicates &predicates)
{
    // 3 is the number of argument item.singleParams
    if (item.singleParams.size() < 3) {
        LOG_ERROR("SingleParams is missing elements, size is %{public}zu", item.singleParams.size());
        return;
    }
    // singleParams[2] is another param
    predicates.NotBetween(item.GetSingle(0), ToValueObject(item.GetSingle(1)), ToValueObject(item.GetSingle(2)));
}

void RdbUtils::CrossJoin(const DataShare::OperationItem &item, RdbPredicates &predicates)
{
    if (item.singleParams.size() < 1) {
        LOG_ERROR("SingleParams is missing elements, size is %{public}zu", item.singleParams.size());
        return;
    }
    predicates.CrossJoin(item.GetSingle(0));
}

void RdbUtils::InnerJoin(const DataShare::OperationItem &item, RdbPredicates &predicates)
{
    if (item.singleParams.size() < 1) {
        LOG_ERROR("SingleParams is missing elements, size is %{public}zu", item.singleParams.size());
        return;
    }
    predicates.InnerJoin(item.GetSingle(0));
}

void RdbUtils::LeftOuterJoin(const DataShare::OperationItem &item, RdbPredicates &predicates)
{
    if (item.singleParams.size() < 1) {
        LOG_ERROR("SingleParams is missing elements, size is %{public}zu", item.singleParams.size());
        return;
    }
    predicates.LeftOuterJoin(item.GetSingle(0));
}

void RdbUtils::Using(const DataShare::OperationItem &item, RdbPredicates &predicates)
{
    if (item.multiParams.size() < 1) {
        LOG_ERROR("MultiParams is missing elements, size is %{public}zu", item.multiParams.size());
        return;
    }
    predicates.Using(MutliValue(item.multiParams[0]));
}

void RdbUtils::On(const DataShare::OperationItem &item, RdbPredicates &predicates)
{
    if (item.multiParams.size() < 1) {
        LOG_ERROR("MultiParams is missing elements, size is %{public}zu", item.multiParams.size());
        return;
    }
    predicates.On(MutliValue(item.multiParams[0]));
}

RdbUtils::RdbUtils()
{
}

RdbUtils::~RdbUtils()
{
}

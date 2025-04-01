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

#include "relational_store_utils.h"
#include "native_log.h"
#include "rdb_store.h"

namespace OHOS {
namespace Relational {
    char* MallocCString(const std::string& origin)
    {
        if (origin.empty()) {
            return nullptr;
        }
        auto len = origin.length() + 1;
        char* res = static_cast<char*>(malloc(sizeof(char) * len));
        if (res == nullptr) {
            return nullptr;
        }
        return std::char_traits<char>::copy(res, origin.c_str(), len);
    }

    NativeRdb::ValueObject ValueTypeToValueObjectBlob(const ValueType& value)
    {
        std::vector<uint8_t> blob = std::vector<uint8_t>();
        for (int64_t j = 0; j < value.Uint8Array.size; j++) {
            blob.push_back(value.Uint8Array.head[j]);
        }
        return NativeRdb::ValueObject(blob);
    }

    NativeRdb::ValueObject ValueTypeToValueObjectAsset(const ValueType& value)
    {
        std::string modifyTime = value.asset.modifyTime;
        std::string size = value.asset.size;
        NativeRdb::ValueObject::Asset asset = {
            .status = value.asset.status,
            .name = value.asset.name,
            .uri = value.asset.uri,
            .createTime = value.asset.createTime,
            .modifyTime = modifyTime,
            .size = size,
            .hash = modifyTime + "_" + size,
            .path = value.asset.path
        };
        return NativeRdb::ValueObject(asset);
    }

    NativeRdb::ValueObject ValueTypeToValueObjectAssets(const ValueType& value)
    {
        std::vector<NativeRdb::ValueObject::Asset> assets = std::vector<NativeRdb::ValueObject::Asset>();
        for (int64_t j = 0; j < value.assets.size; j++) {
            Asset asset = value.assets.head[j];
            std::string modifyTime = asset.modifyTime;
            std::string size = asset.size;
            NativeRdb::ValueObject::Asset nativeAsset = {
                .status = asset.status,
                .name = asset.name,
                .uri = asset.uri,
                .createTime = asset.createTime,
                .modifyTime = modifyTime,
                .size = size,
                .hash = modifyTime + "_" + size,
                .path = asset.path
            };
            assets.push_back(nativeAsset);
        }
        return NativeRdb::ValueObject(assets);
    }

    NativeRdb::ValueObject ValueTypeToValueObject(const ValueType& value)
    {
        NativeRdb::ValueObject valueObject;
        switch (value.tag) {
            case TYPE_NULL: {
                valueObject = NativeRdb::ValueObject();
                break;
            }
            case TYPE_INT: {
                valueObject = NativeRdb::ValueObject(value.integer);
                break;
            }
            case TYPE_DOU: {
                valueObject = NativeRdb::ValueObject(value.dou);
                break;
            }
            case TYPE_STR: {
                valueObject = NativeRdb::ValueObject(value.string);
                break;
            }
            case TYPE_BOOL: {
                valueObject = NativeRdb::ValueObject(value.boolean);
                break;
            }
            case TYPE_BLOB: {
                valueObject = ValueTypeToValueObjectBlob(value);
                break;
            }
            case TYPE_ASSET: {
                valueObject = ValueTypeToValueObjectAsset(value);
                break;
            }
            case TYPE_ASSETS: {
                valueObject = ValueTypeToValueObjectAssets(value);
                break;
            }
            default:
                valueObject = NativeRdb::ValueObject();
                break;
        }
        return valueObject;
    }

    ValueType ValueObjectToValueTypeAsset(const NativeRdb::ValueObject& object)
    {
        NativeRdb::ValueObject::Asset val;
        object.GetAsset(val);
        Asset asset = Asset {
            .name = MallocCString(val.name),
            .uri = MallocCString(val.uri),
            .path = MallocCString(val.path),
            .createTime = MallocCString(val.createTime),
            .modifyTime = MallocCString(val.modifyTime),
            .size = MallocCString(val.size),
            .status = val.status
        };
        return ValueType {.asset = asset, .tag = TYPE_ASSET};
    }

    ValueType ValueObjectToValueTypeAssets(const NativeRdb::ValueObject& object)
    {
        NativeRdb::ValueObject::Assets val;
        object.GetAssets(val);
        if (val.size() == 0) {
            return ValueType {.assets = Assets{nullptr, -1}, .tag = TYPE_ASSETS};
        }
        Assets assets = Assets {.head = static_cast<Asset*>(malloc(val.size() * sizeof(Asset))), .size = val.size()};
        if (assets.head == nullptr) {
            return ValueType {.assets = Assets{nullptr, -1}, .tag = TYPE_ASSETS};
        }
        for (std::size_t i = 0; i < val.size(); i++) {
            assets.head[i] = Asset {
                .name = MallocCString(val[i].name),
                .uri = MallocCString(val[i].uri),
                .path = MallocCString(val[i].path),
                .createTime = MallocCString(val[i].createTime),
                .modifyTime = MallocCString(val[i].modifyTime),
                .size = MallocCString(val[i].size),
                .status = static_cast<int32_t>(val[i].status)
            };
        }
        return ValueType {.assets = assets, .tag = TYPE_ASSETS};
    }

    ValueType ValueObjectToValueType(const NativeRdb::ValueObject& object)
    {
        switch (object.GetType()) {
            case NativeRdb::ValueObject::TYPE_NULL:
                return ValueType {.tag = TYPE_NULL};
            case NativeRdb::ValueObject::TYPE_INT: {
                int64_t val;
                object.GetLong(val);
                return ValueType {.integer = val, .tag = TYPE_INT};
            }
            case NativeRdb::ValueObject::TYPE_DOUBLE: {
                double val;
                object.GetDouble(val);
                return ValueType {.dou = val, .tag = TYPE_DOU};
            }
            case NativeRdb::ValueObject::TYPE_STRING: {
                std::string val;
                object.GetString(val);
                return ValueType {.string = MallocCString(val), .tag = TYPE_STR};
            }
            case NativeRdb::ValueObject::TYPE_BOOL: {
                bool val;
                object.GetBool(val);
                return ValueType {.boolean = val, .tag = TYPE_BOOL};
            }
            case NativeRdb::ValueObject::TYPE_BLOB: {
                std::vector<uint8_t> val;
                object.GetBlob(val);
                if (val.size() == 0) {
                    return ValueType {.Uint8Array = CArrUI8 {nullptr, -1}, .tag = TYPE_BLOB};
                }
                CArrUI8 arr = CArrUI8 {.head = static_cast<uint8_t*>(malloc(val.size() * sizeof(uint8_t))),
                    .size = val.size()};
                if (arr.head == nullptr) {
                    return ValueType {.Uint8Array = CArrUI8 {nullptr, -1}, .tag = TYPE_BLOB};
                }
                return ValueType {.Uint8Array = arr, .tag = TYPE_BLOB};
            }
            case NativeRdb::ValueObject::TYPE_ASSET: {
                return ValueObjectToValueTypeAsset(object);
            }
            case NativeRdb::ValueObject::TYPE_ASSETS: {
                return ValueObjectToValueTypeAssets(object);
            }
            case NativeRdb::ValueObject::TYPE_BUTT:
                return ValueType {.tag = 128};
            default:
                return ValueType {.tag = TYPE_NULL};
        }
    }

    CArrStr VectorToCArrStr(const std::vector<std::string> &devices)
    {
        CArrStr cArrStr{0};
        if (devices.size() == 0) {
            return cArrStr;
        }
        cArrStr.head = static_cast<char**>(malloc(sizeof(char*) * devices.size()));
        if (cArrStr.head == nullptr) {
            return cArrStr;
        }
        for (size_t i = 0; i < devices.size(); i++) {
            cArrStr.head[i] = MallocCString(devices[i]);
        }
        cArrStr.size = static_cast<int64_t>(devices.size());
        return cArrStr;
    }

    std::vector<std::string> CArrStrToVector(CArrStr carr)
    {
        std::vector<std::string> arr;
        for (int i = 0; i < carr.size; i++) {
            if (carr.head[i] != nullptr) {
                arr.push_back(carr.head[i]);
            } else {
                arr.push_back(std::string());
            }
        }
        return arr;
    }

    std::variant<std::monostate, std::string, int64_t, double> RetPRIKeyTypeToVariant(RetPRIKeyType &value)
    {
        switch (value.tag) {
            case NativeRdb::ValueObject::TYPE_INT:
                return std::variant<std::monostate, std::string, int64_t, double>(value.integer);
            case NativeRdb::ValueObject::TYPE_DOUBLE:
                return std::variant<std::monostate, std::string, int64_t, double>(value.dou);
            case NativeRdb::ValueObject::TYPE_STRING:
                return std::variant<std::monostate, std::string, int64_t, double>(value.string);
            default:
                return std::variant<std::monostate, std::string, int64_t, double>(0);
        }
    }

    RetPRIKeyType VariantToRetPRIKeyType(const std::variant<std::monostate, std::string, int64_t, double> &value)
    {
        if (std::holds_alternative<int64_t>(value)) {
            return RetPRIKeyType{ .integer = std::get<int64_t>(value), .dou = 0.0,
                .string = nullptr, .tag = NativeRdb::ValueObject::TYPE_INT };
        } else if (std::holds_alternative<double>(value)) {
            return RetPRIKeyType{ .integer = 0, .dou = std::get<double>(value),
                .string = nullptr, .tag = NativeRdb::ValueObject::TYPE_DOUBLE };
        } else if (std::holds_alternative<std::string>(value)) {
            return RetPRIKeyType{ .integer = 0, .dou = 0.0,
                .string = MallocCString(std::get<std::string>(value)), .tag = NativeRdb::ValueObject::TYPE_STRING };
        } else {
            return RetPRIKeyType{0};
        }
    }

    std::vector<NativeRdb::RdbStore::PRIKey> CArrPRIKeyTypeToPRIKeyArray(CArrPRIKeyType &cPrimaryKeys)
    {
        std::vector<NativeRdb::RdbStore::PRIKey> res = std::vector<NativeRdb::RdbStore::PRIKey>();
        for (int64_t i = 0; i < cPrimaryKeys.size; i++) {
            res.push_back(RetPRIKeyTypeToVariant(cPrimaryKeys.head[i]));
        }
        return res;
    }

    ModifyTime MapToModifyTime(std::map<NativeRdb::RdbStore::PRIKey, NativeRdb::RdbStore::Date> &map, int32_t &errCode)
    {
        ModifyTime modifyTime{0};
        modifyTime.size = static_cast<int64_t>(map.size());
        if (modifyTime.size == 0) {
            return ModifyTime{0};
        }
        modifyTime.key = static_cast<RetPRIKeyType*>(malloc(sizeof(RetPRIKeyType) * modifyTime.size));
        modifyTime.value = static_cast<uint64_t*>(malloc(sizeof(uint64_t) * modifyTime.size));
        if (modifyTime.key == nullptr || modifyTime.value == nullptr) {
            free(modifyTime.key);
            free(modifyTime.value);
            errCode = -1;
            return ModifyTime{0};
        }
        int64_t index = 0;
        for (auto it = map.begin(); it != map.end(); ++it) {
            modifyTime.key[index] = VariantToRetPRIKeyType(it->first);
            modifyTime.value[index] = static_cast<uint64_t>((it->second).date);
            index++;
        }
        return modifyTime;
    }

    CArrPRIKeyType VectorToCArrPRIKeyType(std::vector<DistributedRdb::RdbStoreObserver::PrimaryKey> arr)
    {
        CArrPRIKeyType types{0};
        if (arr.size() == 0) {
            return types;
        }
        types.head = static_cast<RetPRIKeyType*>(malloc(sizeof(RetPRIKeyType) * arr.size()));
        if (types.head == nullptr) {
            return types;
        }
        for (size_t i = 0; i < arr.size(); i++) {
            types.head[i] = VariantToRetPRIKeyType(arr[i]);
        }
        types.size = static_cast<int64_t>(arr.size());
        return types;
    }

    RetChangeInfo ToRetChangeInfo(const DistributedRdb::Origin &origin,
        DistributedRdb::RdbStoreObserver::ChangeInfo::iterator info)
    {
        RetChangeInfo retInfo{0};
        retInfo.table = MallocCString(info->first);
        retInfo.type = origin.dataType;
        retInfo.inserted = VectorToCArrPRIKeyType(info->
            second[DistributedRdb::RdbStoreObserver::ChangeType::CHG_TYPE_INSERT]);
        retInfo.updated = VectorToCArrPRIKeyType(info->
            second[DistributedRdb::RdbStoreObserver::ChangeType::CHG_TYPE_UPDATE]);
        retInfo.deleted = VectorToCArrPRIKeyType(info->
            second[DistributedRdb::RdbStoreObserver::ChangeType::CHG_TYPE_DELETE]);
        return retInfo;
    }

    CArrRetChangeInfo ToCArrRetChangeInfo(const DistributedRdb::Origin &origin,
        const DistributedRdb::RdbStoreObserver::PrimaryFields &fields,
        DistributedRdb::RdbStoreObserver::ChangeInfo &&changeInfo)
    {
        CArrRetChangeInfo infos{0};
        if (changeInfo.size() == 0) {
            return infos;
        }
        infos.head = static_cast<RetChangeInfo*>(malloc(sizeof(RetChangeInfo) * changeInfo.size()));
        if (infos.head == nullptr) {
            return CArrRetChangeInfo{0};
        }
        int64_t index = 0;
        for (auto it = changeInfo.begin(); it != changeInfo.end(); ++it) {
            infos.head[index] = ToRetChangeInfo(origin, it);
            index++;
        }
        infos.size = static_cast<int64_t>(changeInfo.size());
        return infos;
    }

    CStatistic ToStatistic(DistributedRdb::Statistic statistic)
    {
        return CStatistic{ .total = statistic.total, .successful = statistic.success,
            .failed = statistic.failed, .remained = statistic.untreated };
    }

    CTableDetails ToCTableDetails(DistributedRdb::TableDetail detail)
    {
        return CTableDetails{ .upload = ToStatistic(detail.upload), .download = ToStatistic(detail.download) };
    }

    CDetails ToCDetails(DistributedRdb::TableDetails details)
    {
        if (details.size() == 0) {
            return CDetails{0};
        }
        char** key = static_cast<char**>(malloc(sizeof(char*) * details.size()));
        CTableDetails* value = static_cast<CTableDetails*>(malloc(sizeof(CTableDetails) * details.size()));
        if (key == nullptr || value == nullptr) {
            free(key);
            free(value);
            return CDetails{0};
        }
        int64_t index = 0;
        for (auto it = details.begin(); it != details.end(); ++it) {
            key[index] = MallocCString(it->first);
            value[index] = ToCTableDetails(it->second);
            index++;
        }
        return CDetails{ .key = key, .value = value, .size = details.size() };
    }

    CProgressDetails ToCProgressDetails(const  DistributedRdb::Details &details)
    {
        if (details.empty()) {
            return CProgressDetails{0};
        }
        DistributedRdb::ProgressDetail detail = details.begin() ->second;
        return CProgressDetails{ .schedule = detail.progress, .code = detail.code,
            .details = ToCDetails(detail.details) };
    }
}
}
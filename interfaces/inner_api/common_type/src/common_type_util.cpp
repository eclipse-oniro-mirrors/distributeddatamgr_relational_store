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

#include "common_type_util.h"
namespace OHOS::ITypesUtil {
template<>
bool Marshalling(const ValueObject &input, MessageParcel &data)
{
    return Marshal(data, input.value);
}
template<>
bool Unmarshalling(ValueObject &output, MessageParcel &data)
{
    return Unmarshal(data, output.value);
}

template<>
bool Marshalling(const ValuesBucket &input, MessageParcel &data)
{
    return Marshal(data, input.values_);
}
template<>
bool Unmarshalling(ValuesBucket &output, MessageParcel &data)
{
    return Unmarshal(data, output.values_);
}

template<>
bool Marshalling(const Asset &input, MessageParcel &data)
{
    return Marshal(data, input.version, input.name, input.size, input.modifyTime, input.uri);
}
template<>
bool Unmarshalling(Asset &output, MessageParcel &data)
{
    return Unmarshal(data, output.version, output.name, output.size, output.modifyTime, output.uri);
}
}

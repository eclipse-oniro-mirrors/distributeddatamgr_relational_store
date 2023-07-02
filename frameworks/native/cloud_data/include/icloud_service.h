/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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

#ifndef OHOS_DISTRIBUTED_DATA_CLOUD_ICLOUD_SERVICE_H
#define OHOS_DISTRIBUTED_DATA_CLOUD_ICLOUD_SERVICE_H

#include "cloud_service.h"
#include "iremote_broker.h"
#include "distributeddata_relational_store_ipc_interface_code.h"

namespace OHOS::CloudData {
class ICloudService : public CloudService, public IRemoteBroker {
public:
    DECLARE_INTERFACE_DESCRIPTOR(u"OHOS.CloudData.CloudServer");
};

class IKvStoreDataService : public IRemoteBroker {
public:
    virtual sptr<IRemoteObject> GetFeatureInterface(const std::string &name) = 0;

    DECLARE_INTERFACE_DESCRIPTOR(u"OHOS.DistributedKv.IKvStoreDataService");
};
} // namespace OHOS::CloudData
#endif // OHOS_DISTRIBUTED_DATA_CLOUD_ICLOUD_SERVICE_H

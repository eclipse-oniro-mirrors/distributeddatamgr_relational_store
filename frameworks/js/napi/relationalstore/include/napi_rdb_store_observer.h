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

#ifndef NAPI_RDB_STORE_OBSERVER_H
#define NAPI_RDB_STORE_OBSERVER_H

#include "rdb_types.h"
#include "napi_uv_queue.h"

namespace OHOS::RelationalStoreJsKit  {
class NapiRdbStoreObserver : public DistributedRdb::RdbStoreObserver, public NapiUvQueue {
public:
    using Origin = DistributedRdb::Origin;
    explicit NapiRdbStoreObserver(napi_env env, napi_value callback, int32_t mode = DistributedRdb::REMOTE);
    virtual ~NapiRdbStoreObserver() noexcept;

    void OnChange(const std::vector<std::string>& devices) override;

    void OnChange(const Origin &origin, const PrimaryFields &fields, ChangeInfo &&changeInfo) override;

private:
    int32_t mode_ = DistributedRdb::REMOTE;
};
} // namespace OHOS::RelationalStoreJsKit
#endif

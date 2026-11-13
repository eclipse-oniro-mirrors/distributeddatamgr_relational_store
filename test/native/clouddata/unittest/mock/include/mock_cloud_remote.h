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
#ifndef RDB_MOCK_CLOUD_REMOTE_H
#define RDB_MOCK_CLOUD_REMOTE_H

#include <functional>
#include <gmock/gmock.h>

#include "ipc_object_stub.h"

namespace OHOS::CloudData {
class MockCloudRemote : public IPCObjectStub {
public:
    MOCK_METHOD(int, SendRequest, (uint32_t, MessageParcel &, MessageParcel &, MessageOption &), (override));
};

// Model only the remote reply. Server authorization is covered by the service Stub tests.
void ExpectCloudReply(MockCloudRemote &remote, uint32_t code, int32_t status,
    const std::function<void(MessageParcel &)> &inspect = {});
} // namespace OHOS::CloudData
#endif // RDB_MOCK_CLOUD_REMOTE_H

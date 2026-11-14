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
#include "mock_cloud_remote.h"

#include "cloud_service_proxy.h"
#include "cloud_types_util.h"
#include "itypes_util.h"

namespace OHOS::CloudData {
namespace {
bool WriteReply(MessageParcel &reply, uint32_t code, int32_t status)
{
    if (!ITypesUtil::Marshal(reply, status)) {
        return false;
    }
    switch (code) {
        case CloudService::TRANS_QUERY_STATISTICS:
            // Empty statistic map on the wire; the client only implements its decoder.
            return reply.WriteInt32(0);
        case CloudService::TRANS_QUERY_LAST_SYNC_INFO:
            return ITypesUtil::Marshal(reply, QueryLastResults{});
        case CloudService::TRANS_QUERY_LAST_SYNC_INFO_BATCH:
            return ITypesUtil::Marshal(reply, BatchQueryLastResults{});
        case CloudService::TRANS_ALLOC_RESOURCE_AND_SHARE:
            return ITypesUtil::Marshal(reply, std::vector<NativeRdb::ValuesBucket>{});
        case CloudService::TRANS_SHARE:
        case CloudService::TRANS_UNSHARE:
        case CloudService::TRANS_CHANGE_PRIVILEGE:
            return ITypesUtil::Marshal(reply, Results{});
        case CloudService::TRANS_EXIT:
        case CloudService::TRANS_CHANGE_CONFIRMATION:
            return ITypesUtil::Marshal(reply, std::pair<int32_t, std::string>{});
        case CloudService::TRANS_QUERY:
        case CloudService::TRANS_QUERY_BY_INVITATION:
            return ITypesUtil::Marshal(reply, QueryResults{});
        case CloudService::TRANS_CONFIRM_INVITATION:
            return ITypesUtil::Marshal(reply, std::tuple<int32_t, std::string, std::string>{});
        default:
            return true;
    }
}
} // namespace

void ExpectCloudReply(MockCloudRemote &remote, uint32_t code, int32_t status,
    const std::function<void(MessageParcel &)> &inspect)
{
    EXPECT_CALL(remote, SendRequest(code, testing::_, testing::_, testing::_))
        .WillOnce(testing::Invoke([code, status, inspect](
            uint32_t, MessageParcel &request, MessageParcel &reply, MessageOption &) {
            EXPECT_EQ(request.ReadInterfaceToken(), CloudServiceProxy::GetDescriptor())
                << "proxy sent a wrong descriptor, code=" << code;
            if (inspect) {
                inspect(request);
            }
            if (status == CloudService::IPC_ERROR) {
                return -1;
            }
            auto written = WriteReply(reply, code, status);
            EXPECT_TRUE(written) << "failed to build remote reply, code=" << code;
            return written ? 0 : -1;
        }));
}
} // namespace OHOS::CloudData

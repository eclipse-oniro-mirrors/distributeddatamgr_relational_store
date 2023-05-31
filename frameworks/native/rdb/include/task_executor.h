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
#ifndef DISTRIBUTED_NATIVE_RDB_TASK_EXECUTOR_H
#define DISTRIBUTED_NATIVE_RDB_TASK_EXECUTOR_H
#include "executor_pool.h"
namespace OHOS::NativeRdb {
class TaskExecutor {
public:
    using TaskId = ExecutorPool::TaskId;
    using Task = std::function<void()>;
    using Duration = std::chrono::steady_clock::duration;
    static constexpr TaskId INVALID_TASK_ID = ExecutorPool::INVALID_TASK_ID;

    static TaskExecutor &GetInstance();
    std::shared_ptr<ExecutorPool> GetExecutor();
    void SetExecutor(std::shared_ptr<ExecutorPool> executor);

private:
    size_t MAX_THREADS = 2;
    size_t MIN_THREADS = 0;
    TaskExecutor();
    ~TaskExecutor();
    mutable std::shared_mutex rwMutex_;
    std::shared_ptr<ExecutorPool> pool_;
};
} // namespace OHOS::NativeRdb
#endif // DISTRIBUTED_DATA_TASK_EXECUTOR_H

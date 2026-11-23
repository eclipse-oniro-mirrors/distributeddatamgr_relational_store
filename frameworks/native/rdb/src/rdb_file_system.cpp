/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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

#define LOG_TAG "RdbFileSystem"
#include "rdb_file_system.h"

#include <filesystem>
#include <string>
#include <vector>

#include "logger.h"
#include "sqlite_utils.h"

namespace OHOS {
namespace NativeRdb {
using namespace OHOS::Rdb;
std::vector<std::string> RdbFileSystem::GetEntries(const std::string &path)
{
    std::vector<std::string> paths;
    std::error_code ec;
    auto end = std::filesystem::recursive_directory_iterator();
    auto it = std::filesystem::recursive_directory_iterator(path, ec);
    if (ec) {
        LOG_WARN("GetEntries open directory failed, dir: %{public}s, ec: %{public}d",
                 SqliteUtils::Anonymous(path).c_str(), ec.value());
        return paths;
    }
    for (; it != end;) {
        std::string currentPath = it->path().string();
        paths.push_back(currentPath);
        it.increment(ec);
        if (ec) {
            LOG_WARN("GetEntries traverse entry failed, dir: %{public}s, ec: %{public}d",
                     SqliteUtils::Anonymous(currentPath).c_str(), ec.value());
            ec.clear();
        }
    }
    return paths;
}

std::pair<size_t, int32_t> RdbFileSystem::RemoveAll(const std::string &path, bool removeSelf)
{
    std::error_code ec;
    size_t count = 0;

    if (removeSelf) {
        count = std::filesystem::remove_all(path, ec);
        if (ec) {
            LOG_WARN("RemoveAll failed, dir: %{public}s, ec: %{public}d",
                     SqliteUtils::Anonymous(path).c_str(), ec.value());
        }
        return std::make_pair(count, ec.value());
    }
    for (const auto &subPath : RdbFileSystem::GetEntries(path)) {
        auto res = std::filesystem::remove_all(subPath, ec);
        if (ec) {
            LOG_WARN("RemoveAll remove entry failed, dir: %{public}s, ec: %{public}d",
                     SqliteUtils::Anonymous(subPath).c_str(), ec.value());
            ec.clear();
            continue;
        }
        count += res;
    }
    return std::make_pair(count, ec.value());
}

std::string RdbFileSystem::RealPath(const std::string &path)
{
    char *canonicalPath = realpath(path.c_str(), nullptr);
    if (canonicalPath == nullptr) {
        return "";
    }
    std::string realPath = canonicalPath;
    free(canonicalPath);
    canonicalPath = nullptr;
    return realPath;
}
} // namespace NativeRdb
} // namespace OHOS
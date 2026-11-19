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

namespace OHOS {
namespace NativeRdb {
using namespace OHOS::Rdb;
std::vector<std::string> RdbFileSystem::GetEntries(const std::string &path)
{
    std::vector<std::string> paths;
    std::error_code ec;
    constexpr auto opts = std::filesystem::directory_options::skip_permission_denied;
    auto end = std::filesystem::recursive_directory_iterator();
    for (auto it = std::filesystem::recursive_directory_iterator(path, opts, ec); it != end;) {
        if (ec) {
            LOG_WARN("GetEntries skip entry, path: %{private}s, ec: %{public}d", path.c_str(), ec.value());
            ec.clear();
        }
        paths.push_back(it->path().string());
        it.increment(ec);
    }
    return paths;
}

std::pair<size_t, int32_t> RdbFileSystem::RemoveAll(const std::string &path, bool removeSelf)
{
    std::error_code ec;
    size_t count = 0;

    if (removeSelf) {
        count = std::filesystem::remove_all(path, ec);
        return std::make_pair(count, ec.value());
    }
    for (const auto &subPath : RdbFileSystem::GetEntries(path)) {
        auto res = std::filesystem::remove_all(subPath, ec);
        if (ec) {
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
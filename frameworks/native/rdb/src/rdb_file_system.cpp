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
#include <unistd.h>
#include <utility>
#include <vector>

#include "logger.h"
#include "sqlite_utils.h"

namespace OHOS {
namespace NativeRdb {
using namespace OHOS::Rdb;
std::vector<std::string> RdbFileSystem::GetEntries(const std::string &path)
{
    namespace fs = std::filesystem;
    std::vector<std::string> paths;
    std::vector<fs::directory_iterator> directories;
    const fs::directory_iterator end;

    auto openDirectory = [&directories](const fs::path &directory) {
        std::error_code ec;
        fs::directory_iterator entries(directory, ec);
        if (ec) {
            const bool readable = access(directory.c_str(), R_OK) == 0;
            const bool searchable = access(directory.c_str(), X_OK) == 0;
            LOG_WARN("GetEntries open directory failed, path=%{public}s, ec=%{public}d, "
                "realUidCanRead=%{public}d, realUidCanSearch=%{public}d",
                SqliteUtils::Anonymous(directory.string()).c_str(), ec.value(), readable, searchable);
            return;
        }
        directories.push_back(std::move(entries));
    };

    openDirectory(path);
    while (!directories.empty()) {
        auto &current = directories.back();
        if (current == end) {
            directories.pop_back();
            continue;
        }
        const fs::directory_entry entry = *current;
        paths.push_back(entry.path().string());

        std::error_code ec;
        current.increment(ec);
        if (ec) {
            LOG_WARN("GetEntries read directory failed, path=%{public}s, ec=%{public}d",
                SqliteUtils::Anonymous(entry.path().parent_path().string()).c_str(), ec.value());
            directories.pop_back();
        }
        const auto status = entry.symlink_status(ec);
        if (ec) {
            LOG_WARN("GetEntries stat entry failed, path=%{public}s, ec=%{public}d",
                SqliteUtils::Anonymous(entry.path().string()).c_str(), ec.value());
        } else if (fs::is_directory(status)) {
            openDirectory(entry.path());
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
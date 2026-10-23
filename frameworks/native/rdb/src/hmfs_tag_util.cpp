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

#define LOG_TAG "HmfsTagUtil"
#include "hmfs_tag_util.h"

#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <cerrno>
#include <cstdint>
#include <string>

#include "file_ex.h"
#include "logger.h"
#include "securec.h"
#include "sqlite_utils.h"

namespace OHOS {
namespace NativeRdb {
using namespace OHOS::Rdb;
namespace {
constexpr unsigned long HMFS_IOCTL_MAGIC = 0xF5;

// Mirrors the HMFS kernel uapi contract: fd is the fd of the file's parent directory;
// filename is the file name within that directory (at most 255 bytes); flags is reserved
// and must be 0 when SETFLAGS/CLEARFLAGS is invoked, and is the output of GETFLAGS
// (1 means the tag is set).
struct HmfsKeyFileArgs {
    char filename[256];
    uint32_t flags;
};

constexpr unsigned long HMFS_IOC_KEY_FILE_SETFLAGS = _IOW(HMFS_IOCTL_MAGIC, 72, HmfsKeyFileArgs);
constexpr unsigned long HMFS_IOC_KEY_FILE_GETFLAGS = _IOWR(HMFS_IOCTL_MAGIC, 73, HmfsKeyFileArgs);
constexpr unsigned long HMFS_IOC_KEY_FILE_CLEARFLAGS = _IOW(HMFS_IOCTL_MAGIC, 74, HmfsKeyFileArgs);
} // namespace

int HmfsTagUtil::ExecuteIoctl(int dirFd, unsigned long cmd, const std::string &fileName, uint32_t *outFlags)
{
    if (!IsValidFileName(fileName)) {
        LOG_WARN("invalid file name for hmfs key file ioctl, name %{public}s",
            SqliteUtils::Anonymous(fileName).c_str());
        return -EINVAL;
    }
    HmfsKeyFileArgs args = { { 0 }, 0 };
    if (strcpy_s(args.filename, sizeof(args.filename), fileName.c_str()) != EOK) {
        LOG_WARN("copy file name failed, name %{public}s", SqliteUtils::Anonymous(fileName).c_str());
        return -EINVAL;
    }
    args.flags = 0;
    int ret = ioctl(dirFd, cmd, &args);
    if (ret != 0) {
        // GETFLAGS failures are expected on non-HMFS file systems (silent skip); only
        // SET/CLEAR failures indicate a tagging problem worth logging.
        if (cmd != HMFS_IOC_KEY_FILE_GETFLAGS) {
            LOG_WARN("hmfs key file ioctl failed, errno %{public}d, name %{public}s", errno,
                SqliteUtils::Anonymous(fileName).c_str());
        }
        return (errno != 0) ? -errno : -EIO;
    }
    if (outFlags != nullptr) {
        *outFlags = args.flags;
    }
    return 0;
}

int HmfsTagUtil::ExecuteByPath(unsigned long cmd, const std::string &path, uint32_t *outFlags)
{
    if (path.empty()) {
        return -EINVAL;
    }
    auto pos = path.find_last_of('/');
    if (pos == std::string::npos || pos == 0 || pos + 1 == path.length()) {
        LOG_WARN("invalid path for hmfs key file ioctl, path %{public}s", SqliteUtils::Anonymous(path).c_str());
        return -EINVAL;
    }
    std::string dir = path.substr(0, pos);
    std::string fileName = path.substr(pos + 1);
    int dirFd = open(dir.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (dirFd < 0) {
        LOG_WARN("open directory failed, errno %{public}d, path %{public}s", errno,
            SqliteUtils::Anonymous(path).c_str());
        return (errno != 0) ? -errno : -EIO;
    }
    uint64_t tag = fdsan_create_owner_tag(FDSAN_OWNER_TYPE_FILE, 0xD001650);
    fdsan_exchange_owner_tag(dirFd, 0, tag);
    int ret = ExecuteIoctl(dirFd, cmd, fileName, outFlags);
    fdsan_close_with_tag(dirFd, tag);
    return ret;
}

bool HmfsTagUtil::IsValidFileName(const std::string &fileName)
{
    if (fileName.empty() || fileName.length() > static_cast<size_t>(MAX_FILE_NAME_LEN)) {
        return false;
    }
    if (fileName == "." || fileName == "..") {
        return false;
    }
    return fileName.find('/') == std::string::npos;
}

int HmfsTagUtil::SetKeyFileFlag(int dirFd, const std::string &fileName)
{
    return ExecuteIoctl(dirFd, HMFS_IOC_KEY_FILE_SETFLAGS, fileName, nullptr);
}

int HmfsTagUtil::GetKeyFileFlag(int dirFd, const std::string &fileName)
{
    uint32_t flags = 0;
    int ret = ExecuteIoctl(dirFd, HMFS_IOC_KEY_FILE_GETFLAGS, fileName, &flags);
    if (ret != 0) {
        return ret;
    }
    return (flags == static_cast<uint32_t>(FLAG_SET)) ? FLAG_SET : FLAG_UNSET;
}

int HmfsTagUtil::ClearKeyFileFlag(int dirFd, const std::string &fileName)
{
    return ExecuteIoctl(dirFd, HMFS_IOC_KEY_FILE_CLEARFLAGS, fileName, nullptr);
}

int HmfsTagUtil::SetKeyFileFlag(const std::string &path)
{
    return ExecuteByPath(HMFS_IOC_KEY_FILE_SETFLAGS, path, nullptr);
}

int HmfsTagUtil::GetKeyFileFlag(const std::string &path)
{
    uint32_t flags = 0;
    int ret = ExecuteByPath(HMFS_IOC_KEY_FILE_GETFLAGS, path, &flags);
    if (ret != 0) {
        return ret;
    }
    return (flags == static_cast<uint32_t>(FLAG_SET)) ? FLAG_SET : FLAG_UNSET;
}

int HmfsTagUtil::ClearKeyFileFlag(const std::string &path)
{
    return ExecuteByPath(HMFS_IOC_KEY_FILE_CLEARFLAGS, path, nullptr);
}

void HmfsTagUtil::ClearDbFileTags(const std::string &dbPath)
{
    // BR-4/AC-2.5: clear the dump-stack tags of db/wal/shm before a journal-mode switch
    // (Attach/Detach); SQLite removes wal/shm through its own VFS, outside DeleteFile.
    // EX-6: only tagged files are cleared; GETFLAGS failures (non-HMFS) fall through.
    static const char *suffixes[] = { "", "-wal", "-shm" };
    for (auto suffix : suffixes) {
        std::string file = dbPath + suffix;
        if (GetKeyFileFlag(file) != FLAG_SET) {
            continue;
        }
        if (ClearKeyFileFlag(file) != 0) {
            LOG_WARN("clear dump stack tag for mode switch fail, file %{public}s",
                SqliteUtils::Anonymous(file).c_str());
        }
    }
}
} // namespace NativeRdb
} // namespace OHOS

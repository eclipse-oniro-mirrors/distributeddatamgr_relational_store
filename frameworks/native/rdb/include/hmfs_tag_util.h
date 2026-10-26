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

#ifndef NATIVE_RDB_HMFS_TAG_UTIL_H
#define NATIVE_RDB_HMFS_TAG_UTIL_H

#include <cstdint>
#include <string>

namespace OHOS {
namespace NativeRdb {

/**
 * @brief Utility of the HMFS key-file dump-stack tag (deletion dump-stack for db/wal/shm).
 *
 * The tag state is owned by the HMFS file system (inode level); this utility only forwards
 * get/clear requests through HMFS ioctl commands. All methods degrade silently: on
 * failure a negative value is returned and callers must not block database business flows.
 */
class HmfsTagUtil {
public:
    // Return contract of the query API: FLAG_SET (1) means the dump-stack tag is set;
    // 0 means not set; a negative value means failure (silent degrade).
    static constexpr int FLAG_SET = 1; /* NOLINT */

    /**
     * @brief Queries the dump-stack tag of the file.
     * @return FLAG_SET if the tag is set, 0 if not; a negative value on failure.
     */
    static int GetKeyFileFlag(const std::string &path);

    /**
     * @brief Clears the dump-stack tag of the file.
     * @return 0 on success; a negative value on failure.
     */
    static int ClearKeyFileFlag(const std::string &path);

    /**
     * @brief Clears the dump-stack tags of db/wal/shm of the given database path before
     * a journal-mode switch (Attach/Detach); SQLite removes wal/shm through its own VFS,
     * outside DeleteFile. Only tagged files are cleared; failures fall through silently.
     */
    static void ClearDbFileTags(const std::string &dbPath);

private:
    static constexpr int FLAG_UNSET = 0;                       /* NOLINT */
    static constexpr int MAX_FILE_NAME_LEN = 255; /* NOLINT */ // bytes, excluding the terminator

    /**
     * @brief Checks whether the file name is valid for the HMFS key-file ioctl.
     * Only a plain file name (no path separator, not "." or "..", length within
     * MAX_FILE_NAME_LEN) is allowed, which prevents directory traversal.
     */
    static bool IsValidFileName(const std::string &fileName);

    /**
     * @brief Queries the dump-stack tag of the file.
     * @return FLAG_SET if the tag is set, FLAG_UNSET if not; a negative value on failure.
     */
    static int GetKeyFileFlag(int dirFd, const std::string &fileName);

    /**
     * @brief Clears the dump-stack tag of the file.
     * @return 0 on success; a negative value on failure.
     */
    static int ClearKeyFileFlag(int dirFd, const std::string &fileName);

    static int ExecuteIoctl(int dirFd, unsigned long cmd, const std::string &fileName, uint32_t *outFlags);

    static int ExecuteByPath(unsigned long cmd, const std::string &path, uint32_t *outFlags);
};
} // namespace NativeRdb
} // namespace OHOS
#endif // NATIVE_RDB_HMFS_TAG_UTIL_H

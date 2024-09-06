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

#ifndef NATIVE_RDB_RDB_SECURITY_MANAGER_H
#define NATIVE_RDB_RDB_SECURITY_MANAGER_H

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <atomic>
#include <climits>
#include <mutex>
#include <random>
#include <vector>

#include "hks_type.h"

namespace OHOS::NativeRdb {
struct RdbSecretKeyData {
    uint8_t distributed = 0;
    time_t timeValue {};
    std::vector<uint8_t> secretKey {};
    RdbSecretKeyData() = default;
    ~RdbSecretKeyData()
    {
        secretKey.assign(secretKey.size(), 0);
    }
};

class RdbPassword final {
public:
    RdbPassword();
    ~RdbPassword();

    bool isKeyExpired = false;
    bool operator==(const RdbPassword &input) const;
    bool operator!=(const RdbPassword &input) const;

    size_t GetSize() const;
    const uint8_t *GetData() const;
    int SetValue(const uint8_t *inputData, size_t inputSize);
    int Clear();
    bool IsValid() const;

private:
    static constexpr size_t MAX_PASSWORD_SIZE = 128;
    uint8_t data_[MAX_PASSWORD_SIZE] = { UCHAR_MAX };
    size_t size_ = 0;
};

class RdbSecurityManager {
public:
    enum KeyFileType : int32_t {
        PUB_KEY_FILE = 0,
        PUB_KEY_FILE_NEW_KEY,
        PUB_KEY_FILE_BUTT
    };
    class KeyFiles {
    public:
        KeyFiles(const std::string &dbPath, bool openFile = true);
        ~KeyFiles();
        const std::string &GetKeyFile(KeyFileType type);
        int32_t DestroyLock();
        int32_t Lock();
        int32_t Unlock();

    private:
        int32_t lockFd_ = -1;
        std::string lock_;
        std::string keys_[PUB_KEY_FILE_BUTT];
    };
    static RdbSecurityManager &GetInstance();
    int32_t Init(const std::string &bundleName);

    RdbPassword GetRdbPassword(const std::string &dbPath, KeyFileType keyFileType);
    void DelAllKeyFiles(const std::string &dbPath);
    void DelKeyFile(const std::string &dbPath, KeyFileType keyFileType);
    void ChangeKeyFile(const std::string &dbPath);
    int32_t RestoreKeyFile(const std::string &dbPath, const std::vector<uint8_t> &key);
    bool IsKeyFileExists(const std::string &dbPath, KeyFileType keyFileType);

private:
    RdbSecurityManager();
    ~RdbSecurityManager();

    bool HasRootKey();
    int GenerateRootKey(const std::vector<uint8_t> &rootKeyAlias);
    int32_t CheckRootKeyExists(std::vector<uint8_t> &rootKeyAlias);
    std::vector<uint8_t> EncryptWorkKey(std::vector<uint8_t> &key);
    bool DecryptWorkKey(std::vector<uint8_t> &source, std::vector<uint8_t> &key);
    std::vector<uint8_t> GenerateRootKeyAlias(const std::string &bundleName);
    static bool InitPath(const std::string &fileDir);
    std::vector<uint8_t> GenerateRandomNum(int32_t len);
    bool SaveSecretKeyToFile(const std::string &keyFile, const std::vector<uint8_t> &workey = {});
    bool SaveSecretKeyToDisk(const std::string &keyPath, RdbSecretKeyData &keyData);
    RdbPassword LoadSecretKeyFromFile(const std::string &keyFile);
    bool LoadSecretKeyFromDisk(const std::string &keyPath, RdbSecretKeyData &keyData);
    bool IsKeyFileEmpty(const std::string &keyFile);
    static bool IsKeyExpired(const time_t &createTime) ;
    int32_t HksLoopUpdate(const struct HksBlob *handle, const struct HksParamSet *paramSet,
        const struct HksBlob *inData, struct HksBlob *outData);
    int32_t HksEncryptThreeStage(const struct HksBlob *keyAlias, const struct HksParamSet *paramSet,
        const struct HksBlob *plainText, struct HksBlob *cipherText);
    int32_t HksDecryptThreeStage(const struct HksBlob *keyAlias, const struct HksParamSet *paramSet,
        const struct HksBlob *cipherText, struct HksBlob *plainText);

    static constexpr char const *SUFFIX_KEY_LOCK = ".key_lock";
    static constexpr char const *SUFFIX_PUB_KEY = ".pub_key";
    static constexpr char const *SUFFIX_PUB_KEY_NEW = ".pub_key.new";
    static constexpr const char *RDB_ROOT_KEY_ALIAS_PREFIX = "DistributedDataRdb";
    static constexpr const char *RDB_HKS_BLOB_TYPE_NONCE = "Z5s0Bo571Koq";
    static constexpr const char *RDB_HKS_BLOB_TYPE_AAD = "RdbClientAAD";
    static constexpr uint32_t TIMES = 4;
    static constexpr uint32_t MAX_UPDATE_SIZE = 64;
    static constexpr uint32_t MAX_OUTDATA_SIZE = MAX_UPDATE_SIZE * TIMES;
    static constexpr uint8_t AEAD_LEN = 16;
    static constexpr int RDB_KEY_SIZE = 32;

    static constexpr int HOURS_PER_YEAR = (24 * 365);
    static constexpr uint8_t UNDISTRIBUTED = 0;
    static constexpr uint8_t DISTRIBUTED = 1;

    std::vector<uint8_t> rootKeyAlias_ {};
    std::vector<uint8_t> nonce_;
    std::vector<uint8_t> aad_;
    std::mutex mutex_;
    std::atomic<bool> hasRootKey_ = false;
};

} // namespace OHOS::NativeRdb
#endif

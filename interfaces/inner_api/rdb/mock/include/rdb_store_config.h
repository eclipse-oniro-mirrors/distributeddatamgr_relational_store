/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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

#ifndef NATIVE_RDB_RDB_STORE_CONFIG_H
#define NATIVE_RDB_RDB_STORE_CONFIG_H

#include <string>
#include <vector>
#include <map>
#include <functional>

namespace OHOS::NativeRdb {
enum class IntegrityCheck {
    NONE,
    QUICK,
    FULL,
};

// indicates the type of the storage
enum class StorageMode {
    MODE_MEMORY = 101,
    MODE_DISK,
};

enum class JournalMode {
    MODE_DELETE,
    MODE_TRUNCATE,
    MODE_PERSIST,
    MODE_MEMORY,
    MODE_WAL,
    MODE_OFF,
};

enum class SyncMode {
    MODE_OFF,
    MODE_NORMAL,
    MODE_FULL,
    MODE_EXTRA,
};

enum class DatabaseFileType {
    NORMAL,
    BACKUP,
    CORRUPT,
};

enum class SecurityLevel : int32_t {
    S1 = 1,
    S2,
    S3,
    S4,
    LAST
};

enum HAMode : int32_t {
    SINGLE = 0,
    MAIN_REPLICA,
    MANUAL_TRIGGER,
};

enum RoleType : uint32_t {
    OWNER = 0,
    VISITOR,
};

enum DBType : uint32_t {
    DB_SQLITE = 0,
    DB_VECTOR,
    DB_BUTT
};

enum DistributedType {
    RDB_DEVICE_COLLABORATION = 10,
    RDB_DISTRIBUTED_TYPE_MAX
};

enum HmacAlgo : int32_t {
    SHA1 = 0,
    SHA256,
    SHA512
};

enum KdfAlgo : int32_t {
    KDF_SHA1 = 0,
    KDF_SHA256,
    KDF_SHA512
};

enum EncryptAlgo : int32_t {
    AES_256_GCM = 0,
    AES_256_CBC
};


using ScalarFunction = std::function<std::string(const std::vector<std::string>&)>;

struct ScalarFunctionInfo {
    ScalarFunctionInfo(ScalarFunction function, int argc) : function_(function), argc_(argc) {}
    ScalarFunction function_;
    int argc_;
};

class RdbStoreConfig {
public:
    struct CryptoParam {
        mutable int32_t iterNum = 0;
        int32_t encryptAlgo = EncryptAlgo::AES_256_GCM;
        int32_t hmacAlgo = HmacAlgo::SHA256;
        int32_t kdfAlgo = KdfAlgo::KDF_SHA256;
        uint32_t cryptoPageSize = RdbStoreConfig::DB_DEFAULT_CRYPTO_PAGE_SIZE;
        mutable std::vector<uint8_t> encryptKey_{};
        CryptoParam();
        ~CryptoParam();
        bool IsValid() const;
    };
    static constexpr int DB_PAGE_SIZE = 4096;    /* default page size : 4k */
    static constexpr int DB_JOURNAL_SIZE = 1024 * 1024; /* default file size : 1M */
    static constexpr char DB_DEFAULT_JOURNAL_MODE[] = "WAL";
    static constexpr EncryptAlgo DB_DEFAULT_ENCRYPT_ALGO = AES_256_GCM;
    static constexpr uint32_t DB_DEFAULT_CRYPTO_PAGE_SIZE = 1024;
    static constexpr uint32_t DB_INVALID_CRYPTO_PAGE_SIZE_MASK = 0xFFFE03FF;
    RdbStoreConfig(const std::string &path, StorageMode storageMode = StorageMode::MODE_DISK, bool readOnly = false,
        const std::vector<uint8_t> &encryptKey = std::vector<uint8_t>(),
        const std::string &journalMode = DB_DEFAULT_JOURNAL_MODE, const std::string &syncMode = "",
        const std::string &databaseFileType = "", SecurityLevel securityLevel = SecurityLevel::LAST,
        bool isCreateNecessary = true, bool autoCheck = false, int journalSize = 1048576, int pageSize = 4096);
    ~RdbStoreConfig();
    std::string GetName() const;
    std::string GetPath() const;
    StorageMode GetStorageMode() const;
    std::string GetJournalMode() const;
    std::string GetSyncMode() const;

    bool IsReadOnly() const;
    bool IsMemoryRdb() const;
    std::string GetDatabaseFileType() const;
    SecurityLevel GetSecurityLevel() const;
    void SetEncryptStatus(const bool status);
    bool IsEncrypt() const;
    bool IsCreateNecessary() const;

    // set the journal mode, if not set, the default mode is WAL
    void SetName(std::string name);
    void SetJournalMode(JournalMode journalMode);
    void SetPath(std::string path);
    void SetReadOnly(bool readOnly);
    void SetStorageMode(StorageMode storageMode);
    void SetDatabaseFileType(DatabaseFileType type);
    void SetSecurityLevel(SecurityLevel secLevel);
    void SetCreateNecessary(bool isCreateNecessary);
    // distributed rdb
    int SetBundleName(const std::string &bundleName);
    std::string GetBundleName() const;
    int SetDistributedType(DistributedType type);
    DistributedType GetDistributedType() const;
    void SetModuleName(const std::string& moduleName);
    std::string GetModuleName() const;
    void SetServiceName(const std::string& serviceName);
    void SetArea(int32_t area);
    int32_t GetArea() const;
    static std::string GetJournalModeValue(JournalMode journalMode);
    static std::string GetSyncModeValue(SyncMode syncMode);
    static std::string GetDatabaseFileTypeValue(DatabaseFileType databaseFileType);
    bool IsAutoCheck() const;
    void SetAutoCheck(bool autoCheck);
    int GetJournalSize() const;
    void SetJournalSize(int journalSize);
    int GetPageSize() const;
    void SetPageSize(int pageSize);
    EncryptAlgo GetEncryptAlgo() const;
    void SetEncryptAlgo(EncryptAlgo encryptAlgo);
    int GetReadConSize() const;
    void SetReadConSize(int readConSize);
    void SetEncryptKey(const std::vector<uint8_t> &encryptKey);
    void RestoreEncryptKey(const std::vector<uint8_t> &encryptKey) const;
    std::vector<uint8_t> GetEncryptKey() const;
    void ChangeEncryptKey() const;
    std::vector<uint8_t> GetNewEncryptKey() const;
    int32_t Initialize() const;
    void SetScalarFunction(const std::string &functionName, int argc, ScalarFunction function);

    std::map<std::string, ScalarFunctionInfo> GetScalarFunctions() const;
    void SetDataGroupId(const std::string &dataGroupId);
    std::string GetDataGroupId() const;
    void SetAutoClean(bool isAutoClean);
    bool GetAutoClean() const;
    void SetIsVector(bool isVector);
    bool IsVector() const;

    void SetCustomDir(const std::string &customDir);
    std::string GetCustomDir() const;
    void SetVisitorDir(const std::string &visitorDir);
    std::string GetVisitorDir() const;
    bool operator==(const RdbStoreConfig &config) const
    {
        if (this->customScalarFunctions.size() != config.customScalarFunctions.size()) {
            return false;
        }

        auto iter1 = this->customScalarFunctions.begin();
        auto iter2 = config.customScalarFunctions.begin();
        for (; iter1 != this->customScalarFunctions.end(); ++iter1, ++iter2) {
            if (iter1->first != iter2->first) {
                return false;
            }
        }

        if (this->cryptoParam_.encryptKey_.size() != config.cryptoParam_.encryptKey_.size()) {
            return false;
        }

        for (size_t i = 0; i < cryptoParam_.encryptKey_.size(); i++) {
            if (this->cryptoParam_.encryptKey_[i] != config.cryptoParam_.encryptKey_[i]) {
                return false;
            }
        }

        return this->path_ == config.path_ && this->storageMode_ == config.storageMode_ &&
               this->journalMode_ == config.journalMode_ && this->syncMode_ == config.syncMode_ &&
               this->databaseFileType == config.databaseFileType && this->isEncrypt_ == config.isEncrypt_ &&
               this->securityLevel_ == config.securityLevel_ && this->journalSize_ == config.journalSize_ &&
               this->pageSize_ == config.pageSize_ && this->readConSize_ == config.readConSize_ &&
               this->customDir_ == config.customDir_ && this->allowRebuilt_ == config.allowRebuilt_ &&
               this->pluginLibs_ == config.pluginLibs_ && this->haMode_ == config.haMode_;
    }

    bool IsSearchable() const;
    void SetSearchable(bool searchable);
    int GetWriteTime() const;
    void SetWriteTime(int timeout);
    int GetReadTime() const;
    void SetReadTime(int timeout);
    void SetRoleType(RoleType role);
    uint32_t GetRoleType() const;
    void SetAllowRebuild(bool allowRebuild);
    bool GetAllowRebuild() const;
    void SetDBType(int32_t dbType);
    int32_t GetDBType() const;
    void SetIntegrityCheck(IntegrityCheck checkMode);
    IntegrityCheck GetIntegrityCheck() const;
    void SetPluginLibs(const std::vector<std::string> &pluginLibs);
    std::vector<std::string> GetPluginLibs() const;
    void SetIter(int32_t iter) const;
    int32_t GetIter() const;
    int32_t GetHaMode() const;
    void SetHaMode(int32_t haMode);
    void SetNewEncryptKey(const std::vector<uint8_t> newEncryptKey);
    void SetScalarFunctions(const std::map<std::string, ScalarFunctionInfo> functions);
    void SetCryptoParam(CryptoParam cryptoParam);
    CryptoParam GetCryptoParam() const;
    void SetJournalMode(const std::string &journalMode);
    void EnableRekey(bool enable);
    static std::string Format(const RdbStoreConfig &cacheConfig, const RdbStoreConfig &incomingConfig);

private:
    void ClearEncryptKey();
    int32_t GenerateEncryptedKey() const;

    bool readOnly_ = false;
    bool isEncrypt_ = false;
    bool isCreateNecessary_;
    bool isSearchable_ = false;
    bool autoCheck_;
    bool isAutoClean_ = true;
    bool isVector_ = false;
    bool autoRekey_ = false;
    int32_t journalSize_;
    int32_t pageSize_;
    int32_t readConSize_ = 4;
    int32_t area_ = 0;
    int32_t writeTimeout_ = 2; // seconds
    int32_t readTimeout_ = 1; // seconds
    int32_t dbType_ = DB_SQLITE;
    int32_t haMode_ = HAMode::SINGLE;
    SecurityLevel securityLevel_ = SecurityLevel::LAST;
    RoleType role_ = OWNER;
    DistributedType distributedType_ = DistributedType::RDB_DEVICE_COLLABORATION;
    StorageMode storageMode_;
    IntegrityCheck checkType_ = IntegrityCheck::NONE;
    CryptoParam cryptoParam_;
    std::string name_;
    std::string path_;
    std::string journalMode_;
    std::string syncMode_;
    std::string databaseFileType;
    // distributed rdb
    std::string bundleName_;
    std::string moduleName_;
    std::string visitorDir_;
    std::string dataGroupId_;
    std::string customDir_;
    mutable std::vector<uint8_t> newEncryptKey_{};
    std::map<std::string, ScalarFunctionInfo> customScalarFunctions;
    std::vector<std::string> pluginLibs_{};

    static constexpr int MAX_TIMEOUT = 300; // seconds
    static constexpr int MIN_TIMEOUT = 1;   // seconds
    bool allowRebuilt_ = false;
};
} // namespace OHOS::NativeRdb
#endif

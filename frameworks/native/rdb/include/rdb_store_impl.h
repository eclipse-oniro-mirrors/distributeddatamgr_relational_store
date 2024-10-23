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

#ifndef NATIVE_RDB_RDB_STORE_IMPL_H
#define NATIVE_RDB_RDB_STORE_IMPL_H

#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <thread>

#include "concurrent_map.h"
#include "connection_pool.h"
#include "data_ability_observer_stub.h"
#include "dataobs_mgr_client.h"
#include "rdb_errno.h"
#include "rdb_service.h"
#include "rdb_store.h"
#include "rdb_store_config.h"
#include "rdb_types.h"
#include "refbase.h"
#include "sqlite_statement.h"
#include "value_object.h"

namespace OHOS {
class ExecutorPool;
}

namespace OHOS::NativeRdb {
class DelayNotify;
class RdbStoreLocalObserver {
public:
    explicit RdbStoreLocalObserver(DistributedRdb::RdbStoreObserver *observer) : observer_(observer){};
    virtual ~RdbStoreLocalObserver(){};
    void OnChange()
    {
        observer_->OnChange();
    }
    DistributedRdb::RdbStoreObserver *getObserver()
    {
        return observer_;
    }

private:
    DistributedRdb::RdbStoreObserver *observer_ = nullptr;
};

class RdbStoreLocalSharedObserver : public AAFwk::DataAbilityObserverStub {
public:
    explicit RdbStoreLocalSharedObserver(DistributedRdb::RdbStoreObserver *observer) : observer_(observer){};
    virtual ~RdbStoreLocalSharedObserver(){};
    void OnChange() override
    {
        observer_->OnChange();
    }
    DistributedRdb::RdbStoreObserver *getObserver()
    {
        return observer_;
    }

private:
    DistributedRdb::RdbStoreObserver *observer_ = nullptr;
};

class RdbStoreImpl : public RdbStore {
public:
    RdbStoreImpl(const RdbStoreConfig &config);
    RdbStoreImpl(const RdbStoreConfig &config, int &errCode);
    ~RdbStoreImpl() override;
    std::pair<int, int64_t> Insert(const std::string &table, const Row &row, Resolution resolution) override;
    std::pair<int, int64_t> BatchInsert(const std::string& table, const ValuesBuckets& values) override;
    std::pair<int, int> Update(const std::string &table, const Row &row, const std::string &where, const Values &args,
        Resolution resolution) override;
    int Delete(int &deletedRows, const std::string &table, const std::string &whereClause, const Values &args) override;
    std::shared_ptr<AbsSharedResultSet> QuerySql(const std::string &sql, const Values &args) override;
    std::shared_ptr<ResultSet> QueryByStep(const std::string &sql, const Values &args) override;
    std::shared_ptr<ResultSet> RemoteQuery(const std::string &device, const AbsRdbPredicates &predicates,
        const Fields &columns, int &errCode) override;
    std::pair<int32_t, std::shared_ptr<ResultSet>> QuerySharingResource(const AbsRdbPredicates &predicates,
        const Fields &columns) override;
    int ExecuteSql(const std::string &sql, const Values &args) override;
    std::pair<int32_t, ValueObject> Execute(const std::string &sql, const Values &args, int64_t trxId) override;
    int ExecuteAndGetLong(int64_t &outValue, const std::string &sql, const Values &args) override;
    int ExecuteAndGetString(std::string &outValue, const std::string &sql, const Values &args) override;
    int ExecuteForLastInsertedRowId(int64_t &outValue, const std::string &sql, const Values &args) override;
    int ExecuteForChangedRowCount(int64_t &outValue, const std::string &sql, const Values &args) override;
    int GetVersion(int &version) override;
    int SetVersion(int version) override;
    int BeginTransaction() override;
    std::pair<int, int64_t> BeginTrans() override;
    int RollBack() override;
    int RollBack(int64_t trxId) override;
    int Commit() override;
    int Commit(int64_t trxId) override;
    bool IsInTransaction() override;
    bool IsOpen() const override;
    std::string GetPath() override;
    bool IsReadOnly() const override;
    bool IsMemoryRdb() const override;
    bool IsHoldingConnection() override;
    bool IsSlaveDiffFromMaster() const override;
    int Backup(const std::string &databasePath, const std::vector<uint8_t> &encryptKey) override;
    int Restore(const std::string &backupPath, const std::vector<uint8_t> &newKey) override;
    int Count(int64_t &outValue, const AbsRdbPredicates &predicates) override;
    int SetDistributedTables(const std::vector<std::string> &tables, int32_t type,
        const DistributedRdb::DistributedConfig &distributedConfig) override;
    std::string ObtainDistributedTableName(const std::string &device, const std::string &table, int &errCode) override;
    int Sync(const SyncOption &option, const AbsRdbPredicates &predicate, const AsyncBrief &async) override;
    int Sync(const SyncOption &option, const std::vector<std::string> &tables, const AsyncDetail &async) override;
    int Sync(const SyncOption &option, const AbsRdbPredicates &predicate, const AsyncDetail &async) override;
    int Subscribe(const SubscribeOption &option, RdbStoreObserver *observer) override;
    int UnSubscribe(const SubscribeOption &option, RdbStoreObserver *observer) override;
    int SubscribeObserver(const SubscribeOption &option, const std::shared_ptr<RdbStoreObserver> &observer) override;
    int UnsubscribeObserver(const SubscribeOption &option, const std::shared_ptr<RdbStoreObserver> &observer) override;
    int RegisterAutoSyncCallback(std::shared_ptr<DetailProgressObserver> observer) override;
    int UnregisterAutoSyncCallback(std::shared_ptr<DetailProgressObserver> observer) override;
    int Notify(const std::string &event) override;
    int SetSearchable(bool isSearchable) override;
    ModifyTime GetModifyTime(const std::string &table, const std::string &columnName,
        std::vector<PRIKey> &keys) override;
    int GetRebuilt(RebuiltType &rebuilt) override;
    int CleanDirtyData(const std::string &table, uint64_t cursor) override;
    std::pair<int32_t, int32_t> Attach(const RdbStoreConfig &config, const std::string &attachName,
        int32_t waitTime) override;
    std::pair<int32_t, int32_t> Detach(const std::string &attachName, int32_t waitTime) override;
    int ModifyLockStatus(const AbsRdbPredicates &predicates, bool isLock) override;
    int32_t GetDbType() const override;
    std::pair<int32_t, uint32_t> LockCloudContainer() override;
    int32_t UnlockCloudContainer() override;
    int InterruptBackup() override;
    int32_t GetBackupStatus() const override;
    std::pair<int32_t, std::shared_ptr<Transaction>> CreateTransaction(int32_t type) override;

    // not virtual functions /
    const RdbStoreConfig &GetConfig();
    int ConfigLocale(const std::string &localeStr);
    std::string GetName();
    std::string GetFileType();
    int32_t ExchangeSlaverToMaster();

protected:
    std::string GetLogTableName(const std::string &tableName) override;

private:
    using Stmt = std::shared_ptr<Statement>;
    using RdbParam = DistributedRdb::RdbSyncerParam;

    static void AfterOpen(const RdbParam &param, int32_t retry = 0);
    int InnerOpen();
    void InitSyncerParam(const RdbStoreConfig &config, bool created);
    int ExecuteByTrxId(const std::string &sql, int64_t trxId, bool closeConnAfterExecute = false,
        const std::vector<ValueObject> &bindArgs = {});
    std::pair<int32_t, ValueObject> HandleDifferentSqlTypes(std::shared_ptr<Statement> statement,
        const std::string &sql, const ValueObject &object, int sqlType);
    int CheckAttach(const std::string &sql);
    std::pair<int32_t, Stmt> BeginExecuteSql(const std::string &sql);
    int GetDataBasePath(const std::string &databasePath, std::string &backupFilePath);
    void DoCloudSync(const std::string &table);
    int InnerSync(const DistributedRdb::RdbService::Option &option, const DistributedRdb::PredicatesMemo &predicates,
        const AsyncDetail &async);
    int InnerBackup(const std::string &databasePath,
        const std::vector<uint8_t> &destEncryptKey = std::vector<uint8_t>());
    ModifyTime GetModifyTimeByRowId(const std::string &logTable, std::vector<PRIKey> &keys);
    Uri GetUri(const std::string &event);
    int SubscribeLocal(const SubscribeOption &option, RdbStoreObserver *observer);
    int SubscribeLocalShared(const SubscribeOption &option, RdbStoreObserver *observer);
    int32_t SubscribeLocalDetail(const SubscribeOption &option, const std::shared_ptr<RdbStoreObserver> &observer);
    int SubscribeRemote(const SubscribeOption &option, RdbStoreObserver *observer);
    int UnSubscribeLocal(const SubscribeOption &option, RdbStoreObserver *observer);
    int UnSubscribeLocalAll(const SubscribeOption &option);
    int UnSubscribeLocalShared(const SubscribeOption &option, RdbStoreObserver *observer);
    int UnSubscribeLocalSharedAll(const SubscribeOption &option);
    int32_t UnsubscribeLocalDetail(const SubscribeOption &option, const std::shared_ptr<RdbStoreObserver> &observer);
    int UnSubscribeRemote(const SubscribeOption &option, RdbStoreObserver *observer);
    int RegisterDataChangeCallback();
    void InitDelayNotifier();
    std::pair<int32_t, std::shared_ptr<Connection>> CreateWritableConn();
    std::vector<ValueObject> CreateBackupBindArgs(const std::string &databasePath,
        const std::vector<uint8_t> &destEncryptKey);
    std::pair<int32_t, Stmt> GetStatement(const std::string &sql, std::shared_ptr<Connection> conn) const;
    std::pair<int32_t, Stmt> GetStatement(const std::string &sql, bool read = false) const;
    int AttachInner(const RdbStoreConfig &config, const std::string &attachName, const std::string &dbPath,
        const std::vector<uint8_t> &key, int32_t waitTime);
    int SetDefaultEncryptSql(const std::shared_ptr<Statement> &statement, std::string sql,
        const RdbStoreConfig &config);
    int SetDefaultEncryptAlgo(const ConnectionPool::SharedConn &conn, const RdbStoreConfig &config);
    int GetHashKeyForLockRow(const AbsRdbPredicates &predicates, std::vector<std::vector<uint8_t>> &hashKeys);
    int GetSlaveName(const std::string &dbName, std::string &backupFilePath);
    bool TryGetMasterSlaveBackupPath(const std::string &srcPath, std::string &destPath, bool isRestore = false);
    void NotifyDataChange();
    int GetDestPath(std::string &destPath);

    static constexpr char SCHEME_RDB[] = "rdb://";
    static constexpr uint32_t EXPANSION = 2;
    static inline constexpr uint32_t INTERVAL = 10;
    static inline constexpr uint32_t RETRY_INTERVAL = 5; // s
    static inline constexpr int32_t MAX_RETRY_TIMES = 5;
    static constexpr const char *ROW_ID = "ROWID";

    bool isOpen_ = false;
    bool isReadOnly_ = false;
    bool isMemoryRdb_;
    uint32_t rebuild_ = RebuiltType::NONE;
    SlaveStatus slaveStatus_ = SlaveStatus::UNDEFINED;
    int64_t vSchema_ = 0;
    std::atomic<int64_t> newTrxId_ = 1;
    const RdbStoreConfig config_;
    DistributedRdb::RdbSyncerParam syncerParam_;
    std::string path_;
    std::string name_;
    std::string fileType_;
    mutable std::shared_mutex rwMutex_;
    std::mutex mutex_;
    std::shared_ptr<ConnectionPool> connectionPool_ = nullptr;
    std::shared_ptr<DelayNotify> delayNotifier_ = nullptr;
    std::shared_ptr<std::set<std::string>> syncTables_ = nullptr;
    std::set<std::string> cloudTables_;
    std::map<std::string, std::list<std::shared_ptr<RdbStoreLocalObserver>>> localObservers_;
    std::map<std::string, std::list<sptr<RdbStoreLocalSharedObserver>>> localSharedObservers_;
    ConcurrentMap<std::string, std::string> attachedInfo_;
    ConcurrentMap<int64_t, std::shared_ptr<Connection>> trxConnMap_ = {};
    std::list<std::weak_ptr<Transaction>> transactions_;
};
} // namespace OHOS::NativeRdb
#endif

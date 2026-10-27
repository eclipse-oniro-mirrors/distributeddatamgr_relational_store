/**
 * Copyright (C) 2026 Huawei Device Co., Ltd.
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

import { describe, beforeAll, beforeEach, afterEach, afterAll, it, expect } from '@ohos/hypium';
import relationalStore from '@ohos.data.relationalStore';
import { BusinessError } from '@ohos.base';
import AbilityDelegatorRegistry from '@ohos.app.ability.abilityDelegatorRegistry';

const delegator = AbilityDelegatorRegistry.getAbilityDelegator();
const context = delegator.getAppContext().getApplicationContext();
const TAG = '[RdbCloudSyncFullSyncTest]';
const STORE_CONFIG: relationalStore.StoreConfig = {
  name: 'RdbCloudSyncFullSyncTest.db',
  securityLevel: relationalStore.SecurityLevel.S1
};

export default function RdbCloudSyncFullSyncTest() {
  describe('RdbCloudSyncFullSyncTest', () => {
    beforeAll(async () => {
      console.info(`${TAG} beforeAll`);
    });
    beforeEach(() => {
      console.info(`${TAG} beforeEach`);
    });
    afterEach(() => {
      console.info(`${TAG} afterEach`);
    });
    afterAll(() => {
      console.info(`${TAG} afterAll`);
    });

    /**
     * @tc.name RdbCloudSyncFullSync001
     * @tc.desc Verify rdbStore.cloudSync with fullSync=true is accepted
     * @tc.type: FUNC
     * @tc.require:
     */
    it('RdbCloudSyncFullSync001', 0, async (done: () => void) => {
      console.info(`${TAG} RdbCloudSyncFullSync001 start`);
      try {
        const store = await relationalStore.getRdbStore(context, STORE_CONFIG);
        const config: relationalStore.CloudSyncConfig = {
          mode: relationalStore.SyncMode.SYNC_MODE_TIME_FIRST,
          fullSync: true
        };
        await store.cloudSync(config, (progress: relationalStore.ProgressDetails) => {
          console.info(`${TAG} RdbCloudSyncFullSync001 progress: ${JSON.stringify(progress)}`);
        });
        expect(true).assertTrue();
        done();
      } catch (e) {
        expect().assertFail();
        done();
      }
    });

    /**
     * @tc.name RdbCloudSyncFullSync002
     * @tc.desc Verify rdbStore.cloudSync with fullSync=false is accepted
     * @tc.type: FUNC
     * @tc.require:
     */
    it('RdbCloudSyncFullSync002', 0, async (done: () => void) => {
      console.info(`${TAG} RdbCloudSyncFullSync002 start`);
      try {
        const store = await relationalStore.getRdbStore(context, STORE_CONFIG);
        const config: relationalStore.CloudSyncConfig = {
          mode: relationalStore.SyncMode.SYNC_MODE_TIME_FIRST,
          fullSync: false
        };
        await store.cloudSync(config, (progress: relationalStore.ProgressDetails) => {
          console.info(`${TAG} RdbCloudSyncFullSync002 progress: ${JSON.stringify(progress)}`);
        });
        expect(true).assertTrue();
        done();
      } catch (e) {
        expect().assertFail();
        done();
      }
    });

    /**
     * @tc.name RdbCloudSyncFullSync003
     * @tc.desc Verify rdbStore.cloudSync with fullSync omitted is accepted
     * @tc.type: FUNC
     * @tc.require:
     */
    it('RdbCloudSyncFullSync003', 0, async (done: () => void) => {
      console.info(`${TAG} RdbCloudSyncFullSync003 start`);
      try {
        const store = await relationalStore.getRdbStore(context, STORE_CONFIG);
        const config: relationalStore.CloudSyncConfig = {
          mode: relationalStore.SyncMode.SYNC_MODE_TIME_FIRST
        };
        await store.cloudSync(config, (progress: relationalStore.ProgressDetails) => {
          console.info(`${TAG} RdbCloudSyncFullSync003 progress: ${JSON.stringify(progress)}`);
        });
        expect(true).assertTrue();
        done();
      } catch (e) {
        expect().assertFail();
        done();
      }
    });

    /**
     * @tc.name RdbCloudSyncFullSync004
     * @tc.desc Verify rdbStore.cloudSync with fullSync=true and CLOUD_FIRST mode is accepted
     * @tc.type: FUNC
     * @tc.require:
     */
    it('RdbCloudSyncFullSync004', 0, async (done: () => void) => {
      console.info(`${TAG} RdbCloudSyncFullSync004 start`);
      try {
        const store = await relationalStore.getRdbStore(context, STORE_CONFIG);
        const config: relationalStore.CloudSyncConfig = {
          mode: relationalStore.SyncMode.SYNC_MODE_CLOUD_FIRST,
          fullSync: true
        };
        await store.cloudSync(config, (progress: relationalStore.ProgressDetails) => {
          console.info(`${TAG} RdbCloudSyncFullSync004 progress: ${JSON.stringify(progress)}`);
        });
        expect(true).assertTrue();
        done();
      } catch (e) {
        expect().assertFail();
        done();
      }
    });
  });
}

/**
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

// @ts-nocheck
import { describe, beforeAll, beforeEach, afterEach, afterAll, it, expect } from '@ohos/hypium';
import cloudData from '@ohos.data.cloudData';
import relationalStore from '@ohos.data.relationalStore';
import { BusinessError } from '@ohos.base';

export default function CloudSyncFullSyncEtsTest() {
  describe('CloudSyncFullSyncEtsTest', () => {
    beforeAll(() => {})
    beforeEach(() => {})
    afterEach(() => {})
    afterAll(() => {})

    /**
     * @tc.name CloudSyncFullSync001
     * @tc.desc Verify fullSync=true is accepted (API succeeds or throws non-param error)
     * @tc.type: FUNC
     * @tc.require:
     */
    it('CloudSyncFullSync001', 0, async (done: () => void) => {
      const config: relationalStore.CloudSyncConfig = {
        mode: relationalStore.SyncMode.SYNC_MODE_TIME_FIRST,
        fullSync: true
      };
      try {
        await cloudData.Config.cloudSyncEx(
          { bundleName: 'com.example.test', storeId: 'testStore' },
          config,
          (progress: relationalStore.ProgressDetails) => {}
        );
        expect(true).assertTrue();
        done();
      } catch (e) {
        expect().assertFail();
        done();
      }
    });

    /**
     * @tc.name CloudSyncFullSync002
     * @tc.desc Verify fullSync=false is accepted (API succeeds or throws non-param error)
     * @tc.type: FUNC
     * @tc.require:
     */
    it('CloudSyncFullSync002', 0, async (done: () => void) => {
      const config: relationalStore.CloudSyncConfig = {
        mode: relationalStore.SyncMode.SYNC_MODE_TIME_FIRST,
        fullSync: false
      };
      try {
        await cloudData.Config.cloudSyncEx(
          { bundleName: 'com.example.test', storeId: 'testStore' },
          config,
          (progress: relationalStore.ProgressDetails) => {}
        );
        expect(true).assertTrue();
        done();
      } catch (e) {
        expect().assertFail();
        done();
      }
    });

    /**
     * @tc.name CloudSyncFullSync003
     * @tc.desc Verify fullSync omitted is accepted (API succeeds or throws non-param error)
     * @tc.type: FUNC
     * @tc.require:
     */
    it('CloudSyncFullSync003', 0, async (done: () => void) => {
      const config: relationalStore.CloudSyncConfig = {
        mode: relationalStore.SyncMode.SYNC_MODE_TIME_FIRST
      };
      try {
        await cloudData.Config.cloudSyncEx(
          { bundleName: 'com.example.test', storeId: 'testStore' },
          config,
          (progress: relationalStore.ProgressDetails) => {}
        );
        expect(true).assertTrue();
        done();
      } catch (e) {
        expect().assertFail();
        done();
      }
    });

    /**
     * @tc.name CloudSyncFullSync004
     * @tc.desc Verify fullSync=true with CLOUD_FIRST mode is accepted
     * @tc.type: FUNC
     * @tc.require:
     */
    it('CloudSyncFullSync004', 0, async (done: () => void) => {
      const config: relationalStore.CloudSyncConfig = {
        mode: relationalStore.SyncMode.SYNC_MODE_CLOUD_FIRST,
        fullSync: true
      };
      try {
        await cloudData.Config.cloudSyncEx(
          { bundleName: 'com.example.test', storeId: 'testStore' },
          config,
          (progress: relationalStore.ProgressDetails) => {}
        );
        expect(true).assertTrue();
        done();
      } catch (e) {
        expect().assertFail();
        done();
      }
    });

    /**
     * @tc.name CloudSyncFullSync005
     * @tc.desc Verify fullSync=true with downloadOnly=true is accepted
     * @tc.type: FUNC
     * @tc.require:
     */
    it('CloudSyncFullSync005', 0, async (done: () => void) => {
      const config: relationalStore.CloudSyncConfig = {
        mode: relationalStore.SyncMode.SYNC_MODE_TIME_FIRST,
        downloadOnly: true,
        fullSync: true
      };
      try {
        await cloudData.Config.cloudSyncEx(
          { bundleName: 'com.example.test', storeId: 'testStore' },
          config,
          (progress: relationalStore.ProgressDetails) => {}
        );
        expect(true).assertTrue();
        done();
      } catch (e) {
        expect().assertFail();
        done();
      }
    });

    /**
     * @tc.name CloudSyncFullSync006
     * @tc.desc Verify fullSync with non-boolean type returns parameter error 14800001
     * @tc.type: FUNC
     * @tc.require:
     */
    it('CloudSyncFullSync006', 0, async (done: () => void) => {
      const config: relationalStore.CloudSyncConfig = {
        mode: relationalStore.SyncMode.SYNC_MODE_TIME_FIRST,
        fullSync: 'invalid' as unknown as boolean
      };
      try {
        await cloudData.Config.cloudSyncEx(
          { bundleName: 'com.example.test', storeId: 'testStore' },
          config,
          (progress: relationalStore.ProgressDetails) => {}
        );
        expect().assertFail();
        done();
      } catch (e) {
        const error = e as BusinessError;
        console.error('CloudSyncFullSync006 error: ' + `, code: ${error.code}, message: ${error.message}`);
        expect(error.code == 14800001).assertTrue();
        done();
      }
    });

    /**
     * @tc.name CloudSyncFullSync007
     * @tc.desc Verify fullSync explicitly null is accepted
     * @tc.type: FUNC
     * @tc.require:
     */
    it('CloudSyncFullSync007', 0, async (done: () => void) => {
      const config: relationalStore.CloudSyncConfig = {
        mode: relationalStore.SyncMode.SYNC_MODE_TIME_FIRST,
        fullSync: null as unknown as boolean
      };
      try {
        await cloudData.Config.cloudSyncEx(
          { bundleName: 'com.example.test', storeId: 'testStore' },
          config,
          (progress: relationalStore.ProgressDetails) => {}
        );
        expect(true).assertTrue();
        done();
      } catch (e) {
        expect().assertFail();
        done();
      }
    });

    /**
     * @tc.name CloudSyncFullSync008
     * @tc.desc Verify enablePredicate=true without predicate returns parameter error 14800001
     * @tc.type: FUNC
     * @tc.require:
     */
    it('CloudSyncFullSync008', 0, async (done: () => void) => {
      const config: relationalStore.CloudSyncConfig = {
        mode: relationalStore.SyncMode.SYNC_MODE_TIME_FIRST,
        enablePredicate: true,
        fullSync: true
      };
      try {
        await cloudData.Config.cloudSyncEx(
          { bundleName: 'com.example.test', storeId: 'testStore' },
          config,
          (progress: relationalStore.ProgressDetails) => {}
        );
        expect().assertFail();
        done();
      } catch (e) {
        const error = e as BusinessError;
        console.error('CloudSyncFullSync008 error: ' + `, code: ${error.code}, message: ${error.message}`);
        expect(error.code == 14800001).assertTrue();
        done();
      }
    });
  });
}

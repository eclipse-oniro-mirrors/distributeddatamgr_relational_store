/*
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
import relationalStore from '@ohos.data.relationalStore'
import AbilityDelegatorRegistry from '@ohos.app.ability.abilityDelegatorRegistry';
const delegator = AbilityDelegatorRegistry.getAbilityDelegator();
const context = delegator.getAppContext().getApplicationContext();
const TAG = "[RELATIONAL_STORE_ASSET_EXTENSION_TEST]"
const STORE_CONFIG: relationalStore.StoreConfig = {
  name: "AssetExtensionTest.db",
  securityLevel: relationalStore.SecurityLevel.S1,
}
const CREATE_TABLE_SQL = 'CREATE TABLE IF NOT EXISTS asset_test ' +
  '(id INTEGER PRIMARY KEY AUTOINCREMENT, data1 asset, data2 assets)';

let rdbStore: relationalStore.RdbStore | undefined;

function checkAssetEqual(a1: relationalStore.Asset, a2: relationalStore.Asset): boolean {
  return a1.name == a2.name && a1.uri == a2.uri &&
    a1.createTime == a2.createTime && a1.modifyTime == a2.modifyTime &&
    a1.size == a2.size && a1.path == a2.path &&
    a1.extension == a2.extension;
}

function checkAssetsEqual(a1: relationalStore.Assets, a2: relationalStore.Assets): boolean {
  if (a1.length != a2.length) {
    return false;
  }
  for (let i = 0; i < a1.length; i++) {
    if (!checkAssetEqual(a1[i], a2[i])) {
      return false;
    }
  }
  return true;
}

export default function AssetExtensionTest() {
  describe('AssetExtensionTest', () => {
    beforeAll(async () => {
      console.info(TAG + 'beforeAll')
      rdbStore = await relationalStore.getRdbStore(context, STORE_CONFIG);
      if (rdbStore != undefined) {
        try {
          await rdbStore.executeSql(CREATE_TABLE_SQL);
        } catch (e) {
          console.error(`executeSql error, code is ${e.code}, message is ${e.message}`);
          expect(null).assertFail();
        }
      }
    })

    beforeEach(async () => {
      console.info(TAG + 'beforeEach')
      if (rdbStore != undefined) {
        try {
          await rdbStore.executeSql('DELETE FROM asset_test');
        } catch (e) {
          console.error(`beforeEach delete error, code is ${e.code}, message is ${e.message}`);
        }
      }
    })

    afterEach(async () => {
      console.info(TAG + 'afterEach')
    })

    afterAll(async () => {
      console.info(TAG + 'afterAll')
      if (rdbStore != undefined) {
        try {
          await rdbStore.close();
        } catch (e) {
          console.error(`afterAll close error, code is ${e.code}, message is ${e.message}`);
        }
      }
      await relationalStore.deleteRdbStore(context, "AssetExtensionTest.db");
    })

    /**
     * @tc.number AssetExtensionTest0001
     * @tc.name AssetExtension_InsertAndQuery
     * @tc.desc Insert asset with extension field and verify via getRow
     */
    it('AssetExtensionTest0001', 0, async () => {
      console.log(TAG + "************* AssetExtensionTest0001 start *************");
      if (rdbStore != undefined) {
        try {
          const asset: relationalStore.Asset = {
            name: 'file1',
            uri: 'uri1',
            createTime: '2024-01-01',
            modifyTime: '2024-01-02',
            size: '1024',
            path: '/data/file1',
            extension: 'txt',
            status: relationalStore.AssetStatus.ASSET_NORMAL,
          }
          const valueBucket: relationalStore.ValuesBucket = {
            'data1': asset,
            'data2': [asset],
          }
          await rdbStore.insert('asset_test', valueBucket);
          let predicates = new relationalStore.RdbPredicates('asset_test');
          let resultSet = await rdbStore.query(predicates) as relationalStore.ResultSet;
          expect(resultSet.rowCount).assertEqual(1);
          expect(resultSet.goToFirstRow()).assertTrue();
          let row = resultSet.getRow();
          expect(checkAssetEqual(row.data1, asset)).assertTrue();
          expect(checkAssetsEqual(row.data2, [asset])).assertTrue();
          expect(row.data1.extension).assertEqual('txt');
          resultSet.close();
        } catch (e) {
          console.error(`AssetExtensionTest0001 failed, code is ${e.code}, message is ${e.message}`);
          expect(null).assertFail();
        }
      }
      console.log(TAG + "************* AssetExtensionTest0001 end *************");
    })

    /**
     * @tc.number AssetExtensionTest0002
     * @tc.name AssetExtension_AssetsArray
     * @tc.desc Insert assets array with different extension values and verify
     */
    it('AssetExtensionTest0002', 0, async () => {
      console.log(TAG + "************* AssetExtensionTest0002 start *************");
      if (rdbStore != undefined) {
        try {
          const asset1: relationalStore.Asset = {
            name: 'file1',
            uri: 'uri1',
            createTime: '2024-01-01',
            modifyTime: '2024-01-02',
            size: '1024',
            path: '/data/file1',
            extension: 'jpg',
            status: relationalStore.AssetStatus.ASSET_NORMAL,
          }
          const asset2: relationalStore.Asset = {
            name: 'file2',
            uri: 'uri2',
            createTime: '2024-01-03',
            modifyTime: '2024-01-04',
            size: '2048',
            path: '/data/file2',
            extension: 'png',
            status: relationalStore.AssetStatus.ASSET_NORMAL,
          }
          const valueBucket: relationalStore.ValuesBucket = {
            'data1': asset1,
            'data2': [asset1, asset2],
          }
          await rdbStore.insert('asset_test', valueBucket);
          let predicates = new relationalStore.RdbPredicates('asset_test');
          let resultSet = await rdbStore.query(predicates) as relationalStore.ResultSet;
          expect(resultSet.rowCount).assertEqual(1);
          expect(resultSet.goToFirstRow()).assertTrue();
          let row = resultSet.getRow();
          expect(checkAssetEqual(row.data1, asset1)).assertTrue();
          expect(checkAssetsEqual(row.data2, [asset1, asset2])).assertTrue();
          expect(row.data2[0].extension).assertEqual('jpg');
          expect(row.data2[1].extension).assertEqual('png');
          resultSet.close();
        } catch (e) {
          console.error(`AssetExtensionTest0002 failed, code is ${e.code}, message is ${e.message}`);
          expect(null).assertFail();
        }
      }
      console.log(TAG + "************* AssetExtensionTest0002 end *************");
    })

    /**
     * @tc.number AssetExtensionTest0003
     * @tc.name AssetExtension_EmptyString
     * @tc.desc Insert asset with empty extension string and verify
     */
    it('AssetExtensionTest0003', 0, async () => {
      console.log(TAG + "************* AssetExtensionTest0003 start *************");
      if (rdbStore != undefined) {
        try {
          const asset: relationalStore.Asset = {
            name: 'file1',
            uri: 'uri1',
            createTime: '2024-01-01',
            modifyTime: '2024-01-02',
            size: '1024',
            path: '/data/file1',
            extension: '',
            status: relationalStore.AssetStatus.ASSET_NORMAL,
          }
          const valueBucket: relationalStore.ValuesBucket = {
            'data1': asset,
            'data2': [asset],
          }
          await rdbStore.insert('asset_test', valueBucket);
          let predicates = new relationalStore.RdbPredicates('asset_test');
          let resultSet = await rdbStore.query(predicates) as relationalStore.ResultSet;
          expect(resultSet.rowCount).assertEqual(1);
          expect(resultSet.goToFirstRow()).assertTrue();
          let row = resultSet.getRow();
          expect(checkAssetEqual(row.data1, asset)).assertTrue();
          expect(row.data1.extension).assertEqual('');
          resultSet.close();
        } catch (e) {
          console.error(`AssetExtensionTest0003 failed, code is ${e.code}, message is ${e.message}`);
          expect(null).assertFail();
        }
      }
      console.log(TAG + "************* AssetExtensionTest0003 end *************");
    })

    /**
     * @tc.number AssetExtensionTest0004
     * @tc.name AssetExtension_SpecialCharacters
     * @tc.desc Insert asset with special characters in extension field
     */
    it('AssetExtensionTest0004', 0, async () => {
      console.log(TAG + "************* AssetExtensionTest0004 start *************");
      if (rdbStore != undefined) {
        try {
          const asset: relationalStore.Asset = {
            name: 'file1',
            uri: 'uri1',
            createTime: '2024-01-01',
            modifyTime: '2024-01-02',
            size: '1024',
            path: '/data/file1',
            extension: '中文扩展名_测试.123',
            status: relationalStore.AssetStatus.ASSET_NORMAL,
          }
          const valueBucket: relationalStore.ValuesBucket = {
            'data1': asset,
            'data2': [asset],
          }
          await rdbStore.insert('asset_test', valueBucket);
          let predicates = new relationalStore.RdbPredicates('asset_test');
          let resultSet = await rdbStore.query(predicates) as relationalStore.ResultSet;
          expect(resultSet.rowCount).assertEqual(1);
          expect(resultSet.goToFirstRow()).assertTrue();
          let row = resultSet.getRow();
          expect(checkAssetEqual(row.data1, asset)).assertTrue();
          expect(row.data1.extension).assertEqual('中文扩展名_测试.123');
          resultSet.close();
        } catch (e) {
          console.error(`AssetExtensionTest0004 failed, code is ${e.code}, message is ${e.message}`);
          expect(null).assertFail();
        }
      }
      console.log(TAG + "************* AssetExtensionTest0004 end *************");
    })

    /**
     * @tc.number AssetExtensionTest0005
     * @tc.name AssetExtension_LongString
     * @tc.desc Insert asset with long extension string (boundary value)
     */
    it('AssetExtensionTest0005', 0, async () => {
      console.log(TAG + "************* AssetExtensionTest0005 start *************");
      if (rdbStore != undefined) {
        try {
          let longExtension = '';
          for (let i = 0; i < 512; i++) {
            longExtension += 'a';
          }
          const asset: relationalStore.Asset = {
            name: 'file1',
            uri: 'uri1',
            createTime: '2024-01-01',
            modifyTime: '2024-01-02',
            size: '1024',
            path: '/data/file1',
            extension: longExtension,
            status: relationalStore.AssetStatus.ASSET_NORMAL,
          }
          const valueBucket: relationalStore.ValuesBucket = {
            'data1': asset,
            'data2': [asset],
          }
          await rdbStore.insert('asset_test', valueBucket);
          let predicates = new relationalStore.RdbPredicates('asset_test');
          let resultSet = await rdbStore.query(predicates) as relationalStore.ResultSet;
          expect(resultSet.rowCount).assertEqual(1);
          expect(resultSet.goToFirstRow()).assertTrue();
          let row = resultSet.getRow();
          expect(checkAssetEqual(row.data1, asset)).assertTrue();
          expect(row.data1.extension).assertEqual(longExtension);
          expect(row.data1.extension.length).assertEqual(512);
          resultSet.close();
        } catch (e) {
          console.error(`AssetExtensionTest0005 failed, code is ${e.code}, message is ${e.message}`);
          expect(null).assertFail();
        }
      }
      console.log(TAG + "************* AssetExtensionTest0005 end *************");
    })

    /**
     * @tc.number AssetExtensionTest0006
     * @tc.name AssetExtension_NotSet
     * @tc.desc Insert asset without extension field and verify it defaults to empty
     */
    it('AssetExtensionTest0006', 0, async () => {
      console.log(TAG + "************* AssetExtensionTest0006 start *************");
      if (rdbStore != undefined) {
        try {
          const asset: relationalStore.Asset = {
            name: 'file1',
            uri: 'uri1',
            createTime: '2024-01-01',
            modifyTime: '2024-01-02',
            size: '1024',
            path: '/data/file1',
            status: relationalStore.AssetStatus.ASSET_NORMAL,
          }
          const valueBucket: relationalStore.ValuesBucket = {
            'data1': asset,
            'data2': [asset],
          }
          await rdbStore.insert('asset_test', valueBucket);
          let predicates = new relationalStore.RdbPredicates('asset_test');
          let resultSet = await rdbStore.query(predicates) as relationalStore.ResultSet;
          expect(resultSet.rowCount).assertEqual(1);
          expect(resultSet.goToFirstRow()).assertTrue();
          let row = resultSet.getRow();
          expect(row.data1.name).assertEqual('file1');
          expect(row.data1.extension).assertEqual('');
          resultSet.close();
        } catch (e) {
          console.error(`AssetExtensionTest0006 failed, code is ${e.code}, message is ${e.message}`);
          expect(null).assertFail();
        }
      }
      console.log(TAG + "************* AssetExtensionTest0006 end *************");
    })

    /**
     * @tc.number AssetExtensionTest0007
     * @tc.name AssetExtension_Update
     * @tc.desc Update asset extension field and verify the new value
     */
    it('AssetExtensionTest0007', 0, async () => {
      console.log(TAG + "************* AssetExtensionTest0007 start *************");
      if (rdbStore != undefined) {
        try {
          const asset: relationalStore.Asset = {
            name: 'file1',
            uri: 'uri1',
            createTime: '2024-01-01',
            modifyTime: '2024-01-02',
            size: '1024',
            path: '/data/file1',
            extension: 'txt',
            status: relationalStore.AssetStatus.ASSET_NORMAL,
          }
          const valueBucket: relationalStore.ValuesBucket = {
            'data1': asset,
            'data2': [asset],
          }
          await rdbStore.insert('asset_test', valueBucket);

          const updatedAsset: relationalStore.Asset = {
            name: 'file1',
            uri: 'uri1',
            createTime: '2024-01-01',
            modifyTime: '2024-01-05',
            size: '2048',
            path: '/data/file1',
            extension: 'pdf',
            status: relationalStore.AssetStatus.ASSET_NORMAL,
          }
          const updateBucket: relationalStore.ValuesBucket = {
            'data1': updatedAsset,
            'data2': [updatedAsset],
          }
          let predicates = new relationalStore.RdbPredicates('asset_test');
          predicates.equalTo('name', 'file1');
          await rdbStore.update(updateBucket, predicates);

          let queryPredicates = new relationalStore.RdbPredicates('asset_test');
          let resultSet = await rdbStore.query(queryPredicates) as relationalStore.ResultSet;
          expect(resultSet.rowCount).assertEqual(1);
          expect(resultSet.goToFirstRow()).assertTrue();
          let row = resultSet.getRow();
          expect(checkAssetEqual(row.data1, updatedAsset)).assertTrue();
          expect(row.data1.extension).assertEqual('pdf');
          resultSet.close();
        } catch (e) {
          console.error(`AssetExtensionTest0007 failed, code is ${e.code}, message is ${e.message}`);
          expect(null).assertFail();
        }
      }
      console.log(TAG + "************* AssetExtensionTest0007 end *************");
    })

    /**
     * @tc.number AssetExtensionTest0008
     * @tc.name AssetExtension_QuerySql
     * @tc.desc Insert multiple rows and verify extension via querySql getRow
     */
    it('AssetExtensionTest0008', 0, async () => {
      console.log(TAG + "************* AssetExtensionTest0008 start *************");
      if (rdbStore != undefined) {
        try {
          const asset1: relationalStore.Asset = {
            name: 'file1',
            uri: 'uri1',
            createTime: '2024-01-01',
            modifyTime: '2024-01-02',
            size: '1024',
            path: '/data/file1',
            extension: 'txt',
            status: relationalStore.AssetStatus.ASSET_NORMAL,
          }
          const asset2: relationalStore.Asset = {
            name: 'file2',
            uri: 'uri2',
            createTime: '2024-01-03',
            modifyTime: '2024-01-04',
            size: '2048',
            path: '/data/file2',
            extension: 'jpg',
            status: relationalStore.AssetStatus.ASSET_NORMAL,
          }
          await rdbStore.insert('asset_test', { 'data1': asset1, 'data2': [asset1] });
          await rdbStore.insert('asset_test', { 'data1': asset2, 'data2': [asset2] });

          let resultSet = await rdbStore.querySql('SELECT * FROM asset_test ORDER BY id') as relationalStore.ResultSet;
          expect(resultSet.rowCount).assertEqual(2);
          expect(resultSet.goToFirstRow()).assertTrue();
          let row1 = resultSet.getRow();
          expect(checkAssetEqual(row1.data1, asset1)).assertTrue();
          expect(row1.data1.extension).assertEqual('txt');
          expect(resultSet.goToNextRow()).assertTrue();
          let row2 = resultSet.getRow();
          expect(checkAssetEqual(row2.data1, asset2)).assertTrue();
          expect(row2.data1.extension).assertEqual('jpg');
          resultSet.close();
        } catch (e) {
          console.error(`AssetExtensionTest0008 failed, code is ${e.code}, message is ${e.message}`);
          expect(null).assertFail();
        }
      }
      console.log(TAG + "************* AssetExtensionTest0008 end *************");
    })

    /**
     * @tc.number AssetExtensionTest0009
     * @tc.name AssetExtension_GetAssetMethod
     * @tc.desc Verify extension field via ResultSet.getAsset and getAssets methods
     */
    it('AssetExtensionTest0009', 0, async () => {
      console.log(TAG + "************* AssetExtensionTest0009 start *************");
      if (rdbStore != undefined) {
        try {
          const asset: relationalStore.Asset = {
            name: 'file1',
            uri: 'uri1',
            createTime: '2024-01-01',
            modifyTime: '2024-01-02',
            size: '1024',
            path: '/data/file1',
            extension: 'docx',
            status: relationalStore.AssetStatus.ASSET_NORMAL,
          }
          const valueBucket: relationalStore.ValuesBucket = {
            'data1': asset,
            'data2': [asset],
          }
          await rdbStore.insert('asset_test', valueBucket);
          let predicates = new relationalStore.RdbPredicates('asset_test');
          let resultSet = await rdbStore.query(predicates) as relationalStore.ResultSet;
          expect(resultSet.rowCount).assertEqual(1);
          expect(resultSet.goToFirstRow()).assertTrue();
          let data1 = resultSet.getAsset(resultSet.getColumnIndex('data1'));
          expect(data1.name).assertEqual('file1');
          expect(data1.extension).assertEqual('docx');
          let data2 = resultSet.getAssets(resultSet.getColumnIndex('data2'));
          expect(data2.length).assertEqual(1);
          expect(data2[0].extension).assertEqual('docx');
          resultSet.close();
        } catch (e) {
          console.error(`AssetExtensionTest0009 failed, code is ${e.code}, message is ${e.message}`);
          expect(null).assertFail();
        }
      }
      console.log(TAG + "************* AssetExtensionTest0009 end *************");
    })

    /**
     * @tc.number AssetExtensionTest0010
     * @tc.name AssetExtension_MixedExtensions
     * @tc.desc Insert assets with mixed extension values (some set, some empty) in same array
     */
    it('AssetExtensionTest0010', 0, async () => {
      console.log(TAG + "************* AssetExtensionTest0010 start *************");
      if (rdbStore != undefined) {
        try {
          const asset1: relationalStore.Asset = {
            name: 'file1',
            uri: 'uri1',
            createTime: '2024-01-01',
            modifyTime: '2024-01-02',
            size: '1024',
            path: '/data/file1',
            extension: 'mp4',
            status: relationalStore.AssetStatus.ASSET_NORMAL,
          }
          const asset2: relationalStore.Asset = {
            name: 'file2',
            uri: 'uri2',
            createTime: '2024-01-03',
            modifyTime: '2024-01-04',
            size: '2048',
            path: '/data/file2',
            extension: '',
            status: relationalStore.AssetStatus.ASSET_NORMAL,
          }
          const asset3: relationalStore.Asset = {
            name: 'file3',
            uri: 'uri3',
            createTime: '2024-01-05',
            modifyTime: '2024-01-06',
            size: '4096',
            path: '/data/file3',
            extension: 'wav',
            status: relationalStore.AssetStatus.ASSET_NORMAL,
          }
          const valueBucket: relationalStore.ValuesBucket = {
            'data1': asset1,
            'data2': [asset1, asset2, asset3],
          }
          await rdbStore.insert('asset_test', valueBucket);
          let predicates = new relationalStore.RdbPredicates('asset_test');
          let resultSet = await rdbStore.query(predicates) as relationalStore.ResultSet;
          expect(resultSet.rowCount).assertEqual(1);
          expect(resultSet.goToFirstRow()).assertTrue();
          let row = resultSet.getRow();
          expect(checkAssetEqual(row.data1, asset1)).assertTrue();
          expect(checkAssetsEqual(row.data2, [asset1, asset2, asset3])).assertTrue();
          expect(row.data2[0].extension).assertEqual('mp4');
          expect(row.data2[1].extension).assertEqual('');
          expect(row.data2[2].extension).assertEqual('wav');
          resultSet.close();
        } catch (e) {
          console.error(`AssetExtensionTest0010 failed, code is ${e.code}, message is ${e.message}`);
          expect(null).assertFail();
        }
      }
      console.log(TAG + "************* AssetExtensionTest0010 end *************");
    })

    console.log(TAG + "*************Unit Test Begin*************");
  })
}

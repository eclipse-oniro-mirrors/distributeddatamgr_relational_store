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

import RdbPredicatesEtsTest from './RdbstorePredicatesEts.test.ets';
import RdbPredicatesPrefEtsTest from './RdbPredicatesPerfEts.test.ets';
import RelationalStoreDataGroupId from './RdbGroupIdEts.test.ets';
import RdbStoreErrorlogTest from './RdbOnErrorLog.test.ets';
import PerfStatEtsTest from './RdbstorePerfStatEts.test.ets';
import RdbstoreGetValueForFlutterEts from './RdbstoreGetValueForFlutterEts.test.ets';
import RdbstoreGetRowForFlutterEts from './RdbstoreGetRowForFlutterEts.test.ets';
import RdbRekeyEtsTest from './RdbstoreRekeyEts.test.ets';
import RDBstoreGetSqlInfoTest from './RdbstoreGetSqlInfoEts.test.ets';
import RdbStoreTransactionTest from './RdbStoreTransaction.test.ets';
import RdbstoreTransactionJsunit from './RdbstoreTransactionJsunit.test.ets';
import RdbStoreTransInsertJsunit from './RdbStoreTransInsertJsunit.test.ets';
import RdbStoreTransJsunit from './RdbStoreTransJsunit.test.ets';
import RdbstoreCustomTokenizerTest from './RdbstoreCustomTokenizerEts.ets';
export default function testsuite() {
  RdbPredicatesEtsTest();
  RdbPredicatesPrefEtsTest();
  RelationalStoreDataGroupId();
  RdbStoreErrorlogTest();
  PerfStatEtsTest();
  RdbstoreGetValueForFlutterEts();
  RdbstoreGetRowForFlutterEts();
  RdbRekeyEtsTest();
  RDBstoreGetSqlInfoTest();
  RdbStoreTransactionTest();
  RdbstoreTransactionJsunit();
  RdbStoreTransInsertJsunit();
  RdbStoreTransJsunit();
  RdbstoreCustomTokenizerTest();
}
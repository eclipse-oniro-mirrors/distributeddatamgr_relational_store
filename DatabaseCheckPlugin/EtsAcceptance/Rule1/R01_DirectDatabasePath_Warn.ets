/*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the 'License');
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an 'AS IS' BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// Case: R01_DirectDatabasePath_Warn
// Rule: 1
// Expected: 1
// ExpectedAll: 1:1
// Message: Prohibited use
// Point: 直接向 fileIo.openSync 传入 databaseDir 路径

import common from '@ohos.app.ability.common';
import fileIo from '@ohos.file.fs';

export function R01_DirectDatabasePath_Warn(context: common.Context): void {
  fileIo.openSync(context.databaseDir + '/r01_direct.db', fileIo.OpenMode.READ_ONLY);
}

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

// Case: R02_DestinationAlias_Warn
// Rule: 2
// Expected: 1
// ExpectedAll: 2:1
// Message: file interface
// Point: 复制目标路径来自数据库路径变量

import common from '@ohos.app.ability.common';
import fileIo from '@ohos.file.fs';

export function R02_DestinationAlias_Warn(context: common.Context): void {
  const target: string = context.databaseDir + '/r02_target.db';
  fileIo.copyFileSync(context.filesDir + '/input.db', target);
}

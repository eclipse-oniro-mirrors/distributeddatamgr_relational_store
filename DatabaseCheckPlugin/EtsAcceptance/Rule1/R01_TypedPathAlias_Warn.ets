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

// Case: R01_TypedPathAlias_Warn
// Rule: 1
// Expected: 1
// ExpectedAll: 1:1
// Message: Prohibited use
// Point: 带类型的路径变量经两层别名与拼接传给接口

import common from '@ohos.app.ability.common';
import fileIo from '@ohos.file.fs';

export function R01_TypedPathAlias_Warn(context: common.Context): void {
  const root: string = context.databaseDir;
  const alias: string = root;
  const path: string = alias + '/r01_alias.db';
  fileIo.openSync(path, fileIo.OpenMode.READ_ONLY);
}

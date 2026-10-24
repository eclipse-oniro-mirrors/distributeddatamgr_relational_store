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

// Case: R07_LegacyChmod_PathAlias_Warn
// Rule: 7
// Expected: 1
// ExpectedAll: 7:1
// Message: Permission modification
// Point: 旧 fileio 模块的 chmod 接收数据库路径变量

import common from '@ohos.app.ability.common';
import fileIo from '@ohos.fileio';

export async function R07_LegacyChmod_PathAlias_Warn(context: common.Context): Promise<void> {
  const root: string = context.databaseDir;
  const path: string = root + '/r07_target.db';
  await fileIo.chmod(path, 0o600);
}

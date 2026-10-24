/*
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

// Case: R01_FutureAssignment_Pass
// Rule: 1
// Expected: 0
// Message:
// Point: 普通文件调用不借用之后的数据库路径赋值

import common from '@ohos.app.ability.common';
import fileIo from '@ohos.file.fs';

export function R01_FutureAssignment_Pass(context: common.Context): void {
  let path: string = context.filesDir + '/normal.txt';
  fileIo.openSync(path, fileIo.OpenMode.READ_ONLY);
  path = context.databaseDir;
}

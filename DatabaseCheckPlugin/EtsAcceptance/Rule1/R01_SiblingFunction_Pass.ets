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

// Case: R01_SiblingFunction_Pass
// Rule: 1
// Expected: 0
// Message:
// Point: 相邻函数的数据库路径局部变量不污染普通文件函数

import common from '@ohos.app.ability.common';
import fileIo from '@ohos.file.fs';

function unused(context: common.Context): string {
  const path: string = context.databaseDir;
  return path;
}
export function R01_SiblingFunction_Pass(context: common.Context): void {
  const path: string = context.filesDir + '/normal.txt';
  fileIo.openSync(path, fileIo.OpenMode.READ_ONLY);
}

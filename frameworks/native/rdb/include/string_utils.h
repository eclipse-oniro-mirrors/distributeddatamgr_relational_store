/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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

#ifndef NATIVE_RDB_STRINGUTILS_H
#define NATIVE_RDB_STRINGUTILS_H

#include <string>
#include <vector>

namespace OHOS {
namespace NativeRdb {
class StringUtils {
public:
    static std::string SurroundWithQuote(const std::string &value, const std::string &quote);
    static std::string SurroundWithFunction(
        const std::string &function, const std::string &separator, const std::vector<std::string> &array);

    static std::vector<std::string> Split(const std::string &str, const std::string &delim);
    static std::string ExtractFilePath(const std::string &fileFullName);
    static std::string ExtractFileName(const std::string &fileFullName);
    static std::string TruncateAfterFirstParen(const std::string& str);
    static std::string GetParentPath(const std::string &path);
    static bool IsEmpty(std::string source)
    {
        return (source.empty());
    }

    static std::string Trim(const std::string &source)
    {
        if (source.empty()) {
            return "";
        }
        std::string replaceResult = source;
        replaceResult.erase(0, replaceResult.find_first_not_of(" "));
        replaceResult.erase(replaceResult.find_last_not_of(" ") + 1);
        return source;
    }

private:
    StringUtils();
    ~StringUtils();
};
} // namespace NativeRdb
} // namespace OHOS

#endif
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

#include <gtest/gtest.h>

#include <string>

#include "sqlite_utils.h"

using namespace OHOS::NativeRdb;
using namespace testing::ext;

namespace {
class RdbSqlStatementTypePrefixTest : public testing::Test {};

const std::string GRD_PREFIX = "[app_level('1') apl_level('NORMAL')]";

/**
 * @brief The GRD prefix preserves the type of the statement that follows it.
 */
HWTEST_F(RdbSqlStatementTypePrefixTest, SqlType_GrdPrefix_StatementKinds_001, TestSize.Level0)
{
    // Arrange: DELETE is classified as an update; EXPLAIN has no entry in the type map.
    struct StatementCase {
        const char *sql;
        int expected;
    };
    const StatementCase cases[] = {
        { "select * from t", SqliteUtils::STATEMENT_SELECT },
        { "INSERT INTO t VALUES (1)", SqliteUtils::STATEMENT_INSERT },
        { "UPDATE t SET id = 2", SqliteUtils::STATEMENT_UPDATE },
        { "DELETE FROM t", SqliteUtils::STATEMENT_UPDATE },
        { "CREATE TABLE t (id INTEGER)", SqliteUtils::STATEMENT_DDL },
        { "EXPLAIN SELECT * FROM t", SqliteUtils::STATEMENT_OTHER },
    };
    for (const auto &entry : cases) {
        // Act & Assert: classify the same SQL with and without the annotation.
        EXPECT_EQ(SqliteUtils::GetSqlStatementType(entry.sql, DB_VECTOR), entry.expected) << entry.sql;
        const auto annotated = "\x01\t" + GRD_PREFIX + "\r\n\x02 " + entry.sql;
        EXPECT_EQ(SqliteUtils::GetSqlStatementType(annotated, DB_VECTOR), entry.expected) << annotated;
    }
    EXPECT_EQ(SqliteUtils::GetSqlStatementType(GRD_PREFIX + "SELECT 1", DB_VECTOR), SqliteUtils::STATEMENT_SELECT);
}

/**
 * @brief An empty or short statement after the prefix is an error; an unclosed prefix is unrecognized.
 */
HWTEST_F(RdbSqlStatementTypePrefixTest, SqlType_GrdPrefix_IncompleteInput_002, TestSize.Level0)
{
    // With or without a complete annotation, whitespace alone is not a statement.
    EXPECT_EQ(SqliteUtils::GetSqlStatementType(" \t\n", DB_VECTOR), SqliteUtils::STATEMENT_ERROR);
    EXPECT_EQ(SqliteUtils::GetSqlStatementType(GRD_PREFIX, DB_VECTOR), SqliteUtils::STATEMENT_ERROR);
    EXPECT_EQ(SqliteUtils::GetSqlStatementType(GRD_PREFIX + " \t\n", DB_VECTOR), SqliteUtils::STATEMENT_ERROR);
    EXPECT_EQ(SqliteUtils::GetSqlStatementType(GRD_PREFIX + " SE", DB_VECTOR), SqliteUtils::STATEMENT_ERROR);
    const std::string unclosed = "[app_level('1') SELECT * FROM t";
    EXPECT_EQ(SqliteUtils::GetSqlStatementType(unclosed, DB_VECTOR), SqliteUtils::STATEMENT_OTHER);
}
} // namespace

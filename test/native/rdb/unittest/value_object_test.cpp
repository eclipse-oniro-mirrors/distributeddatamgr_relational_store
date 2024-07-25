/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
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

#include "value_object.h"

#include <gtest/gtest.h>

#include <climits>
#include <string>

#include "big_integer.h"
#include "grd_type_export.h"

using namespace testing::ext;
using namespace OHOS::NativeRdb;

namespace Test {
class ValueObjectTest : public testing::Test {
public:
    static void SetUpTestCase(void);
    static void TearDownTestCase(void);
    void SetUp(void){};
    void TearDown(void){};
};

void ValueObjectTest::SetUpTestCase(void)
{
}

void ValueObjectTest::TearDownTestCase(void)
{
}

/**
 * @tc.name: ValueObject_Test_001
 * @tc.desc: test func third line
ValueObject &ValueObject::operator=(ValueObject &&val) noexcept
{
    if (this == &val) {
        return *this;
    }
    value = std::move(val.value);
    return *this;
}
 * @tc.type: FUNC
 */
HWTEST_F(ValueObjectTest, ValueObject_Test_001, TestSize.Level1)
{
    int32_t intValue = 1234;
    ValueObject obj(intValue);
    ValueObject &ref = obj;
    ref = std::move(obj);
    int outValue;
    ref.GetInt(outValue);
    EXPECT_EQ(outValue, intValue);
}

/**
 * @tc.name: ValueObject_Test_001
 * @tc.desc: test func third line
ValueObject &ValueObject::operator=(const ValueObject &val)
{
    if (this == &val) {
        return *this;
    }
    value = val.value;
    return *this;
}
 * @tc.type: FUNC
 */
HWTEST_F(ValueObjectTest, ValueObject_Test_002, TestSize.Level1)
{
    int32_t intValue = 1234;
    ValueObject obj(intValue);
    ValueObject &ref = obj;
    EXPECT_EQ(&ref, &(ref = obj));
}

/**
 * @tc.name: ValueObject_Test_003
 * @tc.desc: test func fouth line
ValueObject::operator BigInt() const
{
    auto val = std::get_if<BigInt>(&value);
    if (val == nullptr) {
        return {};
    }
    return *val;
}
 * @tc.type: FUNC
 */
HWTEST_F(ValueObjectTest, ValueObject_Test_003, TestSize.Level1)
{
    BigInteger bigInt1(1234);
    ValueObject obj(bigInt1);
    BigInteger bigInt = obj;
    EXPECT_TRUE(bigInt == bigInt1);
}
} // namespace Test

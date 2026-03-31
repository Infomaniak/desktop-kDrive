/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2026 Infomaniak Network SA
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "testcommjson.h"

#include "libcommon/commjson.h"
#include "libcommon/utility/utility.h"

#include <Poco/Exception.h>

#include <cstdint>

namespace KDC {

void TestCommJson::testParseObject() {
    const Poco::DynamicStruct parsed = CommJson::parseObject(R"({"answer":42,"ok":true,"text":"hello"})");

    CPPUNIT_ASSERT_EQUAL(int32_t{42}, parsed["answer"].convert<int32_t>());
    CPPUNIT_ASSERT_EQUAL(true, parsed["ok"].convert<bool>());
    CPPUNIT_ASSERT_EQUAL(std::string("hello"), parsed["text"].convert<std::string>());
}

void TestCommJson::testParseCommObject() {
    const CommString json = CommonUtility::str2CommString(R"({"requestId":7,"scope":"gui4"})");
    const Poco::DynamicStruct parsed = CommJson::parseCommObject(json);

    CPPUNIT_ASSERT_EQUAL(int32_t{7}, parsed["requestId"].convert<int32_t>());
    CPPUNIT_ASSERT_EQUAL(std::string("gui4"), parsed["scope"].convert<std::string>());
}

void TestCommJson::testParseObjectRejectsNonObject() {
    CPPUNIT_ASSERT_THROW(CommJson::parseObject(R"([1,2,3])"), Poco::InvalidArgumentException);
}

} // namespace KDC

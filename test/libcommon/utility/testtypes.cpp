/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2024 Infomaniak Network SA
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

#include "config.h"
#include "testutility.h"
#include "libcommon/utility/utility.h"
#include "libcommonserver/log/log.h"
#include "testtypes.h"

namespace KDC {

void KDC::TestTypes::testOtherSide() {
    CPPUNIT_ASSERT_EQUAL(ReplicaSide::Local, otherSide(ReplicaSide::Remote));
    CPPUNIT_ASSERT_EQUAL(ReplicaSide::Remote, otherSide(ReplicaSide::Local));
    CPPUNIT_ASSERT_EQUAL(ReplicaSide::Unknown, otherSide(ReplicaSide::Unknown));
}
void TestTypes::testStreamConversion() {
    // Test enum class to string conversion with code
    CPPUNIT_ASSERT_EQUAL(std::string("Unknown(0)"), toStringWithCode(NodeType::Unknown));
    CPPUNIT_ASSERT_EQUAL(std::string("File(1)"), toStringWithCode(NodeType::File));
    CPPUNIT_ASSERT_EQUAL(std::string("Directory(2)"), toStringWithCode(NodeType::Directory));

    // Test stream operator for enum class without unicode
    std::ostringstream os;
    os << NodeType::Unknown;
    CPPUNIT_ASSERT_EQUAL(std::string("Unknown(0)"), os.str());

    // Test stream operator for enum class with unicode
    std::wostringstream wos;
    wos << NodeType::Unknown;
    CPPUNIT_ASSERT(L"Unknown(0)" == wos.str()); // Can't use CPPUNIT_ASSERT_EQUAL because of issue with wchar_t in CPPUNIT

    // Test Logging of enum class
    LOG_WARN(Log::instance()->getLogger(), "Test log of enumClass: " << NodeType::Unknown);
    std::ifstream is(Log::instance()->getLogFilePath().string());

    // check that the last line of the log file contains the expected string
    std::string line;
    std::string previousLine;
    while (std::getline(is, line)) {
        previousLine = line;
    }
    CPPUNIT_ASSERT(previousLine.find("Test log of enumClass: Unknown(0)") != std::string::npos);
}
} // namespace KDC

/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2025 Infomaniak Network SA
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

#include "utility/utility.h"

namespace KDC {

void TestTypes::testOtherSide() {
    CPPUNIT_ASSERT_EQUAL(ReplicaSide::Local, otherSide(ReplicaSide::Remote));
    CPPUNIT_ASSERT_EQUAL(ReplicaSide::Remote, otherSide(ReplicaSide::Local));
    CPPUNIT_ASSERT_EQUAL(ReplicaSide::Unknown, otherSide(ReplicaSide::Unknown));
}
void TestTypes::testStreamConversion() {
    // Test enum class to string conversion with code
    CPPUNIT_ASSERT_EQUAL(std::string("Unknown(0)"), toStringWithCode(NodeType::Unknown));
    CPPUNIT_ASSERT_EQUAL(std::string("File(1)"), toStringWithCode(NodeType::File));
    CPPUNIT_ASSERT_EQUAL(std::string("Directory(2)"), toStringWithCode(NodeType::Directory));
    CPPUNIT_ASSERT_EQUAL(std::string("Directory(2)"), toStringWithCode(fromInt<NodeType>(2)));
    CPPUNIT_ASSERT(Utility::startsWith(toStringWithCode(fromInt<NodeType>(3)), noConversionStr));

    CPPUNIT_ASSERT_EQUAL(std::string("None(0)"), toStringWithCode(OperationType::None));
    CPPUNIT_ASSERT_EQUAL(std::string("Create(1)"), toStringWithCode(OperationType::Create));
    CPPUNIT_ASSERT_EQUAL(std::string("Move(2)"), toStringWithCode(OperationType::Move));
    CPPUNIT_ASSERT_EQUAL(std::string("Edit(4)"), toStringWithCode(OperationType::Edit));
    CPPUNIT_ASSERT_EQUAL(std::string("Delete(8)"), toStringWithCode(OperationType::Delete));
    CPPUNIT_ASSERT_EQUAL(std::string("Rights(16)"), toStringWithCode(OperationType::Rights));
    CPPUNIT_ASSERT(Utility::startsWith(toStringWithCode(fromInt<OperationType>(0x09)), noConversionStr));
    CPPUNIT_ASSERT(Utility::startsWith(toStringWithCode(OperationType::Create | OperationType::Delete), noConversionStr));

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
void TestTypes::testExitInfo() {
    ExitInfo ei;
    ExitCode ec = ei;
    ExitCause eca = ei;
    CPPUNIT_ASSERT_EQUAL(ExitCode::Unknown, ei.code());
    CPPUNIT_ASSERT_EQUAL(ExitCode::Unknown, ec);
    CPPUNIT_ASSERT_EQUAL(ExitCause::Unknown, ei.cause());
    CPPUNIT_ASSERT_EQUAL(ExitCause::Unknown, eca);
    CPPUNIT_ASSERT_EQUAL(100, static_cast<int>(ei));

    ei = {ExitCode::Ok};
    ec = ei;
    eca = ei;
    CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, ei.code());
    CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, ec);
    CPPUNIT_ASSERT_EQUAL(ExitCause::Unknown, ei.cause());
    CPPUNIT_ASSERT_EQUAL(ExitCause::Unknown, eca);
    CPPUNIT_ASSERT_EQUAL(0, static_cast<int>(ei));

    ei = {ExitCode::NetworkError, ExitCause::DbAccessError};
    ec = ei;
    eca = ei;
    CPPUNIT_ASSERT_EQUAL(ExitCode::NetworkError, ei.code());
    CPPUNIT_ASSERT_EQUAL(ExitCode::NetworkError, ec);
    CPPUNIT_ASSERT_EQUAL(ExitCause::DbAccessError, ei.cause());
    CPPUNIT_ASSERT_EQUAL(ExitCause::DbAccessError, eca);
    CPPUNIT_ASSERT_EQUAL(202, static_cast<int>(ei));


    ec = ExitCode::BackError;
    ei = ec;
    CPPUNIT_ASSERT_EQUAL(ExitCode::BackError, ei.code());
    CPPUNIT_ASSERT_EQUAL(ExitCause::Unknown, ei.cause());
    CPPUNIT_ASSERT_EQUAL(600, static_cast<int>(ei));
}

/* Test that all values of an enum class can be converted to a string
 * Unfortunatly, this test cannot detect missing conversion for the last value of the enum class.
 * This test is only suitable for enum class with continuous values starting from 0.
 */
template<IntegralEnum T>
bool testToStringIntValues() {
    int validationLength = 10; // The test will succeed after 10 missing consecutive missing conversion
    int i = 0;
    while (validationLength != 0) {
        if (toString(fromInt<T>(i)) == noConversionStr) {
            validationLength--;
        } else if (validationLength < 10) {
            return false;
        }
        i++;
    }
    return true;
}

void TestTypes::testToString() {
    CPPUNIT_ASSERT(testToStringIntValues<AppType>());
    CPPUNIT_ASSERT(testToStringIntValues<SignalCategory>());
    CPPUNIT_ASSERT(testToStringIntValues<ReplicaSide>());
    CPPUNIT_ASSERT(testToStringIntValues<NodeType>());
    CPPUNIT_ASSERT(testToStringIntValues<ExitCode>());
    CPPUNIT_ASSERT(testToStringIntValues<ExitCause>());
    CPPUNIT_ASSERT(testToStringIntValues<ConflictType>());
    CPPUNIT_ASSERT(testToStringIntValues<ConflictTypeResolution>());
    CPPUNIT_ASSERT(testToStringIntValues<CancelType>());
    CPPUNIT_ASSERT(testToStringIntValues<NodeStatus>());
    CPPUNIT_ASSERT(testToStringIntValues<SyncStatus>());
    CPPUNIT_ASSERT(testToStringIntValues<UploadSessionType>());
    CPPUNIT_ASSERT(testToStringIntValues<SyncNodeType>());
    CPPUNIT_ASSERT(testToStringIntValues<SyncDirection>());
    CPPUNIT_ASSERT(testToStringIntValues<SyncFileStatus>());
    CPPUNIT_ASSERT(testToStringIntValues<SyncDirection>());
    CPPUNIT_ASSERT(testToStringIntValues<SyncFileInstruction>());
    CPPUNIT_ASSERT(testToStringIntValues<SyncStep>());
    CPPUNIT_ASSERT(testToStringIntValues<ActionType>());
    CPPUNIT_ASSERT(testToStringIntValues<ActionTarget>());
    CPPUNIT_ASSERT(testToStringIntValues<ErrorLevel>());
    CPPUNIT_ASSERT(testToStringIntValues<Language>());
    CPPUNIT_ASSERT(testToStringIntValues<LogLevel>());
    CPPUNIT_ASSERT(testToStringIntValues<NotificationsDisabled>());
    CPPUNIT_ASSERT(testToStringIntValues<VirtualFileMode>());
    CPPUNIT_ASSERT(testToStringIntValues<PinState>());
    CPPUNIT_ASSERT(testToStringIntValues<ProxyType>());
    CPPUNIT_ASSERT(testToStringIntValues<ExclusionTemplateComplexity>());
    CPPUNIT_ASSERT(testToStringIntValues<LinkType>());
    CPPUNIT_ASSERT(testToStringIntValues<IoError>());
    CPPUNIT_ASSERT(testToStringIntValues<AppStateKey>());
    CPPUNIT_ASSERT(testToStringIntValues<LogUploadState>());
    CPPUNIT_ASSERT(testToStringIntValues<UpdateState>());
    CPPUNIT_ASSERT(testToStringIntValues<VersionChannel>());
    CPPUNIT_ASSERT(testToStringIntValues<Platform>());
    CPPUNIT_ASSERT(testToStringIntValues<sentry::ConfidentialityLevel>());
}
} // namespace KDC

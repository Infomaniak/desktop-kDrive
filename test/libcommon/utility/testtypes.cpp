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
    CPPUNIT_ASSERT(CommonUtility::startsWith(toStringWithCode(fromInt<NodeType>(3)), noConversionStr));

    CPPUNIT_ASSERT_EQUAL(std::string("None(0)"), toStringWithCode(OperationType::None));
    CPPUNIT_ASSERT_EQUAL(std::string("Create(1)"), toStringWithCode(OperationType::Create));
    CPPUNIT_ASSERT_EQUAL(std::string("Move(2)"), toStringWithCode(OperationType::Move));
    CPPUNIT_ASSERT_EQUAL(std::string("Edit(4)"), toStringWithCode(OperationType::Edit));
    CPPUNIT_ASSERT_EQUAL(std::string("Delete(8)"), toStringWithCode(OperationType::Delete));
    CPPUNIT_ASSERT_EQUAL(std::string("Rights(16)"), toStringWithCode(OperationType::Rights));
    CPPUNIT_ASSERT(CommonUtility::startsWith(toStringWithCode(fromInt<OperationType>(0x09)), noConversionStr));
    CPPUNIT_ASSERT(CommonUtility::startsWith(toStringWithCode(OperationType::Create | OperationType::Delete), noConversionStr));

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

    // indexInList
    {
        long index =
                ExitInfo::indexInList(ExitCode::SystemError, {ExitCode::SystemError, ExitCode::DbError, ExitCode::BackError});
        CPPUNIT_ASSERT_EQUAL(0L, index);
    }

    {
        long index = ExitInfo::indexInList(ExitCode::BackError, {ExitCode::SystemError, ExitCode::DbError, ExitCode::BackError});
        CPPUNIT_ASSERT_EQUAL(2L, index);
    }

    {
        long index = ExitInfo::indexInList(ExitCode::DataError, {ExitCode::SystemError, ExitCode::DbError, ExitCode::BackError});
        CPPUNIT_ASSERT_EQUAL(3L, index);
    }

    // merge
    {
        ExitInfo exitInfo = ExitCode::Ok;
        ExitInfo exitInfoToMerge = ExitCode::SystemError;
        exitInfo.merge(exitInfoToMerge, {ExitCode::SystemError, ExitCode::DbError, ExitCode::BackError});
        CPPUNIT_ASSERT_EQUAL(ExitCode::SystemError, exitInfo.code());
    }

    {
        ExitInfo exitInfo = ExitCode::DataError;
        ExitInfo exitInfoToMerge = ExitCode::BackError;
        exitInfo.merge(exitInfoToMerge, {ExitCode::SystemError, ExitCode::DbError, ExitCode::BackError});
        CPPUNIT_ASSERT_EQUAL(ExitCode::BackError, exitInfo.code());
    }

    {
        ExitInfo exitInfo = ExitCode::DataError;
        ExitInfo exitInfoToMerge = ExitCode::NetworkError;
        exitInfo.merge(exitInfoToMerge, {ExitCode::SystemError, ExitCode::DbError, ExitCode::BackError});
        CPPUNIT_ASSERT_EQUAL(ExitCode::DataError, exitInfo.code());
    }

    {
        ExitInfo exitInfo = ExitCode::DbError;
        ExitInfo exitInfoToMerge = ExitCode::SystemError;
        exitInfo.merge(exitInfoToMerge, {ExitCode::SystemError, ExitCode::DbError, ExitCode::BackError});
        CPPUNIT_ASSERT_EQUAL(ExitCode::SystemError, exitInfo.code());
    }

    {
        ExitInfo exitInfo = ExitCode::DbError;
        ExitInfo exitInfoToMerge = ExitCode::BackError;
        exitInfo.merge(exitInfoToMerge, {ExitCode::SystemError, ExitCode::DbError, ExitCode::BackError});
        CPPUNIT_ASSERT_EQUAL(ExitCode::DbError, exitInfo.code());
    }

    {
        ExitInfo exitInfo = ExitCode::DbError;
        ExitInfo exitInfoToMerge = ExitCode::Ok;
        exitInfo.merge(exitInfoToMerge, {ExitCode::SystemError, ExitCode::DbError, ExitCode::BackError});
        CPPUNIT_ASSERT_EQUAL(ExitCode::DbError, exitInfo.code());
    }

    // Success code should remain 0 for readability, although comparing an enum to a hardcoded int is generally discouraged
    // (forbidden) It should only be compared with the result of ExitInfo::operator int(). The conversion to int is necessary for
    // use in switch statements.
    CPPUNIT_ASSERT_EQUAL(ExitInfo(ExitCode::Ok, ExitCause::Unknown), ExitInfo::fromInt(0));

    // Ensure the int conversion is consistent in both directions.
    CPPUNIT_ASSERT_EQUAL(ExitInfo(ExitCode::DataError, ExitCause::NotFound),
                         ExitInfo::fromInt(static_cast<int>(ExitInfo(ExitCode::DataError, ExitCause::NotFound))));

    // Because of the implementation of method ExitInfo::int(), we need to make sure that ExitCause enum never has more than 100
    // values.
    CPPUNIT_ASSERT(static_cast<int>(ExitCause::EnumEnd) < 100);
}

template<IntegralEnum T>
void testToStringIntValues() {
    int i = 0;
    while (true) {
        if (fromInt<T>(i) == T::EnumEnd) {
            break;
        }
        if (toString(fromInt<T>(i)) == noConversionStr || toString(fromInt<T>(i)) == "") {
            const std::string failStr = std::string("No string conversion for value ") + std::to_string(i) + std::string(" of ") +
                                        std::string(typeid(T).name());
            CPPUNIT_FAIL(failStr);
        }
        i++;
    }
}

void TestTypes::testToString() {
    testToStringIntValues<AppType>();
    testToStringIntValues<SignalCategory>();
    testToStringIntValues<ReplicaSide>();
    testToStringIntValues<NodeType>();
    testToStringIntValues<ExitCode>();
    testToStringIntValues<ExitCause>();
    testToStringIntValues<ConflictType>();
    testToStringIntValues<CancelType>();
    testToStringIntValues<NodeStatus>();
    testToStringIntValues<SyncStatus>();
    testToStringIntValues<UploadSessionType>();
    testToStringIntValues<SyncNodeType>();
    testToStringIntValues<SyncDirection>();
    testToStringIntValues<SyncFileStatus>();
    testToStringIntValues<SyncDirection>();
    testToStringIntValues<SyncFileInstruction>();
    testToStringIntValues<SyncStep>();
    testToStringIntValues<ActionType>();
    testToStringIntValues<ActionTarget>();
    testToStringIntValues<ErrorLevel>();
    testToStringIntValues<Language>();
    testToStringIntValues<LogLevel>();
    testToStringIntValues<NotificationsDisabled>();
    testToStringIntValues<VirtualFileMode>();
    testToStringIntValues<PinState>();
    testToStringIntValues<ProxyType>();
    testToStringIntValues<ExclusionTemplateComplexity>();
    testToStringIntValues<LinkType>();
    testToStringIntValues<IoError>();
    testToStringIntValues<AppStateKey>();
    testToStringIntValues<LogUploadState>();
    testToStringIntValues<UpdateState>();
    testToStringIntValues<VersionChannel>();
    testToStringIntValues<Platform>();
    testToStringIntValues<sentry::ConfidentialityLevel>();
}

} // namespace KDC

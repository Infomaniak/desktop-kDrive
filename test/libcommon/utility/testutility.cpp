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

namespace KDC {

void TestUtility::testGetAppSupportDir() {
    SyncPath appSupportDir = CommonUtility::getAppSupportDir();
    std::string faillureMessage = "Path: " + appSupportDir.string();
    CPPUNIT_ASSERT_MESSAGE(faillureMessage.c_str(), !appSupportDir.empty());
#ifdef _WIN32
    CPPUNIT_ASSERT_MESSAGE(faillureMessage.c_str(), appSupportDir.string().find("AppData") != std::string::npos);
    CPPUNIT_ASSERT_MESSAGE(faillureMessage.c_str(), appSupportDir.string().find("Local") != std::string::npos);
#elif defined(__APPLE__)
    CPPUNIT_ASSERT_MESSAGE(faillureMessage.c_str(), appSupportDir.string().find("Library") != std::string::npos);
    CPPUNIT_ASSERT_MESSAGE(faillureMessage.c_str(), appSupportDir.string().find("Application") != std::string::npos);
#else
    CPPUNIT_ASSERT_MESSAGE(faillureMessage.c_str(), appSupportDir.string().find(".config") != std::string::npos);
#endif
    CPPUNIT_ASSERT_MESSAGE(faillureMessage.c_str(), appSupportDir.string().find(APPLICATION_NAME) != std::wstring::npos);
}
void TestUtility::testIsVersionLower() {
    CPPUNIT_ASSERT(!CommonUtility::isVersionLower("3.5.8", "3.5.8"));

    CPPUNIT_ASSERT(CommonUtility::isVersionLower("3.5.8", "3.5.9"));
    CPPUNIT_ASSERT(!CommonUtility::isVersionLower("3.5.8", "3.5.7"));

    CPPUNIT_ASSERT(CommonUtility::isVersionLower("3.5.8", "3.6.7"));
    CPPUNIT_ASSERT(CommonUtility::isVersionLower("3.5.8", "3.6.9"));
    CPPUNIT_ASSERT(!CommonUtility::isVersionLower("3.5.8", "3.4.7"));
    CPPUNIT_ASSERT(!CommonUtility::isVersionLower("3.5.8", "3.4.9"));

    CPPUNIT_ASSERT(CommonUtility::isVersionLower("3.5.8", "4.4.7"));
    CPPUNIT_ASSERT(CommonUtility::isVersionLower("3.5.8", "4.4.9"));
    CPPUNIT_ASSERT(CommonUtility::isVersionLower("3.5.8", "4.5.7"));
    CPPUNIT_ASSERT(CommonUtility::isVersionLower("3.5.8", "4.5.9"));
    CPPUNIT_ASSERT(CommonUtility::isVersionLower("3.5.8", "4.6.7"));
    CPPUNIT_ASSERT(CommonUtility::isVersionLower("3.5.8", "4.6.9"));
    CPPUNIT_ASSERT(!CommonUtility::isVersionLower("3.5.8", "2.4.7"));
    CPPUNIT_ASSERT(!CommonUtility::isVersionLower("3.5.8", "2.4.9"));
    CPPUNIT_ASSERT(!CommonUtility::isVersionLower("3.5.8", "2.5.7"));
    CPPUNIT_ASSERT(!CommonUtility::isVersionLower("3.5.8", "2.5.9"));
    CPPUNIT_ASSERT(!CommonUtility::isVersionLower("3.5.8", "2.6.7"));
    CPPUNIT_ASSERT(!CommonUtility::isVersionLower("3.5.8", "2.6.9"));
}
void TestUtility::testStringToAppStateValue() {
    //Normal conditions
    AppStateValue value = std::string();
    CPPUNIT_ASSERT(CommonUtility::stringToAppStateValue("test", value));
    CPPUNIT_ASSERT(std::holds_alternative<std::string>(value));
    CPPUNIT_ASSERT_EQUAL(std::string("test"), std::get<std::string>(value));

    value = int(0);
    CPPUNIT_ASSERT(CommonUtility::stringToAppStateValue("50", value));
    CPPUNIT_ASSERT(std::holds_alternative<int>(value));
    CPPUNIT_ASSERT_EQUAL(50, std ::get<int>(value));

    value = LogUploadState::None;
    CPPUNIT_ASSERT(CommonUtility::stringToAppStateValue("1", value));
    CPPUNIT_ASSERT(std::holds_alternative<LogUploadState>(value));
    CPPUNIT_ASSERT_EQUAL(LogUploadState::Archiving, std::get<LogUploadState>(value));

    value = int64_t(0);
    CPPUNIT_ASSERT(CommonUtility::stringToAppStateValue("50", value));
    CPPUNIT_ASSERT(std::holds_alternative<int64_t>(value));
    CPPUNIT_ASSERT_EQUAL(int64_t(50), std::get<int64_t>(value));

    //Invalid input
    value = int(0);
    CPPUNIT_ASSERT(!CommonUtility::stringToAppStateValue("test", value));

    value = LogUploadState::None;
    CPPUNIT_ASSERT(!CommonUtility::stringToAppStateValue("test", value));

    value = int64_t(0);
    CPPUNIT_ASSERT(!CommonUtility::stringToAppStateValue("test", value));

    //Empty input
    value = int(0);
    CPPUNIT_ASSERT(!CommonUtility::stringToAppStateValue("", value));
}
}  // namespace KDC
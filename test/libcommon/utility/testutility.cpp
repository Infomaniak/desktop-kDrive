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
    CPPUNIT_ASSERT(!appSupportDir.empty());
#ifdef _WIN32
    CPPUNIT_ASSERT(appSupportDir.string().find("AppData") != std::string::npos);
    CPPUNIT_ASSERT(appSupportDir.string().find("Local") != std::string::npos);
#elif defined(__APPLE__)
    CPPUNIT_ASSERT(appSupportDir.string().find(".config") != std::string::npos);
#else
    CPPUNIT_ASSERT(appSupportDir.string().find("Library") != std::string::npos);
    CPPUNIT_ASSERT(appSupportDir.string().find("Application") != std::string::npos);
#endif
    CPPUNIT_ASSERT(appSupportDir.string().find(APPLICATION_NAME) != std::wstring::npos);
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
}  // namespace KDC
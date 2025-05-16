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

#include "testio.h"

#include <windows.h>

#include <regex>

using namespace CppUnit;

namespace KDC {

void TestIo::testGetLongPathName() {
    // The long path name of a long path coincides with the input long path
    const LocalTemporaryDirectory temporaryDirectory;
    SyncPath longPathName;
    auto ioError = IoError::Success;
    _testObj->getLongPathName(temporaryDirectory.path(), longPathName, ioError);
    CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
    CPPUNIT_ASSERT_EQUAL(temporaryDirectory.path(), longPathName);

    // The short path name of a long path is shorter and end with ~ followed by a positive integer
    SyncPath shortPathName;
    _testObj->getShortPathName(temporaryDirectory.path(), shortPathName, ioError);
    CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
    CPPUNIT_ASSERT_LESS(Path2WStr(longPathName).size(), Path2WStr(shortPathName).size());
    CPPUNIT_ASSERT(std::regex_match(Path2WStr(shortPathName), std::wregex(L".*~[1-9][0-9]*$")));

    // Check that getLongPathName reverts getShortPathName
    _testObj->getLongPathName(shortPathName, longPathName, ioError);
    CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
    CPPUNIT_ASSERT_EQUAL(temporaryDirectory.path(), longPathName);
}
} // namespace KDC

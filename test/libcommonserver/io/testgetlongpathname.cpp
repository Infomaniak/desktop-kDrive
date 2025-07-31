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

#include "libcommon/utility/utility.h" // Path2Str

#include <windows.h>

#include <regex>

using namespace CppUnit;

namespace KDC {


namespace {

// https://stackoverflow.com/a/35717/4675396
LONG GetDWORDRegKey(const HKEY hKey, const std::wstring &stringValueName, DWORD &numericValue, const DWORD numericDefaultValue) {
    numericValue = numericDefaultValue;
    DWORD dwBufferSize(sizeof(DWORD));
    DWORD numericResult(0);
    LONG numericError = ::RegQueryValueExW(hKey, stringValueName.c_str(), nullptr, nullptr,
                                           reinterpret_cast<LPBYTE>(&numericResult), &dwBufferSize);
    if (ERROR_SUCCESS == numericError) {
        numericValue = numericResult;
    }

    return numericError;
}

// Check whether the creation of 8dot3 names is activated on every volume (global registry value).
// Note: If "NtfsDisable8dot3NameCreation" is set with 2 (default), it is possible that the creation of 8dot3 names
// is enabled but this will not be detected by this function.
bool areShortNamesEnabled() {
    static const std::wstring regSubKey{L"SYSTEM\\CurrentControlSet\\Control\\FileSystem"};
    static const std::wstring regValue{L"NtfsDisable8dot3NameCreation"};

    HKEY hKey{0};
    (void) RegOpenKeyExW(HKEY_LOCAL_MACHINE, regSubKey.c_str(), 0, KEY_READ, &hKey);

    DWORD numericValue{3};
    const DWORD defaultValue{3};
    (void) GetDWORDRegKey(hKey, regValue, numericValue, defaultValue);

    return numericValue == 0;
}

} // namespace

void TestIo::testGetLongPathName() {
    if (!areShortNamesEnabled()) {
        std::cout << " (Skipped as short names are disabled) ";
        return;
    };

    // The input path length of getLongPath exceeds the system requirements: error
    {
        const SyncPath veryLongPath = makeVeryLonPath("root");
        SyncPath longPathName{"anomalous_input_path"};
        auto ioError = IoError::Success;
        CPPUNIT_ASSERT(!IoHelper::getLongPathName(veryLongPath, longPathName, ioError));
        CPPUNIT_ASSERT_EQUAL(IoError::FileNameTooLong, ioError);
        CPPUNIT_ASSERT(longPathName.empty());
    }

    // The input path indicates a non-existing file
    {
        SyncPath longPathName{"anomalous_input_path"};
        auto ioError = IoError::Success;
        CPPUNIT_ASSERT(!IoHelper::getLongPathName("/root/directory/non-existing-text-file.txt", longPathName, ioError));
        CPPUNIT_ASSERT_EQUAL(IoError::NoSuchFileOrDirectory, ioError);
        CPPUNIT_ASSERT(longPathName.empty());
    }

    // The input path indicates an existing file
    {
        // The long path name of a long path coincides with the input long path
        const LocalTemporaryDirectory temporaryDirectory;
        SyncPath longPathName;
        auto ioError = IoError::Success;
        const SyncPath inputPath = temporaryDirectory.path() / "a_file_name_with_more_than_8_characters.txt";
        { std::ofstream ofs(inputPath); }

        CPPUNIT_ASSERT(IoHelper::getLongPathName(inputPath, longPathName, ioError));
        CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
        CPPUNIT_ASSERT_EQUAL(inputPath, longPathName);

        // The short path name of a long path is shorter and end with ~ followed by a positive integer
        SyncPath shortPathName;
        CPPUNIT_ASSERT(IoHelper::getShortPathName(inputPath, shortPathName, ioError));
        CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);

        CPPUNIT_ASSERT_MESSAGE("Short and long names coincide: " + Path2Str(longPathName), longPathName != shortPathName);
        CPPUNIT_ASSERT_LESS(Path2WStr(longPathName).size(), Path2WStr(shortPathName).size());
        CPPUNIT_ASSERT(std::regex_match(Path2WStr(shortPathName), std::wregex(L".*~[1-9][0-9]*\\.TXT$")));

        // Check that getLongPathName reverts getShortPathName
        CPPUNIT_ASSERT(IoHelper::getLongPathName(shortPathName, longPathName, ioError));
        CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
        CPPUNIT_ASSERT_EQUAL(inputPath, longPathName);
    }

    // The input path indicates an existing file and contains emojis
    {
        // The long path name of a long path coincides with the input long path
        const LocalTemporaryDirectory temporaryDirectory;
        SyncPath longPathName;
        auto ioError = IoError::Success;

        const SyncPath inputPath = temporaryDirectory.path() / makeFileNameWithEmojis();
        { std::ofstream ofs(inputPath); }
        CPPUNIT_ASSERT(IoHelper::getLongPathName(inputPath, longPathName, ioError));
        CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
        CPPUNIT_ASSERT_EQUAL(inputPath, longPathName);

        // The short path name of a long path is shorter and end with ~ followed by a positive integer
        SyncPath shortPathName;
        CPPUNIT_ASSERT(IoHelper::getShortPathName(longPathName, shortPathName, ioError));
        CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
        CPPUNIT_ASSERT_LESS(Path2WStr(longPathName).size(), Path2WStr(shortPathName).size());
        CPPUNIT_ASSERT(std::regex_match(Path2WStr(shortPathName), std::wregex(L".*~[1-9][0-9]*$")));

        // Check that getLongPathName reverts getShortPathName
        CPPUNIT_ASSERT(IoHelper::getLongPathName(shortPathName, longPathName, ioError));
        CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
        CPPUNIT_ASSERT_EQUAL(inputPath, longPathName);
    }
}

void TestIo::testGetShortPathName() {
    if (!areShortNamesEnabled()) {
        std::cout << " (Skipped as short names are disabled) ";
        return;
    };

    // The input path length of getShorPath exceeds the system requirements: error
    {
        const SyncPath veryLongPath = makeVeryLonPath("root");
        SyncPath shortPathName{"anomalous_input_path"};
        auto ioError = IoError::Success;
        CPPUNIT_ASSERT(!IoHelper::getShortPathName(veryLongPath, shortPathName, ioError));
        CPPUNIT_ASSERT_EQUAL(IoError::FileNameTooLong, ioError);
        CPPUNIT_ASSERT(shortPathName.empty());
    }

    // The input path indicates a non-existing file
    {
        SyncPath shortPathName{"anomalous_input_path"};
        auto ioError = IoError::Success;
        CPPUNIT_ASSERT(!IoHelper::getShortPathName("/root/directory/non-existing-text-file.txt", shortPathName, ioError));
        CPPUNIT_ASSERT_EQUAL(IoError::NoSuchFileOrDirectory, ioError);
        CPPUNIT_ASSERT(shortPathName.empty());
    }
}
} // namespace KDC

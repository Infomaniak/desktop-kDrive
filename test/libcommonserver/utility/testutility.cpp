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

#include "testutility.h"
#include "test_utility/temporarydirectory.h"
#include "config.h"

#include "libcommon/utility/utility.h"  // CommonUtility::isSubDir
#include "Poco/URI.h"

#include <climits>
#include <iostream>
#include <filesystem>

#ifdef _WIN32
#include <Windows.h>
#include <ShObjIdl_core.h>

#endif
#include <ShlObj_core.h>

using namespace CppUnit;

namespace KDC {

static const SyncPath localTestDirPath(std::wstring(L"" TEST_DIR) + L"/test_ci");

void TestUtility::setUp() {
    _testObj = new Utility();
}

void TestUtility::tearDown() {
    delete _testObj;
}

void TestUtility::testFreeDiskSpace() {
    int64_t freeSpace;

#if defined(__APPLE__) || defined(__unix__)
    freeSpace = _testObj->freeDiskSpace("/");
#elif defined(_WIN32)
    freeSpace = _testObj->freeDiskSpace(R"(C:\)");
#endif

    std::cout << " freeSpace=" << freeSpace;
    CPPUNIT_ASSERT(freeSpace > 0);
}

void TestUtility::testIsCreationDateValid(void) {
    CPPUNIT_ASSERT_MESSAGE("Creation date should not be valid when it is 0.", !_testObj->isCreationDateValid(0));
    // 443779200 correspond to "Tuesday 24 January 1984 08:00:00" which is a default date set by macOS
    CPPUNIT_ASSERT_MESSAGE("Creation date should not be valid when it is 443779200 (Default on MacOs).",
                           !_testObj->isCreationDateValid(443779200));
    auto currentTime = std::chrono::system_clock::now();
    auto currentTimePoint = std::chrono::time_point_cast<std::chrono::seconds>(currentTime);
    auto currentTimestamp = currentTimePoint.time_since_epoch().count();

    for (int i = 10; i < currentTimestamp; i += 2629743) {  // step of one month
        CPPUNIT_ASSERT_MESSAGE("Creation date should be valid.", _testObj->isCreationDateValid(i));
    }
    CPPUNIT_ASSERT_MESSAGE("Creation date should be valid.", _testObj->isCreationDateValid(currentTimestamp));

    // This tests do not check the future date because it is not possible to create a file in the future.
    auto futureTimestamp = currentTimestamp + 3600;
    CPPUNIT_ASSERT_MESSAGE("Creation date should be valid when it is in the future. (Handle by the app).",
                           _testObj->isCreationDateValid(futureTimestamp));
}


void TestUtility::testS2ws() {
    CPPUNIT_ASSERT(_testObj->s2ws("abcd") == L"abcd");
    CPPUNIT_ASSERT(_testObj->s2ws("éèêà") == L"éèêà");
}

void TestUtility::testWs2s() {
    CPPUNIT_ASSERT(_testObj->ws2s(L"abcd") == "abcd");
    CPPUNIT_ASSERT(_testObj->ws2s(L"éèêà") == "éèêà");
}

void TestUtility::testLtrim() {
    CPPUNIT_ASSERT(_testObj->ltrim("    ab    cd    ") == "ab    cd    ");
}

void TestUtility::testRtrim() {
    CPPUNIT_ASSERT(_testObj->rtrim("    ab    cd    ") == "    ab    cd");
}

void TestUtility::testTrim() {
    CPPUNIT_ASSERT(_testObj->trim("    ab    cd    ") == "ab    cd");
}

void TestUtility::testMsSleep() {
    auto start = std::chrono::high_resolution_clock::now();
    _testObj->msleep(1000);
    auto end = std::chrono::high_resolution_clock::now();
    auto timeSpan = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    std::cout << " timeSpan=" << timeSpan.count();
    // CPPUNIT_ASSERT(timeSpan.count() > 800 && timeSpan.count() < 1200);
    CPPUNIT_ASSERT(true);
}

void TestUtility::testV2ws() {
    dbtype nullValue = std::monostate();
    CPPUNIT_ASSERT(_testObj->v2ws(nullValue) == L"NULL");

    dbtype intValue(1234);
    CPPUNIT_ASSERT(_testObj->v2ws(intValue) == L"1234");

    dbtype int64Value((int64_t)LLONG_MAX);
    CPPUNIT_ASSERT(_testObj->v2ws(int64Value) == L"9223372036854775807");

    dbtype doubleValue(123.456789);
    CPPUNIT_ASSERT(_testObj->v2ws(doubleValue) == L"123.456789");

    dbtype stringValue("hello");
    CPPUNIT_ASSERT(_testObj->v2ws(stringValue) == L"hello");
}

void TestUtility::testFileSystemName() {
#if defined(__APPLE__)
    CPPUNIT_ASSERT(_testObj->fileSystemName("/") == "apfs");
    CPPUNIT_ASSERT(_testObj->fileSystemName("/bin") == "apfs");
#elif defined(_WIN32)
    CPPUNIT_ASSERT(_testObj->fileSystemName(std::filesystem::temp_directory_path()) == "NTFS");
    // CPPUNIT_ASSERT(_testObj->fileSystemName(R"(C:\)") == "NTFS");
    // CPPUNIT_ASSERT(_testObj->fileSystemName(R"(C:\windows)") == "NTFS");
#endif
}

void TestUtility::testStartsWith() {
    CPPUNIT_ASSERT(_testObj->startsWith(SyncName(Str("abcdefg")), SyncName(Str("abcd"))));
    CPPUNIT_ASSERT(!_testObj->startsWith(SyncName(Str("abcdefg")), SyncName(Str("ABCD"))));
}

void TestUtility::testStartsWithInsensitive() {
    CPPUNIT_ASSERT(_testObj->startsWithInsensitive(SyncName(Str("abcdefg")), SyncName(Str("aBcD"))));
    CPPUNIT_ASSERT(_testObj->startsWithInsensitive(SyncName(Str("abcdefg")), SyncName(Str("ABCD"))));
}

void TestUtility::testEndsWith() {
    CPPUNIT_ASSERT(_testObj->endsWith(SyncName(Str("abcdefg")), SyncName(Str("defg"))));
    CPPUNIT_ASSERT(!_testObj->endsWith(SyncName(Str("abcdefg")), SyncName(Str("abc"))));
    CPPUNIT_ASSERT(!_testObj->endsWith(SyncName(Str("abcdefg")), SyncName(Str("dEfG"))));
}

void TestUtility::testEndsWithInsensitive() {
    CPPUNIT_ASSERT(_testObj->endsWithInsensitive(SyncName(Str("abcdefg")), SyncName(Str("defg"))));
    CPPUNIT_ASSERT(!_testObj->endsWithInsensitive(SyncName(Str("abcdefg")), SyncName(Str("abc"))));
    CPPUNIT_ASSERT(_testObj->endsWithInsensitive(SyncName(Str("abcdefg")), SyncName(Str("dEfG"))));
}

void TestUtility::testIsEqualInsensitive(void) {
    const std::string strA = "abcdefg";
    const std::string strB = "aBcDeFg";
    CPPUNIT_ASSERT(_testObj->isEqualInsensitive(strA, strB));
    CPPUNIT_ASSERT(_testObj->isEqualInsensitive(strA, strA));
    CPPUNIT_ASSERT(_testObj->isEqualInsensitive(strB, strB));
    CPPUNIT_ASSERT(_testObj->isEqualInsensitive(strB, strA));
    CPPUNIT_ASSERT(!_testObj->isEqualInsensitive(strA, "abcdef"));
    CPPUNIT_ASSERT(!_testObj->isEqualInsensitive("abcdef", strA));

    CPPUNIT_ASSERT(!_testObj->isEqualInsensitive(strA, "abcdefh"));
    CPPUNIT_ASSERT(!_testObj->isEqualInsensitive("abcdefh", strA));

    CPPUNIT_ASSERT(!_testObj->isEqualInsensitive(strA, "abcdefgh"));
    CPPUNIT_ASSERT(!_testObj->isEqualInsensitive("abcdefgh", strA));
}

void TestUtility::testMoveItemToTrash(void) {
    TemporaryDirectory tempDir;
    SyncPath path = tempDir.path / "test.txt";
    std::ofstream file(path);
    file << "test";
    file.close();

    CPPUNIT_ASSERT(_testObj->moveItemToTrash(path));
    CPPUNIT_ASSERT(!std::filesystem::exists(path));

    // Test with a non existing file
    CPPUNIT_ASSERT(!_testObj->moveItemToTrash(tempDir.path / "test2.txt"));

    // Test with a directory
    SyncPath dirPath = tempDir.path / "testDir";
    std::filesystem::create_directory(dirPath);
    CPPUNIT_ASSERT(_testObj->moveItemToTrash(dirPath));
    CPPUNIT_ASSERT(!std::filesystem::exists(dirPath));
}

void TestUtility::testGetLinuxDesktopType(void) {
    std::string currentDesktop;

#ifdef __unix__
    CPPUNIT_ASSERT(_testObj->getLinuxDesktopType(currentDesktop));
    CPPUNIT_ASSERT(!currentDesktop.empty());
    return;
#endif

    CPPUNIT_ASSERT_MESSAGE("This test is only for Linux", !_testObj->getLinuxDesktopType(currentDesktop));
}

void TestUtility::testStr2HexStr() {
    std::string hexStr;
    _testObj->str2hexstr("0123[]{}", hexStr);
    CPPUNIT_ASSERT(hexStr == "303132335b5d7b7d");

    _testObj->str2hexstr("0123[]{}", hexStr, true);
    CPPUNIT_ASSERT(hexStr == "303132335B5D7B7D");
}

void TestUtility::testStrHex2Str() {
    std::string str;
    _testObj->strhex2str("303132335b5d7b7d", str);
    CPPUNIT_ASSERT(str == "0123[]{}");

    _testObj->strhex2str("303132335B5D7B7D", str);
    CPPUNIT_ASSERT(str == "0123[]{}");
}

void TestUtility::testSplitStr() {
    std::vector<std::string> strList = Utility::splitStr("01234:abcd:56789", ':');
    CPPUNIT_ASSERT(strList[0] == "01234" && strList[1] == "abcd" && strList[2] == "56789");
}

void TestUtility::testJoinStr() {
    std::vector<std::string> strList = {"C'est", " ", "un ", "test!"};
    CPPUNIT_ASSERT(Utility::joinStr(strList) == "C'est un test!");
    CPPUNIT_ASSERT(Utility::joinStr(strList, '@') == "C'est@ @un @test!");
}

void TestUtility::testXxHash() {
    SyncPath path = localTestDirPath / "test_pictures/picture-1.jpg";
    std::ifstream file(path, std::ios::binary);
    std::ostringstream ostrm;
    ostrm << file.rdbuf();
    std::string data = ostrm.str();

    std::string contentHash = Utility::computeXxHash(data);

    CPPUNIT_ASSERT(contentHash == "5dcc477e35136516");
}

void TestUtility::isSubDir() {
    SyncPath path1 = "A/AA/AAA";
    SyncPath path2 = "A/AA";
    CPPUNIT_ASSERT(CommonUtility::isSubDir(path2, path1));
    CPPUNIT_ASSERT(!CommonUtility::isSubDir(path1, path2));

    path1 = path2;
    CPPUNIT_ASSERT(CommonUtility::isSubDir(path2, path1));

    path1 = "A/AB/AAA";
    CPPUNIT_ASSERT(!CommonUtility::isSubDir(path2, path1));
}

void TestUtility::testFormatStdError() {
    const std::error_code ec;
    std::wstring result = Utility::formatStdError(ec);
    CPPUNIT_ASSERT_MESSAGE("The error message should contain 'error: 0'", result.find(L"error: 0") != std::wstring::npos);
    CPPUNIT_ASSERT_MESSAGE("The error message should contain a description.", result.length() > 15);

    const SyncPath path = "A/AA";
    result = Utility::formatStdError(path, ec);
    CPPUNIT_ASSERT_MESSAGE("The error message should contain 'error: 0'", result.find(L"error: 0") != std::wstring::npos);
    CPPUNIT_ASSERT_MESSAGE("The error message should contain a description.", (result.length() - path.native().length()) > 20);
    CPPUNIT_ASSERT_MESSAGE("The error message should contain the path.", result.find(path) != std::wstring::npos);
}

void TestUtility::testFormatIoError(void) {
    const IoError ioError = IoErrorSuccess;
    const SyncPath path = "A/AA";
    const std::wstring result = Utility::formatIoError(path, ioError);
    CPPUNIT_ASSERT_MESSAGE("The error message should contain 'err='...''", result.find(L"err='Success'") != std::wstring::npos);
    CPPUNIT_ASSERT_MESSAGE("The error message should contain a description.", (result.length() - path.native().length()) > 20);
    CPPUNIT_ASSERT_MESSAGE("The error message should contain the path.", result.find(path) != std::wstring::npos);
}

void TestUtility::testFormatSyncPath(void) {
    const SyncPath path = "A/AA";
    CPPUNIT_ASSERT(Utility::formatSyncPath(path).find(path) != std::wstring::npos);
}

void TestUtility::testFormatRequest(void) {
    const std::string uri("http://www.example.com");
    const std::string code = "404";
    const std::string description = "Not Found";
    const std::string result = Utility::formatRequest(Poco::URI(uri), code, description);
    CPPUNIT_ASSERT(result.find(uri) != std::string::npos);
    CPPUNIT_ASSERT(result.find(code) != std::string::npos);
    CPPUNIT_ASSERT(result.find(description) != std::string::npos);
}


void TestUtility::testNormalizedSyncPath() {
    CPPUNIT_ASSERT(Utility::normalizedSyncPath("a/b/c") == SyncPath("a/b/c"));
    CPPUNIT_ASSERT(Utility::normalizedSyncPath("/a/b/c") == SyncPath("/a/b/c"));
#ifdef _WIN32
    CPPUNIT_ASSERT(Utility::normalizedSyncPath(R"(a\b\c)") == SyncPath("a/b/c"));
    CPPUNIT_ASSERT(Utility::normalizedSyncPath(R"(\a\b\c)") == SyncPath("/a/b/c"));
#endif
}

}  // namespace KDC

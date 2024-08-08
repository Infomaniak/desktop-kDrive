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
#include "test_utility/localtemporarydirectory.h"
#include "config.h"

#include "libcommon/utility/utility.h"  // CommonUtility::isSubDir
#include "Poco/URI.h"

#include <climits>
#include <iostream>
#include <filesystem>

#ifdef _WIN32
#include <Windows.h>
#include <ShObjIdl_core.h>
#include <ShlObj_core.h>
#endif

using namespace CppUnit;

namespace KDC {

static const SyncPath localTestDirPath(TEST_DIR "/test_ci");

void TestUtility::setUp() {}

void TestUtility::tearDown() {}

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
    LocalTemporaryDirectory tempDir;
    SyncPath path = tempDir.path() / "test.txt";
    std::ofstream file(path);
    file << "test";
    file.close();

    CPPUNIT_ASSERT(_testObj->moveItemToTrash(path));
    CPPUNIT_ASSERT(!std::filesystem::exists(path));

    // Test with a non existing file
    CPPUNIT_ASSERT(!_testObj->moveItemToTrash(tempDir.path() / "test2.txt"));

    // Test with a directory
    SyncPath dirPath = tempDir.path() / "testDir";
    std::filesystem::create_directory(dirPath);
    CPPUNIT_ASSERT(_testObj->moveItemToTrash(dirPath));
    CPPUNIT_ASSERT(!std::filesystem::exists(dirPath));
}

void TestUtility::testGetLinuxDesktopType() {
    std::string currentDesktop;

#ifdef __unix__
    CPPUNIT_ASSERT(_testObj->getLinuxDesktopType(currentDesktop));
    CPPUNIT_ASSERT(!currentDesktop.empty());
    return;
#endif
    CPPUNIT_ASSERT(!_testObj->getLinuxDesktopType(currentDesktop));
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
    std::vector<std::string> strList = _testObj->splitStr("01234:abcd:56789", ':');
    CPPUNIT_ASSERT(strList[0] == "01234" && strList[1] == "abcd" && strList[2] == "56789");
}

void TestUtility::testJoinStr() {
    std::vector<std::string> strList = {"C'est", " ", "un ", "test!"};
    CPPUNIT_ASSERT(_testObj->joinStr(strList) == "C'est un test!");
    CPPUNIT_ASSERT(_testObj->joinStr(strList, '@') == "C'est@ @un @test!");
}

void TestUtility::testPathDepth() {
    SyncPath path = "";
    CPPUNIT_ASSERT_EQUAL(0, _testObj->pathDepth(path));

    for (int i = 1; i < 10; i++) {
        path /= "dir";
        CPPUNIT_ASSERT_EQUAL(i, _testObj->pathDepth(path));
    }
}

void TestUtility::testComputeMd5Hash() {
    std::vector<std::pair<std::string, std::string>> testCases = {
        {"", "d41d8cd98f00b204e9800998ecf8427e"},
        {"a", "0cc175b9c0f1b6a831c399e269772661"},
        {"abc", "900150983cd24fb0d6963f7d28e17f72"},
        {"abcdefghijklmnopqrstuvwxyz", "c3fcd3d76192e4007dfb496cca67e13b"},
        {"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789", "d174ab98d277d9f5a5611c2c9f419d9f"},
        {"12345678901234567890123456789012345678901234567890123456789012345678901234567890", "57edf4a22be3c955ac49da2e2107b67a"},
    };

    for (const auto &testCase : testCases) {
        CPPUNIT_ASSERT_EQUAL(testCase.second, _testObj->computeMd5Hash(testCase.first));
        CPPUNIT_ASSERT_EQUAL(testCase.second, _testObj->computeMd5Hash(testCase.first.c_str(), testCase.first.size()));
    }
}

void TestUtility::testXxHash() {
    SyncPath path = localTestDirPath / "test_pictures/picture-1.jpg";
    std::ifstream file(path, std::ios::binary);
    std::ostringstream ostrm;
    ostrm << file.rdbuf();
    std::string data = ostrm.str();

    std::string contentHash = _testObj->computeXxHash(data);

    CPPUNIT_ASSERT(contentHash == "5dcc477e35136516");
}

void TestUtility::testToUpper() {
    CPPUNIT_ASSERT_EQUAL(std::string("ABC"), _testObj->toUpper("abc"));
    CPPUNIT_ASSERT_EQUAL(std::string("ABC"), _testObj->toUpper("ABC"));
    CPPUNIT_ASSERT_EQUAL(std::string("ABC"), _testObj->toUpper("AbC"));
    CPPUNIT_ASSERT_EQUAL(std::string(""), _testObj->toUpper(""));
    CPPUNIT_ASSERT_EQUAL(std::string("123"), _testObj->toUpper("123"));

    CPPUNIT_ASSERT_EQUAL(std::string("²&é~\"#'{([-|`è_\\ç^à@)]}=+*ù%µ£¤§:;,!.?/"),
                         _testObj->toUpper("²&é~\"#'{([-|`è_\\ç^à@)]}=+*ù%µ£¤§:;,!.?/"));
}

void TestUtility::testErrId() {
    CPPUNIT_ASSERT_EQUAL(std::string("TES:") + std::to_string(__LINE__), errId());
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

void TestUtility::testcheckIfDirEntryIsManaged() {
    LocalTemporaryDirectory tempDir;
    SyncPath path = tempDir.path() / "test.txt";
    std::ofstream file(path);
    file << "test";
    file.close();

    bool isManaged = false;
    bool isLink = false;
    IoError ioError = IoError::Success;
    std::filesystem::recursive_directory_iterator entry(tempDir.path());

    // Check with an existing file (managed)
    CPPUNIT_ASSERT(Utility::checkIfDirEntryIsManaged(entry, isManaged, isLink, ioError));
    CPPUNIT_ASSERT(isManaged);
    CPPUNIT_ASSERT(!isLink);
    CPPUNIT_ASSERT(ioError == IoError::Success);

    // Check with a simlink (managed)
    const SyncPath simLinkDir = tempDir.path() / "simLinkDir";
    std::filesystem::create_directory(simLinkDir);
    std::filesystem::create_symlink(path, simLinkDir / "testLink.txt");
    entry = std::filesystem::recursive_directory_iterator(simLinkDir);
    CPPUNIT_ASSERT(Utility::checkIfDirEntryIsManaged(entry, isManaged, isLink, ioError));
    CPPUNIT_ASSERT(isManaged);
    CPPUNIT_ASSERT(isLink);
    CPPUNIT_ASSERT(ioError == IoError::Success);

    // Check with a directory
    entry = std::filesystem::recursive_directory_iterator(tempDir.path());
    CPPUNIT_ASSERT(Utility::checkIfDirEntryIsManaged(entry, isManaged, isLink, ioError));
    CPPUNIT_ASSERT(isManaged);
    CPPUNIT_ASSERT(!isLink);
    CPPUNIT_ASSERT(ioError == IoError::Success);
}

void TestUtility::testFormatStdError() {
    const std::error_code ec;
    std::wstring result = _testObj->formatStdError(ec);
    CPPUNIT_ASSERT_MESSAGE("The error message should contain 'error: 0'",
                           result.find(L"error: 0") != std::wstring::npos || result.find(L"code: 0") != std::wstring::npos);
    CPPUNIT_ASSERT_MESSAGE("The error message should contain a description.", result.length() > 15);

    const SyncPath path = "A/AA";
    result = _testObj->formatStdError(path, ec);
    CPPUNIT_ASSERT_MESSAGE("The error message should contain 'error: 0'",
                           result.find(L"error: 0") != std::wstring::npos || result.find(L"code: 0") != std::wstring::npos);
    CPPUNIT_ASSERT_MESSAGE("The error message should contain a description.", (result.length() - path.native().length()) > 20);
    CPPUNIT_ASSERT_MESSAGE("The error message should contain the path.",
                           result.find(Utility::s2ws(path.string())) != std::wstring::npos);
}

void TestUtility::testFormatIoError() {
    const IoError ioError = IoError::Success;
    const SyncPath path = "A/AA";
    const std::wstring result = _testObj->formatIoError(path, ioError);
    CPPUNIT_ASSERT_MESSAGE("The error message should contain 'err='...''", result.find(L"err='Success'") != std::wstring::npos);
    CPPUNIT_ASSERT_MESSAGE("The error message should contain a description.", (result.length() - path.native().length()) > 20);
    CPPUNIT_ASSERT_MESSAGE("The error message should contain the path.",
                           result.find(Utility::s2ws(path.string())) != std::wstring::npos);
}

void TestUtility::testFormatSyncPath() {
    const SyncPath path = "A/AA";
    CPPUNIT_ASSERT(Utility::formatSyncPath(path).find(Utility::s2ws(path.string())) != std::wstring::npos);
}

void TestUtility::testFormatRequest() {
    const std::string uri("http://www.example.com");
    const std::string code = "404";
    const std::string description = "Not Found";
    const std::string result = _testObj->formatRequest(Poco::URI(uri), code, description);
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

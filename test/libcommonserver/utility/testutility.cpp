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

#include "testutility.h"
#include "test_utility/localtemporarydirectory.h"
#include "test_utility/testhelpers.h"
#include "config.h"
#include "libcommon/utility/utility.h" // CommonUtility::isSubDir
#include "libcommonserver/log/log.h"

#include <Poco/URI.h>

#include <climits>
#include <iostream>
#include <filesystem>

#if defined(KD_WINDOWS)
#include <Windows.h>
#include <ShObjIdl_core.h>
#include <ShlObj_core.h>
#endif

using namespace CppUnit;

namespace KDC {

void TestUtility::testFreeDiskSpace() {
    int64_t freeSpace;

#if defined(KD_MACOS) || defined(KD_LINUX)
    freeSpace = _testObj->getFreeDiskSpace("/");
#elif defined(KD_WINDOWS)
    freeSpace = _testObj->getFreeDiskSpace(R"(C:\)");
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

    for (int64_t i = 10; i < currentTimestamp; i += 2629743) { // step of one month
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
    long long timeSpanCount = static_cast<long long>(timeSpan.count());
    CPPUNIT_ASSERT_GREATER((long long) (800), timeSpanCount);
    CPPUNIT_ASSERT_LESS((long long) (2000), timeSpanCount);
}

void TestUtility::testV2ws() {
    dbtype nullValue = std::monostate();
    CPPUNIT_ASSERT(_testObj->v2ws(nullValue) == L"NULL");

    dbtype intValue(1234);
    CPPUNIT_ASSERT(_testObj->v2ws(intValue) == L"1234");

    dbtype int64Value((int64_t) LLONG_MAX);
    CPPUNIT_ASSERT(_testObj->v2ws(int64Value) == L"9223372036854775807");

    dbtype doubleValue(123.456789);
    CPPUNIT_ASSERT(_testObj->v2ws(doubleValue) == L"123.456789");

    dbtype stringValue("hello");
    CPPUNIT_ASSERT(_testObj->v2ws(stringValue) == L"hello");
}

void TestUtility::testFileSystemName() {
#if defined(KD_MACOS)
    CPPUNIT_ASSERT(_testObj->fileSystemName("/") == "apfs");
    CPPUNIT_ASSERT(_testObj->fileSystemName("/bin") == "apfs");
#elif defined(KD_WINDOWS)
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

void TestUtility::testIsEqualUpToCaseAndEnc(void) {
    bool isEqual = false;
    const SyncPath pathA{"abcdefg"};
    const SyncPath pathB{"aBcDeFg"};
    CPPUNIT_ASSERT(Utility::checkIfEqualUpToCaseAndEncoding(pathA, pathB, isEqual) && isEqual);
    CPPUNIT_ASSERT(Utility::checkIfEqualUpToCaseAndEncoding(pathA, pathA, isEqual) && isEqual);
    CPPUNIT_ASSERT(Utility::checkIfEqualUpToCaseAndEncoding(pathB, pathB, isEqual) && isEqual);
    CPPUNIT_ASSERT(Utility::checkIfEqualUpToCaseAndEncoding(pathB, pathA, isEqual) && isEqual);
    CPPUNIT_ASSERT(Utility::checkIfEqualUpToCaseAndEncoding(pathA, "abcdef", isEqual) && !isEqual);
    CPPUNIT_ASSERT(Utility::checkIfEqualUpToCaseAndEncoding("abcdef", pathA, isEqual) && !isEqual);

    CPPUNIT_ASSERT(Utility::checkIfEqualUpToCaseAndEncoding(pathA, "abcdefh", isEqual) && !isEqual);
    CPPUNIT_ASSERT(Utility::checkIfEqualUpToCaseAndEncoding("abcdefh", pathA, isEqual) && !isEqual);

    CPPUNIT_ASSERT(Utility::checkIfEqualUpToCaseAndEncoding(pathA, "abcdefgh", isEqual) && !isEqual);
    CPPUNIT_ASSERT(Utility::checkIfEqualUpToCaseAndEncoding("abcdefgh", pathA, isEqual) && !isEqual);

    // NFC vs NFD
    SyncName nfcNormalizedName;
    CPPUNIT_ASSERT(Utility::normalizedSyncName(Str("éééé"), nfcNormalizedName));
    SyncName nfdNormalizedName;
    CPPUNIT_ASSERT(Utility::normalizedSyncName(Str("éééé"), nfdNormalizedName, UnicodeNormalization::NFD));
    CPPUNIT_ASSERT(Utility::checkIfEqualUpToCaseAndEncoding(nfcNormalizedName, nfdNormalizedName, isEqual) && isEqual);
}

void TestUtility::testMoveItemToTrash(void) {
    // !!! Linux - Move to trash fails on tmpfs
    if (Utility::userName() == "docker") return;
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

#if defined(KD_LINUX)
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
    CPPUNIT_ASSERT_EQUAL(0, _testObj->pathDepth({}));
    CPPUNIT_ASSERT_EQUAL(1, _testObj->pathDepth(SyncPath{"/"}));
    CPPUNIT_ASSERT_EQUAL(1, _testObj->pathDepth(SyncPath{"A"}));
    CPPUNIT_ASSERT_EQUAL(2, _testObj->pathDepth(SyncPath{"A/"}));
    CPPUNIT_ASSERT_EQUAL(2, _testObj->pathDepth(SyncPath{"/A"}));
    CPPUNIT_ASSERT_EQUAL(3, _testObj->pathDepth(SyncPath{"/A/"}));
    CPPUNIT_ASSERT_EQUAL(2, _testObj->pathDepth(SyncPath{"A/B"}));
    CPPUNIT_ASSERT_EQUAL(3, _testObj->pathDepth(SyncPath{"A/B/C"}));
    CPPUNIT_ASSERT_EQUAL(4, _testObj->pathDepth(SyncPath{"/A/B/C"}));
    CPPUNIT_ASSERT_EQUAL(5, _testObj->pathDepth(SyncPath{"/A/B/C/"}));

    SyncPath path;
    for (int i = 1; i < 5; i++) {
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
            {"12345678901234567890123456789012345678901234567890123456789012345678901234567890",
             "57edf4a22be3c955ac49da2e2107b67a"},
    };

    for (const auto &testCase: testCases) {
        CPPUNIT_ASSERT_EQUAL(testCase.second, _testObj->computeMd5Hash(testCase.first));
        CPPUNIT_ASSERT_EQUAL(testCase.second, _testObj->computeMd5Hash(testCase.first.c_str(), testCase.first.size()));
    }
}

void TestUtility::testXxHash() {
    SyncPath path = testhelpers::localTestDirPath() / "test_pictures/picture-1.jpg";
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

void TestUtility::testIsSubDir() {
    SyncPath path1 = "A/AA/AAA";
    SyncPath path2 = "A/AA";
    CPPUNIT_ASSERT(CommonUtility::isSubDir(path2, path1));
    CPPUNIT_ASSERT(!CommonUtility::isSubDir(path1, path2));

    path1 = path2;
    CPPUNIT_ASSERT(CommonUtility::isSubDir(path2, path1));

    path1 = "A/AB/AAA";
    CPPUNIT_ASSERT(!CommonUtility::isSubDir(path2, path1));
}

void TestUtility::testIsDiskRootFolder() {
    SyncPath suggestedPath = "A/AA/AAA";
    CPPUNIT_ASSERT_EQUAL(false, CommonUtility::isDiskRootFolder("A/AA/AAA", suggestedPath));
    CPPUNIT_ASSERT_EQUAL(false, CommonUtility::isDiskRootFolder("/Users", suggestedPath));
    CPPUNIT_ASSERT_EQUAL(false, CommonUtility::isDiskRootFolder("/home", suggestedPath));

    SyncPath dummyPath;
    CPPUNIT_ASSERT_EQUAL_MESSAGE(suggestedPath.string(), true, CommonUtility::isDiskRootFolder("/", suggestedPath));
    CPPUNIT_ASSERT(suggestedPath.empty() || !CommonUtility::isDiskRootFolder(suggestedPath, dummyPath));

#if defined(KD_WINDOWS)
    CPPUNIT_ASSERT_EQUAL(false, CommonUtility::isDiskRootFolder("C:\\Users", suggestedPath));

    CPPUNIT_ASSERT_EQUAL_MESSAGE(suggestedPath.string(), true, CommonUtility::isDiskRootFolder("C:\\", suggestedPath));
    CPPUNIT_ASSERT(suggestedPath.empty() || !CommonUtility::isDiskRootFolder(suggestedPath, dummyPath));
#elif defined(KD_MACOS)
    CPPUNIT_ASSERT_EQUAL(false, CommonUtility::isDiskRootFolder("/Volumes/drivename/kDrive", suggestedPath));

    CPPUNIT_ASSERT_EQUAL_MESSAGE(suggestedPath.string(), true,
                                 CommonUtility::isDiskRootFolder("/Volumes/drivename", suggestedPath));
    CPPUNIT_ASSERT(suggestedPath.empty() || !CommonUtility::isDiskRootFolder(suggestedPath, dummyPath));
#else
    CPPUNIT_ASSERT_EQUAL(false, CommonUtility::isDiskRootFolder("/media/username/drivename/kDrive", suggestedPath));

    CPPUNIT_ASSERT_EQUAL_MESSAGE(suggestedPath.string(), true, CommonUtility::isDiskRootFolder("/media", suggestedPath));
    CPPUNIT_ASSERT(suggestedPath.empty() || !CommonUtility::isDiskRootFolder(suggestedPath, dummyPath));
    CPPUNIT_ASSERT_EQUAL_MESSAGE(suggestedPath.string(), true, CommonUtility::isDiskRootFolder("/media/username", suggestedPath));
    CPPUNIT_ASSERT(suggestedPath.empty() || !CommonUtility::isDiskRootFolder(suggestedPath, dummyPath));
    CPPUNIT_ASSERT_EQUAL_MESSAGE(suggestedPath.string(), true,
                                 CommonUtility::isDiskRootFolder("/media/username/drivename", suggestedPath));
    CPPUNIT_ASSERT(suggestedPath.empty() || !CommonUtility::isDiskRootFolder(suggestedPath, dummyPath));
#endif
}

void TestUtility::testCheckIfDirEntryIsManaged() {
    LocalTemporaryDirectory tempDir;
    SyncPath path = tempDir.path() / "test.txt";
    std::ofstream file(path);
    file << "test";
    file.close();

    bool isManaged = false;
    IoError ioError = IoError::Success;
    std::filesystem::recursive_directory_iterator entry(tempDir.path());

    // Check with an existing file (managed)
    CPPUNIT_ASSERT(Utility::checkIfDirEntryIsManaged(*entry, isManaged, ioError));
    CPPUNIT_ASSERT(isManaged);
    CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);

    // Check with a simlink (managed)
    const SyncPath simLinkDir = tempDir.path() / "simLinkDir";
    std::filesystem::create_directory(simLinkDir);
    std::filesystem::create_symlink(path, simLinkDir / "testLink.txt");
    entry = std::filesystem::recursive_directory_iterator(simLinkDir);
    CPPUNIT_ASSERT(Utility::checkIfDirEntryIsManaged(*entry, isManaged, ioError));
    CPPUNIT_ASSERT(isManaged);
    CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);

    // Check with a directory
    entry = std::filesystem::recursive_directory_iterator(tempDir.path());
    CPPUNIT_ASSERT(Utility::checkIfDirEntryIsManaged(*entry, isManaged, ioError));
    CPPUNIT_ASSERT(isManaged);
    CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);
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
    {
        const IoError ioError = IoError::Success;
        const SyncPath path = "A/AA";
        const std::wstring result = _testObj->formatIoError(path, ioError);
        CPPUNIT_ASSERT_MESSAGE("The error message should contain 'err='...''",
                               result.find(L"err='Success'") != std::wstring::npos);
        CPPUNIT_ASSERT_MESSAGE("The error message should contain a description.",
                               (result.length() - path.native().length()) > 20);
        CPPUNIT_ASSERT_MESSAGE("The error message should contain the path.",
                               result.find(Utility::s2ws(path.string())) != std::wstring::npos);
    }

    {
        const IoError ioError = IoError::Success;
        const QString path = "A/AA";
        const std::wstring result = _testObj->formatIoError(path, ioError);
        CPPUNIT_ASSERT_MESSAGE("The error message should contain 'err='...''",
                               result.find(L"err='Success'") != std::wstring::npos);
        CPPUNIT_ASSERT_MESSAGE("The error message should contain a description.",
                               (result.length() - path.toStdWString().length()) > 20);
        CPPUNIT_ASSERT_MESSAGE("The error message should contain the path.",
                               result.find(path.toStdWString()) != std::wstring::npos);
    }
}

void TestUtility::testFormatSyncName() {
    const SyncName name = Str("FileA.txt");
    CPPUNIT_ASSERT(Utility::formatSyncName(name).find(SyncName2WStr(name)) != std::wstring::npos);
}

void TestUtility::testFormatPath() {
    const QString path = "A/AA";
    CPPUNIT_ASSERT(Utility::formatPath(path).find(path.toStdWString()) != std::wstring::npos);
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

void TestUtility::testNormalizedSyncName() {
    SyncName normalizedName;
    CPPUNIT_ASSERT(Utility::normalizedSyncName(SyncName(), normalizedName) && normalizedName.empty());

    // The two Unicode normalizations coincide.
    bool equal = false;
    CPPUNIT_ASSERT(checkNfcAndNfdNamesEqual(Str(""), equal) && equal);
    CPPUNIT_ASSERT(checkNfcAndNfdNamesEqual(Str("a"), equal) && equal);
    CPPUNIT_ASSERT(checkNfcAndNfdNamesEqual(Str("@"), equal) && equal);
    CPPUNIT_ASSERT(checkNfcAndNfdNamesEqual(Str("$"), equal) && equal);
    CPPUNIT_ASSERT(checkNfcAndNfdNamesEqual(Str("!"), equal) && equal);

    CPPUNIT_ASSERT(checkNfcAndNfdNamesEqual(Str("abcd"), equal) && equal);
    CPPUNIT_ASSERT(checkNfcAndNfdNamesEqual(Str("(1234%)"), equal) && equal);

    // The two Unicode normalizations don't coincide.
    CPPUNIT_ASSERT(checkNfcAndNfdNamesEqual(Str("à"), equal) && !equal);
    CPPUNIT_ASSERT(checkNfcAndNfdNamesEqual(Str("é"), equal) && !equal);
    CPPUNIT_ASSERT(checkNfcAndNfdNamesEqual(Str("è"), equal) && !equal);
    CPPUNIT_ASSERT(checkNfcAndNfdNamesEqual(Str("ê"), equal) && !equal);
    CPPUNIT_ASSERT(checkNfcAndNfdNamesEqual(Str("ü"), equal) && !equal);
    CPPUNIT_ASSERT(checkNfcAndNfdNamesEqual(Str("ö"), equal) && !equal);

    CPPUNIT_ASSERT(checkNfcAndNfdNamesEqual(Str("aöe"), equal) && !equal);
}

void TestUtility::testNormalizedSyncPath() {
    SyncPath normalizedPath;
    CPPUNIT_ASSERT(Utility::normalizedSyncPath("a/b/c", normalizedPath) && normalizedPath == SyncPath("a/b/c"));
    CPPUNIT_ASSERT(Utility::normalizedSyncPath("/a/b/c", normalizedPath) && normalizedPath == SyncPath("/a/b/c"));
#if defined(KD_WINDOWS)
    CPPUNIT_ASSERT(Utility::normalizedSyncPath(R"(a\b\c)", normalizedPath) && normalizedPath == SyncPath("a/b/c"));
    CPPUNIT_ASSERT(Utility::normalizedSyncPath(R"(\a\b\c)", normalizedPath) && normalizedPath == SyncPath("/a/b/c"));
    CPPUNIT_ASSERT(Utility::normalizedSyncPath("/a\\b/c", normalizedPath) && normalizedPath == SyncPath("/a/b/c"));
#else
    CPPUNIT_ASSERT(Utility::normalizedSyncPath("/a\\b/c", normalizedPath) && normalizedPath != SyncPath("/a/b/c"));
#endif
}

bool TestUtility::checkNfcAndNfdNamesEqual(const SyncName &name, bool &equal) {
    equal = false;

    SyncName nfcNormalized;
    if (!Utility::normalizedSyncName(name, nfcNormalized)) {
        return false;
    }
    SyncName nfdNormalized;
    if (!Utility::normalizedSyncName(name, nfdNormalized, UnicodeNormalization::NFD)) {
        return false;
    }
    equal = (nfcNormalized == nfdNormalized);
    return true;
}

void TestUtility::testIsSameOrParentPath() {
    CPPUNIT_ASSERT(!Utility::isDescendantOrEqual("", "a"));
    CPPUNIT_ASSERT(!Utility::isDescendantOrEqual("a", "a/b"));
    CPPUNIT_ASSERT(!Utility::isDescendantOrEqual("a", "a/b/c"));
    CPPUNIT_ASSERT(!Utility::isDescendantOrEqual("a/b", "a/b/c"));
    CPPUNIT_ASSERT(!Utility::isDescendantOrEqual("a/b/c", "a/b/c1"));
    CPPUNIT_ASSERT(!Utility::isDescendantOrEqual("a/b/c1", "a/b/c"));
    CPPUNIT_ASSERT(!Utility::isDescendantOrEqual("/a/b/c", "a/b/c"));

    CPPUNIT_ASSERT(Utility::isDescendantOrEqual("", ""));
    CPPUNIT_ASSERT(Utility::isDescendantOrEqual("a/b/c", "a/b/c"));
    CPPUNIT_ASSERT(Utility::isDescendantOrEqual("a", ""));
    CPPUNIT_ASSERT(Utility::isDescendantOrEqual("a/b/c", "a/b"));
    CPPUNIT_ASSERT(Utility::isDescendantOrEqual("a/b/c", "a"));
}

void TestUtility::testUserName() {
    std::string userName(Utility::userName());
    LOG_DEBUG(Log::instance()->getLogger(), "userName=" << userName);

#if defined(KD_WINDOWS)
    const char *value = std::getenv("USERPROFILE");
    CPPUNIT_ASSERT(value);
    const SyncPath homeDir(value);
    LOGW_DEBUG(Log::instance()->getLogger(), L"homeDir=" << Utility::formatSyncPath(homeDir));

    if (homeDir.filename().native() == std::wstring(L"systemprofile")) {
        // CI execution
        CPPUNIT_ASSERT_EQUAL(std::string("SYSTEM"), userName);
    } else {
        CPPUNIT_ASSERT_EQUAL(SyncName2Str(homeDir.filename().native()), userName);
    }
#else
    const char *value = std::getenv("HOME");
    CPPUNIT_ASSERT(value);
    const SyncPath homeDir(value);
    LOGW_DEBUG(Log::instance()->getLogger(), L"homeDir=" << Utility::formatSyncPath(homeDir));
    CPPUNIT_ASSERT_EQUAL(homeDir.filename().native(), userName);
#endif
}

} // namespace KDC

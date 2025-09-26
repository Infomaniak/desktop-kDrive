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
#include "test_utility/testhelpers.h"
#include "libcommon/utility/logiffail.h"
#include "libcommon/utility/utility.h"
#include "libcommon/utility/sourcelocation.h"
#include "libcommonserver/io/iohelper.h"
#include "test_utility/localtemporarydirectory.h"
#include "utility/utility_base.h"


#include <QLocale>
#include <iostream>
#include <regex>

namespace KDC {

void TestUtility::testGetAppSupportDir() {
    SyncPath appSupportDir = CommonUtility::getAppSupportDir();
    std::string faillureMessage = "Path: " + appSupportDir.string();
    CPPUNIT_ASSERT_MESSAGE(faillureMessage.c_str(), !appSupportDir.empty());
#if defined(KD_WINDOWS)
    CPPUNIT_ASSERT_MESSAGE(faillureMessage.c_str(), appSupportDir.string().find("AppData") != std::string::npos);
    CPPUNIT_ASSERT_MESSAGE(faillureMessage.c_str(), appSupportDir.string().find("Local") != std::string::npos);
#elif defined(KD_MACOS)
    CPPUNIT_ASSERT_MESSAGE(faillureMessage.c_str(), appSupportDir.string().find("Library") != std::string::npos);
    CPPUNIT_ASSERT_MESSAGE(faillureMessage.c_str(), appSupportDir.string().find("Application") != std::string::npos);
#else
    CPPUNIT_ASSERT_MESSAGE(faillureMessage.c_str(), appSupportDir.string().find(".config") != std::string::npos);
#endif
    CPPUNIT_ASSERT_MESSAGE(faillureMessage.c_str(), appSupportDir.string().find(APPLICATION_NAME) != std::wstring::npos);
}

void TestUtility::extractIntFromStrVersion() {
    {
        const std::string versionString = "3.7.6";
        std::vector<int> versionNumberComponents;

        CommonUtility::extractIntFromStrVersion(versionString, versionNumberComponents);
        CPPUNIT_ASSERT(bool(std::vector<int>{3, 7, 6} == versionNumberComponents));
    }

    {
        const std::string versionString = "3.7.61.10.12";
        std::vector<int> versionNumberComponents;

        CommonUtility::extractIntFromStrVersion(versionString, versionNumberComponents);
        CPPUNIT_ASSERT(bool(std::vector<int>{3, 7, 61, 10, 12} == versionNumberComponents));
    }

    {
        const std::string versionString = "155.75.0 (build 20250221)";
        std::vector<int> versionNumberComponents;

        CommonUtility::extractIntFromStrVersion(versionString, versionNumberComponents);
        CPPUNIT_ASSERT(bool(std::vector<int>{155, 75, 0, 20250221} == versionNumberComponents));
    }

    // Invalid version string
    {
        const std::string versionString = ".0";
        std::vector<int> versionNumberComponents;

        CommonUtility::extractIntFromStrVersion(versionString, versionNumberComponents);
        CPPUNIT_ASSERT(bool(std::vector<int>{} == versionNumberComponents));
    }

    // Invalid version string
    {
        const std::string versionString = "1.x.0";
        std::vector<int> versionNumberComponents;

        CommonUtility::extractIntFromStrVersion(versionString, versionNumberComponents);
        CPPUNIT_ASSERT(bool(std::vector<int>{1} == versionNumberComponents));
    }
}

void TestUtility::testIsVersionLower() {
    CPPUNIT_ASSERT(!CommonUtility::isVersionLower("3.5.8", "3.5.8"));

    // Single digit major, minor or patch versions
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


    // Double digit major, minor or patch versions
    CPPUNIT_ASSERT(CommonUtility::isVersionLower("1.0.0", "55.0.0"));
    CPPUNIT_ASSERT(CommonUtility::isVersionLower("9.0.0", "55.0.0"));

    CPPUNIT_ASSERT(CommonUtility::isVersionLower("25.0.0", "55.0.0"));
    CPPUNIT_ASSERT(!CommonUtility::isVersionLower("75.0.0", "55.0.0"));

    CPPUNIT_ASSERT(CommonUtility::isVersionLower("55.0.25", "55.0.55"));
    CPPUNIT_ASSERT(!CommonUtility::isVersionLower("55.0.75", "55.0.55"));

    CPPUNIT_ASSERT(CommonUtility::isVersionLower("55.25.0", "55.55.0"));
    CPPUNIT_ASSERT(!CommonUtility::isVersionLower("55.75.0", "55.55.0"));

    // With a build version
    CPPUNIT_ASSERT(CommonUtility::isVersionLower("155.75.0 (build 20250221)", "155.75.0 (build 20250222)"));
    CPPUNIT_ASSERT(CommonUtility::isVersionLower("255.85.0 (build 20240221)", "255.85.0 (build 20250222)"));

    // With an invalid version
    CPPUNIT_ASSERT(!CommonUtility::isVersionLower(".155.75.0", "156.75.0"));
    CPPUNIT_ASSERT(!CommonUtility::isVersionLower("155.75.0", ".156.75.0"));
    CPPUNIT_ASSERT(!CommonUtility::isVersionLower("1.x.0", "156.75.0"));
    CPPUNIT_ASSERT(!CommonUtility::isVersionLower("156.75.0", "1.x.0"));
    CPPUNIT_ASSERT(!CommonUtility::isVersionLower("a.0.0", "156.75.0"));
    CPPUNIT_ASSERT(!CommonUtility::isVersionLower("156.75.0", "a.0.0"));
}

void TestUtility::testStringToAppStateValue() {
    // Normal conditions
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

    // Invalid input
    value = int(0);
    CPPUNIT_ASSERT(!CommonUtility::stringToAppStateValue("test", value));

    value = LogUploadState::None;
    CPPUNIT_ASSERT(!CommonUtility::stringToAppStateValue("test", value));

    value = int64_t(0);
    CPPUNIT_ASSERT(!CommonUtility::stringToAppStateValue("test", value));

    // Empty input
    value = int(0);
    CPPUNIT_ASSERT(!CommonUtility::stringToAppStateValue("", value));
}

void TestUtility::testArgsWriter() {
    {
        const int a = 1;
        const auto byteArray = QByteArray(ArgsReader(a));

        int b = -1;
        ArgsWriter(byteArray).write(b);

        CPPUNIT_ASSERT_EQUAL(1, b);
    }

    {
        const char c = 'c';
        const auto byteArray = QByteArray(ArgsReader(c));

        char d = ' ';
        ArgsWriter(byteArray).write(d);

        CPPUNIT_ASSERT_EQUAL(c, d);
    }

    {
        const double e = 1.5;
        const float f = -2.0f;

        const auto byteArray = QByteArray(ArgsReader(e, f));

        double g = -1.0;
        float h = -1.0f;
        ArgsWriter(byteArray).write(g, h);

        CPPUNIT_ASSERT_EQUAL(e, g);
        CPPUNIT_ASSERT_EQUAL(f, h);
    }

    {
        const char a = 'a';
        const bool b = true;
        const float c = 1.0f;

        const auto byteArray = QByteArray(ArgsReader(a, b, c));

        char d = 'd';
        bool e = false;
        float f = -1.0f;
        ArgsWriter(byteArray).write(d, e, f);

        CPPUNIT_ASSERT_EQUAL(a, d);
        CPPUNIT_ASSERT_EQUAL(b, e);
        CPPUNIT_ASSERT_EQUAL(c, f);
    }

    {
        const qint8 a = 100;
        const qint16 b = 200;
        const qint32 c = 300;

        const auto byteArray = QByteArray(ArgsReader(a, b, c));

        qint8 d = -1;
        qint16 e = 0;
        qint32 f = -1;
        ArgsWriter(byteArray).write(d, e, f);

        CPPUNIT_ASSERT_EQUAL(a, d);
        CPPUNIT_ASSERT_EQUAL(b, e);
        CPPUNIT_ASSERT_EQUAL(c, f);
    }

    {
        const qint32 a = 100;
        const qint64 b = 200;
        const char *c = "test";
        const double d = 1.0;

        const auto byteArray = QByteArray(ArgsReader(a, b, c, d));

        qint32 e = -1;
        qint64 f = 0;
        char *g = nullptr;
        double h = -1.0;
        ArgsWriter(byteArray).write(e, f, g, h);

        CPPUNIT_ASSERT_EQUAL(a, e);
        CPPUNIT_ASSERT_EQUAL(b, f);
        CPPUNIT_ASSERT_EQUAL(0, strcmp(c, g));
        CPPUNIT_ASSERT_EQUAL(d, h);
    }

    {
        const qint16 a = 100;
        const qint32 b = 200;
        const char *c = "test";
        const double d = 1.0;
        const float e = 2.0f;

        const auto byteArray = QByteArray(ArgsReader(a, b, c, d, e));

        qint16 f = -1;
        qint32 g = 0;
        char *h = nullptr;
        double i = -1.0;
        float j = -1.0f;
        ArgsWriter(byteArray).write(f, g, h, i, j);

        CPPUNIT_ASSERT_EQUAL(a, f);
        CPPUNIT_ASSERT_EQUAL(b, g);
        CPPUNIT_ASSERT_EQUAL(0, strcmp(c, h));
        CPPUNIT_ASSERT_EQUAL(d, i);
        CPPUNIT_ASSERT_EQUAL(e, j);
    }
}

void TestUtility::testCompressFile() {
    LocalTemporaryDirectory tmpDir("CommonUtility_compressFile");
    SyncPath filePath = tmpDir.path() / "testFile.txt";

    // Test with an empty file
    std::ofstream file(filePath);
    file.close();

    SyncPath outPath = tmpDir.path() / "resEmptyFile.zip";
    CommonUtility::compressFile(filePath.string(), outPath.string());

    bool exists = false;
    IoError ioError = IoError::Unknown;
    CPPUNIT_ASSERT_MESSAGE(toString(ioError), IoHelper::checkIfPathExists(outPath, exists, ioError));
    CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
    CPPUNIT_ASSERT(exists);

    // Test with a non empty file
    file.open(filePath);
    for (int i = 0; i < 100; i++) {
        file << "test" << std::endl;
    }
    file.close();

    uint64_t size = 0;
    CPPUNIT_ASSERT_MESSAGE(toString(ioError), IoHelper::getFileSize(filePath, size, ioError));
    CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);

    outPath = tmpDir.path() / "resFile.zip";
    CommonUtility::compressFile(filePath.string(), outPath.string());

    CPPUNIT_ASSERT_MESSAGE(toString(ioError), IoHelper::checkIfPathExists(outPath, exists, ioError));
    CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
    CPPUNIT_ASSERT(exists);

    uint64_t compressedSize = 0;
    CPPUNIT_ASSERT_MESSAGE(toString(ioError), IoHelper::getFileSize(outPath, compressedSize, ioError));
    CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
    CPPUNIT_ASSERT(compressedSize < size);

    // Test with a non existing file
    outPath = tmpDir.path() / "resNonExistingFile.zip";
    CPPUNIT_ASSERT(!CommonUtility::compressFile("nonExistingFile.txt", outPath.string()));
    CPPUNIT_ASSERT_MESSAGE(toString(ioError), IoHelper::checkIfPathExists(outPath, exists, ioError));
    CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
    CPPUNIT_ASSERT(!exists);

    // Test with a non existing output dir
    outPath = tmpDir.path() / "nonExistingDir" / "resNonExistingDir.zip";
    CPPUNIT_ASSERT(!CommonUtility::compressFile(filePath.string(), outPath.string()));
    CPPUNIT_ASSERT_MESSAGE(toString(ioError), IoHelper::checkIfPathExists(outPath, exists, ioError));
    CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
    CPPUNIT_ASSERT(!exists);

    // Test with wstring path
    outPath = tmpDir.path() / "resWstring.zip";
    CommonUtility::compressFile(filePath.wstring(), outPath.wstring());
    CPPUNIT_ASSERT_MESSAGE(toString(ioError), IoHelper::checkIfPathExists(outPath, exists, ioError));
    CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
    CPPUNIT_ASSERT(exists);
}

void TestUtility::testCurrentVersion() {
    const std::string test = CommonUtility::currentVersion();
    CPPUNIT_ASSERT(std::regex_match(test, std::regex(R"(\d{1,2}\.{1}\d{1,2}\.{1}\d{1,2}\.{1}\d{0,8}$)")));
}

SourceLocation testSourceLocationFooFunc(uint32_t &constructLine, SourceLocation location = SourceLocation::currentLoc()) {
    constructLine = __LINE__ - 1;
    return location;
}

void TestUtility::testSourceLocation() {
    SourceLocation location = SourceLocation::currentLoc();
    uint32_t correctLine = __LINE__ - 1;

    CPPUNIT_ASSERT_EQUAL(std::string("testutility.cpp"), location.fileName());
    CPPUNIT_ASSERT_EQUAL(correctLine, location.line());

#ifdef SRC_LOC_AVALAIBALE
    CPPUNIT_ASSERT_EQUAL(std::string("testSourceLocation"), location.functionName());
#else
    CPPUNIT_ASSERT_EQUAL(std::string(""), location.functionName());
#endif

    // Test as a default argument
    uint32_t fooFuncLine = 0;
    location = testSourceLocationFooFunc(fooFuncLine);
    correctLine = __LINE__ - 1;

    CPPUNIT_ASSERT_EQUAL(std::string("testutility.cpp"), location.fileName());
#ifdef SRC_LOC_AVALAIBALE
    CPPUNIT_ASSERT_EQUAL(std::string("testSourceLocation"), location.functionName());
    CPPUNIT_ASSERT_EQUAL(correctLine, location.line());
#else
    CPPUNIT_ASSERT_EQUAL(std::string(""), location.functionName());
    CPPUNIT_ASSERT_EQUAL(fooFuncLine, location.line());
#endif
}

void TestUtility::testGenerateRandomStringAlphaNum() {
    if (!testhelpers::isExtendedTest()) return;
    {
        int err = 0;
        std::set<std::string> results;
        for (int i = 0; i < 100000; i++) {
            std::string str = CommonUtility::generateRandomStringAlphaNum();
            if (!results.insert(str).second) {
                err++;
            }
        }
        CPPUNIT_ASSERT(err == 0);
    }

    {
        int err = 0;
        for (int c = 0; c < 100000; c++) {
            std::vector<std::thread> workers;
            std::set<std::string> results;
            std::mutex resultsMutex;
            bool wait = true;
            for (int i = 0; i < 3; i++) {
                workers.push_back(std::thread([&]() {
                    while (wait) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    };
                    std::string str = CommonUtility::generateRandomStringAlphaNum();
                    const std::lock_guard<std::mutex> lock(resultsMutex);
                    if (!results.insert(str).second) {
                        err++;
                    }
                }));
            }
            wait = false;
            std::for_each(workers.begin(), workers.end(), [](std::thread &t) { t.join(); });
        }
        CPPUNIT_ASSERT(err == 0);
    }
}
void TestUtility::testLanguageCode() {
    // Only the C language is installed in docker
    if (Utility::userName() == "docker") return;

    CPPUNIT_ASSERT_EQUAL(std::string("en"), CommonUtility::languageCode(Language::English).toStdString());
    CPPUNIT_ASSERT_EQUAL(std::string("fr"), CommonUtility::languageCode(Language::French).toStdString());
    CPPUNIT_ASSERT_EQUAL(std::string("de"), CommonUtility::languageCode(Language::German).toStdString());
    CPPUNIT_ASSERT_EQUAL(std::string("es"), CommonUtility::languageCode(Language::Spanish).toStdString());
    CPPUNIT_ASSERT_EQUAL(std::string("it"), CommonUtility::languageCode(Language::Italian).toStdString());

    const auto systemLanguage = QLocale::languageToCode(QLocale::system().language());
    CPPUNIT_ASSERT_EQUAL(systemLanguage.toStdString(), CommonUtility::languageCode(Language::Default).toStdString());

    // English is the default language and is always returned if the provided language code is unknown.
    CPPUNIT_ASSERT_EQUAL(std::string("en"), CommonUtility::languageCode(static_cast<Language>(18)).toStdString());
}

void TestUtility::testIsSupportedLanguage() {
    CPPUNIT_ASSERT_EQUAL(true, CommonUtility::isSupportedLanguage("en"));
    CPPUNIT_ASSERT_EQUAL(true, CommonUtility::isSupportedLanguage("fr"));
    CPPUNIT_ASSERT_EQUAL(true, CommonUtility::isSupportedLanguage("de"));
    CPPUNIT_ASSERT_EQUAL(true, CommonUtility::isSupportedLanguage("es"));
    CPPUNIT_ASSERT_EQUAL(true, CommonUtility::isSupportedLanguage("it"));
    CPPUNIT_ASSERT_EQUAL(false, CommonUtility::isSupportedLanguage("ita"));
    CPPUNIT_ASSERT_EQUAL(false, CommonUtility::isSupportedLanguage("zc"));
    CPPUNIT_ASSERT_EQUAL(false, CommonUtility::isSupportedLanguage(""));
}

#if defined(KD_WINDOWS)
void TestUtility::testGetLastErrorMessage() {
    // Ensure that the error message is reset.
    SetLastError(0);

    // No actual error. Display the expected success message.
    {
        const std::wstring msg = utility_base::getLastErrorMessage();
        CPPUNIT_ASSERT_MESSAGE(SyncName2Str(msg.c_str()), msg.starts_with(L"(0) - "));
    }
    // Display the file-not-found error message.
    {
        GetFileAttributesW(L"this_file_does_not_exist.txt");
        const std::wstring msg = utility_base::getLastErrorMessage();
        CPPUNIT_ASSERT(msg.starts_with(L"(2) - "));
    }
}

#endif

void TestUtility::generatePaths(const std::vector<std::string> &itemsNames, const std::vector<char> &separators,
                                bool startWithSeparator, std::vector<SyncPath> &result, const std::string &start, size_t pos) {
    if (pos == itemsNames.size()) {
        (void) result.emplace_back(start);
        for (const auto &separator: separators) {
            (void) result.emplace_back(start + separator);
        }
        return;
    }
    if (!startWithSeparator && start.empty()) {
        generatePaths(itemsNames, separators, false, result, itemsNames.at(pos), pos + 1);

    } else {
        for (const auto &separator: separators) {
            generatePaths(itemsNames, separators, false, result, start + separator + itemsNames.at(pos), pos + 1);
        }
    }
}

void TestUtility::testLogIfFail() {
    // Logs nothing. Don't abort execution.
    auto logger = Log::instance()->getLogger();
    LOG_IF_FAIL(logger, true)
    LOG_MSG_IF_FAIL(Log::instance()->getLogger(), true, "Surprisingly incorrect!")

#ifdef NDEBUG
    // Log failure messages but do not abort execution.
    LOG_IF_FAIL(Log::instance()->getLogger(), false)
    LOG_MSG_IF_FAIL(Log::instance()->getLogger(), false, "Check your logs, something went wrong.")
#endif
}

void TestUtility::testRelativePath() {
    const std::vector<char> separators = {'/', '\\'};
    const std::vector<std::string> rootPathItems = {"dir1", "dir2", "??"}; // "/dir1/dir2/??
    const std::vector<std::string> relativePathItems = {"syncDir1", "syncDir2", "syncDir3"}; // "syncDir1/syncDir2/syncDir3

    // Root path is empty
    std::vector<SyncPath> relativePaths;
    generatePaths(relativePathItems, separators, false, relativePaths);

    for (const auto &relativePath: relativePaths) {
        CPPUNIT_ASSERT_EQUAL(relativePath, CommonUtility::relativePath(SyncPath(), relativePath));
    }

    // Absolute path is empty
    std::vector<SyncPath> rootPaths;
    generatePaths(rootPathItems, separators, true, rootPaths);

    for (const auto &rootPath: rootPaths) {
        CPPUNIT_ASSERT_EQUAL(SyncPath(), CommonUtility::relativePath(rootPath, SyncPath()));
    }

    // Both paths are empty
    CPPUNIT_ASSERT_EQUAL(SyncPath(), CommonUtility::relativePath(SyncPath(), SyncPath()));

    // Both paths are the same
    for (const auto &rootPath: rootPaths) {
        for (const auto &absolutePath: rootPaths) {
            CPPUNIT_ASSERT_EQUAL(SyncPath(), CommonUtility::relativePath(rootPath, absolutePath));
        }
    }

    // Absolute path is a subpath of the root path
    std::vector<std::pair<SyncPath /*Absolute path*/, SyncPath /*Relative paths*/>> absolutePaths;
    for (const auto &rootPath: rootPaths) {
        if (rootPath.string().back() == separators[0] || rootPath.string().back() == separators[1]) continue;
        for (const auto &relativePath: relativePaths) {
            for (const auto &separator: separators) {
                const SyncPath absolutePath = rootPath.string() + separator + relativePath.string();
                (void) absolutePaths.emplace_back(absolutePath, relativePath);
            }
        }
    }

    for (const auto &rootPath: rootPaths) {
        for (const auto &[absolutePath, relativePaths]: absolutePaths) {
            CPPUNIT_ASSERT_EQUAL_MESSAGE("Root path: " + rootPath.string() + " Absolute path: " + absolutePath.string(),
                                         relativePaths, CommonUtility::relativePath(rootPath, absolutePath));
        }
    }

    // Absolute path is not a subpath of the root path
    CPPUNIT_ASSERT_EQUAL(SyncPath(), CommonUtility::relativePath(SyncPath("dir1"), SyncPath("dir2/test")));
}

void TestUtility::testSplitSyncPath() {
    auto splitting = CommonUtility::splitSyncPath(SyncPath{});
    CPPUNIT_ASSERT_EQUAL(size_t{0}, splitting.size());
    CPPUNIT_ASSERT(splitting.empty());

    splitting = CommonUtility::splitSyncPath(SyncPath("A") / "B" / "file.txt");
    CPPUNIT_ASSERT_EQUAL(size_t{3}, splitting.size());
    CPPUNIT_ASSERT_EQUAL(SyncName2Str(SyncName{Str("A")}), SyncName2Str(splitting.front()));
    splitting.pop_front();
    CPPUNIT_ASSERT_EQUAL(SyncName2Str(SyncName{Str("B")}), SyncName2Str(splitting.front()));
    splitting.pop_front();
    CPPUNIT_ASSERT_EQUAL(SyncName2Str(SyncName{Str("file.txt")}), SyncName2Str(splitting.front()));

    splitting = CommonUtility::splitSyncPath(SyncPath("") / "B" / "file.txt");
    CPPUNIT_ASSERT_EQUAL(size_t{2}, splitting.size());
    CPPUNIT_ASSERT_EQUAL(SyncName2Str(SyncName{Str("B")}), SyncName2Str(splitting.front()));
    splitting.pop_front();
    CPPUNIT_ASSERT_EQUAL(SyncName2Str(SyncName{Str("file.txt")}), SyncName2Str(splitting.front()));

    SyncName noSegment = Str("*_blacklisted_*_*_*");
    splitting = CommonUtility::splitSyncPath(SyncPath{noSegment});
    CPPUNIT_ASSERT_EQUAL(size_t{1}, splitting.size());
    CPPUNIT_ASSERT_EQUAL(SyncName2Str(noSegment), SyncName2Str(splitting.front()));

    SyncName oneSeparator = Str("A/B");
    splitting = CommonUtility::splitSyncPath(SyncPath{oneSeparator});
    CPPUNIT_ASSERT_EQUAL(size_t{2}, splitting.size());
    CPPUNIT_ASSERT_EQUAL(SyncName2Str(SyncName{Str("A")}), SyncName2Str(splitting.front()));
    splitting.pop_front();
    CPPUNIT_ASSERT_EQUAL(SyncName2Str(SyncName{Str("B")}), SyncName2Str(splitting.front()));

    SyncName twoSeparators = Str("/A/B");
    splitting = CommonUtility::splitSyncPath(SyncPath{twoSeparators});
    CPPUNIT_ASSERT_EQUAL(size_t{2}, splitting.size());
    CPPUNIT_ASSERT_EQUAL(SyncName2Str(SyncName{Str("A")}), SyncName2Str(splitting.front()));
    splitting.pop_front();
    CPPUNIT_ASSERT_EQUAL(SyncName2Str(SyncName{Str("B")}), SyncName2Str(splitting.front()));

    twoSeparators = Str("A/B/");
    splitting = CommonUtility::splitSyncPath(SyncPath{twoSeparators});
    CPPUNIT_ASSERT_EQUAL(size_t{3}, splitting.size());
    CPPUNIT_ASSERT_EQUAL(SyncName2Str(SyncName{Str("A")}), SyncName2Str(splitting.front()));
    splitting.pop_front();
    CPPUNIT_ASSERT_EQUAL(SyncName2Str(SyncName{Str("B")}), SyncName2Str(splitting.front()));
    splitting.pop_front();
    CPPUNIT_ASSERT_EQUAL(SyncName2Str(SyncName{}), SyncName2Str(splitting.front()));

    twoSeparators = Str("A/B/C");
    splitting = CommonUtility::splitSyncPath(SyncPath{twoSeparators});
    CPPUNIT_ASSERT_EQUAL(size_t{3}, splitting.size());
    CPPUNIT_ASSERT_EQUAL(SyncName2Str(SyncName{Str("A")}), SyncName2Str(splitting.front()));
    splitting.pop_front();
    CPPUNIT_ASSERT_EQUAL(SyncName2Str(SyncName{Str("B")}), SyncName2Str(splitting.front()));
    splitting.pop_front();
    CPPUNIT_ASSERT_EQUAL(SyncName2Str(SyncName{Str("C")}), SyncName2Str(splitting.front()));

#if defined(KD_WINDOWS)
    twoSeparators = Str("A\\B\\C");
    splitting = CommonUtility::splitSyncPath(SyncPath{twoSeparators});
    CPPUNIT_ASSERT_EQUAL(size_t{3}, splitting.size());
    CPPUNIT_ASSERT_EQUAL(SyncName2Str(SyncName{Str("A")}), SyncName2Str(splitting.front()));
    splitting.pop_front();
    CPPUNIT_ASSERT_EQUAL(SyncName2Str(SyncName{Str("B")}), SyncName2Str(splitting.front()));
    splitting.pop_front();
    CPPUNIT_ASSERT_EQUAL(SyncName2Str(SyncName{Str("C")}), SyncName2Str(splitting.front()));
#endif
}

void TestUtility::testSplitSyncName() {
    SyncName empty;
    auto splitting = CommonUtility::splitSyncName(empty, Str("/"));
    CPPUNIT_ASSERT_EQUAL(size_t{1}, splitting.size());
    CPPUNIT_ASSERT(splitting.at(0).empty());

    SyncName noSegment = Str("*_blacklisted_*_*_*");
    splitting = CommonUtility::splitSyncName(noSegment, Str("/"));
    CPPUNIT_ASSERT_EQUAL(size_t{1}, splitting.size());
    CPPUNIT_ASSERT_EQUAL(SyncName2Str(noSegment), SyncName2Str(splitting.at(0)));

    SyncName oneSeparation = Str("A/B");
    splitting = CommonUtility::splitSyncName(oneSeparation, Str("/"));
    CPPUNIT_ASSERT_EQUAL(size_t{2}, splitting.size());
    CPPUNIT_ASSERT_EQUAL(SyncName2Str(SyncName{Str("A")}), SyncName2Str(splitting.at(0)));
    CPPUNIT_ASSERT_EQUAL(SyncName2Str(SyncName{Str("B")}), SyncName2Str(splitting.at(1)));

    SyncName twoSeparations = Str("/A/B");
    splitting = CommonUtility::splitSyncName(twoSeparations, Str("/"));
    CPPUNIT_ASSERT_EQUAL(size_t{3}, splitting.size());
    CPPUNIT_ASSERT_EQUAL(SyncName2Str(SyncName{}), SyncName2Str(splitting.at(0)));
    CPPUNIT_ASSERT_EQUAL(SyncName2Str(SyncName{Str("A")}), SyncName2Str(splitting.at(1)));
    CPPUNIT_ASSERT_EQUAL(SyncName2Str(SyncName{Str("B")}), SyncName2Str(splitting.at(2)));

    twoSeparations = Str("A/B/");
    splitting = CommonUtility::splitSyncName(twoSeparations, Str("/"));
    CPPUNIT_ASSERT_EQUAL(size_t{3}, splitting.size());
    CPPUNIT_ASSERT_EQUAL(SyncName2Str(SyncName{Str("A")}), SyncName2Str(splitting.at(0)));
    CPPUNIT_ASSERT_EQUAL(SyncName2Str(SyncName{Str("B")}), SyncName2Str(splitting.at(1)));
    CPPUNIT_ASSERT_EQUAL(SyncName2Str(SyncName{}), SyncName2Str(splitting.at(2)));

    twoSeparations = Str("A/B/C");
    splitting = CommonUtility::splitSyncName(twoSeparations, Str("/"));
    CPPUNIT_ASSERT_EQUAL(size_t{3}, splitting.size());
    CPPUNIT_ASSERT_EQUAL(SyncName2Str(SyncName{Str("A")}), SyncName2Str(splitting.at(0)));
    CPPUNIT_ASSERT_EQUAL(SyncName2Str(SyncName{Str("B")}), SyncName2Str(splitting.at(1)));
    CPPUNIT_ASSERT_EQUAL(SyncName2Str(SyncName{Str("C")}), SyncName2Str(splitting.at(2)));

    twoSeparations = Str("A\\B\\C");
    splitting = CommonUtility::splitSyncName(twoSeparations, Str("\\"));
    CPPUNIT_ASSERT_EQUAL(size_t{3}, splitting.size());
    CPPUNIT_ASSERT_EQUAL(SyncName2Str(SyncName{Str("A")}), SyncName2Str(splitting.at(0)));
    CPPUNIT_ASSERT_EQUAL(SyncName2Str(SyncName{Str("B")}), SyncName2Str(splitting.at(1)));
    CPPUNIT_ASSERT_EQUAL(SyncName2Str(SyncName{Str("C")}), SyncName2Str(splitting.at(2)));
}

void TestUtility::testSplitPathFromSyncName() {
    SyncName empty;
    auto splitting = CommonUtility::splitPath(empty);
    CPPUNIT_ASSERT_EQUAL(size_t{1}, splitting.size());
    CPPUNIT_ASSERT(splitting.at(0).empty());

    SyncName noSegment = Str("*_blacklisted_*_*_*");
    splitting = CommonUtility::splitPath(noSegment);
    CPPUNIT_ASSERT_EQUAL(size_t{1}, splitting.size());
    CPPUNIT_ASSERT_EQUAL(SyncName2Str(noSegment), SyncName2Str(splitting.at(0)));

    SyncName oneSeparator = Str("A/B");
    splitting = CommonUtility::splitPath(oneSeparator);
    CPPUNIT_ASSERT_EQUAL(size_t{2}, splitting.size());
    CPPUNIT_ASSERT_EQUAL(SyncName2Str(SyncName{Str("A")}), SyncName2Str(splitting.at(0)));
    CPPUNIT_ASSERT_EQUAL(SyncName2Str(SyncName{Str("B")}), SyncName2Str(splitting.at(1)));

    SyncName twoSeparators = Str("/A/B");
    splitting = CommonUtility::splitPath(twoSeparators);
    CPPUNIT_ASSERT_EQUAL(size_t{3}, splitting.size());
    CPPUNIT_ASSERT_EQUAL(SyncName2Str(SyncName{}), SyncName2Str(splitting.at(0)));
    CPPUNIT_ASSERT_EQUAL(SyncName2Str(SyncName{Str("A")}), SyncName2Str(splitting.at(1)));
    CPPUNIT_ASSERT_EQUAL(SyncName2Str(SyncName{Str("B")}), SyncName2Str(splitting.at(2)));

    twoSeparators = Str("A/B/");
    splitting = CommonUtility::splitPath(twoSeparators);
    CPPUNIT_ASSERT_EQUAL(size_t{3}, splitting.size());
    CPPUNIT_ASSERT_EQUAL(SyncName2Str(SyncName{Str("A")}), SyncName2Str(splitting.at(0)));
    CPPUNIT_ASSERT_EQUAL(SyncName2Str(SyncName{Str("B")}), SyncName2Str(splitting.at(1)));
    CPPUNIT_ASSERT_EQUAL(SyncName2Str(SyncName{}), SyncName2Str(splitting.at(2)));

    twoSeparators = Str("A/B/C");
    splitting = CommonUtility::splitPath(twoSeparators);
    CPPUNIT_ASSERT_EQUAL(size_t{3}, splitting.size());
    CPPUNIT_ASSERT_EQUAL(SyncName2Str(SyncName{Str("A")}), SyncName2Str(splitting.at(0)));
    CPPUNIT_ASSERT_EQUAL(SyncName2Str(SyncName{Str("B")}), SyncName2Str(splitting.at(1)));
    CPPUNIT_ASSERT_EQUAL(SyncName2Str(SyncName{Str("C")}), SyncName2Str(splitting.at(2)));

#if defined(KD_WINDOWS)
    twoSeparators = Str("A/B\\C");
    splitting = CommonUtility::splitPath(twoSeparators);
    CPPUNIT_ASSERT_EQUAL(size_t{3}, splitting.size());
    CPPUNIT_ASSERT_EQUAL(SyncName2Str(SyncName{Str("A")}), SyncName2Str(splitting.at(0)));
    CPPUNIT_ASSERT_EQUAL(SyncName2Str(SyncName{Str("B")}), SyncName2Str(splitting.at(1)));
    CPPUNIT_ASSERT_EQUAL(SyncName2Str(SyncName{Str("C")}), SyncName2Str(splitting.at(2)));
#endif
}

void TestUtility::testComputeSyncNameNormalizations() {
    const SyncName name = Str("éèä");
    auto normalizations = CommonUtility::computeSyncNameNormalizations(name);
    CPPUNIT_ASSERT_EQUAL(size_t{2}, normalizations.size());

    SyncName nfcName;
    CommonUtility::normalizedSyncName(name, nfcName, UnicodeNormalization::NFC);
    CPPUNIT_ASSERT(normalizations.contains(nfcName));

    SyncName nfdName;
    CommonUtility::normalizedSyncName(name, nfdName, UnicodeNormalization::NFD);
    CPPUNIT_ASSERT(normalizations.contains(nfdName));
}

namespace {
SyncNameSet computeExpectedPathNormalizations() {
    SyncName nfcName;
    (void) CommonUtility::normalizedSyncName(Str("é"), nfcName, UnicodeNormalization::NFC);

    SyncName nfdName;
    (void) CommonUtility::normalizedSyncName(Str("é"), nfdName, UnicodeNormalization::NFD);

    SyncNameSet result;
    for (const auto &n1: {nfcName, nfdName}) {
        for (const auto &n2: {nfcName, nfdName}) {
            for (const auto &n3: {nfcName, nfdName}) {
                SyncName concatenated =
                        n1 + CommonUtility::preferredPathSeparator() + n2 + CommonUtility::preferredPathSeparator() + n3;
                (void) result.emplace(concatenated);
            }
        }
    }

    return result;
}
} // namespace

void TestUtility::testComputePathNormalizations() {
    const SyncName path1 = Str("/é/è");
    auto normalizations = CommonUtility::computePathNormalizations(path1);
    CPPUNIT_ASSERT_EQUAL(size_t{4}, normalizations.size());

    const SyncName path2 = Str("à/é/");
    normalizations = CommonUtility::computePathNormalizations(path2);
    CPPUNIT_ASSERT_EQUAL(size_t{4}, normalizations.size());

    const SyncName path3 = Str("é/é/é");
    normalizations = CommonUtility::computePathNormalizations(path3);
    CPPUNIT_ASSERT_EQUAL(size_t{8}, normalizations.size());
    CPPUNIT_ASSERT(computeExpectedPathNormalizations() == normalizations);

#if defined(KD_WINDOWS)
    const SyncName path4 = Str("é/é\\é");
    normalizations = CommonUtility::computePathNormalizations(path4);
    CPPUNIT_ASSERT_EQUAL(size_t{8}, normalizations.size());
    CPPUNIT_ASSERT(computeExpectedPathNormalizations() == normalizations);
#endif
}

void TestUtility::testStartsWith() {
    CPPUNIT_ASSERT(CommonUtility::startsWith(SyncName(Str("abcdefg")), SyncName(Str("abcd"))));
    CPPUNIT_ASSERT(!CommonUtility::startsWith(SyncName(Str("abcdefg")), SyncName(Str("ABCD"))));
}

void TestUtility::testStartsWithInsensitive() {
    CPPUNIT_ASSERT(CommonUtility::startsWithInsensitive(SyncName(Str("abcdefg")), SyncName(Str("aBcD"))));
    CPPUNIT_ASSERT(CommonUtility::startsWithInsensitive(SyncName(Str("abcdefg")), SyncName(Str("ABCD"))));
}

void TestUtility::testEndsWith() {
    CPPUNIT_ASSERT(CommonUtility::endsWith(SyncName(Str("abcdefg")), SyncName(Str("defg"))));
    CPPUNIT_ASSERT(!CommonUtility::endsWith(SyncName(Str("abcdefg")), SyncName(Str("abc"))));
    CPPUNIT_ASSERT(!CommonUtility::endsWith(SyncName(Str("abcdefg")), SyncName(Str("dEfG"))));
}

void TestUtility::testEndsWithInsensitive() {
    CPPUNIT_ASSERT(CommonUtility::endsWithInsensitive(SyncName(Str("abcdefg")), SyncName(Str("defg"))));
    CPPUNIT_ASSERT(!CommonUtility::endsWithInsensitive(SyncName(Str("abcdefg")), SyncName(Str("abc"))));
    CPPUNIT_ASSERT(CommonUtility::endsWithInsensitive(SyncName(Str("abcdefg")), SyncName(Str("dEfG"))));
}

void TestUtility::testToUpper() {
    CPPUNIT_ASSERT_EQUAL(std::string("ABC"), CommonUtility::toUpper("abc"));
    CPPUNIT_ASSERT_EQUAL(std::string("ABC"), CommonUtility::toUpper("ABC"));
    CPPUNIT_ASSERT_EQUAL(std::string("ABC"), CommonUtility::toUpper("AbC"));
    CPPUNIT_ASSERT_EQUAL(std::string(""), CommonUtility::toUpper(""));
    CPPUNIT_ASSERT_EQUAL(std::string("123"), CommonUtility::toUpper("123"));

    CPPUNIT_ASSERT_EQUAL(std::string("²&é~\"#'{([-|`è_\\ç^à@)]}=+*ù%µ£¤§:;,!.?/"),
                         CommonUtility::toUpper("²&é~\"#'{([-|`è_\\ç^à@)]}=+*ù%µ£¤§:;,!.?/"));
}

void TestUtility::testToLower() {
    CPPUNIT_ASSERT_EQUAL(std::string("abc"), CommonUtility::toLower("abc"));
    CPPUNIT_ASSERT_EQUAL(std::string("abc"), CommonUtility::toLower("ABC"));
    CPPUNIT_ASSERT_EQUAL(std::string("abc"), CommonUtility::toLower("AbC"));
    CPPUNIT_ASSERT_EQUAL(std::string(""), CommonUtility::toLower(""));
    CPPUNIT_ASSERT_EQUAL(std::string("123"), CommonUtility::toLower("123"));

    CPPUNIT_ASSERT_EQUAL(std::string("²&é~\"#'{([-|`è_\\ç^à@)]}=+*ù%µ£¤§:;,!.?/"),
                         CommonUtility::toUpper("²&é~\"#'{([-|`è_\\ç^à@)]}=+*ù%µ£¤§:;,!.?/"));
}

void TestUtility::testIsSameOrParentPath() {
    CPPUNIT_ASSERT(!CommonUtility::isDescendantOrEqual("", "a"));
    CPPUNIT_ASSERT(!CommonUtility::isDescendantOrEqual("a", "a/b"));
    CPPUNIT_ASSERT(!CommonUtility::isDescendantOrEqual("a", "a/b/c"));
    CPPUNIT_ASSERT(!CommonUtility::isDescendantOrEqual("a/b", "a/b/c"));
    CPPUNIT_ASSERT(!CommonUtility::isDescendantOrEqual("a/b/c", "a/b/c1"));
    CPPUNIT_ASSERT(!CommonUtility::isDescendantOrEqual("a/b/c1", "a/b/c"));
    CPPUNIT_ASSERT(!CommonUtility::isDescendantOrEqual("/a/b/c", "a/b/c"));

    CPPUNIT_ASSERT(CommonUtility::isDescendantOrEqual("", ""));
    CPPUNIT_ASSERT(CommonUtility::isDescendantOrEqual("a/b/c", "a/b/c"));
    CPPUNIT_ASSERT(CommonUtility::isDescendantOrEqual("a", ""));
    CPPUNIT_ASSERT(CommonUtility::isDescendantOrEqual("a/b/c", "a/b"));
    CPPUNIT_ASSERT(CommonUtility::isDescendantOrEqual("a/b/c", "a"));
}

void TestUtility::testFileSystemName() {
#if defined(KD_MACOS)
    CPPUNIT_ASSERT(CommonUtility::fileSystemName("/") == "apfs");
    CPPUNIT_ASSERT(CommonUtility::fileSystemName("/bin") == "apfs");
#elif defined(KD_WINDOWS)
    CPPUNIT_ASSERT(CommonUtility::fileSystemName(std::filesystem::temp_directory_path()) == "NTFS");
    // CPPUNIT_ASSERT(CommonUtility::fileSystemName(R"(C:\)") == "NTFS");
    // CPPUNIT_ASSERT(CommonUtility::fileSystemName(R"(C:\windows)") == "NTFS");
#endif
}

void TestUtility::testS2ws() {
    CPPUNIT_ASSERT(CommonUtility::s2ws("abcd") == L"abcd");
    CPPUNIT_ASSERT(CommonUtility::s2ws("éèêà") == L"éèêà");
}

void TestUtility::testWs2s() {
    CPPUNIT_ASSERT(CommonUtility::ws2s(L"abcd") == "abcd");
    CPPUNIT_ASSERT(CommonUtility::ws2s(L"éèêà") == "éèêà");
}

void TestUtility::testLtrim() {
    CPPUNIT_ASSERT(CommonUtility::ltrim("    ab    cd    ") == "ab    cd    ");
}

void TestUtility::testRtrim() {
    CPPUNIT_ASSERT(CommonUtility::rtrim("    ab    cd    ") == "    ab    cd");
}

void TestUtility::testTrim() {
    CPPUNIT_ASSERT(CommonUtility::trim("    ab    cd    ") == "ab    cd");
}

} // namespace KDC

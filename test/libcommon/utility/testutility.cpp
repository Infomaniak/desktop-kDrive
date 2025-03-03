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
#include "libcommon/utility/utility.h"
#include "libcommon/utility/sourcelocation.h"
#include "libcommonserver/io/iohelper.h"
#include "test_utility/localtemporarydirectory.h"
#include <iostream>
#include <regex>

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
    IoError error = IoError::Unknown;
    CPPUNIT_ASSERT(IoHelper::checkIfPathExists(outPath, exists, error));
    CPPUNIT_ASSERT_EQUAL(IoError::Success, error);
    CPPUNIT_ASSERT(exists);

    // Test with a non empty file
    file.open(filePath);
    for (int i = 0; i < 100; i++) {
        file << "test" << std::endl;
    }
    file.close();

    uint64_t size = 0;
    CPPUNIT_ASSERT(IoHelper::getFileSize(filePath, size, error));
    CPPUNIT_ASSERT_EQUAL(IoError::Success, error);

    outPath = tmpDir.path() / "resFile.zip";
    CommonUtility::compressFile(filePath.string(), outPath.string());

    CPPUNIT_ASSERT(IoHelper::checkIfPathExists(outPath, exists, error));
    CPPUNIT_ASSERT_EQUAL(IoError::Success, error);
    CPPUNIT_ASSERT(exists);

    uint64_t compressedSize = 0;
    CPPUNIT_ASSERT(IoHelper::getFileSize(outPath, compressedSize, error));
    CPPUNIT_ASSERT_EQUAL(IoError::Success, error);
    CPPUNIT_ASSERT(compressedSize < size);

    // Test with a non existing file
    outPath = tmpDir.path() / "resNonExistingFile.zip";
    CPPUNIT_ASSERT(!CommonUtility::compressFile("nonExistingFile.txt", outPath.string()));
    CPPUNIT_ASSERT(IoHelper::checkIfPathExists(outPath, exists, error));
    CPPUNIT_ASSERT_EQUAL(IoError::Success, error);
    CPPUNIT_ASSERT(!exists);

    // Test with a non existing output dir
    outPath = tmpDir.path() / "nonExistingDir" / "resNonExistingDir.zip";
    CPPUNIT_ASSERT(!CommonUtility::compressFile(filePath.string(), outPath.string()));
    CPPUNIT_ASSERT(IoHelper::checkIfPathExists(outPath, exists, error));
    CPPUNIT_ASSERT_EQUAL(IoError::Success, error);
    CPPUNIT_ASSERT(!exists);

    // Test with wstring path
    outPath = tmpDir.path() / "resWstring.zip";
    CommonUtility::compressFile(filePath.wstring(), outPath.wstring());
    CPPUNIT_ASSERT(IoHelper::checkIfPathExists(outPath, exists, error));
    CPPUNIT_ASSERT_EQUAL(IoError::Success, error);
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
    CPPUNIT_ASSERT_EQUAL(std::string("en"), CommonUtility::languageCode(Language::English).toStdString());
    CPPUNIT_ASSERT_EQUAL(std::string("fr"), CommonUtility::languageCode(Language::French).toStdString());
    CPPUNIT_ASSERT_EQUAL(std::string("de"), CommonUtility::languageCode(Language::German).toStdString());
    CPPUNIT_ASSERT_EQUAL(std::string("es"), CommonUtility::languageCode(Language::Spanish).toStdString());
    CPPUNIT_ASSERT_EQUAL(std::string("it"), CommonUtility::languageCode(Language::Italian).toStdString());
    // English is the default language and is always returned of the provided language code is unknown.
    CPPUNIT_ASSERT_EQUAL(std::string("en"), CommonUtility::languageCode(Language::Default).toStdString());
    CPPUNIT_ASSERT_EQUAL(std::string("en"), CommonUtility::languageCode(static_cast<Language>(18)).toStdString());
}

#ifdef _WIN32
void TestUtility::testGetLastErrorMessage() {
    // No actual error. Display the expected success message.
    {
        const std::wstring msg = CommonUtility::getLastErrorMessage();
        CPPUNIT_ASSERT(msg.starts_with(L"(0) - "));
    }
    // Display the file-not-found error message.
    {
        GetFileAttributesW(L"this_file_does_not_exist.txt");
        const std::wstring msg = CommonUtility::getLastErrorMessage();
        CPPUNIT_ASSERT(msg.starts_with(L"(2) - "));
    }
}
#endif

void TestUtility::testTruncateLongLogMessage() {
    // No truncation
    {
        const QString message = "short";
        CPPUNIT_ASSERT_EQUAL(std::string("short"), CommonUtility::truncateLongLogMessage(message).toStdString());
    }

    // Truncation of one character
    {
        const auto message = std::string(2049, 'a');
        const QString truncatedMessage = CommonUtility::truncateLongLogMessage(QString::fromStdString(message));
        CPPUNIT_ASSERT(QString::fromStdString(message.substr(0, 2048) + std::string(" (truncated)")) == truncatedMessage);
    }
}
} // namespace KDC

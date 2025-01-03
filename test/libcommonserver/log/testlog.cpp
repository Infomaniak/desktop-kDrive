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

#include "test_utility/testhelpers.h"
#include "testlog.h"

#include "libcommonserver/log/log.h"
#include "libcommonserver/utility/utility.h"
#include "libcommonserver/io/iohelper.h"
#include "libcommon/utility/utility.h"

#include <config.h>
#include <log/customrollingfileappender.h>
#include <log4cplus/loggingmacros.h>

#include <iostream>
#include <chrono>

using namespace CppUnit;
namespace KDC {

void TestLog::setUp() {
    _logger = Log::instance()->getLogger();
    _logDir = Log::instance()->getLogFilePath().parent_path();
}

void TestLog::testLog() {
    LOG_DEBUG(_logger, "Test debug log " << (int) 1 << " " << true << " " << (double) 1.0);
    LOG_INFO(_logger, "Test info log " << (unsigned int) 2 << " " << false << " " << (float) 1.0);
    LOG_WARN(_logger, "Test warn log " << (long unsigned int) 3 << " " << false << " " << std::error_code{});
    LOG_ERROR(_logger, "Test error log " << (long long unsigned int) 4 << " " << true);
    const QIODevice *device = nullptr;
    LOG_FATAL(_logger, "Test fatal log" << std::error_code{} << " " << device);

    LOGW_DEBUG(_logger, L"Test debug log " << (int) 1 << L" " << true << L" " << (double) 1.0);
    LOGW_INFO(_logger, L"Test info log " << (unsigned int) 2 << L" " << false << L" " << (float) 1.0);
    LOGW_WARN(_logger, L"Test warn log " << (long unsigned int) 3 << L" " << false << L" " << std::error_code{});
    LOGW_ERROR(_logger, L"Test error log " << (long long unsigned int) 4 << L" " << true);
    LOGW_FATAL(_logger, L"Test fatal log " << std::error_code{} << L" " << device << L" " << (long unsigned int) 5);

    // Wide characters are mandatory in this case as a bare call to LOG4CPLUS_DEBUG fails to print the string below.
    LOGW_DEBUG(_logger, L"Test debug log " << L" " << L"家屋香袈睷晦");

    CPPUNIT_ASSERT(true);
}

void TestLog::testLargeLogRolling(void) {
    clearLogDirectory();
    log4cplus::SharedAppenderPtr rfAppenderPtr = _logger.getAppender(Log::rfName);
    auto *customRollingFileAppender = static_cast<CustomRollingFileAppender *>(rfAppenderPtr.get());

    const int maxSize = 1024; // 1KB
    const long previousMaxSize = customRollingFileAppender->getMaxFileSize();
    customRollingFileAppender->setMaxFileSize(maxSize);

    LOG_DEBUG(_logger, "Ensure the log file is created");

    CPPUNIT_ASSERT_GREATER(1, countFilesInDirectory(_logDir));

    // Generate a log larger than the max log file size. (log header is 50bytes)
    const auto testLog = std::string(maxSize, 'a');
    LOG_DEBUG(_logger, testLog.c_str());

    CPPUNIT_ASSERT_GREATER(2, countFilesInDirectory(_logDir));

    SyncPath rolledFile = _logDir / (Log::instance()->getLogFilePath().filename().string() + ".1.gz");
    std::error_code ec;
    CPPUNIT_ASSERT(std::filesystem::exists(rolledFile, ec));
    CPPUNIT_ASSERT(!ec);

    // Restore the previous max file size
    customRollingFileAppender->setMaxFileSize(previousMaxSize);
}

void TestLog::testExpiredLogFiles(void) {
    // This test checks that old archived log files are deleted after a certain time
    clearLogDirectory();

    // Generate a fake old log file
    std::ofstream fakeLogFile(_logDir / APPLICATION_NAME "_fake.log.gz");
    fakeLogFile << "Fake old log file" << std::endl;
    fakeLogFile.close();

    // Ensure that a new log file is created
    LOG_INFO(_logger, "Test log file expiration"); 

    // Check that we got 2 log files (the current one and the fake old one)
    CPPUNIT_ASSERT_EQUAL(2, countFilesInDirectory(_logDir)); 

    // Set the expiration time to 2 seconds
    auto *appender = static_cast<CustomRollingFileAppender *>(_logger.getAppender(Log::rfName).get());
    appender->setExpire(2); // 2 seconds

    const auto start = std::chrono::system_clock::now();
    auto now = std::chrono::system_clock::now();
    while (now - start < std::chrono::seconds(3)) {
        now = std::chrono::system_clock::now();
        KDC::testhelpers::setModificationDate(Log::instance()->getLogFilePath(),
                                              now); // Prevent the current log file from being deleted.
        appender->checkForExpiredFiles();
        if (now - start < std::chrono::milliseconds(1500)) { // The fake log file should not be deleted yet.
            CPPUNIT_ASSERT_EQUAL(2, countFilesInDirectory(_logDir));
        } else if (countFilesInDirectory(_logDir) == 1) { // The fake log file MIGHT be deleted now.
            break;        
        }
        Utility::msleep(500);
    }

    CPPUNIT_ASSERT_EQUAL(1, countFilesInDirectory(_logDir)); // The fake log file SHOULD be deleted now.
    appender->setExpire(CommonUtility::logsPurgeRate * 24 * 3600);
}

int TestLog::countFilesInDirectory(const SyncPath &directory) const {
    bool endOfDirectory = false;
    IoError ioError = IoError::Success;
    IoHelper::DirectoryIterator dirIt(directory, false, ioError);
    CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);

    int count = 0;
    DirectoryEntry entry;
    while (dirIt.next(entry, endOfDirectory, ioError) && !endOfDirectory) {
        CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
        count++;
    }
    CPPUNIT_ASSERT(endOfDirectory);

    return count;
}

void TestLog::clearLogDirectory(void) const {
    // Clear the log directory
    IoError ioError = IoError::Success;
    IoHelper::DirectoryIterator dirIt(_logDir, false, ioError);

    bool endOfDirectory = false;
    DirectoryEntry entry;
    while (dirIt.next(entry, endOfDirectory, ioError) && !endOfDirectory && ioError == IoError::Success) {
        if (entry.path().filename().string() == Log::instance()->getLogFilePath().filename().string()) {
            continue;
        }
        IoHelper::deleteItem(entry.path(), ioError);
        CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);

    }
    CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
    CPPUNIT_ASSERT(endOfDirectory);
    CPPUNIT_ASSERT_EQUAL(1, countFilesInDirectory(_logDir));
}
} // namespace KDC

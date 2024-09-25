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

#include "testlog.h"
#include "libcommonserver/log/log.h"
#include "libcommonserver/utility/utility.h"
#include "test_utility/localtemporarydirectory.h"
#include "libcommonserver/io/iohelper.h"
#include "libcommon/utility/utility.h"

#include <config.h>
#include <log4cplus/loggingmacros.h>
#include <iostream>
#include <log/customrollingfileappender.h>

using namespace CppUnit;
namespace KDC {

void TestLog::setUp() {
    _logger = Log::instance()->getLogger();
    _logDir = Log::instance()->getLogFilePath().parent_path();
}

void TestLog::testLog() {
    LOG4CPLUS_TRACE(_logger, "Test trace log");
    LOG4CPLUS_DEBUG(_logger, "Test debug log");
    LOG4CPLUS_INFO(_logger, "Test info log");
    LOG4CPLUS_WARN(_logger, "Test warn log");
    LOG4CPLUS_ERROR(_logger, "Test error log");
    LOG4CPLUS_FATAL(_logger, "Test fatal log");

    LOG4CPLUS_DEBUG(_logger, L"家屋香袈睷晦");

    CPPUNIT_ASSERT(true);
}

void TestLog::testLargeLogRolling(void) {
    clearLogDirectory();
    log4cplus::SharedAppenderPtr rfAppenderPtr = _logger.getAppender(Log::rfName);
    auto *customRollingFileAppender = static_cast<CustomRollingFileAppender *>(rfAppenderPtr.get());

    const int maxSize = 1024; // 1KB
    const int previousMaxSize = customRollingFileAppender->getMaxFileSize();
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
    // This test check that old archived log files are deleted after a certain time
    clearLogDirectory();

    // Generate a fake log file
    std::ofstream fakeLogFile(_logDir / APPLICATION_NAME "_fake.log.gz");
    fakeLogFile << "Fake old log file" << std::endl;
    fakeLogFile.close();
    LOG_INFO(_logger, "Test log file expiration"); // Ensure the log file is created

    CPPUNIT_ASSERT_EQUAL(2, countFilesInDirectory(_logDir)); // The current log file and the fake archived log file

    auto *appender = static_cast<CustomRollingFileAppender *>(_logger.getAppender(Log::rfName).get());
    appender->setExpire(2); // 1 seconds
    Utility::msleep(1000);
    appender->checkForExpiredFiles();
    CPPUNIT_ASSERT_EQUAL(2, countFilesInDirectory(_logDir)); // The fake log file should not be deleted (< 2 seconds)

    Utility::msleep(1000);
    appender->checkForExpiredFiles();
    CPPUNIT_ASSERT_EQUAL(1, countFilesInDirectory(_logDir)); // The fake log file should be deleted
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
    }
    CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
    CPPUNIT_ASSERT(endOfDirectory);
    CPPUNIT_ASSERT_EQUAL(1, countFilesInDirectory(_logDir));
}
} // namespace KDC

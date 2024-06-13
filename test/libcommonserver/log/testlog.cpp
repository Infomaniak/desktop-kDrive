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
#include "test_utility/temporarydirectory.h"
#include "libcommonserver/io/iohelper.h"
#include "libcommon/utility/utility.h"

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
    auto customRollingFileAppender = static_cast<CustomRollingFileAppender*>(rfAppenderPtr.get());

    const int maxSize = 1024 * 1024 * 1;  // 1MB
    const int previousMaxSize = customRollingFileAppender->getMaxFileSize();
    customRollingFileAppender->setMaxFileSize(maxSize);

    // Generate a log larger than the max log file size. (log header is 50bytes)
    const std::string testLog = std::string(maxSize, 'a');
    LOG_DEBUG(_logger, testLog.c_str());

    customRollingFileAppender->setMaxFileSize(previousMaxSize);
    // Check that a new log file has been created (might be 3 files if the "old" log file is already archived)
    CPPUNIT_ASSERT_GREATER(1, countFilesInDirectory(_logDir));
}

void TestLog::testExpiredLogFiles(void) {
    clearLogDirectory();

    // Generate a fake log file
    std::ofstream fakeLogFile(_logDir / APPLICATION_NAME "_fake.log.gz");
    fakeLogFile << "Fake old log file" << std::endl;
    fakeLogFile.close();
    CPPUNIT_ASSERT_EQUAL(2, countFilesInDirectory(_logDir));

    LOG_DEBUG(_logger, "Ensure the log file is not older than 5s");

    log4cplus::SharedAppenderPtr rfAppenderPtr = _logger.getAppender(Log::rfName);
    static_cast<CustomRollingFileAppender*>(rfAppenderPtr.get())->setExpire(5);
    Utility::msleep(2000);
    LOG_DEBUG(_logger, "Ensure the two log files do not expire at the same time.");  // No log file should be deleted
    CPPUNIT_ASSERT_EQUAL(2, countFilesInDirectory(_logDir));
    Utility::msleep(4000);  // Wait for the fake log file to expire
    static_cast<CustomRollingFileAppender*>(rfAppenderPtr.get())
        ->setExpire(5);                                  // Force the check of exspired files at the next log
    LOG_DEBUG(_logger, "Log to trigger the appender.");  // Generate a log to trigger the appender
    CPPUNIT_ASSERT_EQUAL(1, countFilesInDirectory(_logDir));
}

int TestLog::countFilesInDirectory(const SyncPath& directory) const {
    bool endOfDirectory = false;
    IoError ioError = IoErrorSuccess;
    IoHelper::DirectoryIterator dirIt(directory, false, ioError);
    CPPUNIT_ASSERT_EQUAL(IoErrorSuccess, ioError);

    int count = 0;
    DirectoryEntry entry;
    while (dirIt.next(entry, endOfDirectory, ioError) && !endOfDirectory) {
        CPPUNIT_ASSERT_EQUAL(IoErrorSuccess, ioError);
        count++;
    }
    CPPUNIT_ASSERT(endOfDirectory);
    return count;
}

void TestLog::clearLogDirectory(void) const {
    // Clear the log directory
    IoError ioError = IoErrorSuccess;
    IoHelper::DirectoryIterator dirIt(_logDir, false, ioError);

    bool endOfDirectory = false;
    DirectoryEntry entry;
    while (dirIt.next(entry, endOfDirectory, ioError) && !endOfDirectory && ioError == IoErrorSuccess) {
        if (entry.path().filename().string() == Log::instance()->getLogFilePath().filename().string()) {
            continue;
        }
        IoHelper::deleteDirectory(entry.path(), ioError);
    }
    CPPUNIT_ASSERT_EQUAL(IoErrorSuccess, ioError);
    CPPUNIT_ASSERT(endOfDirectory);
    CPPUNIT_ASSERT_EQUAL(1, countFilesInDirectory(_logDir));
}
}  // namespace KDC

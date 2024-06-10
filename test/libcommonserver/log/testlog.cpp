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

void TestLog::testLogRollingAndExpiration(void) {
    // Clear the log directory
    IoError ioError = IoErrorSuccess;
    IoHelper::DirectoryIterator dirIt(_logDir, false, ioError);

    bool endOfDirectory = false;
    DirectoryEntry entry;
    while (dirIt.next(entry, endOfDirectory, ioError) && !endOfDirectory && ioError == IoErrorSuccess) {
        IoHelper::deleteDirectory(entry.path(), ioError);
    }
    LOG_INFO(_logger, "Ensure the current log file is recreated if it does not exist anymore."); //On Linux and MacOs, the current log file is deleted where on Windows it can't be deleted
    testLargeLogRolling();
    testExpiredLogFiles();
}

void TestLog::testLargeLogRolling(void) {

    CPPUNIT_ASSERT_EQUAL(1, countFilesInDirectory(_logDir));
    // Generate a log larger than the max log file size.
    std::string testLog = "Test info log/";
    for (int i = 0; i < 1000; i++) {
        testLog += "Test info log/";
    }
    uint64_t currentSize = 0;
    std::cout << std::endl << "|......................[100%]|" << std::endl << "|";
    int progressBarProgress = 0;
    while (currentSize < CommonUtility::logMaxSize) {
        currentSize += testLog.size() * sizeof(testLog[0]);
        LOG_DEBUG(_logger, testLog.c_str());
        if (currentSize > CommonUtility::logMaxSize / 100 * progressBarProgress) {
            std::cout << ".";
            progressBarProgress += 5;
        }
    }

    // Check that a new log file
    CPPUNIT_ASSERT_EQUAL(2, countFilesInDirectory(_logDir));
}

void TestLog::testExpiredLogFiles(void) {
    CPPUNIT_ASSERT_EQUAL(2, countFilesInDirectory(_logDir));
    log4cplus::SharedAppenderPtr rfAppenderPtr = _logger.getAppender(Log::rfName);
    static_cast<CustomRollingFileAppender*>(rfAppenderPtr.get())->setExpire(5);
    Utility::msleep(2000);
    LOG_DEBUG(_logger, "Ensure the two log files do not expire at thz same time.");  // No log file should be deleted
    CPPUNIT_ASSERT_EQUAL(2, countFilesInDirectory(_logDir));
    Utility::msleep(4000);  // Wait for the first log file to expire
    static_cast<CustomRollingFileAppender*>(rfAppenderPtr.get())
        ->setExpire(5);                   // Force the check of expired files at the next log
    LOG_DEBUG(_logger, "Test info log");  // Generate a log to trigger the appender
    CPPUNIT_ASSERT_EQUAL(1, countFilesInDirectory(_logDir));
}

int TestLog::countFilesInDirectory(const SyncPath& directory) const {
    bool endOfDirectory = false;
    IoError ioErorr = IoErrorSuccess;
    IoHelper::DirectoryIterator dirIt(directory, false, ioErorr);
    CPPUNIT_ASSERT_EQUAL(IoErrorSuccess, ioErorr);

    int count = 0;
    DirectoryEntry entry;
    while (dirIt.next(entry, endOfDirectory, ioErorr) && !endOfDirectory) {
        CPPUNIT_ASSERT_EQUAL(IoErrorSuccess, ioErorr);
        count++;
    }
    CPPUNIT_ASSERT(endOfDirectory);
    return count;
}
}  // namespace KDC

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

#include "testincludes.h"
#include "testlog.h"
#include "libcommonserver/log/log.h"
#include "test_utility/temporarydirectory.h"
#include "libcommonserver/io/iohelper.h"
#include "libcommon/utility/utility.h"
#include <log4cplus/loggingmacros.h>

#include <iostream>  //TO DO remove

using namespace CppUnit;

namespace KDC {

void TestLog::setUp() {
    _logger = Log::instance()->getLogger();
}

void TestLog::tearDown() {}

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

void TestLog::testGetLogEstimatedSize(void) {
    IoError err = IoErrorSuccess;
    uint64_t size = 0;
    LOG_DEBUG(_logger, "Ensure that the log file is created (test)");
    const bool res = Log::instance()->getLogEstimatedSize(size, err);

    CPPUNIT_ASSERT_EQUAL(true, res);
    CPPUNIT_ASSERT_EQUAL(IoErrorSuccess, err);
    CPPUNIT_ASSERT(size >= 0);
    for (int i = 0; i < 100; i++) {
        LOG_DEBUG(_logger, "Test debug log");
    }
    uint64_t newSize = 0;
    Log::instance()->getLogEstimatedSize(newSize, err);
    CPPUNIT_ASSERT_EQUAL(IoErrorSuccess, err);
    CPPUNIT_ASSERT(newSize > size);
}

void TestLog::testCopyLogsTo(void) {
    {  // Test with old logs
        TemporaryDirectory tempDir;
        LOG_DEBUG(_logger, "Ensure that the log file is created (test)");

        IoError err = IoErrorSuccess;
        uint64_t logDirsize = 0;
        Log::instance()->getLogEstimatedSize(logDirsize, err);
        CPPUNIT_ASSERT_EQUAL(IoErrorSuccess, err);
        CPPUNIT_ASSERT(logDirsize >= 0);

        ExitCause cause = ExitCauseUnknown;
        ExitCode exitCode = Log::instance()->copyLogsTo(tempDir.path, true, cause);
        CPPUNIT_ASSERT_EQUAL(ExitCauseUnknown, cause);
        CPPUNIT_ASSERT_EQUAL(ExitCodeOk, exitCode);

        uint64_t tempDirSize = 0;
        IoHelper::getDirectorySize(tempDir.path, tempDirSize, err);
        CPPUNIT_ASSERT_EQUAL(IoErrorSuccess, err);
        CPPUNIT_ASSERT_EQUAL(logDirsize, tempDirSize);
    }

    {  // Test without old logs
        TemporaryDirectory tempDir;
        SyncPath logDir = Log::instance()->getLogFilePath().parent_path();

        // create a fake log file
        std::ofstream logFile(tempDir.path / "test.log");
        for (int i = 0; i < 10; i++) {
            logFile << "Test log line " << i << std::endl;
        }
        logFile.close();

        // compress the log file
        ExitCause cause = ExitCauseUnknown;
        ExitCode exitCode = Log::instance()->compressLogFiles(tempDir.path, cause);
        CPPUNIT_ASSERT_EQUAL(ExitCauseUnknown, cause);
        CPPUNIT_ASSERT_EQUAL(ExitCodeOk, exitCode);

        // copy the compressed log file to the log directory
        IoError err = IoErrorSuccess;
        CPPUNIT_ASSERT_EQUAL(true, IoHelper::copyFileOrDirectory(tempDir.path / "test.log.gz", logDir / "test.log.gz", err));
        CPPUNIT_ASSERT_EQUAL(IoErrorSuccess, err);

        IoHelper::deleteDirectory(tempDir.path / "test.log.gz", err);

        exitCode = Log::instance()->copyLogsTo(tempDir.path, false, cause);
        IoHelper::deleteDirectory(logDir / "test.log.gz", err);

        CPPUNIT_ASSERT_EQUAL(ExitCauseUnknown, cause);
        CPPUNIT_ASSERT_EQUAL(ExitCodeOk, exitCode);

        // Check we do not have test.log.gz in the temp directory
        bool exists = false;
        CPPUNIT_ASSERT_EQUAL(true, IoHelper::checkIfPathExists(tempDir.path / "test.log.gz", exists, err));
        CPPUNIT_ASSERT_EQUAL(IoErrorNoSuchFileOrDirectory, err);
        CPPUNIT_ASSERT_EQUAL(false, exists);
    }
}

void TestLog::testCopyParmsDbTo(void) {
    {
        TemporaryDirectory tempDir;
        const SyncPath parmsDbName = ".parms.db";
        const SyncPath parmsDbPath = CommonUtility::getAppSupportDir() / parmsDbName;

        uint64_t parmsDbSize = 0;
        IoError err = IoErrorSuccess;
        IoHelper::getFileSize(parmsDbPath, parmsDbSize, err);
        CPPUNIT_ASSERT_EQUAL(IoErrorSuccess, err);
        CPPUNIT_ASSERT(parmsDbSize >= 0);

        ExitCause cause = ExitCauseUnknown;
        ExitCode exitCode = Log::instance()->copyParmsDbTo(tempDir.path, cause);
        CPPUNIT_ASSERT_EQUAL(ExitCauseUnknown, cause);
        CPPUNIT_ASSERT_EQUAL(ExitCodeOk, exitCode);

        uint64_t tempDirSize = 0;
        IoHelper::getDirectorySize(tempDir.path, tempDirSize, err);
        CPPUNIT_ASSERT_EQUAL(IoErrorSuccess, err);
        CPPUNIT_ASSERT_EQUAL(parmsDbSize, tempDirSize);
    }
}

void TestLog::testCompressLogs(void) {
    {
        TemporaryDirectory tempDir;

        std::ofstream logFile(tempDir.path / "test.log");
        for (int i = 0; i < 10000; i++) {
            logFile << "Test log line " << i << std::endl;
        }
        logFile.close();

        const SyncPath logDir = tempDir.path / "log";
        IoError err = IoErrorSuccess;
        CPPUNIT_ASSERT_EQUAL(true, IoHelper::createDirectory(logDir, err));
        CPPUNIT_ASSERT_EQUAL(IoErrorSuccess, err);

        const SyncPath logFilePath = logDir / "test.log";
        CPPUNIT_ASSERT_EQUAL(true, IoHelper::copyFileOrDirectory(tempDir.path / "test.log", logFilePath, err));
        CPPUNIT_ASSERT_EQUAL(IoErrorSuccess, err);

        uint64_t logDirSize = 0;
        CPPUNIT_ASSERT_EQUAL(true, IoHelper::getDirectorySize(logDir, logDirSize, err));
        CPPUNIT_ASSERT_EQUAL(IoErrorSuccess, err);
        CPPUNIT_ASSERT(logDirSize >= 0);

        ExitCause cause = ExitCauseUnknown;
        ExitCode exitCode = Log::instance()->compressLogFiles(tempDir.path, cause);

        CPPUNIT_ASSERT_EQUAL(ExitCauseUnknown, cause);
        CPPUNIT_ASSERT_EQUAL(ExitCodeOk, exitCode);

        uint64_t tempDirSize = 0;
        IoHelper::getDirectorySize(tempDir.path, tempDirSize, err);
        CPPUNIT_ASSERT_EQUAL(IoErrorSuccess, err);
        CPPUNIT_ASSERT(tempDirSize < logDirSize);

        bool exists = false;
        CPPUNIT_ASSERT_EQUAL(true, IoHelper::checkIfPathExists(tempDir.path / "test.log.gz", exists, err));
        CPPUNIT_ASSERT_EQUAL(IoErrorSuccess, err);
        CPPUNIT_ASSERT_EQUAL(true, exists);

        CPPUNIT_ASSERT_EQUAL(true, IoHelper::checkIfPathExists(logDir / "test.log.gz", exists, err));
        CPPUNIT_ASSERT_EQUAL(IoErrorSuccess, err);
        CPPUNIT_ASSERT_EQUAL(true, exists);
    }

    {  // test the progress bar
        TemporaryDirectory tempDir;
        for (int i = 0; i < 30; i++) {
            std::ofstream logFile(tempDir.path / ("test" + std::to_string(i) + ".log"));
            for (int j = 0; j < 10; j++) {
                logFile << "Test log line " << j << std::endl;
            }
            logFile.close();
        }

        int percent = 0;
        int oldPercent = 0;
        std::function<void(int)> progress = [&percent, &oldPercent](int p) {
            percent = p;
            CPPUNIT_ASSERT(percent >= oldPercent);
            oldPercent = percent;
        };

        ExitCause cause = ExitCauseUnknown;

        ExitCode exitCode = Log::instance()->compressLogFiles(tempDir.path, cause, progress);

        CPPUNIT_ASSERT_EQUAL(ExitCauseUnknown, cause);
        CPPUNIT_ASSERT_EQUAL(ExitCodeOk, exitCode);
        CPPUNIT_ASSERT_EQUAL(100, percent);
    }
}

void TestLog::testGenerateUserDescriptionFile(void) {
    {
        TemporaryDirectory tempDir;
        const SyncPath userDescriptionFile = tempDir.path / "user_description.txt";
        ExitCause cause = ExitCauseUnknown;
        ExitCode code = Log::instance()->generateUserDescriptionFile(userDescriptionFile, cause);
        CPPUNIT_ASSERT_EQUAL(ExitCauseUnknown, cause);
        CPPUNIT_ASSERT_EQUAL(ExitCodeOk, code);

        bool exists = false;
        IoError err = IoErrorSuccess;
        CPPUNIT_ASSERT_EQUAL(true, IoHelper::checkIfPathExists(userDescriptionFile, exists, err));
        CPPUNIT_ASSERT_EQUAL(IoErrorSuccess, err);
        CPPUNIT_ASSERT_EQUAL(true, exists);

        // Chack there is at least 5 lines in the file
        std::ifstream file(userDescriptionFile);
        std::string line;
        int count = 0;
        while (std::getline(file, line)) {
            count++;
        }
        CPPUNIT_ASSERT(count >= 5);
        file.close();
    }
}

void TestLog::testGenerateLogsSupportArchive(void) {
    {
        TemporaryDirectory tempDir;
        const SyncPath archiveFile = tempDir.path / "logs_support.tar.gz";
        ExitCause cause = ExitCauseUnknown;

        ExitCode code = Log::instance()->generateLogsSupportArchive(true, tempDir.path, archiveFile.filename(), cause);
        CPPUNIT_ASSERT_EQUAL(ExitCauseUnknown, cause);
        CPPUNIT_ASSERT_EQUAL(ExitCodeOk, code);

        bool exists = false;
        IoError err = IoErrorSuccess;
        CPPUNIT_ASSERT_EQUAL(true, IoHelper::checkIfPathExists(archiveFile, exists, err));
        CPPUNIT_ASSERT_EQUAL(IoErrorSuccess, err);
        CPPUNIT_ASSERT_EQUAL(true, exists);
    }
}

}  // namespace KDC

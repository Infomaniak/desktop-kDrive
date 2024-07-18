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

#include "testlogarchiver.h"
#include "server/logarchiver.h"
#include "libcommonserver/log/log.h"
#include "test_utility/localtemporarydirectory.h"
#include "libcommonserver/io/iohelper.h"
#include "libcommon/utility/utility.h"
#include "libcommonserver/db/db.h"

#include <log4cplus/loggingmacros.h>

#include <iostream>

using namespace CppUnit;

namespace KDC {

void TestLogArchiver::setUp() {
    _logger = Log::instance()->getLogger();
    bool alreadyExist = false;
    Db::makeDbName(alreadyExist);
}

void TestLogArchiver::testGetLogEstimatedSize(void) {
    IoError err = IoErrorSuccess;
    uint64_t size = 0;
    LOG_DEBUG(_logger, "Ensure that the log file is created (test)");
    const bool res = LogArchiver::getLogDirEstimatedSize(size, err);

    CPPUNIT_ASSERT(res);
    CPPUNIT_ASSERT_EQUAL(IoErrorSuccess, err);
    CPPUNIT_ASSERT(size >= 0);
    for (int i = 0; i < 100; i++) {
        LOG_DEBUG(_logger, "Test debug log");
    }
    uint64_t newSize = 0;
    LogArchiver::getLogDirEstimatedSize(newSize, err);
    CPPUNIT_ASSERT_EQUAL(IoErrorSuccess, err);
    CPPUNIT_ASSERT(newSize > size);
}

void TestLogArchiver::testCopyLogsTo(void) {
    {  // Test with archivedLogs
        LocalTemporaryDirectory tempDir;
        LOG_DEBUG(_logger, "Ensure that the log file is created (test)");

        IoError err = IoErrorSuccess;
        uint64_t logDirsize = 0;

        LogArchiver::getLogDirEstimatedSize(logDirsize, err);
        CPPUNIT_ASSERT_EQUAL(IoErrorSuccess, err);
        CPPUNIT_ASSERT(logDirsize >= 0);

        ExitCause cause = ExitCauseUnknown;
        ExitCode exitCode = LogArchiver::copyLogsTo(tempDir.path, true, cause);
        CPPUNIT_ASSERT_EQUAL(ExitCauseUnknown, cause);
        CPPUNIT_ASSERT_EQUAL(ExitCodeOk, exitCode);

        uint64_t tempDirSize = 0;
        bool tooDeep = false;
        IoHelper::getDirectorySize(tempDir.path, tempDirSize, err, 0);
        CPPUNIT_ASSERT(err == IoErrorSuccess || err == IoErrorMaxDepthExceeded);
        CPPUNIT_ASSERT_GREATER(logDirsize, tempDirSize);
    }

    {  // Test without archivedLogs
        LocalTemporaryDirectory tempDir;
        SyncPath logDir = Log::instance()->getLogFilePath().parent_path();

        // create a fake log file
        std::ofstream logFile(tempDir.path / "test.log");
        CPPUNIT_ASSERT(logFile.is_open());
        logFile << "Test log line." << std::endl;
        logFile.close();

        // compress the log file
        ExitCause cause = ExitCauseUnknown;
        ExitCode exitCode = LogArchiver::compressLogFiles(tempDir.path, nullptr, cause);
        CPPUNIT_ASSERT_EQUAL(ExitCauseUnknown, cause);
        CPPUNIT_ASSERT_EQUAL(ExitCodeOk, exitCode);

        // copy the compressed log file to the log directory
        IoError err = IoErrorSuccess;
        CPPUNIT_ASSERT_EQUAL(true, IoHelper::copyFileOrDirectory(tempDir.path / "test.log.gz", logDir / "test.log.gz", err));
        CPPUNIT_ASSERT_EQUAL(IoErrorSuccess, err);

        IoHelper::deleteDirectory(tempDir.path / "test.log.gz", err);

        exitCode = LogArchiver::copyLogsTo(tempDir.path, false, cause);
        IoHelper::deleteDirectory(logDir / "test.log.gz", err);

        CPPUNIT_ASSERT_EQUAL(ExitCauseUnknown, cause);
        CPPUNIT_ASSERT_EQUAL(ExitCodeOk, exitCode);

        // Check that `test.log.gz` does not exist anymore.
        bool exists = false;
        CPPUNIT_ASSERT_EQUAL(true, IoHelper::checkIfPathExists(tempDir.path / "test.log.gz", exists, err));
        CPPUNIT_ASSERT_EQUAL(IoErrorSuccess, err);
        CPPUNIT_ASSERT_EQUAL(false, exists);
    }
}

void TestLogArchiver::testCopyParmsDbTo(void) {
    {
        if (!parmsDbFileExist()) {
            std::cout << std::endl << "No .parms.db file, this test will not be relevant (skipped)." << std::endl;
            LOG_WARN(_logger, "No .parms.db file, this test will not be relevant (skipped).");
            return;
        }

        LocalTemporaryDirectory tempDir;
        const SyncPath parmsDbName = ".parms.db";
        const SyncPath parmsDbPath = CommonUtility::getAppSupportDir() / parmsDbName;

        uint64_t parmsDbSize = 0;
        IoError err = IoErrorSuccess;
        IoHelper::getFileSize(parmsDbPath, parmsDbSize, err);


        CPPUNIT_ASSERT_EQUAL(IoErrorSuccess, err);
        CPPUNIT_ASSERT(parmsDbSize >= 0);

        ExitCause cause = ExitCauseUnknown;
        ExitCode exitCode = LogArchiver::copyParmsDbTo(tempDir.path, cause);
        CPPUNIT_ASSERT_EQUAL(ExitCauseUnknown, cause);
        CPPUNIT_ASSERT_EQUAL(ExitCodeOk, exitCode);

        uint64_t tempDirSize = 0;
        bool tooDeep = false;
        IoHelper::getDirectorySize(tempDir.path, tempDirSize, err, 0);
        CPPUNIT_ASSERT(err == IoErrorSuccess || err == IoErrorMaxDepthExceeded);
        CPPUNIT_ASSERT_EQUAL(parmsDbSize, tempDirSize);
    }
}

void TestLogArchiver::testCompressLogs(void) {
    {
        LocalTemporaryDirectory tempDir;

        std::ofstream logFile(tempDir.path / "test.log");
        for (int i = 0; i < 5; i++) {
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

        CPPUNIT_ASSERT_EQUAL(true, IoHelper::getDirectorySize(logDir, logDirSize, err, 0));
        CPPUNIT_ASSERT(err == IoErrorSuccess || err == IoErrorMaxDepthExceeded);
        CPPUNIT_ASSERT(logDirSize >= 0);

        ExitCause cause = ExitCauseUnknown;
        const ExitCode exitCode = LogArchiver::compressLogFiles(tempDir.path, std::function<bool(int)>(), cause);

        CPPUNIT_ASSERT_EQUAL(ExitCauseUnknown, cause);
        CPPUNIT_ASSERT_EQUAL(ExitCodeOk, exitCode);

        uint64_t tempDirSize = 0;
        IoHelper::getDirectorySize(tempDir.path, tempDirSize, err, 0);
        CPPUNIT_ASSERT(err == IoErrorSuccess || err == IoErrorMaxDepthExceeded);
        CPPUNIT_ASSERT(tempDirSize < logDirSize);

        bool exists = false;
        CPPUNIT_ASSERT_EQUAL(true, IoHelper::checkIfPathExists(tempDir.path / "test.log.gz", exists, err));
        CPPUNIT_ASSERT_EQUAL(IoErrorSuccess, err);
        CPPUNIT_ASSERT(exists);

        CPPUNIT_ASSERT_EQUAL(true, IoHelper::checkIfPathExists(logDir / "test.log.gz", exists, err));
        CPPUNIT_ASSERT_EQUAL(IoErrorSuccess, err);
        CPPUNIT_ASSERT(exists);
    }

    {  // test the progress callback
        LOG_DEBUG(_logger, "Test log compression with progress callback");
        LocalTemporaryDirectory tempDir("logArchiver");
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
        for (int i = 0; i < 30; i++) {
            std::ofstream logFile(tempDir.path / ("test" + std::to_string(i) + ".log"));
            for (int j = 0; j < 10; j++) {
                logFile << "Test log line " << j << std::endl;
            }
            logFile.close();
        }

        int percent = 0;
        int oldPercent = 0;
        const std::function<bool(int)> progress = [&percent, &oldPercent](int p) {
            percent = p;
            CPPUNIT_ASSERT(percent >= oldPercent);
            oldPercent = percent;
            return true;
        };

        ExitCause cause = ExitCauseUnknown;
        ExitCode exitCode = LogArchiver::compressLogFiles(tempDir.path, progress, cause);

        CPPUNIT_ASSERT_EQUAL(ExitCauseUnknown, cause);
        CPPUNIT_ASSERT_EQUAL(ExitCodeOk, exitCode);
        CPPUNIT_ASSERT_GREATER(90, percent);
    }

    {  // Test the progress callback with a cancel
        LOG_DEBUG(_logger, "Test log compression with progress callback and cancel");
        LocalTemporaryDirectory tempDir("logArchiver");
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
        for (int i = 0; i < 30; i++) {
            std::ofstream logFile(tempDir.path / ("test" + std::to_string(i) + ".log"));
            for (int j = 0; j < 10; j++) {
                logFile << "Test log line " << j << std::endl;
            }
            logFile.close();
        }

        int percent = 0;
        int oldPercent = 0;
        const std::function<bool(int)> progress = [&percent, &oldPercent](int p) {
            percent = p;
            CPPUNIT_ASSERT(percent >= oldPercent);
            oldPercent = percent;
            return false;
        };

        ExitCause cause = ExitCauseUnknown;
        const ExitCode exitCode = LogArchiver::compressLogFiles(tempDir.path, progress, cause);

        CPPUNIT_ASSERT_EQUAL(ExitCauseOperationCanceled, cause);
        CPPUNIT_ASSERT_EQUAL(ExitCodeOk, exitCode);
    }
}

void TestLogArchiver::testGenerateUserDescriptionFile(void) {
    {
        LocalTemporaryDirectory tempDir;
        const SyncPath userDescriptionFile = tempDir.path / "user_description.txt";
        ExitCause cause = ExitCauseUnknown;
        const ExitCode code = LogArchiver::generateUserDescriptionFile(userDescriptionFile, cause);
        CPPUNIT_ASSERT_EQUAL(ExitCauseUnknown, cause);
        CPPUNIT_ASSERT_EQUAL(ExitCodeOk, code);

        bool exists = false;
        IoError err = IoErrorSuccess;
        CPPUNIT_ASSERT_EQUAL(true, IoHelper::checkIfPathExists(userDescriptionFile, exists, err));
        CPPUNIT_ASSERT_EQUAL(IoErrorSuccess, err);
        CPPUNIT_ASSERT(exists);

        // Check if there is at least 5 lines in the file
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

void TestLogArchiver::testGenerateLogsSupportArchive(void) {
    if (!parmsDbFileExist()) {
        std::cout << std::endl << "No .parms.db file, this test will not be relevant (skipped)." << std::endl;
        LOG_WARN(_logger, "No .parms.db file, this test will not be relevant (skipped).");
        return;
    }

    {  // Test the generation of the archive
        LocalTemporaryDirectory tempDir("GenerateLogsSupportArchive");
        SyncPath archivePath;
        ExitCause cause = ExitCauseUnknown;
        int previousPercent = 0;
        std::function<bool(int)> progress = [&previousPercent](int percent) {
            CPPUNIT_ASSERT(percent >= 0);
            CPPUNIT_ASSERT(percent <= 100);
            CPPUNIT_ASSERT(percent >= previousPercent);
            previousPercent = percent;
            return true;
        };

        const ExitCode code = LogArchiver::generateLogsSupportArchive(true, tempDir.path, progress, archivePath, cause, true);
        CPPUNIT_ASSERT_EQUAL(ExitCauseUnknown, cause);
        CPPUNIT_ASSERT_EQUAL(ExitCodeOk, code);
        CPPUNIT_ASSERT_EQUAL(tempDir.path / archivePath.filename(), archivePath);

        bool exists = false;
        IoError err = IoErrorSuccess;
        CPPUNIT_ASSERT_EQUAL(true, IoHelper::checkIfPathExists(archivePath, exists, err));
        CPPUNIT_ASSERT_EQUAL(IoErrorSuccess, err);
        CPPUNIT_ASSERT(exists);
    }

    {  // Test with a cancel
        LocalTemporaryDirectory tempDir("GenerateLogsSupportArchiveCancel");
        SyncPath archiveFile;
        ExitCause cause = ExitCauseUnknown;
        std::function<bool(int)> progress = [](int) { return false; };

        const ExitCode code = LogArchiver::generateLogsSupportArchive(true, tempDir.path, progress, archiveFile, cause, true);
        CPPUNIT_ASSERT_EQUAL(ExitCauseOperationCanceled, cause);
        CPPUNIT_ASSERT_EQUAL(ExitCodeOk, code);
    }
}

bool TestLogArchiver::parmsDbFileExist() {
    const SyncPath parmsDbName = ".parms.db";
    const SyncPath parmsDbPath = CommonUtility::getAppSupportDir() / parmsDbName;

    IoError err = IoErrorSuccess;
    bool exists = false;

    if (!IoHelper::checkIfPathExists(parmsDbPath, exists, err)) {
        return false;
    }

    return exists;
}

}  // namespace KDC

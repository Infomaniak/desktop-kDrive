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

#include "testio.h"
#include "test_utility/testhelpers.h"
#include "test_utility/timeouthelper.h"

#include <thread>
using namespace CppUnit;

namespace KDC {

bool checkContent(std::ifstream &file) {
    std::string content;
    std::getline(file, content);
    return content.find("test") != std::string::npos;
}

void TestIo::testOpenFileSuccess() {
    LocalTemporaryDirectory tempDir("testOpenFileSuccess");
    SyncPath filePath = tempDir.path() / "testOpenFileSuccess.txt";
    testhelpers::generateOrEditTestFile(filePath);
    std::ifstream file;
    CPPUNIT_ASSERT(IoHelper::openFile(filePath, file));
    CPPUNIT_ASSERT(file.is_open());
    CPPUNIT_ASSERT(checkContent(file));
    file.close();
}

void TestIo::testOpenFileAccessDenied() {
    LocalTemporaryDirectory tempDir("testOpenFileAccessDenied");
    SyncPath filePath = tempDir.path() / "testOpenFileAccessDenied.txt";
    testhelpers::generateOrEditTestFile(filePath);
    IoError ioError;
    IoHelper::setRights(filePath, false, true, true, ioError);
    CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);

    // Without timeout
    std::ifstream file;
    ExitInfo openFileResult;
    CPPUNIT_ASSERT(TimeoutHelper::checkExecutionTime<ExitInfo>([&]() { return IoHelper::openFile(filePath, file, 0); },
                                                               openFileResult, std::chrono::milliseconds(200)));

    CPPUNIT_ASSERT_EQUAL(ExitInfo(ExitCode::SystemError, ExitCause::FileAccessError), openFileResult);
    CPPUNIT_ASSERT(!file.is_open());

    // Check timeout
    CPPUNIT_ASSERT(TimeoutHelper::checkExecutionTime<ExitInfo>([&]() { return IoHelper::openFile(filePath, file, 1); },
                                                               openFileResult, std::chrono::milliseconds(1000),
                                                               std::chrono::milliseconds(2000)));

    CPPUNIT_ASSERT_EQUAL(ExitInfo(ExitCode::SystemError, ExitCause::FileAccessError), openFileResult);
    CPPUNIT_ASSERT(!file.is_open());
}

void TestIo::testOpenFileNonExisting() {
    LocalTemporaryDirectory tempDir("testOpenFileNonExisting");
    SyncPath filePath = tempDir.path() / "testOpenFileNonExisting.txt";
    std::ifstream file;
    ExitInfo openFileResult;
    CPPUNIT_ASSERT(TimeoutHelper::checkExecutionTime<ExitInfo>([&]() { return IoHelper::openFile(filePath, file, 5); },
                                                               openFileResult, std::chrono::milliseconds(200)));

    CPPUNIT_ASSERT_EQUAL(ExitInfo(ExitCode::SystemError, ExitCause::NotFound), openFileResult);
    CPPUNIT_ASSERT(!file.is_open());
}

void TestIo::testOpenLockedFileRemovedBeforeTimedOut() {
    LocalTemporaryDirectory tempDir("testOpenLockedFileRemovedBeforeTimedOut");
    SyncPath filePath = tempDir.path() / "testOpenLockedFileRemovedBeforeTimedOut.txt";
    testhelpers::generateOrEditTestFile(filePath);
    IoError ioError;
    IoHelper::setRights(filePath, false, true, true, ioError);
    CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);

    std::ifstream file;
    ExitInfo exitInfo = IoHelper::openFile(filePath, file, 5);
    CPPUNIT_ASSERT_EQUAL(ExitInfo(ExitCode::SystemError, ExitCause::FileAccessError), exitInfo);
    CPPUNIT_ASSERT(!file.is_open());

    IoHelper::setRights(filePath, false, true, true, ioError);
    CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);

    std::function restoreRights = [filePath, &ioError]() {
        Utility::msleep(900);
        IoHelper::setRights(filePath, true, true, true, ioError);
        CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
    };

    std::thread restoreRightsThread(restoreRights);

    ExitInfo openFileResult;
    CPPUNIT_ASSERT(TimeoutHelper::checkExecutionTime<ExitInfo>([&]() { return IoHelper::openFile(filePath, file, 4); },
                                                               openFileResult, std::chrono::milliseconds(1000),
                                                               std::chrono::milliseconds(1200)));

    CPPUNIT_ASSERT_EQUAL(ExitInfo(ExitCode::Ok), openFileResult);
    restoreRightsThread.join();
    CPPUNIT_ASSERT(file.is_open());
    CPPUNIT_ASSERT(checkContent(file));
    file.close();
}
} // namespace KDC

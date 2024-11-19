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

#include "testio.h"
#include "test_utility/testhelpers.h"
#include <thread>
using namespace CppUnit;

namespace KDC {

class TimeOutChecker {
    public:
        explicit TimeOutChecker(bool start = false) {
            if (start) this->start();
        }
        void start() { _time = std::chrono::steady_clock::now(); }
        void stop() {
            auto end = std::chrono::steady_clock::now();
            _diff = std::chrono::duration_cast<std::chrono::milliseconds>(end - _time).count();
        }
        bool lessThan(long long value) {
            if (_diff >= value) std::cout << "TimeOutChecker::lessThan: " << _diff << " >= " << value << std::endl;
            return _diff < value;
        }
        bool greaterThan(long long value) {
            if (_diff <= value) std::cout << "TimeOutChecker::greaterThan: " << _diff << " <= " << value << std::endl;
            return _diff > value;
        }
        bool between(long long min, long long max) {
            if (_diff <= min || _diff >= max)
                std::cout << "TimeOutChecker::between: " << _diff << " <= " << min << " || " << _diff << " >= " << max
                          << std::endl;
            return _diff > min && _diff < max;
        }

    private:
        std::chrono::steady_clock::time_point _time;
        long long _diff{0};
};

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
    TimeOutChecker timeOutChecker(true);
    CPPUNIT_ASSERT_EQUAL(ExitInfo(ExitCode::SystemError, ExitCause::FileAccessError), IoHelper::openFile(filePath, file, 0));
    timeOutChecker.stop();
    CPPUNIT_ASSERT(timeOutChecker.lessThan(200));
    CPPUNIT_ASSERT(!file.is_open());

    // Check timeout
    timeOutChecker.start();
    CPPUNIT_ASSERT_EQUAL(ExitInfo(ExitCode::SystemError, ExitCause::FileAccessError), IoHelper::openFile(filePath, file, 1));
    timeOutChecker.stop();
    CPPUNIT_ASSERT(timeOutChecker.between(1000, 1200));
    CPPUNIT_ASSERT(!file.is_open());
}

void TestIo::testOpenFileNonExisting() {
    LocalTemporaryDirectory tempDir("testOpenFileNonExisting");
    SyncPath filePath = tempDir.path() / "testOpenFileNonExisting.txt";
    std::ifstream file;
    TimeOutChecker timeOutChecker(true);
    CPPUNIT_ASSERT_EQUAL(ExitInfo(ExitCode::SystemError, ExitCause::NotFound), IoHelper::openFile(filePath, file, 5));
    timeOutChecker.stop();
    CPPUNIT_ASSERT(timeOutChecker.lessThan(200));
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
    TimeOutChecker timeOutChecker(true);
    CPPUNIT_ASSERT_EQUAL(ExitInfo(ExitCode::Ok), IoHelper::openFile(filePath, file, 4));
    timeOutChecker.stop();
    restoreRightsThread.join();
    CPPUNIT_ASSERT(timeOutChecker.between(1000, 1200));
    CPPUNIT_ASSERT(file.is_open());
    CPPUNIT_ASSERT(checkContent(file));
    file.close();
}
} // namespace KDC

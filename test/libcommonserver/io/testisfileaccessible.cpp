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

#define WIN32_LEAN_AND_MEAN

#include "testio.h"

#include "libcommonserver/utility/utility.h"
#include "jobs/local/localcopyjob.h"
#include "jobs/jobmanager.h"

using namespace CppUnit;

namespace KDC {

void TestIo::testIsFileAccessible() {
    const TemporaryDirectory temporaryDirectory;
    const SyncPath sourcePath = _localTestDirPath / "big_file_dir/test_big_file.mov";

    std::shared_ptr<LocalCopyJob> copyJob =
        std::make_shared<LocalCopyJob>(sourcePath, temporaryDirectory.path / "test_big_file.mov");
    JobManager::instance()->queueAsyncJob(copyJob);

    while (!copyJob->isRunning()) {
        // Just wait for the job to start
    }
    Utility::msleep(10);

    IoError ioError = IoErrorUnknown;
    bool res = IoHelper::isFileAccessible(temporaryDirectory.path / "test_big_file.mov", ioError);
    // IoHelper::isFileAccessible returns instantly `true` on MacOSX and Linux.
#ifdef _WIN32
    CPPUNIT_ASSERT(!res);
#endif

    // File copy not finished yet.
    Utility::msleep(1000);

    // File copy is now finished.
    res = IoHelper::isFileAccessible(temporaryDirectory.path / "test_big_file.mov", ioError);
    CPPUNIT_ASSERT(res);
}

}  // namespace KDC

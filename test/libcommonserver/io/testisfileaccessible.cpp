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
    ;

    std::cout << std::endl;
    std::cout << "Creating job" << std::endl;

    std::shared_ptr<LocalCopyJob> copyJob =
        std::make_shared<LocalCopyJob>(sourcePath, temporaryDirectory.path / "test_big_file.mov");
    JobManager::instance()->queueAsyncJob(copyJob);

    std::cout << "Waiting for job to start" << std::endl;

    while (!copyJob->isRunning()) {
        // Just wait for the job to start
    }
    Utility::msleep(10);

    std::cout << "Job started" << std::endl;

    IoError ioError = IoErrorUnknown;
    bool res = IoHelper::isFileAccessible(temporaryDirectory.path / "test_big_file.mov", ioError);
    std::cout << "res: " << res << ", ioError: " << ioError << std::endl;
    CPPUNIT_ASSERT(!res);
    std::cout << "File copy not finished" << std::endl;
    Utility::msleep(1000);
    res = IoHelper::isFileAccessible(temporaryDirectory.path / "test_big_file.mov", ioError);
    std::cout << "res: " << res << ", ioError: " << ioError << std::endl;
    CPPUNIT_ASSERT(res);
    std::cout << "File copy is finished!" << std::endl;
}

}  // namespace KDC

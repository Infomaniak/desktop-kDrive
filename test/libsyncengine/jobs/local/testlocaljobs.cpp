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

#include "testlocaljobs.h"

#include <memory>

#include "config.h"
#include "jobs/local/localcreatedirjob.h"
#include "jobs/local/localdeletejob.h"
#include "jobs/local/localmovejob.h"
#include "test_utility/localtemporarydirectory.h"
#include "jobs/local/localcopyjob.h"
#include "requests/parameterscache.h"
#include "test_utility/testhelpers.h"

using namespace CppUnit;

namespace KDC {

void KDC::TestLocalJobs::setUp() {
    // Setup parameter in test mode
    ParametersCache::instance(true);
}

void KDC::TestLocalJobs::testLocalJobs() {
    const LocalTemporaryDirectory temporaryDirectory("testLocalJobs");
    const SyncPath localDirPath = temporaryDirectory.path() / "tmp_dir";

    // Create
    LocalCreateDirJob createJob(localDirPath);
    createJob.runSynchronously();

    CPPUNIT_ASSERT(std::filesystem::exists(localDirPath));

    // Add a file in "tmp_dir"
    const SyncPath picturesPath = testhelpers::localTestDirPath / "test_pictures";
    std::filesystem::copy(picturesPath / "picture-1.jpg", localDirPath / "tmp_picture.jpg");

    // Copy
    const SyncPath copyDirPath = temporaryDirectory.path() / "tmp_dir2";
    LocalCopyJob copyJob(localDirPath, copyDirPath);
    copyJob.runSynchronously();

    CPPUNIT_ASSERT(std::filesystem::exists(copyDirPath));
    CPPUNIT_ASSERT(std::filesystem::exists(copyDirPath / "tmp_picture.jpg"));

    // Move
    LocalMoveJob moveJob(localDirPath, copyDirPath / "tmp_dir");
    moveJob.runSynchronously();

    CPPUNIT_ASSERT(std::filesystem::exists(copyDirPath / "tmp_dir"));
    CPPUNIT_ASSERT(std::filesystem::exists(copyDirPath / "tmp_dir" / "tmp_picture.jpg"));

    // Delete
    LocalDeleteJob deleteJob(copyDirPath);
    deleteJob.runSynchronously();

    CPPUNIT_ASSERT(!std::filesystem::exists(copyDirPath));
}

}  // namespace KDC

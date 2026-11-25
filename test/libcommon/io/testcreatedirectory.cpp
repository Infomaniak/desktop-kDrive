/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2026 Infomaniak Network SA
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

#include <filesystem>

using namespace CppUnit;

namespace KDC {

void TestIo::testCreateDirectoryWithParentReferences() {
    // Parent references (..) located below missing ancestors must be validated before the recursive creation: the
    // resolution of the first missing ancestor discards them, while create_directories still receives the whole path
    // and resolves them once the missing ancestors have been created.

    // Fails to create a directory whose path contains parent references below a missing ancestor
    {
        const LocalTemporaryDirectory temporaryDirectory("testCreateDirectoryWithParentReferences");
        const SyncPath syncRootPath = temporaryDirectory.path();

        // With `missing` absent, the first missing ancestor resolves to `sync_dir/missing` and the parent references are
        // discarded before validation. Once `missing` has been created by the recursive creation, they would make
        // `outside_directory` be created next to the sync directory.
        const SyncPath dirPath = syncRootPath / "missing" / ".." / ".." / "outside_directory";

        IoError ioError = IoError::Success;
        CPPUNIT_ASSERT(!IoHelper::createDirectory(dirPath, true, ioError, syncRootPath));
        CPPUNIT_ASSERT_EQUAL(IoError::InvalidArgument, ioError);

        // Nothing must have been created, neither inside nor outside of the sync directory.
        CPPUNIT_ASSERT(!std::filesystem::exists(syncRootPath / "missing"));
        CPPUNIT_ASSERT(!std::filesystem::exists(syncRootPath.parent_path() / "outside_directory"));
    }

    // Creates successfully a directory with missing ancestors when the path contains no parent reference
    {
        const LocalTemporaryDirectory temporaryDirectory("testCreateDirectoryWithParentReferences_valid");
        const SyncPath syncRootPath = temporaryDirectory.path();

        const SyncPath dirPath = syncRootPath / "missing_1" / "missing_2";
        IoError ioError = IoError::Success;
        CPPUNIT_ASSERT_MESSAGE(toString(ioError), IoHelper::createDirectory(dirPath, true, ioError, syncRootPath));
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError), IoError::Success, ioError);
        CPPUNIT_ASSERT(std::filesystem::is_directory(dirPath));
    }
}

} // namespace KDC

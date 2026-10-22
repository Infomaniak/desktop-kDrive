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

#pragma once

#include "testincludes.h"
#include "test_utility/localtemporarydirectory.h"

#include "syncpal/syncpal.h"

using namespace CppUnit;

namespace KDC {

class TestConflictingFilesCorrector : public CppUnit::TestFixture, public TestBase {
    public:
        CPPUNIT_TEST_SUITE(TestConflictingFilesCorrector);
        CPPUNIT_TEST(testKeepLocalResolution);
        CPPUNIT_TEST(testKeepRemoteResolution);
        CPPUNIT_TEST(testRejectInvalidDotComponentDestinationPaths);
        CPPUNIT_TEST(testKeepRemoteVersionWithSymlinkedParentPath);
        CPPUNIT_TEST(testKeepLocalVersionWithSymlinkedParentPaths);
        CPPUNIT_TEST_SUITE_END();

    public:
        void setUp() override;
        void tearDown() override;

    protected:
        //! Resolving a conflict with the "keep local" strategy deletes the local copy of the remote version and renames the
        //! conflicting local copy into the remote version path.
        void testKeepLocalResolution();
        //! Resolving a conflict with the "keep remote" strategy deletes the local conflicting copy and keeps the local copy of
        //! the remote version.
        void testKeepRemoteResolution();
        //! Destination paths with dot components, either rejected upfront or escaping the sync directory, must not cause any
        //! operation on the file system.
        void testRejectInvalidDotComponentDestinationPaths();
        //! A destination path whose parent directory is a symlink pointing outside the sync directory must not cause the
        //! deletion of the pointed item.
        void testKeepRemoteVersionWithSymlinkedParentPath();
        //! Same as above for the "keep local" strategy: neither the deletion nor the rename operations must be performed.
        void testKeepLocalVersionWithSymlinkedParentPaths();

    private:
        //! Insert a conflict error into `ParmsDb` and return it, with its database ID set.
        static Error makeConflictError(const SyncPath &relativePath, const SyncPath &destinationPath);
        //! Check that the error with the specified ID is defined in, respectively absent from, `ParmsDb`.
        static void checkErrorInParmsDb(const ErrorDbId errorDbId, bool expectedFound);

        std::shared_ptr<SyncPal> _syncPal = nullptr;
        LocalTemporaryDirectory _localParmsDbTempDir{std::string("TestConflictingFilesCorrector")};
        LocalTemporaryDirectory _syncTempDir{std::string("TestConflictingFilesCorrector_sync")};
        SyncPath _syncDirPath;
};

} // namespace KDC

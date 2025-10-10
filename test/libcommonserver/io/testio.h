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

#pragma once

#include "testincludes.h"
#include "test_utility/localtemporarydirectory.h"
#include "test_utility/iohelpertestutilities.h"
#include "libcommonserver/io/iohelper.h"

#include <algorithm>

using namespace CppUnit;

namespace KDC {
class TestIo : public CppUnit::TestFixture, public TestBase {
        CPPUNIT_TEST_SUITE(TestIo);
        CPPUNIT_TEST(testCheckSetAndGetRights); // Keep this test before any tests that may use set/get right functions
        CPPUNIT_TEST(testLock);
        CPPUNIT_TEST(testReadOnly);
        CPPUNIT_TEST(testPermissionsHolder);
        CPPUNIT_TEST(testGetItemType);
        CPPUNIT_TEST(testGetFileSize);
        CPPUNIT_TEST(testTempDirectoryPath);
        CPPUNIT_TEST(testCacheDirectoryPath);
        CPPUNIT_TEST(testLogDirectoryPath);
        CPPUNIT_TEST(testCheckIfPathExists);
        CPPUNIT_TEST(testCheckIfIsDirectory);
        CPPUNIT_TEST(testCreateDirectory);
        CPPUNIT_TEST(testCreateSymlink);
        CPPUNIT_TEST(testGetNodeId);
        CPPUNIT_TEST(testGetFileStat);
        CPPUNIT_TEST(testGetRights);
        // CPPUNIT_TEST(testIsFileAccessible); // Temporary disabled: Infinite loop on Linux CI
        CPPUNIT_TEST(testFileChanged);
        CPPUNIT_TEST(testCheckIfIsHiddenFile);
        CPPUNIT_TEST(testCheckDirectoryIterator);
#if defined(KD_MACOS) || defined(KD_WINDOWS)
        CPPUNIT_TEST(testGetXAttrValue);
        CPPUNIT_TEST(testSetXAttrValue);
#endif

#if defined(KD_MACOS)
        CPPUNIT_TEST(testRemoveXAttr);
        CPPUNIT_TEST(testCreateAlias);
#endif
#if defined(KD_WINDOWS)
        CPPUNIT_TEST(testCreateJunction);
        CPPUNIT_TEST(testGetLongPathName);
        CPPUNIT_TEST(testGetShortPathName);
#endif
        CPPUNIT_TEST(testCheckIfFileIsDehydrated);
        CPPUNIT_TEST(testAccesDeniedOnLockedFiles);
        CPPUNIT_TEST(testOpenFileSuccess);
        CPPUNIT_TEST(testOpenFileAccessDenied);
        CPPUNIT_TEST(testOpenFileNonExisting);
        CPPUNIT_TEST(testOpenLockedFileRemovedBeforeTimedOut);
        CPPUNIT_TEST(testSetFileDates);
        CPPUNIT_TEST(testGetDirectorySize);
        CPPUNIT_TEST_SUITE_END();

    public:
        TestIo();
        void setUp(void) override;
        void tearDown(void) override;

    protected:
        void testGetItemType(void);
        void testGetFileSize(void);
        void testTempDirectoryPath(void);
        void testCacheDirectoryPath(void);
        void testLogDirectoryPath(void);
        void testGetNodeId(void);
        void testCheckDirectoryIterator(void);
        void testCheckIfPathExists(void);
        void testCheckIfIsDirectory(void);
        void testCreateDirectory(void);
        void testCreateSymlink(void);
        void testGetFileStat(void);
        void testGetRights(void);
        void testIsFileAccessible(void);
        void testFileChanged(void);
        void testCheckIfIsHiddenFile(void);
#if defined(KD_MACOS) || defined(KD_WINDOWS)
        void testGetXAttrValue(void);
        void testSetXAttrValue(void);
#endif
#if defined(KD_MACOS)
        void testRemoveXAttr(void);
        void testCreateAlias(void);
#elif defined(KD_WINDOWS)
        void testCreateJunction();
        void testGetLongPathName();
        void testGetShortPathName();
#endif
        void testCheckIfFileIsDehydrated();
        void testCheckSetAndGetRights();
        void testLock();
        void testReadOnly();
        void testPermissionsHolder();

    private:
        void testGetItemTypeSimpleCases();
        void testGetItemTypeEdgeCases();
        void testGetItemTypeAllBranches();

        void testGetFileSizeSimpleCases();
        void testGetFileSizeAllBranches();

        void testCheckIfPathExistsSimpleCases();
        void testCheckIfPathExistWithDistinctEncodings();

        void testCheckIfPathExistsWithSameNodeIdSimpleCases();

        void testCheckDirectoryIteratorNonExistingPath();
        void testCheckDirectoryIteratorExistingPath();
        void testCheckDirectoryIteratotNextAfterEndOfDir();
        void testCheckDirectoryIteratorPermission();
        void testCheckDirectoryRecursive();
        void testCheckDirectoryIteratorUnexpectedDelete();
        void testCheckDirectoryPermissionLost();
        void testCheckDirectoryIteratorSymlinkEntry();
        void testAccesDeniedOnLockedFiles();
        void testOpenFileSuccess();
        void testOpenFileAccessDenied();
        void testOpenFileNonExisting();
        void testOpenLockedFileRemovedBeforeTimedOut();
        void testCheckIfPathExistsMixedSeparators();

        void testSetFileDates();
        void testGetDirectorySize();

    private:
        IoHelperTestUtilities *_testObj;
        const SyncPath _localTestDirPath;
};


struct GetItemChecker {
        struct Result {
                bool success{true};
                std::string message;
        };

        static std::string makeMessage(const CppUnit::Exception &e);
        Result checkSuccessfulRetrieval(const SyncPath &path, NodeType fileType) noexcept;
        Result checkSuccessfulLinkRetrieval(const SyncPath &path, const SyncPath &targetpath, LinkType linkType,
                                            NodeType fileType) noexcept;

        Result checkItemIsNotFound(const SyncPath &path) noexcept;
        Result checkSuccessfullRetrievalOfDanglingLink(const SyncPath &path, const SyncPath &targetPath, LinkType linkType,
                                                       NodeType targetType) noexcept;

        Result checkAccessIsDenied(const SyncPath &path) noexcept;
};

SyncPath makeVeryLonPath(const SyncPath &rootPath);
SyncPath makeFileNameWithEmojis();


} // namespace KDC

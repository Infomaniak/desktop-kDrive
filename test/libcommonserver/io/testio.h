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

#pragma once

#include "testincludes.h"
#include "libcommonserver/io/iohelper.h"

using namespace CppUnit;

namespace KDC {

struct TemporaryDirectory {
        SyncPath path;
        TemporaryDirectory();
        ~TemporaryDirectory();
};

struct IoHelperTests : public IoHelper {
        IoHelperTests();
        static void setIsDirectoryFunction(std::function<bool(const SyncPath &path, std::error_code &ec)> f);
        static void setIsSymlinkFunction(std::function<bool(const SyncPath &path, std::error_code &ec)> f);
        static void setReadSymlinkFunction(std::function<SyncPath(const SyncPath &path, std::error_code &ec)> f);
        static void setFileSizeFunction(std::function<std::uintmax_t(const SyncPath &path, std::error_code &ec)> f);
        static void setTempDirectoryPathFunction(std::function<SyncPath(std::error_code &ec)> f);

#ifdef __APPLE__
        static void setReadAliasFunction(std::function<bool(const SyncPath &path, SyncPath &targetPath, IoError &ioError)> f);
#endif

        static void resetFunctions();
};

class TestIo : public CppUnit::TestFixture {
        CPPUNIT_TEST_SUITE(TestIo);
        CPPUNIT_TEST(testGetItemType);
        CPPUNIT_TEST(testGetFileSize);
        CPPUNIT_TEST(testTempDirectoryPath);
        CPPUNIT_TEST(testLogDirectoryPath);
        CPPUNIT_TEST(testCheckIfPathExists);
        CPPUNIT_TEST(testCheckIfIsDirectory);
        CPPUNIT_TEST(testCreateDirectory);
        CPPUNIT_TEST(testCreateSymlink);
        CPPUNIT_TEST(testGetNodeId);
        CPPUNIT_TEST(testGetFileStat);
        CPPUNIT_TEST(testIsFileAccessible);
        CPPUNIT_TEST(testFileChanged);
        CPPUNIT_TEST(testCheckIfIsHiddenFile);
#if defined(__APPLE__) || defined(_WIN32)
        CPPUNIT_TEST(testGetXAttrValue);
        CPPUNIT_TEST(testSetXAttrValue);
#endif

#if defined(__APPLE__)
        CPPUNIT_TEST(testCreateAlias);
#endif
#if defined(_WIN32)
        CPPUNIT_TEST(testCreateJunction);
#endif
        CPPUNIT_TEST(testCheckIfFileIsDehydrated);
        CPPUNIT_TEST_SUITE_END();

    public:
        TestIo();
        void setUp(void) override;
        void tearDown(void) override;

    protected:
        void testGetItemType(void);
        void testGetFileSize(void);
        void testTempDirectoryPath(void);
        void testLogDirectoryPath(void);
        void testGetNodeId(void);
        void testCheckIfPathExists(void);
        void testCheckIfIsDirectory(void);
        void testCreateDirectory(void);
        void testCreateSymlink(void);
        void testGetFileStat(void);
        void testIsFileAccessible(void);
        void testFileChanged(void);
        void testCheckIfIsHiddenFile(void);
#if defined(__APPLE__) || defined(_WIN32)
        void testGetXAttrValue(void);
        void testSetXAttrValue(void);
#endif
#if defined(__APPLE__)
        void testCreateAlias(void);
#endif
#if defined(_WIN32)
        void testCreateJunction(void);
#endif
        void testCheckIfFileIsDehydrated(void);

    private:
        void testGetItemTypeSimpleCases(void);
        void testGetItemTypeAllBranches(void);

        void testGetFileSizeSimpleCases(void);
        void testGetFileSizeAllBranches(void);

        void testCheckIfPathExistsSimpleCases(void);
        void testCheckIfPathExistsAllBranches(void);

        void testCheckIfPathExistsWithSameNodeIdSimpleCases(void);
        void testCheckIfPathExistsWithSameNodeIdAllBranches(void);

    private:
        IoHelperTests *_testObj;
        const SyncPath _localTestDirPath;
};


struct GetItemChecker {
        GetItemChecker(IoHelperTests *iohelper);

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

    private:
        IoHelperTests *_iohelper{nullptr};
};

SyncPath makeVeryLonPath(const SyncPath &rootPath);


}  // namespace KDC

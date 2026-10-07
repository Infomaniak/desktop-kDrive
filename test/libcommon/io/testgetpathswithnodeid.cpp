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

#include "libcommonserver/io/filestat.h"

#include <algorithm>
#include <filesystem>
#include <fstream>

using namespace CppUnit;

namespace KDC {

namespace {

void createFile(const SyncPath &path, const std::string &content) {
    std::ofstream file(path, std::ios::binary);
    file << content;
    CPPUNIT_ASSERT_MESSAGE("Failed to create the file", file.good());
}

NodeId nodeIdOf(const SyncPath &path) {
    FileStat fileStat;
    auto ioError = IoError::Unknown;
    CPPUNIT_ASSERT_MESSAGE(toString(ioError),
                           IoHelper::getFileStat(path, &fileStat, ioError, IoHelper::PathCheckOption::Insensitive));
    CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
    return std::to_string(fileStat.inode);
}

bool contains(const std::vector<SyncPath> &paths, const SyncPath &path) {
    return std::find(paths.begin(), paths.end(), path) != paths.end();
}

} // namespace

void TestIo::testGetPathsWithNodeId() {
    // All the hardlinks of the file are found, including the ones located in subdirectories. Other files and symbolic links
    // are not selected.
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const auto filePath = temporaryDirectory.path() / "file.txt";
        createFile(filePath, "content");

        const auto subDirPath = temporaryDirectory.path() / "sub_dir";
        std::error_code ec;
        CPPUNIT_ASSERT(std::filesystem::create_directories(subDirPath, ec) && ec.value() == 0);

        const auto linkPath = subDirPath / "link.txt";
        std::filesystem::create_hard_link(filePath, linkPath, ec);
        CPPUNIT_ASSERT_MESSAGE(ec.message(), ec.value() == 0);

        createFile(subDirPath / "other.txt", "other content");

        const auto symlinkPath = temporaryDirectory.path() / "symlink.txt";
        auto ioError = IoError::Unknown;
        CPPUNIT_ASSERT_MESSAGE(toString(ioError), IoHelper::createSymlink(filePath, symlinkPath, false, ioError));

        std::vector<SyncPath> paths;
        CPPUNIT_ASSERT_MESSAGE(toString(ioError),
                               IoHelper::getPathsWithNodeId(temporaryDirectory.path(), nodeIdOf(filePath), paths, ioError));
        CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
        CPPUNIT_ASSERT_EQUAL(size_t(2), paths.size());
        CPPUNIT_ASSERT(contains(paths, filePath));
        CPPUNIT_ASSERT(contains(paths, linkPath));
    }

    // The links are found even if the path through which the file has been created has been removed.
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const auto filePath = temporaryDirectory.path() / "file.txt";
        createFile(filePath, "content");
        const NodeId nodeId = nodeIdOf(filePath);

        const auto subDirPath = temporaryDirectory.path() / "sub_dir";
        std::error_code ec;
        CPPUNIT_ASSERT(std::filesystem::create_directories(subDirPath, ec) && ec.value() == 0);

        const auto linkPath = subDirPath / "link.txt";
        std::filesystem::create_hard_link(filePath, linkPath, ec);
        CPPUNIT_ASSERT_MESSAGE(ec.message(), ec.value() == 0);
        CPPUNIT_ASSERT(std::filesystem::remove(filePath, ec) && ec.value() == 0);

        std::vector<SyncPath> paths;
        auto ioError = IoError::Unknown;
        CPPUNIT_ASSERT_MESSAGE(toString(ioError),
                               IoHelper::getPathsWithNodeId(temporaryDirectory.path(), nodeId, paths, ioError));
        CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
        CPPUNIT_ASSERT_EQUAL(size_t(1), paths.size());
        CPPUNIT_ASSERT_EQUAL(linkPath, paths.front());
    }

    // No file has the searched node identifier anymore.
    {
        const LocalTemporaryDirectory temporaryDirectory;
        createFile(temporaryDirectory.path() / "other.txt", "other content");
        const auto filePath = temporaryDirectory.path() / "file.txt";
        createFile(filePath, "content");
        const NodeId nodeId = nodeIdOf(filePath);

        std::error_code ec;
        CPPUNIT_ASSERT(std::filesystem::remove(filePath, ec) && ec.value() == 0);

        std::vector<SyncPath> paths{SyncPath("dummy")};
        auto ioError = IoError::Unknown;
        CPPUNIT_ASSERT_MESSAGE(toString(ioError),
                               IoHelper::getPathsWithNodeId(temporaryDirectory.path(), nodeId, paths, ioError));
        CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
        CPPUNIT_ASSERT(paths.empty());
    }

    // The search root does not exist: the search fails.
    {
        const LocalTemporaryDirectory temporaryDirectory;
        std::vector<SyncPath> paths;
        auto ioError = IoError::Success;
        CPPUNIT_ASSERT(!IoHelper::getPathsWithNodeId(temporaryDirectory.path() / "non_existing_dir", "1", paths, ioError));
        CPPUNIT_ASSERT_EQUAL(IoError::NoSuchFileOrDirectory, ioError);
    }
}

} // namespace KDC

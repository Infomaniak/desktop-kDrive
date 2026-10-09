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

// A directory link (symbolic link on POSIX, junction on Windows) located inside the search root must neither be followed
// nor make the links located outside of the search root be returned, even if they share the node identifier of the searched
// item: the link search drives permanent deletions, and an item located outside of the search root must never be returned.
void TestIo::testLinkSearchDoesNotFollowDirectoryLinks() {
    const LocalTemporaryDirectory temporaryDirectory;
    const auto syncRootPath = temporaryDirectory.path() / "sync_root";
    const auto outsideDirPath = temporaryDirectory.path() / "outside_dir";
    std::error_code ec;
    CPPUNIT_ASSERT(std::filesystem::create_directories(syncRootPath, ec) && ec.value() == 0);
    CPPUNIT_ASSERT(std::filesystem::create_directories(outsideDirPath, ec) && ec.value() == 0);

    const auto filePath = syncRootPath / "file.txt";
    createFile(filePath, "content");
    const NodeId nodeId = nodeIdOf(filePath);

    const auto linkPath = syncRootPath / "link.txt";
    std::filesystem::create_hard_link(filePath, linkPath, ec);
    CPPUNIT_ASSERT_MESSAGE(ec.message(), ec.value() == 0);

    // A hardlink of the searched item, located outside of the search root and reachable through the directory link.
    const auto outsideLinkPath = outsideDirPath / "outside_link.txt";
    std::filesystem::create_hard_link(filePath, outsideLinkPath, ec);
    CPPUNIT_ASSERT_MESSAGE(ec.message(), ec.value() == 0);
    CPPUNIT_ASSERT_EQUAL_MESSAGE("The item outside of the search root should be a link of the searched item", nodeId,
                                 nodeIdOf(outsideLinkPath));

    IoError ioError = IoError::Unknown;
    const auto directoryLinkPath = syncRootPath / "directory_link";
#if defined(KD_WINDOWS)
    CPPUNIT_ASSERT_MESSAGE(toString(ioError), IoHelper::createJunctionFromPath(outsideDirPath, directoryLinkPath, ioError));
    CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
#else
    std::filesystem::create_directory_symlink(outsideDirPath, directoryLinkPath, ec);
    CPPUNIT_ASSERT_MESSAGE(ec.message(), ec.value() == 0);
#endif

    std::vector<SyncPath> hardlinkPaths;
#if defined(KD_WINDOWS)
    ioError = IoError::Unknown;
    CPPUNIT_ASSERT_MESSAGE(toString(ioError), IoHelper::getHardlinkPaths(syncRootPath, nodeId, hardlinkPaths, ioError));
    CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
    CPPUNIT_ASSERT_EQUAL(size_t(2), hardlinkPaths.size());
    CPPUNIT_ASSERT(contains(hardlinkPaths, filePath));
    CPPUNIT_ASSERT(contains(hardlinkPaths, linkPath));
    CPPUNIT_ASSERT_MESSAGE("A link located outside of the search root has been returned",
                           !contains(hardlinkPaths, outsideLinkPath));
#else
    // The hardlink enumeration is only available on Windows.
    ioError = IoError::Unknown;
    CPPUNIT_ASSERT_MESSAGE(toString(ioError), !IoHelper::getHardlinkPaths(syncRootPath, nodeId, hardlinkPaths, ioError));
    CPPUNIT_ASSERT_EQUAL(IoError::FunctionNotSupported, ioError);
    CPPUNIT_ASSERT(hardlinkPaths.empty());
#endif
}

} // namespace KDC

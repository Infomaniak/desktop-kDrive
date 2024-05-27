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

#include <filesystem>

using namespace CppUnit;

namespace KDC {


GetItemChecker::GetItemChecker(IoHelperTests *iohelper) : _iohelper{iohelper} {};

std::string GetItemChecker::makeMessage(const CppUnit::Exception &e) {
    std::string msg = "Details: \n    -" + e.message().details();
    msg += "    -- line: " + e.sourceLine().fileName() + ":" + std::to_string(e.sourceLine().lineNumber());

    return msg;
}

GetItemChecker::Result GetItemChecker::checkSuccessfulRetrieval(const SyncPath &path, NodeType fileType) noexcept {
    ItemType itemType;
    try {
        CPPUNIT_ASSERT(_iohelper->getItemType(path, itemType));
        CPPUNIT_ASSERT(itemType.ioError == IoErrorSuccess);
        CPPUNIT_ASSERT(itemType.nodeType == fileType);
        CPPUNIT_ASSERT(itemType.linkType == LinkTypeNone);
        CPPUNIT_ASSERT(itemType.targetType == NodeTypeUnknown);
        CPPUNIT_ASSERT(itemType.targetPath == SyncPath{});
    } catch (const CppUnit::Exception &e) {
        return Result{false, makeMessage(e)};
    };

    return {};
}

GetItemChecker::Result GetItemChecker::checkSuccessfulLinkRetrieval(const SyncPath &path, const SyncPath &targetPath,
                                                                    LinkType linkType, NodeType fileType) noexcept {
    ItemType itemType;
    try {
        CPPUNIT_ASSERT(_iohelper->getItemType(path, itemType));
        CPPUNIT_ASSERT(itemType.ioError == IoErrorSuccess);
        CPPUNIT_ASSERT(itemType.nodeType == NodeTypeFile);
        CPPUNIT_ASSERT(itemType.linkType == linkType);
        CPPUNIT_ASSERT(itemType.targetType == fileType);
        CPPUNIT_ASSERT(itemType.targetPath == targetPath);
    } catch (const CppUnit::Exception &e) {
        return Result{false, makeMessage(e)};
    };

    return {};
}

GetItemChecker::Result GetItemChecker::checkItemIsNotFound(const SyncPath &path) noexcept {
    ItemType itemType;
    try {
        CPPUNIT_ASSERT(_iohelper->getItemType(path, itemType));
        CPPUNIT_ASSERT(itemType.ioError == IoErrorNoSuchFileOrDirectory);
        CPPUNIT_ASSERT(itemType.nodeType == NodeTypeUnknown);
        CPPUNIT_ASSERT(itemType.linkType == LinkTypeNone);
        CPPUNIT_ASSERT(itemType.targetType == NodeTypeUnknown);
        CPPUNIT_ASSERT(itemType.targetPath == SyncPath{});
    } catch (const CppUnit::Exception &e) {
        return Result{false, makeMessage(e)};
    };

    return {};
}

GetItemChecker::Result GetItemChecker::checkSuccessfullRetrievalOfDanglingLink(const SyncPath &path, const SyncPath &targetPath,
                                                                               LinkType linkType, NodeType targetType) noexcept {
    ItemType itemType;
    try {
        CPPUNIT_ASSERT(_iohelper->getItemType(path, itemType));
        CPPUNIT_ASSERT(itemType.ioError == IoErrorSuccess);  // Although `targetPath` is invalid.
        CPPUNIT_ASSERT(itemType.nodeType == NodeTypeFile);
        CPPUNIT_ASSERT(itemType.linkType == linkType);
        CPPUNIT_ASSERT(itemType.targetType == targetType);
        CPPUNIT_ASSERT(itemType.targetPath == targetPath);
    } catch (const CppUnit::Exception &e) {
        return Result{false, makeMessage(e)};
    };

    return Result{};
}

GetItemChecker::Result GetItemChecker::checkAccessIsDenied(const SyncPath &path) noexcept {
    ItemType itemType;
    try {
        CPPUNIT_ASSERT(_iohelper->getItemType(path, itemType));
        CPPUNIT_ASSERT(itemType.ioError == IoErrorAccessDenied);
        CPPUNIT_ASSERT(itemType.nodeType == NodeTypeUnknown);
        CPPUNIT_ASSERT(itemType.linkType == LinkTypeNone);
        CPPUNIT_ASSERT(itemType.targetType == NodeTypeUnknown);
        CPPUNIT_ASSERT(itemType.targetPath == SyncPath{});
    } catch (const CppUnit::Exception &e) {
        return Result{false, makeMessage(e)};
    };

    return {};
}


void TestIo::testGetItemTypeSimpleCases() {
    GetItemChecker checker{_testObj};

    // A regular file
    {
        const auto result = checker.checkSuccessfulRetrieval(_localTestDirPath / "test_pictures/picture-1.jpg", NodeTypeFile);
        CPPUNIT_ASSERT_MESSAGE(result.message, result.success);
    }
    // A regular directory
    {
        const auto result = checker.checkSuccessfulRetrieval(_localTestDirPath / "test_pictures", NodeTypeDirectory);
        CPPUNIT_ASSERT_MESSAGE(result.message, result.success);
    }
    // A regular symbolic link on a file
    {
        const SyncPath targetPath = _localTestDirPath / "test_pictures/picture-1.jpg";
        const TemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path / "regular_file_symbolic_link";
        std::filesystem::create_symlink(targetPath, path);

        const auto result = checker.checkSuccessfulLinkRetrieval(path, targetPath, LinkTypeSymlink, NodeTypeFile);
        CPPUNIT_ASSERT_MESSAGE(result.message, result.success);
    }

    // A regular symbolic link on a folder
    {
        const SyncPath targetPath = _localTestDirPath / "test_pictures";
        const TemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path / "regular_dir_symbolic_link";
        std::filesystem::create_directory_symlink(targetPath, path);

        const auto result = checker.checkSuccessfulLinkRetrieval(path, targetPath, LinkTypeSymlink, NodeTypeDirectory);
        CPPUNIT_ASSERT_MESSAGE(result.message, result.success);
    }

    // A non-existing file
    {
        const auto result = checker.checkItemIsNotFound(_localTestDirPath / "non-existing.jpg");  // This file does not exist.
        CPPUNIT_ASSERT_MESSAGE(result.message, result.success);
    }

    // A non-existing file with a very long name
    {
        const std::string veryLongfileName(1000, 'a');  // Exceeds the max allowed name length on every file system of interest.
        const SyncPath path = _localTestDirPath / veryLongfileName;  // This file doesn't exist.
        ItemType itemType;
#ifdef _WIN32
        CPPUNIT_ASSERT(_testObj->getItemType(path, itemType));
        CPPUNIT_ASSERT(itemType.ioError == IoErrorNoSuchFileOrDirectory);
#else
        // We got std::errc::filename_too_long, equivalent to the POSIX error ENAMETOOLONG
        CPPUNIT_ASSERT(!_testObj->getItemType(path, itemType));
        CPPUNIT_ASSERT(itemType.ioError == IoErrorFileNameTooLong);
#endif
        CPPUNIT_ASSERT(itemType.nodeType == NodeTypeUnknown);
        CPPUNIT_ASSERT(itemType.linkType == LinkTypeNone);
        CPPUNIT_ASSERT(itemType.targetType == NodeTypeUnknown);
        CPPUNIT_ASSERT(itemType.targetPath == SyncPath{});
    }

    // A non-existing file with a very long path
    {
        const std::string pathSegment(50, 'a');
        SyncPath path = _localTestDirPath;
        for (int i = 0; i < 1000; ++i) {
            path /= pathSegment;  // Eventually exceeds the max allowed path length on every file system of interest.
        }
        ItemType itemType;
#ifdef _WIN32
        CPPUNIT_ASSERT(_testObj->getItemType(path, itemType));
        CPPUNIT_ASSERT(itemType.ioError == IoErrorNoSuchFileOrDirectory);
#else
        // We got std::errc::filename_too_long, equivalent to the POSIX error ENAMETOOLONG
        CPPUNIT_ASSERT(!_testObj->getItemType(path, itemType));
        CPPUNIT_ASSERT(itemType.ioError == IoErrorFileNameTooLong);
#endif
        CPPUNIT_ASSERT(itemType.nodeType == NodeTypeUnknown);
        CPPUNIT_ASSERT(itemType.linkType == LinkTypeNone);
        CPPUNIT_ASSERT(itemType.targetType == NodeTypeUnknown);
        CPPUNIT_ASSERT(itemType.targetPath == SyncPath{});
    }

    // An existing file with dots and colons in its name
    {
        const TemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path / ":.file.::.name.:";
        { std::ofstream ofs(path); }

#ifdef _WIN32
        ItemType itemType;
        CPPUNIT_ASSERT(_testObj->getItemType(
            path, itemType));  // Invalid name is considered as IoErrorNoSuchFileOrDirectory (expected error)
        CPPUNIT_ASSERT(itemType.ioError == IoErrorNoSuchFileOrDirectory);
        CPPUNIT_ASSERT(itemType.nodeType == NodeTypeUnknown);
        CPPUNIT_ASSERT(itemType.linkType == LinkTypeNone);
        CPPUNIT_ASSERT(itemType.targetType == NodeTypeUnknown);
        CPPUNIT_ASSERT(itemType.targetPath == SyncPath{});
#else
        const auto result = checker.checkSuccessfulRetrieval(path, NodeTypeFile);
        CPPUNIT_ASSERT_MESSAGE(result.message, result.success);
#endif
    }

    // An existing file with emojis in its name
    {
        using namespace std::string_literals;  // operator ""s
        const TemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path / std::string{u8"🫃😋🌲👣🍔🕉️⛎"s};
        { std::ofstream ofs(path); }

        const auto result = checker.checkSuccessfulRetrieval(path, NodeTypeFile);
        CPPUNIT_ASSERT_MESSAGE(result.message, result.success);
    }

    // A dangling symbolic link
    {
        const TemporaryDirectory temporaryDirectory;
        const SyncPath targetPath = temporaryDirectory.path / "non_existing_test_file.txt";  // This file does not exist.
        const SyncPath path = temporaryDirectory.path / "dangling_symbolic_link";
        std::filesystem::create_symlink(targetPath, path);

        // Assumptions made to implement getItemType.
        CPPUNIT_ASSERT(std::filesystem::is_symlink(path));
        CPPUNIT_ASSERT(!std::filesystem::exists(path));  // Follow links
        std::error_code ec;
        const SyncPath targetPath_ = std::filesystem::read_symlink(path, ec);
        CPPUNIT_ASSERT(targetPath == targetPath_ && ec.value() == 0);

        // Actual test
#ifdef _WIN32
        const auto result = checker.checkSuccessfullRetrievalOfDanglingLink(path, targetPath, LinkTypeSymlink, NodeTypeFile);
#else
        const auto result = checker.checkSuccessfullRetrievalOfDanglingLink(path, targetPath, LinkTypeSymlink, NodeTypeUnknown);
#endif
        CPPUNIT_ASSERT_MESSAGE(result.message, result.success);
    }

#if defined(__APPLE__)
    // A MacOSX Finder alias on a regular file.
    {
        const TemporaryDirectory temporaryDirectory;
        const SyncPath targetPath = _localTestDirPath / "test_pictures/picture-1.jpg";
        const SyncPath path = temporaryDirectory.path / "regular_file_alias";

        IoError aliasError = IoErrorSuccess;
        IoHelper::createAliasFromPath(targetPath, path, aliasError);

        const auto result = checker.checkSuccessfulLinkRetrieval(path, targetPath, LinkTypeFinderAlias, NodeTypeFile);
        CPPUNIT_ASSERT_MESSAGE(result.message, result.success);
    }

    // A MacOSX Finder alias on a regular directory.
    {
        const TemporaryDirectory temporaryDirectory;
        const SyncPath targetPath = _localTestDirPath / "test_pictures";
        const SyncPath path = temporaryDirectory.path / "regular_dir_alias";

        IoError aliasError = IoErrorSuccess;
        IoHelper::createAliasFromPath(targetPath, path, aliasError);

        const auto result = checker.checkSuccessfulLinkRetrieval(path, targetPath, LinkTypeFinderAlias, NodeTypeDirectory);
        CPPUNIT_ASSERT_MESSAGE(result.message, result.success);
    }

    // A dangling MacOSX Finder alias on a non-existing file.
    {
        const TemporaryDirectory temporaryDirectory;
        const SyncPath targetPath = temporaryDirectory.path / "file_to_be_deleted.png";  // This file will be deleted.
        const SyncPath path = temporaryDirectory.path / "dangling_file_alias";
        { std::ofstream ofs(targetPath); }

        IoError aliasError;
        IoHelper::createAliasFromPath(targetPath, path, aliasError);
        std::filesystem::remove(targetPath);

        const auto result =
            checker.checkSuccessfullRetrievalOfDanglingLink(path, SyncPath{}, LinkTypeFinderAlias, NodeTypeUnknown);
        CPPUNIT_ASSERT_MESSAGE(result.message, result.success);
    }

    // A dangling MacOSX Finder alias on a non-existing directory.
    {
        const TemporaryDirectory temporaryDirectory;
        const SyncPath targetPath = temporaryDirectory.path / "directory_to_be_deleted";  // This directory will be deleted.
        std::filesystem::create_directory(targetPath);

        const SyncPath path = temporaryDirectory.path / "dangling_directory_alias";

        IoError aliasError = IoErrorSuccess;
        IoHelper::createAliasFromPath(targetPath, path, aliasError);
        std::filesystem::remove_all(targetPath);

        const auto result =
            checker.checkSuccessfullRetrievalOfDanglingLink(path, SyncPath{}, LinkTypeFinderAlias, NodeTypeUnknown);
#endif
        CPPUNIT_ASSERT_MESSAGE(result.message, result.success);
    }

#if defined(_WIN32)
    // A Windows junction on a regular folder.
    {
        const TemporaryDirectory temporaryDirectory;
        const SyncPath targetPath = _localTestDirPath / "test_pictures";
        const SyncPath path = temporaryDirectory.path / "regular_dir_junction";

        IoError ioError = IoErrorSuccess;
        _testObj->createJunctionFromPath(targetPath, path, ioError);

        ItemType itemType;
        CPPUNIT_ASSERT(_testObj->getItemType(path, itemType));
        CPPUNIT_ASSERT(itemType.nodeType == NodeTypeFile);
        CPPUNIT_ASSERT(itemType.linkType == LinkTypeJunction);
        CPPUNIT_ASSERT(itemType.targetType == NodeTypeDirectory);
        CPPUNIT_ASSERT(itemType.targetPath == targetPath);
    }
#endif

    // A regular file missing all permissions (no error expected)
    {
        const TemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path / "permission_less_file.txt";
        { std::ofstream ofs(path); }

        std::filesystem::permissions(path, std::filesystem::perms::all, std::filesystem::perm_options::remove);

        const auto result = checker.checkSuccessfulRetrieval(path, NodeTypeFile);
        std::filesystem::permissions(path, std::filesystem::perms::all, std::filesystem::perm_options::add);
        CPPUNIT_ASSERT_MESSAGE(result.message, result.success);
    }

    // A regular directory missing all permissions (no error expected)
    {
        const TemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path / "permission_less_directory";
        std::filesystem::create_directory(path);

        const auto allPermissions =
            std::filesystem::perms::owner_all | std::filesystem::perms::others_all | std::filesystem::perms::group_all;
        std::filesystem::permissions(path, allPermissions, std::filesystem::perm_options::remove);

        const auto result = checker.checkSuccessfulRetrieval(path, NodeTypeDirectory);
        std::filesystem::permissions(path, allPermissions, std::filesystem::perm_options::add);
        CPPUNIT_ASSERT_MESSAGE(result.message, result.success);
    }

    // A regular file within a subdirectory that misses owner read permission (no error expected)
    {
        const TemporaryDirectory temporaryDirectory;
        const SyncPath subdir = temporaryDirectory.path / "permission_less_subdirectory";
        std::filesystem::create_directory(subdir);
        const SyncPath path = subdir / "file.txt";
        { std::ofstream ofs(path); }
        std::filesystem::permissions(subdir, std::filesystem::perms::owner_read, std::filesystem::perm_options::remove);

        const auto result = checker.checkSuccessfulRetrieval(path, NodeTypeFile);
        // Restore permission to allow subdir removal
        std::filesystem::permissions(subdir, std::filesystem::perms::owner_read, std::filesystem::perm_options::add);
        CPPUNIT_ASSERT_MESSAGE(result.message, result.success);
    }

    // A regular file within a subdirectory that misses owner exec permission:
    // - access denied expected on MacOSX
    // - no error on Windows
    {
        const TemporaryDirectory temporaryDirectory;
        const SyncPath subdir = temporaryDirectory.path / "permission_less_subdirectory";
        std::filesystem::create_directory(subdir);
        const SyncPath path = subdir / "file.txt";
        { std::ofstream ofs(path); }
        std::filesystem::permissions(subdir, std::filesystem::perms::owner_exec, std::filesystem::perm_options::remove);

        ItemType itemType;
        CPPUNIT_ASSERT(_testObj->getItemType(path, itemType));
#ifdef _WIN32
        CPPUNIT_ASSERT(itemType.ioError == IoErrorSuccess);
        CPPUNIT_ASSERT(itemType.nodeType == NodeTypeFile);
#else
    CPPUNIT_ASSERT(itemType.ioError == IoErrorAccessDenied);
    CPPUNIT_ASSERT(itemType.nodeType == NodeTypeUnknown);
#endif
        CPPUNIT_ASSERT(itemType.linkType == LinkTypeNone);
        CPPUNIT_ASSERT(itemType.targetType == NodeTypeUnknown);
        CPPUNIT_ASSERT(itemType.targetPath == SyncPath{});

        // Restore permission to allow subdir removal
        std::filesystem::permissions(subdir, std::filesystem::perms::owner_exec, std::filesystem::perm_options::add);
    }

    // A symbolic link within a subdirectory that misses owner exec permission:
    // - access denied expected on MacOSX
    // - no error expected on Windows
    {
        const TemporaryDirectory temporaryDirectory;
        const SyncPath subdir = temporaryDirectory.path / "permission_less_subdirectory";
        std::filesystem::create_directory(subdir);

        const SyncPath targetPath = _localTestDirPath / "test_pictures/picture-1.jpg";
        const SyncPath path = subdir / "regular_file_symbolic_link";
        std::filesystem::create_symlink(targetPath, path);

        std::filesystem::permissions(subdir, std::filesystem::perms::owner_exec, std::filesystem::perm_options::remove);

#ifdef _WIN32
        ItemType itemType;
        CPPUNIT_ASSERT(_testObj->getItemType(path, itemType));
        // Restore permission to allow subdir removal
        std::filesystem::permissions(subdir, std::filesystem::perms::owner_exec, std::filesystem::perm_options::add);
        CPPUNIT_ASSERT(itemType.ioError == IoErrorSuccess);
        CPPUNIT_ASSERT(itemType.nodeType == NodeTypeFile);
        CPPUNIT_ASSERT(itemType.linkType == LinkTypeSymlink);
        CPPUNIT_ASSERT(itemType.targetType == NodeTypeFile);
        CPPUNIT_ASSERT(itemType.targetPath == targetPath);
#else
    const auto result = checker.checkAccessIsDenied(path);
    // Restore permission to allow subdir removal
    std::filesystem::permissions(subdir, std::filesystem::perms::owner_exec, std::filesystem::perm_options::add);
    CPPUNIT_ASSERT_MESSAGE(result.message, result.success);
#endif
    }
}

void TestIo::testGetItemTypeAllBranches() {
    GetItemChecker checker{_testObj};

    // Failing to read a regular symbolic link because of an unexpected error.
    {
        const SyncPath targetPath = _localTestDirPath / "test_pictures";
        const TemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path / "regular_dir_symbolic_link";
        std::filesystem::create_symlink(targetPath, path);

        _testObj->setIsSymlinkFunction([](const SyncPath &, std::error_code &ec) -> bool {
            ec = std::make_error_code(std::errc::state_not_recoverable);  // Not handled -> IoErrorUnknown.
            return false;
        });

        ItemType itemType;
        CPPUNIT_ASSERT(!_testObj->getItemType(path, itemType));
        CPPUNIT_ASSERT(itemType.ioError == IoErrorUnknown);
        CPPUNIT_ASSERT(itemType.nodeType == NodeTypeUnknown);
        CPPUNIT_ASSERT(itemType.linkType == LinkTypeNone);
        CPPUNIT_ASSERT(itemType.targetType == NodeTypeUnknown);
        CPPUNIT_ASSERT(itemType.targetPath == SyncPath{});

        _testObj->resetFunctions();
    }

    // Reading a regular symbolic link that is removed after `filesystem::is_simlink` was called.
    {
        const SyncPath targetPath = _localTestDirPath / "test_pictures";
        const TemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path / "regular_dir_symbolic_link";
        std::filesystem::create_symlink(targetPath, path);

        _testObj->setReadSymlinkFunction([](const SyncPath &path, std::error_code &ec) -> SyncPath {
            std::filesystem::remove(path);
            return std::filesystem::read_symlink(path, ec);
        });

        const auto result = checker.checkItemIsNotFound(path);
        CPPUNIT_ASSERT_MESSAGE(result.message, result.success);

        _testObj->resetFunctions();
    }

    // Reading a symlink within a subdirectory whose owner exec permission is removed
    // after `filesystem::is_simlink` was called.
    {
        const TemporaryDirectory temporaryDirectory;
        const SyncPath subdir = temporaryDirectory.path / "permission_less_subdirectory";
        std::filesystem::create_directory(subdir);

        const SyncPath targetPath = _localTestDirPath / "test_pictures/picture-1.jpg";
        const SyncPath path = subdir / "regular_file_symbolic_link";
        std::filesystem::create_symlink(targetPath, path);

        _testObj->setReadSymlinkFunction([&subdir](const SyncPath &path, std::error_code &ec) -> SyncPath {
            std::filesystem::permissions(subdir, std::filesystem::perms::owner_exec, std::filesystem::perm_options::remove);
            return std::filesystem::read_symlink(path, ec);
        });

#ifdef _WIN32
        ItemType itemType;
        CPPUNIT_ASSERT(_testObj->getItemType(path, itemType));
        CPPUNIT_ASSERT(itemType.ioError == IoErrorSuccess);
        CPPUNIT_ASSERT(itemType.nodeType == NodeTypeFile);
        CPPUNIT_ASSERT(itemType.linkType == LinkTypeSymlink);
        CPPUNIT_ASSERT(itemType.targetType == NodeTypeFile);
        CPPUNIT_ASSERT(itemType.targetPath == targetPath);
#else
        const auto result = checker.checkAccessIsDenied(path);
        CPPUNIT_ASSERT_MESSAGE(result.message, result.success);
#endif

        // Restore permission to allow subdir removal
        std::filesystem::permissions(subdir, std::filesystem::perms::owner_exec, std::filesystem::perm_options::add);
        _testObj->resetFunctions();
    }

    // Reading a regular symbolic link that is removed after `filesystem::is_simlink` was called.
    {
        const SyncPath targetPath = _localTestDirPath / "test_pictures";
        const TemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path / "regular_dir_symbolic_link";
        std::filesystem::create_symlink(targetPath, path);

        _testObj->setReadSymlinkFunction([](const SyncPath &path, std::error_code &ec) -> SyncPath {
            std::filesystem::remove(path);
            return std::filesystem::read_symlink(path, ec);
        });

        const auto result = checker.checkItemIsNotFound(path);
        CPPUNIT_ASSERT_MESSAGE(result.message, result.success);

        _testObj->resetFunctions();
    }

    // Checking the target type of a symlink after the target file has been removed
    {
        const TemporaryDirectory temporaryDirectory;
        const SyncPath targetPath = temporaryDirectory.path / "file.txt";
        const SyncPath path = temporaryDirectory.path / "symlink_to_file.txt";
        { std::ofstream ofs(targetPath); }
        std::filesystem::create_symlink(targetPath, path);

        _testObj->setIsDirectoryFunction([](const SyncPath &path, std::error_code &ec) -> bool {
            std::filesystem::remove(path);
            return std::filesystem::is_directory(path, ec);
        });

        const auto result = checker.checkSuccessfullRetrievalOfDanglingLink(path, targetPath, LinkTypeSymlink, NodeTypeFile);
        CPPUNIT_ASSERT_MESSAGE(result.message, result.success);

        _testObj->resetFunctions();
    }

#ifdef __APPLE__
    // Failing to read a regular MacOSX Finder alias link after `checkIfAlias` was called.
    {
        const SyncPath targetPath = _localTestDirPath / "test_pictures";
        const TemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path / "test_pictures_alias";

        IoError aliasError;
        CPPUNIT_ASSERT(_testObj->createAliasFromPath(targetPath, path, aliasError));
        CPPUNIT_ASSERT(aliasError == IoErrorSuccess);
        CPPUNIT_ASSERT(std::filesystem::exists(path));

        _testObj->setReadAliasFunction([](const SyncPath &, SyncPath &, IoError &ioError) -> bool {
            ioError = IoErrorUnknown;
            return false;
        });

        ItemType itemType;
        CPPUNIT_ASSERT(!_testObj->getItemType(path, itemType));
        CPPUNIT_ASSERT(itemType.ioError == IoErrorUnknown);
        CPPUNIT_ASSERT(itemType.nodeType == NodeTypeUnknown);
        CPPUNIT_ASSERT(itemType.linkType == LinkTypeNone);
        CPPUNIT_ASSERT(itemType.targetType == NodeTypeUnknown);
        CPPUNIT_ASSERT(itemType.targetPath == SyncPath{});

        _testObj->resetFunctions();
    }

    // A regular MacOSX Finder alias in a directory that misses search permission.
    {
        const TemporaryDirectory temporaryDirectory;
        const SyncPath subdir = temporaryDirectory.path / "directory_without_search_permission";
        std::filesystem::create_directory(subdir);

        const SyncPath targetPath = _localTestDirPath / "test_pictures" / "picture-1.jpg";
        const SyncPath path = subdir / "regular_file_alias";

        IoError aliasError;
        CPPUNIT_ASSERT(_testObj->createAliasFromPath(targetPath, path, aliasError));
        CPPUNIT_ASSERT(aliasError == IoErrorSuccess);
        CPPUNIT_ASSERT(std::filesystem::exists(path));

        std::filesystem::permissions(subdir, std::filesystem::perms::owner_exec, std::filesystem::perm_options::remove);
        _testObj->setIsSymlinkFunction([](const SyncPath &, std::error_code &ec) -> bool {
            ec = std::error_code{};
            return false;
        });

        const auto result = checker.checkAccessIsDenied(path);

        std::filesystem::permissions(subdir, std::filesystem::perms::owner_exec, std::filesystem::perm_options::add);
        std::filesystem::remove_all(subdir);  // required to allow automated deletion of `temporaryDirectory`

        CPPUNIT_ASSERT_MESSAGE(result.message, result.success);

        _testObj->resetFunctions();
    }
#endif
}

void TestIo::testGetItemTypeEdgeCases() {
#ifdef __APPLE__
    // A regular file with a file path of length 1023 which contains moreover a Japanese character.
    // Such a path causes the function `getResourceValue` to issue an error with code 258 in the following excerpt:
    //
    // NSURL *pathURL = [NSURL fileURLWithPath:pathStr]
    // NSError *error = nil;
    // NSNumber *isAliasNumber = nil;
    // BOOL ret = [pathURL getResourceValue:&isAliasNumber forKey:NSURLIsAliasFileKey error:&error];
    //
    // Above `ret` is FALSE and `error` has code 258 as in
    // https://developer.apple.com/documentation/foundation/1448136-nserror_codes/nsfilereadinvalidfilenameerror?language=objc
    // It is unclear which of `fileURLWithPath` or `getResourceValue` is the culprit.

    const TemporaryDirectory temporaryDirectory;
    const int bound = (1020 - temporaryDirectory.path.string().size()) / 4;
    std::string segment(bound, 'a');

    SyncPath path = temporaryDirectory.path / segment / segment / segment / u8"놔";

    std::error_code ec;
    CPPUNIT_ASSERT(std::filesystem::create_directories(path, ec));
    CPPUNIT_ASSERT(!ec);
    CPPUNIT_ASSERT(std::filesystem::exists(path));
    CPPUNIT_ASSERT(!ec);
    CPPUNIT_ASSERT(std::filesystem::is_directory(path, ec));
    CPPUNIT_ASSERT(!ec);

    path = path / std::string(1022 - path.string().size(), 'a');
    // The length of a file path can be at most 1024 on MacOSX, including the terminating null character.
    CPPUNIT_ASSERT_EQUAL(size_t(1023), path.string().size());
    { std::ofstream{path}; }

    ItemType itemType;
    CPPUNIT_ASSERT(!_testObj->getItemType(path, itemType));
    // The outcome depends on the value of `temporaryDirectory.path`.
    if (std::filesystem::exists(path, ec)) {
        CPPUNIT_ASSERT_EQUAL(
            IoErrorInvalidFileName,  // Ooops! Because of NSFileReadInvalidNameError with code 258 issued within `checkIfAlias_`
            itemType.ioError);
    } else {
        CPPUNIT_ASSERT_EQUAL(
            IoErrorFileNameTooLong,  // Ooops! Because of NSFileReadInvalidNameError with code 258 issued within `checkIfAlias_`
            itemType.ioError);
    }
#endif
}

void TestIo::testGetItemType() {
    testGetItemTypeSimpleCases();
    testGetItemTypeAllBranches();
    testGetItemTypeEdgeCases();
}

}  // namespace KDC

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

#include "libcommonserver/io/filestat.h"
#include "utility/utility.h"
#include "server/vfs/mac/litesyncextconnector.h"

#include <filesystem>

using namespace CppUnit;

namespace KDC {

void TestIo::testGetFileStat() {
    // A regular file
    {
        const SyncPath path = _localTestDirPath / "test_pictures/picture-1.jpg";
        FileStat fileStat;
        IoError ioError = IoErrorUnknown;

        CPPUNIT_ASSERT(_testObj->getFileStat(path, &fileStat, ioError));
        CPPUNIT_ASSERT(!fileStat.isHidden);
        CPPUNIT_ASSERT(fileStat.size == 408278u);
        CPPUNIT_ASSERT(fileStat.creationTime > 0);
        CPPUNIT_ASSERT(fileStat.modtime == fileStat.creationTime);
        CPPUNIT_ASSERT(fileStat.nodeType == NodeTypeFile);
        CPPUNIT_ASSERT(!fileStat.isEmptyOnDisk);
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);
    }

    // A regular directory
    {
        const SyncPath path = _localTestDirPath / "test_pictures";
        FileStat fileStat;
        IoError ioError = IoErrorUnknown;

        CPPUNIT_ASSERT(_testObj->getFileStat(path, &fileStat, ioError));
        CPPUNIT_ASSERT(!fileStat.isHidden);
#ifdef _WIN32
        CPPUNIT_ASSERT(fileStat.size == 0u);
#else
        CPPUNIT_ASSERT(fileStat.size > 0u);
#endif
        CPPUNIT_ASSERT(fileStat.creationTime > 0);
        CPPUNIT_ASSERT(fileStat.modtime >= fileStat.creationTime);
        CPPUNIT_ASSERT(fileStat.nodeType == NodeTypeDirectory);
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);
    }

    // A regular symbolic link on a file
    {
        const SyncPath targetPath = _localTestDirPath / "test_pictures/picture-1.jpg";
        const TemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path / "regular_file_symbolic_link";
        std::filesystem::create_symlink(targetPath, path);

        FileStat fileStat;
        IoError ioError = IoErrorUnknown;

        CPPUNIT_ASSERT(_testObj->getFileStat(path, &fileStat, ioError));
        CPPUNIT_ASSERT(!fileStat.isHidden);
#ifdef _WIN32
        CPPUNIT_ASSERT(fileStat.size == 0u);
#else
        CPPUNIT_ASSERT(fileStat.size == static_cast<int64_t>(targetPath.native().length()));
#endif
        CPPUNIT_ASSERT(fileStat.modtime == fileStat.creationTime);
        CPPUNIT_ASSERT(fileStat.nodeType == NodeTypeFile);
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);
    }

    // A regular symbolic link on a folder
    {
        const SyncPath targetPath = _localTestDirPath / "test_pictures";
        const TemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path / "regular_dir_symbolic_link";
        std::filesystem::create_directory_symlink(targetPath, path);

        FileStat fileStat;
        IoError ioError = IoErrorUnknown;

        CPPUNIT_ASSERT(_testObj->getFileStat(path, &fileStat, ioError));
        CPPUNIT_ASSERT(!fileStat.isHidden);
#ifdef _WIN32
        CPPUNIT_ASSERT(fileStat.size == 0u);
#else
        CPPUNIT_ASSERT(fileStat.size == static_cast<int64_t>(targetPath.native().length()));
#endif
        CPPUNIT_ASSERT(fileStat.nodeType == NodeTypeDirectory);
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);
    }
    // A non-existing file
    {
        const SyncPath path = _localTestDirPath / "non-existing.jpg";  // This file does not exist.
        FileStat fileStat;
        IoError ioError = IoErrorSuccess;

        CPPUNIT_ASSERT(_testObj->getFileStat(path, &fileStat, ioError));
        CPPUNIT_ASSERT(!fileStat.isHidden);
        CPPUNIT_ASSERT(fileStat.size == 0u);
        CPPUNIT_ASSERT(fileStat.modtime == 0);
        CPPUNIT_ASSERT(fileStat.creationTime == 0);
        CPPUNIT_ASSERT(fileStat.nodeType == NodeTypeUnknown);
        CPPUNIT_ASSERT(ioError == IoErrorNoSuchFileOrDirectory);
    }

    // A non-existing file with a very long name
    {
        const std::string veryLongfileName(1000, 'a');  // Exceeds the max allowed name length on every file system of interest.
        const SyncPath path = _localTestDirPath / veryLongfileName;  // This file doesn't exist.
        FileStat fileStat;
        IoError ioError = IoErrorSuccess;
        CPPUNIT_ASSERT(!_testObj->getFileStat(path, &fileStat, ioError));
        CPPUNIT_ASSERT(!fileStat.isHidden);
        CPPUNIT_ASSERT(fileStat.size == 0u);
        CPPUNIT_ASSERT(fileStat.modtime == 0);
        CPPUNIT_ASSERT(fileStat.creationTime == 0);
        CPPUNIT_ASSERT(fileStat.nodeType == NodeTypeUnknown);
#ifdef _WIN32
        CPPUNIT_ASSERT(ioError == IoErrorInvalidArgument);
#else
        CPPUNIT_ASSERT(ioError == IoErrorFileNameTooLong);
#endif
    }
    // A non-existing file with a very long path
    {
        const std::string pathSegment(50, 'a');
        SyncPath path = _localTestDirPath;
        for (int i = 0; i < 1000; ++i) {
            path /= pathSegment;  // Eventually exceeds the max allowed path length on every file system of interest.
        }
        FileStat fileStat;
        IoError ioError = IoErrorSuccess;

        CPPUNIT_ASSERT(!_testObj->getFileStat(path, &fileStat, ioError));
        CPPUNIT_ASSERT(!fileStat.isHidden);
        CPPUNIT_ASSERT(fileStat.size == 0u);
        CPPUNIT_ASSERT(fileStat.modtime == 0);
        CPPUNIT_ASSERT(fileStat.creationTime == 0);
        CPPUNIT_ASSERT(fileStat.nodeType == NodeTypeUnknown);
        CPPUNIT_ASSERT(ioError == IoErrorFileNameTooLong);
    }
    // A hidden file
    {
        const TemporaryDirectory temporaryDirectory;
#if defined(__APPLE__) || defined(WIN32)
        const SyncPath path = temporaryDirectory.path / "hidden_file.txt";
#else
        const SyncPath path = temporaryDirectory.path / ".hidden_file.txt";
#endif
        {
            std::ofstream ofs(path);
            ofs << "Some content.\n";
        }

#if defined(__APPLE__) || defined(WIN32)
        _testObj->setFileHidden(path, true);
#endif

        FileStat fileStat;
        IoError ioError = IoErrorUnknown;

        CPPUNIT_ASSERT(_testObj->getFileStat(path, &fileStat, ioError));

        CPPUNIT_ASSERT(fileStat.isHidden);
        CPPUNIT_ASSERT(fileStat.size > 0);
        CPPUNIT_ASSERT(fileStat.modtime == fileStat.creationTime);
        CPPUNIT_ASSERT(fileStat.nodeType == NodeTypeFile);
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);
    }

    // A hidden directory
    {
        const TemporaryDirectory temporaryDirectory;
#if defined(__APPLE__) || defined(WIN32)
        const SyncPath path = temporaryDirectory.path;
#else
        const SyncPath path = temporaryDirectory.path / ".hidden_directory";
        std::filesystem::create_directory(path);
#endif

#if defined(__APPLE__) || defined(WIN32)
        _testObj->setFileHidden(path, true);
#endif
        FileStat fileStat;
        IoError ioError = IoErrorUnknown;

        CPPUNIT_ASSERT(_testObj->getFileStat(path, &fileStat, ioError));
        CPPUNIT_ASSERT(fileStat.isHidden);
        CPPUNIT_ASSERT(fileStat.modtime == fileStat.creationTime);
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);
        CPPUNIT_ASSERT(fileStat.nodeType == NodeTypeDirectory);
    }
    // An existing file with dots and colons in its name
    {
        const TemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path / ":.file.::.name.:";
        {
            std::ofstream ofs(path);
            ofs << "Some content.\n";
        }

        FileStat fileStat;
        IoError ioError = IoErrorUnknown;
#ifdef _WIN32
        CPPUNIT_ASSERT(!_testObj->getFileStat(path, &fileStat, ioError));
        CPPUNIT_ASSERT(!fileStat.isHidden);
        CPPUNIT_ASSERT(fileStat.size == 0u);
        CPPUNIT_ASSERT(fileStat.modtime == 0u);
        CPPUNIT_ASSERT(fileStat.modtime == fileStat.creationTime);
        CPPUNIT_ASSERT(fileStat.nodeType == NodeTypeUnknown);
        CPPUNIT_ASSERT(ioError == IoErrorUnknown);
#else
        CPPUNIT_ASSERT(_testObj->getFileStat(path, &fileStat, ioError));
        CPPUNIT_ASSERT(!fileStat.isHidden);
        CPPUNIT_ASSERT(fileStat.size > 0);
        CPPUNIT_ASSERT(fileStat.modtime == fileStat.creationTime);
        CPPUNIT_ASSERT(fileStat.nodeType == NodeTypeFile);
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);
#endif
    }

    // An existing file with emojis in its name
    {
        using namespace std::string_literals;  // operator ""s
        const TemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path / std::string{u8"🫃😋🌲👣🍔🕉️⛎"s};
        {
            std::ofstream ofs(path);
            ofs << "Some content.\n";
            ofs.close();
        }

        FileStat fileStat;
        IoError ioError = IoErrorUnknown;

        CPPUNIT_ASSERT(_testObj->getFileStat(path, &fileStat, ioError));
        CPPUNIT_ASSERT(!fileStat.isHidden);
        CPPUNIT_ASSERT(fileStat.size > 0);
        CPPUNIT_ASSERT(fileStat.modtime == fileStat.creationTime);
        CPPUNIT_ASSERT(fileStat.nodeType == NodeTypeFile);
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);
    }

    // A dangling symbolic link
    {
        const TemporaryDirectory temporaryDirectory;
        const SyncPath targetPath = temporaryDirectory.path / "non_existing_test_file.txt";  // This file does not exist.
        const SyncPath path = temporaryDirectory.path / "dangling_symbolic_link";
        std::filesystem::create_symlink(targetPath, path);

        FileStat fileStat;
        IoError ioError = IoErrorUnknown;

        CPPUNIT_ASSERT(_testObj->getFileStat(path, &fileStat, ioError));
        CPPUNIT_ASSERT(!fileStat.isHidden);
#ifdef _WIN32
        CPPUNIT_ASSERT(fileStat.size == 0u);
#else
        CPPUNIT_ASSERT(fileStat.size == static_cast<int64_t>(targetPath.native().length()));
#endif
        CPPUNIT_ASSERT(fileStat.modtime == fileStat.creationTime);
#ifdef _WIN32
        CPPUNIT_ASSERT(fileStat.nodeType == NodeTypeFile);
#else
        CPPUNIT_ASSERT(fileStat.nodeType == NodeTypeUnknown);
#endif
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);
    }
#if defined(__APPLE__)
    // A MacOSX Finder alias on a regular file.
    {
        const TemporaryDirectory temporaryDirectory;
        const SyncPath targetPath = _localTestDirPath / "test_pictures/picture-1.jpg";
        const SyncPath path = temporaryDirectory.path / "regular_file_alias";

        IoError aliasError;
        IoHelper::createAliasFromPath(targetPath, path, aliasError);

        FileStat fileStat;
        IoError ioError = IoErrorUnknown;

        CPPUNIT_ASSERT(_testObj->getFileStat(path, &fileStat, ioError));
        CPPUNIT_ASSERT(!fileStat.isHidden);
        CPPUNIT_ASSERT(fileStat.size > 0);  // `fileStat.size` is greater than `static_cast<int64_t>(path.native().length())`
        CPPUNIT_ASSERT(fileStat.modtime == fileStat.creationTime);
        CPPUNIT_ASSERT(fileStat.nodeType == NodeTypeFile);
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);
    }

    // A MacOSX Finder alias on a regular directory.
    {
        const TemporaryDirectory temporaryDirectory;
        const SyncPath targetPath = _localTestDirPath / "test_pictures";
        const SyncPath path = temporaryDirectory.path / "regular_dir_alias";

        IoError aliasError;
        IoHelper::createAliasFromPath(targetPath, path, aliasError);

        FileStat fileStat;
        IoError ioError = IoErrorUnknown;

        CPPUNIT_ASSERT(_testObj->getFileStat(path, &fileStat, ioError));
        CPPUNIT_ASSERT(!fileStat.isHidden);
        CPPUNIT_ASSERT(fileStat.size > 0);  // `fileStat.size` is greater than `static_cast<int64_t>(path.native().length())`
        CPPUNIT_ASSERT(fileStat.modtime == fileStat.creationTime);
        CPPUNIT_ASSERT(fileStat.nodeType == NodeTypeFile);
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);
    }

    // A dangling MacOSX Finder alias on a non-existing file.
    {
        const TemporaryDirectory temporaryDirectory;
        const SyncPath targetPath = temporaryDirectory.path / "file_to_be_deleted.png";  // This file will be deleted.
        const SyncPath path = temporaryDirectory.path / "dangling_file_alias";
        {
            std::ofstream ofs(targetPath);
            ofs << "Some content.\n";
        }

        IoError aliasError;
        IoHelper::createAliasFromPath(targetPath, path, aliasError);
        std::filesystem::remove(targetPath);

        FileStat fileStat;
        IoError ioError = IoErrorUnknown;

        CPPUNIT_ASSERT(_testObj->getFileStat(path, &fileStat, ioError));
        CPPUNIT_ASSERT(!fileStat.isHidden);
        CPPUNIT_ASSERT(fileStat.size > 0);  // `fileStat.size` is greater than `static_cast<int64_t>(path.native().length())`
        CPPUNIT_ASSERT(fileStat.modtime == fileStat.creationTime);
        CPPUNIT_ASSERT(fileStat.nodeType == NodeTypeFile);
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);
    }

    // A dangling MacOSX Finder alias on a non-existing directory.
    {
        const TemporaryDirectory temporaryDirectory;
        const SyncPath targetPath = temporaryDirectory.path / "directory_to_be_deleted";  // This directory will be deleted.
        std::filesystem::create_directory(targetPath);

        const SyncPath path = temporaryDirectory.path / "dangling_directory_alias";

        IoError aliasError;
        CPPUNIT_ASSERT(IoHelper::createAliasFromPath(targetPath, path, aliasError));
        std::filesystem::remove_all(targetPath);

        FileStat fileStat;
        IoError ioError = IoErrorUnknown;

        CPPUNIT_ASSERT(_testObj->getFileStat(path, &fileStat, ioError));
        CPPUNIT_ASSERT(!fileStat.isHidden);
        CPPUNIT_ASSERT(fileStat.size > 0);  // `fileStat.size` is greater than `static_cast<int64_t>(path.native().length())`
        CPPUNIT_ASSERT(fileStat.modtime == fileStat.creationTime);
        CPPUNIT_ASSERT(fileStat.nodeType == NodeTypeFile);
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);
    }
#endif
    // A regular file missing all permissions (no error expected)
    {
        const TemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path / "permission_less_file.txt";
        {
            std::ofstream ofs(path);
            ofs << "Some content.\n";
        }

        std::filesystem::permissions(path, std::filesystem::perms::all, std::filesystem::perm_options::remove);

        FileStat fileStat;
        IoError ioError = IoErrorUnknown;

        CPPUNIT_ASSERT(_testObj->getFileStat(path, &fileStat, ioError));

        std::filesystem::permissions(path, std::filesystem::perms::all, std::filesystem::perm_options::add);

        CPPUNIT_ASSERT(!fileStat.isHidden);
        CPPUNIT_ASSERT(fileStat.size > 0);
        CPPUNIT_ASSERT(fileStat.modtime == fileStat.creationTime);
        CPPUNIT_ASSERT(fileStat.nodeType == NodeTypeFile);
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);
    }

    // A regular directory missing all permissions (no error expected)
    {
        const TemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path / "permission_less_directory";
        std::filesystem::create_directory(path);

        std::filesystem::permissions(path, std::filesystem::perms::all, std::filesystem::perm_options::remove);

        FileStat fileStat;
        IoError ioError = IoErrorUnknown;

        CPPUNIT_ASSERT(_testObj->getFileStat(path, &fileStat, ioError));

        std::filesystem::permissions(path, std::filesystem::perms::all, std::filesystem::perm_options::add);

        CPPUNIT_ASSERT(!fileStat.isHidden);
#ifdef _WIN32
        CPPUNIT_ASSERT(fileStat.size == 0u);
#else
        CPPUNIT_ASSERT(fileStat.size > 0u);
#endif
        CPPUNIT_ASSERT(fileStat.modtime == fileStat.creationTime);
        CPPUNIT_ASSERT(fileStat.nodeType == NodeTypeDirectory);
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);
    }

    // A regular file within a subdirectory that misses owner read permission (no error expected)
    {
        const TemporaryDirectory temporaryDirectory;
        const SyncPath subdir = temporaryDirectory.path / "permission_less_subdirectory";
        std::filesystem::create_directory(subdir);
        const SyncPath path = subdir / "file.txt";
        {
            std::ofstream ofs(path);
            ofs << "Some content.\n";
        }
        std::filesystem::permissions(subdir, std::filesystem::perms::owner_read, std::filesystem::perm_options::remove);

        FileStat fileStat;
        IoError ioError = IoErrorUnknown;
#ifdef _WIN32
        CPPUNIT_ASSERT(_testObj->getFileStat(path, &fileStat, ioError));
#else
        CPPUNIT_ASSERT(_testObj->getFileStat(path, &fileStat, ioError));
#endif
        // Restore permission to allow subdir removal
        std::filesystem::permissions(subdir, std::filesystem::perms::owner_read, std::filesystem::perm_options::add);

        CPPUNIT_ASSERT(!fileStat.isHidden);
        CPPUNIT_ASSERT(fileStat.size > 0);
        CPPUNIT_ASSERT(fileStat.modtime == fileStat.creationTime);
        CPPUNIT_ASSERT(fileStat.nodeType == NodeTypeFile);
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);
    }

    // A regular file within a subdirectory that misses owner search/exec permission:
    // - access denied expected on MacOSX
    // - no error expected on Windows
    {
        const TemporaryDirectory temporaryDirectory;
        const SyncPath subdir = temporaryDirectory.path / "permission_less_subdirectory";
        std::filesystem::create_directory(subdir);
        const SyncPath path = subdir / "file.txt";
        {
            std::ofstream ofs(path);
            ofs << "Some content.\n";
        }
        std::filesystem::permissions(subdir, std::filesystem::perms::owner_exec, std::filesystem::perm_options::remove);

        FileStat fileStat;
        IoError ioError = IoErrorUnknown;
        CPPUNIT_ASSERT(_testObj->getFileStat(path, &fileStat, ioError));

        // Restore permission to allow subdir removal
        std::filesystem::permissions(subdir, std::filesystem::perms::owner_exec, std::filesystem::perm_options::add);

        CPPUNIT_ASSERT(!fileStat.isHidden);
#ifdef _WIN32
        CPPUNIT_ASSERT(fileStat.size > 0u);
        CPPUNIT_ASSERT(fileStat.modtime > 0);
        CPPUNIT_ASSERT(fileStat.modtime == fileStat.creationTime);
        CPPUNIT_ASSERT(fileStat.nodeType == NodeTypeFile);
#else
        CPPUNIT_ASSERT(fileStat.size == 0u);
        CPPUNIT_ASSERT(fileStat.modtime == 0);
        CPPUNIT_ASSERT(fileStat.creationTime == 0);
        CPPUNIT_ASSERT(fileStat.nodeType == NodeTypeUnknown);
        CPPUNIT_ASSERT(ioError == IoErrorAccessDenied);
#endif
    }

#if defined(__APPLE__)
    // A hydrated placeholder
    {
        // Create temp directory and file.
        const TemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path / "test_file.txt";
        {
            std::ofstream ofs(path);
            ofs << "abc";
            ofs.close();
        }
        // Convert file to placeholder
        const auto extConnector = LiteSyncExtConnector::instance(Log::instance()->getLogger(), ExecuteCommand());
        extConnector->vfsConvertToPlaceHolder(Path2QStr(path), true);

        FileStat fileStat;
        IoError ioError = IoErrorUnknown;
        CPPUNIT_ASSERT(_testObj->getFileStat(path, &fileStat, ioError));
        CPPUNIT_ASSERT(!fileStat.isHidden);
        CPPUNIT_ASSERT(fileStat.nodeType == NodeTypeFile);
        CPPUNIT_ASSERT(!fileStat.isEmptyOnDisk);
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);
    }

    // A dehydrated placeholder
    {
        // Create temp directory and file.
        const TemporaryDirectory temporaryDirectory;
        // Create placeholder
        const auto extConnector = LiteSyncExtConnector::instance(Log::instance()->getLogger(), ExecuteCommand());
        const auto timeNow =
            std::chrono::duration_cast<std::chrono::hours>(std::chrono::system_clock::now().time_since_epoch()).count();
        struct stat fileInfo;
        fileInfo.st_size = 123;
        fileInfo.st_mtimespec = {timeNow, 0};
        fileInfo.st_atimespec = {timeNow, 0};
        fileInfo.st_birthtimespec = {timeNow, 0};
        fileInfo.st_mode = S_IFREG;
        SyncName fileName = "test_file.txt";
        extConnector->vfsCreatePlaceHolder(SyncName2QStr(fileName), Path2QStr(temporaryDirectory.path), &fileInfo);

        FileStat fileStat;
        IoError ioError = IoErrorUnknown;
        CPPUNIT_ASSERT(_testObj->getFileStat(temporaryDirectory.path / fileName, &fileStat, ioError));
        CPPUNIT_ASSERT(!fileStat.isHidden);
        CPPUNIT_ASSERT(fileStat.nodeType == NodeTypeFile);
        CPPUNIT_ASSERT(fileStat.isEmptyOnDisk);
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);
    }
#endif
}

}  // namespace KDC

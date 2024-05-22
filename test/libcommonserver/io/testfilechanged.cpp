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

#include <filesystem>

using namespace CppUnit;

namespace KDC {

void TestIo::testFileChanged() {
    // An unmodified regular file
    {
        const SyncPath path = _localTestDirPath / "test_pictures/picture-1.jpg";
        FileStat fileStat;
        IoError ioError = IoErrorSuccess;
        _testObj->getFileStat(path, &fileStat, ioError);

        bool changed = true;
        CPPUNIT_ASSERT(_testObj->checkIfFileChanged(path, fileStat.size, fileStat.modtime, changed, ioError));
        CPPUNIT_ASSERT(!changed);
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);
    }

    // An unmodified regular directory
    {
        const SyncPath path = _localTestDirPath / "test_pictures";
        FileStat fileStat;
        IoError ioError = IoErrorSuccess;

        _testObj->getFileStat(path, &fileStat, ioError);

        bool changed = true;
        CPPUNIT_ASSERT(_testObj->checkIfFileChanged(path, fileStat.size, fileStat.modtime, changed, ioError));
        CPPUNIT_ASSERT(!changed);
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);
    }

    // An unmodified regular symbolic link on a file
    {
        const SyncPath targetPath = _localTestDirPath / "test_pictures/picture-1.jpg";
        const TemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path / "regular_file_symbolic_link";
        std::filesystem::create_symlink(targetPath, path);

        FileStat fileStat;
        IoError ioError = IoErrorSuccess;
        _testObj->getFileStat(path, &fileStat, ioError);

        bool changed = true;
        CPPUNIT_ASSERT(_testObj->checkIfFileChanged(path, fileStat.size, fileStat.modtime, changed, ioError));
        CPPUNIT_ASSERT(!changed);
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);
    }

    // A non-existing file
    {
        const SyncPath path = _localTestDirPath / "non-existing.jpg";  // This file does not exist.
        IoError ioError = IoErrorSuccess;
        bool changed = true;

        CPPUNIT_ASSERT(_testObj->checkIfFileChanged(path, 0, 0, changed, ioError));
        CPPUNIT_ASSERT(!changed);
        CPPUNIT_ASSERT(ioError == IoErrorNoSuchFileOrDirectory);
    }

    // A file whose content has been modified
    {
        const TemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path / "file.txt";
        {
            std::ofstream ofs(path);
            ofs << "Some content.\n";
        }

        FileStat fileStat;
        IoError ioError = IoErrorSuccess;
        _testObj->getFileStat(path, &fileStat, ioError);

        // Editing
        {
            std::ofstream ofs(path, std::ios_base::app | std::ios_base::out);
            ofs << "A new line";
        }

        bool changed = false;
        CPPUNIT_ASSERT(_testObj->checkIfFileChanged(path, fileStat.size, fileStat.modtime, changed, ioError));
        CPPUNIT_ASSERT(changed);
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);
    }

    // A file whose permissions have been modified: no change detected
    {
        const TemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path / "file.txt";
        {
            std::ofstream ofs(path);
            ofs << "Some content.\n";
        }

        FileStat fileStat;
        IoError ioError = IoErrorSuccess;

        _testObj->getFileStat(path, &fileStat, ioError);

        std::filesystem::permissions(path, std::filesystem::perms::all, std::filesystem::perm_options::remove);

        bool changed = true;
        CPPUNIT_ASSERT(_testObj->checkIfFileChanged(path, fileStat.size, fileStat.modtime, changed, ioError));

        std::filesystem::permissions(path, std::filesystem::perms::all, std::filesystem::perm_options::add);

        CPPUNIT_ASSERT(!changed);
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);
    }

#if defined(__APPLE__) || defined(WIN32)
    // A file that is set to "hidden" on MacOSX or Windows: no change detected
    {
        const TemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path / "visible_file.txt";
        { std::ofstream ofs(path); }

        FileStat fileStat;
        IoError ioError = IoErrorSuccess;

        _testObj->getFileStat(path, &fileStat, ioError);

        _testObj->setFileHidden(path, true);
        bool changed = true;
        CPPUNIT_ASSERT(_testObj->checkIfFileChanged(path, fileStat.size, fileStat.modtime, changed, ioError));
        CPPUNIT_ASSERT(!changed);
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);
    }
#endif
}


void TestIo::testCheckIfIsHiddenFile() {
    // A non-hidden file
    {
        const SyncPath path = _localTestDirPath / "test_pictures/picture-1.jpg";
        bool isHidden = true;
        IoError ioError = IoErrorUnknown;

        CPPUNIT_ASSERT(_testObj->checkIfIsHiddenFile(path, isHidden, ioError));
        CPPUNIT_ASSERT(!isHidden);
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);
    }

    // A non-hidden directory
    {
        bool isHidden = true;
        IoError ioError = IoErrorUnknown;

        CPPUNIT_ASSERT(_testObj->checkIfIsHiddenFile(_localTestDirPath, isHidden, ioError));
        CPPUNIT_ASSERT(!isHidden);
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);
    }

#if defined(__APPLE__) || defined(WIN32)
    // A hidden file on MacOSX and Windows
    {
        const TemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path / "hidden_file.txt";
        { std::ofstream ofs(path); }

        _testObj->setFileHidden(path, true);
        // On MacOSX, the '/var' folder is hidden.
        // This is why we only check the item indicated by `path` and not its ancestors.
        bool isHidden = true;
        IoError ioError = IoErrorUnknown;
        CPPUNIT_ASSERT(_testObj->checkIfIsHiddenFile(path, false, isHidden, ioError));
        CPPUNIT_ASSERT(isHidden);
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);

        // Toggling the hidden status
        isHidden = true;
        ioError = IoErrorUnknown;
        _testObj->setFileHidden(path, false);
        CPPUNIT_ASSERT(_testObj->checkIfIsHiddenFile(path, false, isHidden, ioError));
        CPPUNIT_ASSERT(!isHidden);
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);
    }
#endif

#if !defined(WIN32)
    // A hidden file on MacOSX and Linux
    {
        const TemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path / ".hidden_file.txt";
        { std::ofstream ofs(path); }
        bool isHidden = false;
        IoError ioError = IoErrorUnknown;

        CPPUNIT_ASSERT(_testObj->checkIfIsHiddenFile(path, isHidden, ioError));
        CPPUNIT_ASSERT(isHidden);
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);

        isHidden = false;
        ioError = IoErrorUnknown;

        CPPUNIT_ASSERT(_testObj->checkIfIsHiddenFile(path, false, isHidden, ioError));
        CPPUNIT_ASSERT(isHidden);
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);
    }
#endif

#if defined(__APPLE__) || defined(WIN32)
    // A non-hidden file within a hidden directory
    {
        const TemporaryDirectory temporaryDirectory;
        const SyncPath hiddenSubdir = temporaryDirectory.path / "hidden";
        std::filesystem::create_directory(hiddenSubdir);
        const SyncPath path = hiddenSubdir / "visible_file.txt";
        {
            std::ofstream ofs(path);
            ofs << "Some content.\n";
            ofs.close();
        }

        _testObj->setFileHidden(hiddenSubdir, true);
        bool isHidden = false;
        IoError ioError = IoErrorUnknown;

        CPPUNIT_ASSERT(_testObj->checkIfIsHiddenFile(hiddenSubdir, false, isHidden, ioError));
        CPPUNIT_ASSERT(isHidden);
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);

        isHidden = true;
        ioError = IoErrorUnknown;
        CPPUNIT_ASSERT(_testObj->checkIfIsHiddenFile(path, false, isHidden, ioError));
        CPPUNIT_ASSERT(!isHidden);
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);

        isHidden = false;
        ioError = IoErrorUnknown;
        CPPUNIT_ASSERT(_testObj->checkIfIsHiddenFile(path, isHidden, ioError));
        CPPUNIT_ASSERT(isHidden);
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);

        // Toggling the hidden status
        _testObj->setFileHidden(hiddenSubdir, false);
        isHidden = true;
        ioError = IoErrorUnknown;
        CPPUNIT_ASSERT(_testObj->checkIfIsHiddenFile(hiddenSubdir, false, isHidden, ioError));
        CPPUNIT_ASSERT(!isHidden);
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);

        isHidden = true;
        ioError = IoErrorUnknown;
        CPPUNIT_ASSERT(_testObj->checkIfIsHiddenFile(path, false, isHidden, ioError));
        CPPUNIT_ASSERT(!isHidden);
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);
    }
#endif

#if !defined(WIN32)
    // A non-hidden file within a hidden directory
    {
        const TemporaryDirectory temporaryDirectory;
        const SyncPath hiddenSubdir = temporaryDirectory.path / ".hidden";
        std::filesystem::create_directory(hiddenSubdir);
        const SyncPath path = hiddenSubdir / "visible_file.txt";
        { std::ofstream ofs(path); }

        bool isHidden = false;
        IoError ioError = IoErrorUnknown;
        CPPUNIT_ASSERT(_testObj->checkIfIsHiddenFile(hiddenSubdir, false, isHidden, ioError));
        CPPUNIT_ASSERT(isHidden);
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);

        isHidden = false;
        ioError = IoErrorUnknown;
        CPPUNIT_ASSERT(_testObj->checkIfIsHiddenFile(hiddenSubdir, isHidden, ioError));
        CPPUNIT_ASSERT(isHidden);
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);

        isHidden = true;
        ioError = IoErrorUnknown;
        CPPUNIT_ASSERT(_testObj->checkIfIsHiddenFile(path, false, isHidden, ioError));
        CPPUNIT_ASSERT(!isHidden);
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);

        isHidden = false;
        ioError = IoErrorUnknown;
        CPPUNIT_ASSERT(_testObj->checkIfIsHiddenFile(path, isHidden, ioError));
        CPPUNIT_ASSERT(isHidden);
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);
    }
#endif

    // A non-existing file is not hidden if its name doesn't start with a dot
    {
        const SyncPath path = _localTestDirPath / "non-existing.jpg";  // This file does not exist.
        bool isHidden = true;
        IoError ioError = IoErrorUnknown;

        CPPUNIT_ASSERT(_testObj->checkIfIsHiddenFile(path, false, isHidden, ioError));
        CPPUNIT_ASSERT(!isHidden);
        CPPUNIT_ASSERT(ioError == IoErrorNoSuchFileOrDirectory);
    }

#if !defined(WIN32)
    // A non-existing file is hidden if its name starts with a dot
    {
        const TemporaryDirectory temporaryDirectory;
        const SyncPath path = ".non_existing_hidden_file.txt";

        bool isHidden = false;
        IoError ioError = IoErrorUnknown;
        CPPUNIT_ASSERT(_testObj->checkIfIsHiddenFile(path, false, isHidden, ioError));
        CPPUNIT_ASSERT(isHidden);
        CPPUNIT_ASSERT(ioError == IoErrorNoSuchFileOrDirectory);
    }
#endif

    // A non-existing file with a very long path causes an error.
    {
        const std::string pathSegment(50, 'a');
        SyncPath path = _localTestDirPath;
        for (int i = 0; i < 1000; ++i) {
            path /= pathSegment;  // Eventually exceeds the max allowed path length on every file system of interest.
        }

        bool isHidden = true;
        IoError ioError = IoErrorSuccess;
#ifdef _WIN32
        CPPUNIT_ASSERT(_testObj->checkIfIsHiddenFile(path, false, isHidden, ioError));
        CPPUNIT_ASSERT(ioError == IoErrorNoSuchFileOrDirectory);
#else
        CPPUNIT_ASSERT(!_testObj->checkIfIsHiddenFile(path, false, isHidden, ioError));
        CPPUNIT_ASSERT(ioError == IoErrorFileNameTooLong);
#endif
        CPPUNIT_ASSERT(!isHidden);
    }

#if !defined(WIN32)
    // A non-existing file with a very long path is hidden if its name starts with a dot
    {
        std::string pathSegment(50, '.');
        SyncPath path = _localTestDirPath;
        for (int i = 0; i < 1000; ++i) {
            path /= pathSegment;  // Eventually exceeds the max allowed path length on every file system of interest.
        }

        bool isHidden = true;
        IoError ioError = IoErrorSuccess;
        CPPUNIT_ASSERT(!_testObj->checkIfIsHiddenFile(path, false, isHidden, ioError));
        CPPUNIT_ASSERT(!isHidden);
        CPPUNIT_ASSERT(ioError == IoErrorFileNameTooLong);
    }
#endif

#if defined(__APPLE__)
    // On MacOSX /Volumes is hidden by the Finder whereas kDrive considers it visible
    {
        bool isHidden = true;
        IoError ioError = IoErrorUnknown;
        CPPUNIT_ASSERT(_testObj->checkIfIsHiddenFile("/Volumes", false, isHidden, ioError));
        CPPUNIT_ASSERT(!isHidden);
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);

        isHidden = true;
        ioError = IoErrorUnknown;
        CPPUNIT_ASSERT(_testObj->checkIfIsHiddenFile("/Volumes/visible", isHidden, ioError));
        CPPUNIT_ASSERT(!isHidden);
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);
    }
#endif
}
}  // namespace KDC

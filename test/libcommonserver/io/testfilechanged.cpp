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
        IoError ioError = IoError::Success;
        IoHelper::getFileStat(path, &fileStat, ioError);

        bool changed = true;
        CPPUNIT_ASSERT(IoHelper::checkIfFileChanged(path, fileStat.size, fileStat.modificationTime, fileStat.creationTime,
                                                    changed, ioError));
        CPPUNIT_ASSERT(!changed);
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);
    }

    // An unmodified regular directory
    {
        const SyncPath path = _localTestDirPath / "test_pictures";
        FileStat fileStat;
        IoError ioError = IoError::Success;

        IoHelper::getFileStat(path, &fileStat, ioError);

        bool changed = true;
        CPPUNIT_ASSERT(IoHelper::checkIfFileChanged(path, fileStat.size, fileStat.modificationTime, fileStat.creationTime,
                                                    changed, ioError));
        CPPUNIT_ASSERT(!changed);
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);
    }

    // An unmodified regular symbolic link on a file
    {
        const SyncPath targetPath = _localTestDirPath / "test_pictures/picture-1.jpg";
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path() / "regular_file_symbolic_link";
        std::filesystem::create_symlink(targetPath, path);

        FileStat fileStat;
        IoError ioError = IoError::Success;
        IoHelper::getFileStat(path, &fileStat, ioError);

        bool changed = true;
        CPPUNIT_ASSERT(IoHelper::checkIfFileChanged(path, fileStat.size, fileStat.modificationTime, fileStat.creationTime,
                                                    changed, ioError));
        CPPUNIT_ASSERT(!changed);
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);
    }

    // A non-existing file
    {
        const SyncPath path = _localTestDirPath / "non-existing.jpg"; // This file does not exist.
        IoError ioError = IoError::Success;
        bool changed = true;

        CPPUNIT_ASSERT(IoHelper::checkIfFileChanged(path, 0, 0, 0, changed, ioError));
        CPPUNIT_ASSERT(!changed);
        CPPUNIT_ASSERT(ioError == IoError::NoSuchFileOrDirectory);
    }

    // A file whose content has been modified
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path() / "file.txt";
        {
            std::ofstream ofs(path);
            ofs << "Some content.\n";
        }

        FileStat fileStat;
        IoError ioError = IoError::Success;
        IoHelper::getFileStat(path, &fileStat, ioError);

        // Editing
        {
            std::ofstream ofs(path, std::ios_base::app | std::ios_base::out);
            ofs << "A new line";
        }

        bool changed = false;
        CPPUNIT_ASSERT(IoHelper::checkIfFileChanged(path, fileStat.size, fileStat.modificationTime, fileStat.creationTime,
                                                    changed, ioError));
        CPPUNIT_ASSERT(changed);
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);
    }

    // A file whose permissions have been modified: no change detected
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path() / "file.txt";
        {
            std::ofstream ofs(path);
            ofs << "Some content.\n";
        }

        FileStat fileStat;
        IoError ioError = IoError::Success;

        IoHelper::getFileStat(path, &fileStat, ioError);

        std::filesystem::permissions(path, std::filesystem::perms::all, std::filesystem::perm_options::remove);

        bool changed = true;
        CPPUNIT_ASSERT(IoHelper::checkIfFileChanged(path, fileStat.size, fileStat.modificationTime, fileStat.creationTime,
                                                    changed, ioError));

        std::filesystem::permissions(path, std::filesystem::perms::all, std::filesystem::perm_options::add);

        CPPUNIT_ASSERT(!changed);
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);
    }

#if defined(KD_MACOS) || defined(KD_WINDOWS)
    // A file that is set to "hidden" on MacOSX or Windows: no change detected
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path() / "visible_file.txt";
        { std::ofstream ofs(path); }

        FileStat fileStat;
        IoError ioError = IoError::Success;

        IoHelper::getFileStat(path, &fileStat, ioError);

        IoHelper::setFileHidden(path, true);
        bool changed = true;
        CPPUNIT_ASSERT(IoHelper::checkIfFileChanged(path, fileStat.size, fileStat.modificationTime, fileStat.creationTime,
                                                    changed, ioError));
        CPPUNIT_ASSERT(!changed);
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);
    }
#endif
}


void TestIo::testCheckIfIsHiddenFile() {
    // A non-hidden file
    {
        const SyncPath path = _localTestDirPath / "test_pictures/picture-1.jpg";
        bool isHidden = true;
        IoError ioError = IoError::Unknown;

        CPPUNIT_ASSERT(IoHelper::checkIfIsHiddenFile(path, isHidden, ioError));
        CPPUNIT_ASSERT(!isHidden);
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);
    }

    // A non-hidden directory
    {
        bool isHidden = true;
        IoError ioError = IoError::Unknown;

        CPPUNIT_ASSERT(IoHelper::checkIfIsHiddenFile(_localTestDirPath, isHidden, ioError));
        CPPUNIT_ASSERT(!isHidden);
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);
    }

#if defined(KD_MACOS) || defined(KD_WINDOWS)
    // A hidden file on MacOSX and Windows
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path() / "hidden_file.txt";
        { std::ofstream ofs(path); }

        IoHelper::setFileHidden(path, true);
        // On MacOSX, the '/var' folder is hidden.
        // This is why we only check the item indicated by `path` and not its ancestors.
        bool isHidden = true;
        IoError ioError = IoError::Unknown;
        CPPUNIT_ASSERT(IoHelper::checkIfIsHiddenFile(path, false, isHidden, ioError));
        CPPUNIT_ASSERT(isHidden);
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);

        // Toggling the hidden status
        isHidden = true;
        ioError = IoError::Unknown;
        IoHelper::setFileHidden(path, false);
        CPPUNIT_ASSERT(IoHelper::checkIfIsHiddenFile(path, false, isHidden, ioError));
        CPPUNIT_ASSERT(!isHidden);
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);
    }
#endif

#if !defined(KD_WINDOWS)
    // A hidden file on MacOSX and Linux
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path() / ".hidden_file.txt";
        { std::ofstream ofs(path); }
        bool isHidden = false;
        IoError ioError = IoError::Unknown;

        CPPUNIT_ASSERT(IoHelper::checkIfIsHiddenFile(path, isHidden, ioError));
        CPPUNIT_ASSERT(isHidden);
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);

        isHidden = false;
        ioError = IoError::Unknown;

        CPPUNIT_ASSERT(IoHelper::checkIfIsHiddenFile(path, false, isHidden, ioError));
        CPPUNIT_ASSERT(isHidden);
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);
    }
#endif

#if defined(KD_MACOS) || defined(KD_WINDOWS)
    // A non-hidden file within a hidden directory
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath hiddenSubdir = temporaryDirectory.path() / "hidden";
        std::filesystem::create_directory(hiddenSubdir);
        const SyncPath path = hiddenSubdir / "visible_file.txt";
        {
            std::ofstream ofs(path);
            ofs << "Some content.\n";
            ofs.close();
        }

        IoHelper::setFileHidden(hiddenSubdir, true);
        bool isHidden = false;
        IoError ioError = IoError::Unknown;

        CPPUNIT_ASSERT(IoHelper::checkIfIsHiddenFile(hiddenSubdir, false, isHidden, ioError));
        CPPUNIT_ASSERT(isHidden);
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);

        isHidden = true;
        ioError = IoError::Unknown;
        CPPUNIT_ASSERT(IoHelper::checkIfIsHiddenFile(path, false, isHidden, ioError));
        CPPUNIT_ASSERT(!isHidden);
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);

        isHidden = false;
        ioError = IoError::Unknown;
        CPPUNIT_ASSERT(IoHelper::checkIfIsHiddenFile(path, isHidden, ioError));
        CPPUNIT_ASSERT(isHidden);
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);

        // Toggling the hidden status
        IoHelper::setFileHidden(hiddenSubdir, false);
        isHidden = true;
        ioError = IoError::Unknown;
        CPPUNIT_ASSERT(IoHelper::checkIfIsHiddenFile(hiddenSubdir, false, isHidden, ioError));
        CPPUNIT_ASSERT(!isHidden);
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);

        isHidden = true;
        ioError = IoError::Unknown;
        CPPUNIT_ASSERT(IoHelper::checkIfIsHiddenFile(path, false, isHidden, ioError));
        CPPUNIT_ASSERT(!isHidden);
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);
    }
#endif

#if !defined(KD_WINDOWS)
    // A non-hidden file within a hidden directory
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath hiddenSubdir = temporaryDirectory.path() / ".hidden";
        std::filesystem::create_directory(hiddenSubdir);
        const SyncPath path = hiddenSubdir / "visible_file.txt";
        { std::ofstream ofs(path); }

        bool isHidden = false;
        IoError ioError = IoError::Unknown;
        CPPUNIT_ASSERT(IoHelper::checkIfIsHiddenFile(hiddenSubdir, false, isHidden, ioError));
        CPPUNIT_ASSERT(isHidden);
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);

        isHidden = false;
        ioError = IoError::Unknown;
        CPPUNIT_ASSERT(IoHelper::checkIfIsHiddenFile(hiddenSubdir, isHidden, ioError));
        CPPUNIT_ASSERT(isHidden);
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);

        isHidden = true;
        ioError = IoError::Unknown;
        CPPUNIT_ASSERT(IoHelper::checkIfIsHiddenFile(path, false, isHidden, ioError));
        CPPUNIT_ASSERT(!isHidden);
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);

        isHidden = false;
        ioError = IoError::Unknown;
        CPPUNIT_ASSERT(IoHelper::checkIfIsHiddenFile(path, isHidden, ioError));
        CPPUNIT_ASSERT(isHidden);
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);
    }
#endif

    // A non-existing file is not hidden if its name doesn't start with a dot
    {
        const SyncPath path = _localTestDirPath / "non-existing.jpg"; // This file does not exist.
        bool isHidden = true;
        IoError ioError = IoError::Unknown;

        CPPUNIT_ASSERT(IoHelper::checkIfIsHiddenFile(path, false, isHidden, ioError));
        CPPUNIT_ASSERT(!isHidden);
#if defined(KD_LINUX)
        CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
#else
        CPPUNIT_ASSERT_EQUAL(IoError::NoSuchFileOrDirectory, ioError);
#endif
    }

#if !defined(KD_WINDOWS)
    // A non-existing file is hidden if its name starts with a dot
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath path = ".non_existing_hidden_file.txt";

        bool isHidden = false;
        IoError ioError = IoError::Unknown;
        CPPUNIT_ASSERT(IoHelper::checkIfIsHiddenFile(path, false, isHidden, ioError));
        CPPUNIT_ASSERT(isHidden);
        CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
    }
#endif

    // A non-existing file with a very long path causes an error.
    {
        const std::string pathSegment(50, 'a');
        SyncPath path = _localTestDirPath;
        for (int i = 0; i < 1000; ++i) {
            path /= pathSegment; // Eventually exceeds the max allowed path length on every file system of interest.
        }

        bool isHidden = true;
        IoError ioError = IoError::Success;
#if defined(KD_WINDOWS)
        CPPUNIT_ASSERT(IoHelper::checkIfIsHiddenFile(path, false, isHidden, ioError));
        CPPUNIT_ASSERT_EQUAL(IoError::NoSuchFileOrDirectory, ioError);
#elif defined(KD_MACOS)
        CPPUNIT_ASSERT(!IoHelper::checkIfIsHiddenFile(path, false, isHidden, ioError));
        CPPUNIT_ASSERT_EQUAL(IoError::FileNameTooLong, ioError);
#elif defined(KD_LINUX)
        CPPUNIT_ASSERT(IoHelper::checkIfIsHiddenFile(path, false, isHidden, ioError));
        CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
#endif
        CPPUNIT_ASSERT(!isHidden);
    }

#if !defined(KD_WINDOWS)
    // A non-existing file with a very long path is hidden if its name starts with a dot
    {
        std::string pathSegment(50, '.');
        SyncPath path = _localTestDirPath;
        for (int i = 0; i < 1000; ++i) {
            path /= pathSegment; // Eventually exceeds the max allowed path length on every file system of interest.
        }

        bool isHidden = true;
        IoError ioError = IoError::Success;
        CPPUNIT_ASSERT(IoHelper::checkIfIsHiddenFile(path, false, isHidden, ioError));
        CPPUNIT_ASSERT(isHidden);
        CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
    }
#endif

#if defined(KD_MACOS)
    // On MacOSX /Volumes is hidden by the Finder whereas kDrive considers it visible
    {
        bool isHidden = true;
        IoError ioError = IoError::Unknown;
        CPPUNIT_ASSERT(IoHelper::checkIfIsHiddenFile("/Volumes", false, isHidden, ioError));
        CPPUNIT_ASSERT(!isHidden);
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);

        isHidden = true;
        ioError = IoError::Unknown;
        CPPUNIT_ASSERT(IoHelper::checkIfIsHiddenFile("/Volumes/visible", isHidden, ioError));
        CPPUNIT_ASSERT(!isHidden);
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);
    }
#endif
}
} // namespace KDC

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

void TestIo::testGetPathWithCanonicalParent() {
    // Succeeds for an existing item and returns the canonical parent directory followed by the file name
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath directoryPath = temporaryDirectory.path() / "directory";
        CPPUNIT_ASSERT(std::filesystem::create_directory(directoryPath));

        const SyncPath filePath = directoryPath / "file.txt";
        { std::ofstream ofs(filePath); }

        IoError ioError = IoError::Unknown;
        SyncPath canonicalPath;
        CPPUNIT_ASSERT(IoHelper::getPathWithCanonicalParent(filePath, canonicalPath, ioError));
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);
        CPPUNIT_ASSERT(canonicalPath == std::filesystem::canonical(directoryPath) / "file.txt");
    }

    // Resolves the symlinks in the parent directory path
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath directoryPath = temporaryDirectory.path() / "directory";
        CPPUNIT_ASSERT(std::filesystem::create_directory(directoryPath));

        const SyncPath symlinkPath = temporaryDirectory.path() / "symlink_to_directory";
        std::filesystem::create_directory_symlink(directoryPath, symlinkPath);

        const SyncPath filePath = symlinkPath / "file.txt";
        { std::ofstream ofs(filePath); }

        IoError ioError = IoError::Unknown;
        SyncPath canonicalPath;
        CPPUNIT_ASSERT(IoHelper::getPathWithCanonicalParent(filePath, canonicalPath, ioError));
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);
        CPPUNIT_ASSERT(canonicalPath == std::filesystem::canonical(directoryPath) / "file.txt");
    }

    // Succeeds even if the item itself does not exist: only the parent directory is canonicalized
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath filePath = temporaryDirectory.path() / "non_existing_item.txt";

        IoError ioError = IoError::Unknown;
        SyncPath canonicalPath;
        CPPUNIT_ASSERT(IoHelper::getPathWithCanonicalParent(filePath, canonicalPath, ioError));
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);
        CPPUNIT_ASSERT(canonicalPath == std::filesystem::canonical(temporaryDirectory.path()) / "non_existing_item.txt");
    }

    // Does not resolve the final path component when it is a valid symlink
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath targetFilePath = temporaryDirectory.path() / "target_file.txt";
        { std::ofstream ofs(targetFilePath); }

        const SyncPath symlinkFilePath = temporaryDirectory.path() / "symlink_to_file.txt";
        std::filesystem::create_symlink(targetFilePath, symlinkFilePath);

        IoError ioError = IoError::Unknown;
        SyncPath canonicalPath;
        CPPUNIT_ASSERT(IoHelper::getPathWithCanonicalParent(symlinkFilePath, canonicalPath, ioError));
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);
        // The file name of the symlink is preserved: the symlink itself is not resolved, unlike std::filesystem::canonical
        CPPUNIT_ASSERT(canonicalPath == std::filesystem::canonical(temporaryDirectory.path()) / "symlink_to_file.txt");
        CPPUNIT_ASSERT(std::filesystem::is_symlink(canonicalPath));
    }

    // The file name is preserved as is (not canonicalized)
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath directoryPath = temporaryDirectory.path() / "directory";
        CPPUNIT_ASSERT(std::filesystem::create_directory(directoryPath));

        const SyncPath filePath = directoryPath / "MixedCase.TXT";
        { std::ofstream ofs(filePath); }

        const SyncPath differentlyCasedFilePath = directoryPath / "mixedcase.txt";

        IoError ioError = IoError::Unknown;
        SyncPath canonicalPath;
        CPPUNIT_ASSERT(IoHelper::getPathWithCanonicalParent(differentlyCasedFilePath, canonicalPath, ioError));
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);
        CPPUNIT_ASSERT(canonicalPath == std::filesystem::canonical(directoryPath) / "mixedcase.txt");
    }

    // Fails if the item path is very long.
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath filePath = makeVeryLonPath(temporaryDirectory.path());

        IoError ioError = IoError::Success;
        SyncPath canonicalPath;
        CPPUNIT_ASSERT(!IoHelper::getPathWithCanonicalParent(filePath, canonicalPath, ioError));
#if defined(KD_WINDOWS)
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::NoSuchFileOrDirectory),
                                     IoError::NoSuchFileOrDirectory, ioError);
#else
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::FileNameTooLong), IoError::FileNameTooLong,
                                     ioError);
#endif
        CPPUNIT_ASSERT(canonicalPath.empty());
    }

    // Fails if the parent directory does not exist
    {
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath filePath = temporaryDirectory.path() / "non_existing_directory/file.txt";

        IoError ioError = IoError::Success;
        SyncPath canonicalPath;
        CPPUNIT_ASSERT(!IoHelper::getPathWithCanonicalParent(filePath, canonicalPath, ioError));
        CPPUNIT_ASSERT_EQUAL(IoError::NoSuchFileOrDirectory, ioError);
        CPPUNIT_ASSERT(canonicalPath.empty());
    }
}

} // namespace KDC

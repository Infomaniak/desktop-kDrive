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
#include "config.h"
#include "io/filestat.h"
#include "test_utility/testhelpers.h"

#include <filesystem>

using namespace CppUnit;

namespace KDC {

IoHelperTests::IoHelperTests() :
    IoHelper() {}

void IoHelperTests::setIsDirectoryFunction(std::function<bool(const SyncPath &path, std::error_code &ec)> f) {
    _isDirectory = f;
};
void IoHelperTests::setIsSymlinkFunction(std::function<bool(const SyncPath &path, std::error_code &ec)> f) {
    _isSymlink = f;
};
void IoHelperTests::setReadSymlinkFunction(std::function<SyncPath(const SyncPath &path, std::error_code &ec)> f) {
    _readSymlink = f;
};
void IoHelperTests::setFileSizeFunction(std::function<std::uintmax_t(const SyncPath &path, std::error_code &ec)> f) {
    _fileSize = f;
}

void IoHelperTests::setTempDirectoryPathFunction(std::function<SyncPath(std::error_code &ec)> f) {
    _tempDirectoryPath = f;
}

#ifdef __APPLE__
void IoHelperTests::setReadAliasFunction(std::function<bool(const SyncPath &path, SyncPath &targetPath, IoError &ioError)> f) {
    _readAlias = f;
};
#endif

void IoHelperTests::resetFunctions() {
    // Reset to default std::filesytem implementation.
    setIsDirectoryFunction(static_cast<bool (*)(const SyncPath &path, std::error_code &ec)>(&std::filesystem::is_directory));
    setIsSymlinkFunction(static_cast<bool (*)(const SyncPath &path, std::error_code &ec)>(&std::filesystem::is_symlink));
    setReadSymlinkFunction(static_cast<SyncPath (*)(const SyncPath &path, std::error_code &ec)>(&std::filesystem::read_symlink));
    setFileSizeFunction(static_cast<std::uintmax_t (*)(const SyncPath &path, std::error_code &ec)>(&std::filesystem::file_size));
    setTempDirectoryPathFunction(static_cast<SyncPath (*)(std::error_code &ec)>(&std::filesystem::temp_directory_path));

#ifdef __APPLE__
    // Default Utility::readAlias implementation
    setReadAliasFunction([](const SyncPath &path, SyncPath &targetPath, IoError &ioError) -> bool {
        std::string data;
        return readAlias(path, data, targetPath, ioError);
    });
#endif
}

SyncPath makeVeryLonPath(const SyncPath &rootPath) {
    const std::string pathSegment(50, 'a');
    SyncPath path = rootPath;
    for (int i = 0; i < 1000; ++i) {
        path /= pathSegment; // Eventually exceeds the max allowed path length on every file system of interest.
    }

    return path;
}

SyncPath makeFileNameWithEmojis() {
    return u8"🫃😋🌲👣🍔🕉️⛎";
}

TestIo::TestIo() :
    CppUnit::TestFixture(),
    _localTestDirPath(std::wstring(L"" TEST_DIR) + L"/test_ci") {}

void TestIo::setUp() {
    TestBase::start();
    _testObj = new IoHelperTests();
}

void TestIo::tearDown() {
    delete _testObj;
    TestBase::stop();
}

void TestIo::testTempDirectoryPath() {
    {
        SyncPath tmpPath;
        IoError ioError = IoError::Success;
        CPPUNIT_ASSERT(_testObj->tempDirectoryPath(tmpPath, ioError));
        CPPUNIT_ASSERT(!tmpPath.empty());
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);
    }

    {
        SyncPath tmpPath;
        IoError ioError = IoError::Success;

        _testObj->setTempDirectoryPathFunction([](std::error_code &ec) -> SyncPath {
            ec = std::make_error_code(std::errc::not_enough_memory);
            return SyncPath{};
        });

        CPPUNIT_ASSERT(!_testObj->tempDirectoryPath(tmpPath, ioError));
        CPPUNIT_ASSERT(tmpPath.empty());
        CPPUNIT_ASSERT(ioError == IoError::Unknown);

        _testObj->resetFunctions();
    }
}

void TestIo::testLogDirectoryPath() {
    {
        SyncPath logDirPath;
        IoError ioError = IoError::Success;
        CPPUNIT_ASSERT(_testObj->logDirectoryPath(logDirPath, ioError));
        CPPUNIT_ASSERT(!logDirPath.empty());
        CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);
    }

    {
        SyncPath logDirPath;
        IoError ioError = IoError::Success;

        _testObj->setTempDirectoryPathFunction([](std::error_code &ec) -> SyncPath {
            ec = std::make_error_code(std::errc::not_enough_memory);
            return SyncPath{};
        });
        /* As IoHelper::logDirectoryPath() use the path returned by Log::instance()->getLogFilePath().parent_path() if
         * Log::_instance is not null, we need to reset Log::_instance to ensure that we are in the default state where the
         * function generates the log directory path from the temp directory path.
         */
        auto logInstance = Log::_instance;
        Log::_instance.reset();
        CPPUNIT_ASSERT(!_testObj->logDirectoryPath(logDirPath, ioError));
        Log::_instance = logInstance;
        CPPUNIT_ASSERT(logDirPath.empty());
        CPPUNIT_ASSERT(ioError == IoError::Unknown);

        _testObj->resetFunctions();
    }
}

void TestIo::testAccesDeniedOnLockedFiles() {
#ifdef _WIN32 // This test is only relevant on Windows, as on Unix systems, there is no standard way to lock files.
    LocalTemporaryDirectory tmpDir("TestIo-testAccesDeniedOnLockedFiles");
    const SyncPath lockedFile = tmpDir.path() / "lockedFile.txt";
    std::ofstream file(lockedFile);
    CPPUNIT_ASSERT(file.is_open());
    file.close();

    // Lock the file
    auto hFile = CreateFile(lockedFile.c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    CPPUNIT_ASSERT(hFile != INVALID_HANDLE_VALUE);

    // Try to delete the file
    std::error_code ec;
    std::filesystem::remove_all(lockedFile, ec);
    const IoError ioError = IoHelper::stdError2ioError(ec);

    // Unlock the file
    CloseHandle(hFile);
    CPPUNIT_ASSERT_EQUAL(IoError::AccessDenied, ioError);
#endif
}

void TestIo::testSetFileDates() {
    SyncPath filepath;
    const auto timestamp = testhelpers::defaultTime;

    {
        const LocalTemporaryDirectory tempDir("testSetFileDates");
        filepath = tempDir.path() / "test.txt";
        testhelpers::generateOrEditTestFile(filepath);

        // Test on a normal file.
        auto ioError = IoHelper::setFileDates(filepath, timestamp, timestamp, false);
        CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);

        FileStat filestat;
        (void) IoHelper::getFileStat(filepath, &filestat, ioError);
#if defined(__APPLE__) || defined(_WIN32) //  Creation date not set on Linux
        CPPUNIT_ASSERT_EQUAL(timestamp, filestat.creationTime);
#endif
        CPPUNIT_ASSERT_EQUAL(timestamp, filestat.modtime);

        // Test on a normal folder.
        const SyncPath folderPath = tempDir.path() / "test_dir";
        std::error_code ec;
        (void) std::filesystem::create_directory(folderPath, ec);
        ioError = IoHelper::setFileDates(folderPath, timestamp, timestamp, false);
        CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);

        (void) IoHelper::getFileStat(folderPath, &filestat, ioError);
#if defined(__APPLE__) || defined(_WIN32) //  Creation date not set on Linux
        CPPUNIT_ASSERT_EQUAL(timestamp, filestat.creationTime);
#endif
        CPPUNIT_ASSERT_EQUAL(timestamp, filestat.modtime);

        // Test on a file without access right.
#ifdef _WIN32
        // On Windows, we can edit a file even if we do not have access to its parent.
        const auto &rightPath = filepath;
#else
        const auto &rightPath = tempDir.path();
#endif
        bool result = IoHelper::setRights(rightPath, false, false, false, ioError);
        result &= ioError == IoError::Success;
        if (!result) {
            (void) IoHelper::setRights(rightPath, true, true, true, ioError);
            CPPUNIT_ASSERT_MESSAGE("setRights failed", false);
        }
        ioError = IoHelper::setFileDates(filepath, timestamp, timestamp + 10, false);
        CPPUNIT_ASSERT_EQUAL(IoError::AccessDenied, ioError);

        (void) IoHelper::getFileStat(filepath, &filestat, ioError);
        CPPUNIT_ASSERT_EQUAL(timestamp, filestat.modtime);

        (void) IoHelper::setRights(rightPath, true, true, true, ioError);

#if defined(__APPLE__) || defined(_WIN32) // Disable for now the test on symlink on Linux
        // Test on a symlink on a file.
        auto linkTimestamp = timestamp + 10;
        SyncPath linkPath = tempDir.path() / "test_link_file";
        (void) IoHelper::createSymlink(filepath, linkPath, false, ioError);
        ioError = IoHelper::setFileDates(linkPath, linkTimestamp, linkTimestamp, true);
        CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);

        (void) IoHelper::getFileStat(linkPath, &filestat, ioError);
        CPPUNIT_ASSERT_EQUAL(linkTimestamp, filestat.modtime);
        (void) IoHelper::getFileStat(filepath, &filestat, ioError);
        CPPUNIT_ASSERT_EQUAL(timestamp, filestat.modtime);

        // Test on a symlink on a folder.
        linkPath = tempDir.path() / "test_link_folder";
        (void) IoHelper::createSymlink(folderPath, linkPath, true, ioError);
        ioError = IoHelper::setFileDates(linkPath, linkTimestamp, linkTimestamp, true);
        CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);

        (void) IoHelper::getFileStat(linkPath, &filestat, ioError);
        CPPUNIT_ASSERT_EQUAL(linkTimestamp, filestat.modtime);
        (void) IoHelper::getFileStat(folderPath, &filestat, ioError);
        CPPUNIT_ASSERT_EQUAL(timestamp, filestat.modtime);
#endif

#if defined(__APPLE__)
        // Test on an alias on a file.
        linkPath = tempDir.path() / "test_alias_file";

        (void) IoHelper::createAliasFromPath(filepath, linkPath, ioError);

        ioError = IoHelper::setFileDates(linkPath, linkTimestamp, linkTimestamp, true);
        CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);

        (void) IoHelper::getFileStat(linkPath, &filestat, ioError);
        CPPUNIT_ASSERT_EQUAL(linkTimestamp, filestat.modtime);
        (void) IoHelper::getFileStat(filepath, &filestat, ioError);
        CPPUNIT_ASSERT_EQUAL(timestamp, filestat.modtime);

        // Test on an alias on a folder.
        linkPath = tempDir.path() / "test_alias_folder";

        (void) IoHelper::createAliasFromPath(filepath, linkPath, ioError);

        ioError = IoHelper::setFileDates(linkPath, linkTimestamp, linkTimestamp, true);
        CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);

        (void) IoHelper::getFileStat(linkPath, &filestat, ioError);
        CPPUNIT_ASSERT_EQUAL(linkTimestamp, filestat.modtime);
        (void) IoHelper::getFileStat(filepath, &filestat, ioError);
        CPPUNIT_ASSERT_EQUAL(timestamp, filestat.modtime);
#endif

#ifdef _WIN32
        // Test on an alias on a file.
        linkPath = tempDir.path() / "test_junction_file";

        (void) IoHelper::createJunctionFromPath(filepath, linkPath, ioError);

        ioError = IoHelper::setFileDates(linkPath, linkTimestamp, linkTimestamp, true);
        CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);

        (void) IoHelper::getFileStat(linkPath, &filestat, ioError);
        CPPUNIT_ASSERT_EQUAL(linkTimestamp, filestat.modtime);
        (void) IoHelper::getFileStat(filepath, &filestat, ioError);
        CPPUNIT_ASSERT_EQUAL(timestamp, filestat.modtime);

        // Test on an alias on a folder.
        linkPath = tempDir.path() / "test_junction_folder";

        (void) IoHelper::createJunctionFromPath(filepath, linkPath, ioError);

        ioError = IoHelper::setFileDates(linkPath, linkTimestamp, linkTimestamp, true);
        CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);

        (void) IoHelper::getFileStat(linkPath, &filestat, ioError);
        CPPUNIT_ASSERT_EQUAL(linkTimestamp, filestat.modtime);
        (void) IoHelper::getFileStat(filepath, &filestat, ioError);
        CPPUNIT_ASSERT_EQUAL(timestamp, filestat.modtime);
#endif

        // Test with creation date > modification date.
        ioError = IoHelper::setFileDates(filepath, timestamp + 10, timestamp, false);
        CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);

        (void) IoHelper::getFileStat(filepath, &filestat, ioError);
#if defined(__APPLE__)
        // On macOS, if creation date > modification date, creation date is set to modification date.
        CPPUNIT_ASSERT_EQUAL(timestamp, filestat.creationTime);
#endif
        CPPUNIT_ASSERT_EQUAL(timestamp, filestat.modtime);
    }

    // Test on a non-existing file.
    const auto ioError = IoHelper::setFileDates(filepath, timestamp, timestamp, false);
    CPPUNIT_ASSERT_EQUAL(IoError::NoSuchFileOrDirectory, ioError);
}

} // namespace KDC

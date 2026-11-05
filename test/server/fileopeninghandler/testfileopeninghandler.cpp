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

#include "testfileopeninghandler.h"

#include "fileopeninghandler.h"
#include "libsyncengine/jobs/local/openfilejob.h"
#include "test_utility/localtemporarydirectory.h"

#include <Poco/File.h>

#include <fstream>

namespace KDC {

void TestFileOpeningHandler::setUp() {
    TestBase::start();
}

void TestFileOpeningHandler::tearDown() {
    TestBase::stop();
}

void TestFileOpeningHandler::testIsOpenFileUrl() {
    CPPUNIT_ASSERT(FileOpeningHandler::isFileOpeningDeeplink("kdrive://open/user/99/drive/1234/file/456"));
    CPPUNIT_ASSERT(
            FileOpeningHandler::isFileOpeningDeeplink("KDRIVE://OPEN/user/99/drive/1234/file/456")); // Scheme and
                                                                                                     // host are case-insensitive.

    CPPUNIT_ASSERT(!FileOpeningHandler::isFileOpeningDeeplink(""));
    CPPUNIT_ASSERT(!FileOpeningHandler::isFileOpeningDeeplink("kdrive://auth-desktop?code=1&state=2")); // OAuth redirection.
    CPPUNIT_ASSERT(!FileOpeningHandler::isFileOpeningDeeplink("kdrive://other/user/99/drive/1234/file/456"));
    CPPUNIT_ASSERT(!FileOpeningHandler::isFileOpeningDeeplink("https://open/user/99/drive/1234/file/456"));
    CPPUNIT_ASSERT(!FileOpeningHandler::isFileOpeningDeeplink("user/99/drive/1234/file/456"));
    CPPUNIT_ASSERT(!FileOpeningHandler::isFileOpeningDeeplink("kdrive://open/user/99/drive/1234/file/%")); // Invalid
                                                                                                           // percent-encoding.
}

void TestFileOpeningHandler::testParseUrl() {
    {
        OpenFileJob::Request request;
        CPPUNIT_ASSERT(OpenFileJob::parseDeeplinkUrl("kdrive://open/user/99/drive/1234/file/456", request));
        CPPUNIT_ASSERT_EQUAL(UserId{99}, request.user.userId());
        CPPUNIT_ASSERT_EQUAL(DriveId{1234}, request.driveId);
        CPPUNIT_ASSERT_EQUAL(std::string{"456"}, request.remoteNodeId);
    }

    {
        // Query items are ignored.
        OpenFileJob::Request request;
        CPPUNIT_ASSERT(OpenFileJob::parseDeeplinkUrl("kdrive://open/user/99/drive/1234/file/456?foo=bar", request));
        CPPUNIT_ASSERT_EQUAL(UserId{99}, request.user.userId());
        CPPUNIT_ASSERT_EQUAL(DriveId{1234}, request.driveId);
        CPPUNIT_ASSERT_EQUAL(std::string{"456"}, request.remoteNodeId);
    }

    {
        // Trailing slash.
        OpenFileJob::Request request;
        CPPUNIT_ASSERT(OpenFileJob::parseDeeplinkUrl("kdrive://open/user/99/drive/1234/file/456/", request));
        CPPUNIT_ASSERT_EQUAL(UserId{99}, request.user.userId());
        CPPUNIT_ASSERT_EQUAL(DriveId{1234}, request.driveId);
        CPPUNIT_ASSERT_EQUAL(std::string{"456"}, request.remoteNodeId);
    }

    {
        // Wrong number of path segments.
        OpenFileJob::Request request;
        CPPUNIT_ASSERT(!OpenFileJob::parseDeeplinkUrl("kdrive://open", request));
        CPPUNIT_ASSERT(!OpenFileJob::parseDeeplinkUrl("kdrive://open/user", request));
        CPPUNIT_ASSERT(!OpenFileJob::parseDeeplinkUrl("kdrive://open/user/99", request));
        CPPUNIT_ASSERT(!OpenFileJob::parseDeeplinkUrl("kdrive://open/user/99/drive", request));
        CPPUNIT_ASSERT(!OpenFileJob::parseDeeplinkUrl("kdrive://open/user/99/drive/1234", request));
        CPPUNIT_ASSERT(!OpenFileJob::parseDeeplinkUrl("kdrive://open/user/99/drive/1234/file", request));
        CPPUNIT_ASSERT(!OpenFileJob::parseDeeplinkUrl("kdrive://open/user/99/drive/1234/file/456/extra", request));
        CPPUNIT_ASSERT(!OpenFileJob::parseDeeplinkUrl("kdrive://open/drive/1234/file/456",
                                                      request)); // Legacy URL
                                                                 // format without the user id.
    }

    {
        // Unsupported resource kind.
        OpenFileJob::Request request;
        CPPUNIT_ASSERT(!OpenFileJob::parseDeeplinkUrl("kdrive://open/folder/99/drive/1234/file/456", request));
        CPPUNIT_ASSERT(!OpenFileJob::parseDeeplinkUrl("kdrive://open/user/99/folder/1234/file/456", request));
        CPPUNIT_ASSERT(!OpenFileJob::parseDeeplinkUrl("kdrive://open/user/99/drive/1234/folder/456", request));
    }

    {
        // Invalid user ids.
        OpenFileJob::Request request;
        CPPUNIT_ASSERT(!OpenFileJob::parseDeeplinkUrl("kdrive://open/user/abc/drive/1234/file/456", request));
        CPPUNIT_ASSERT(!OpenFileJob::parseDeeplinkUrl("kdrive://open/user/-1/drive/1234/file/456", request));
        CPPUNIT_ASSERT(!OpenFileJob::parseDeeplinkUrl("kdrive://open/user/0/drive/1234/file/456", request));
        CPPUNIT_ASSERT(!OpenFileJob::parseDeeplinkUrl("kdrive://open/user//drive/1234/file/456", request));
    }

    {
        // Invalid drive ids.
        OpenFileJob::Request request;
        CPPUNIT_ASSERT(!OpenFileJob::parseDeeplinkUrl("kdrive://open/user/99/drive/abc/file/456", request));
        CPPUNIT_ASSERT(!OpenFileJob::parseDeeplinkUrl("kdrive://open/user/99/drive/-1/file/456", request));
        CPPUNIT_ASSERT(!OpenFileJob::parseDeeplinkUrl("kdrive://open/user/99/drive/0/file/456", request));
        CPPUNIT_ASSERT(!OpenFileJob::parseDeeplinkUrl("kdrive://open/user/99/drive//file/456", request));
    }

    {
        // Invalid node ids.
        OpenFileJob::Request request;
        CPPUNIT_ASSERT(!OpenFileJob::parseDeeplinkUrl("kdrive://open/user/99/drive/1234/file", request));
        CPPUNIT_ASSERT(!OpenFileJob::parseDeeplinkUrl("kdrive://open/user/99/drive/1234/file/", request));
        CPPUNIT_ASSERT(!OpenFileJob::parseDeeplinkUrl("kdrive://open/user/99/drive/1234/file/%", request)); // Invalid
                                                                                                            // percent-encoding.
    }

    {
        // Not a file opening URL.
        OpenFileJob::Request request;
        CPPUNIT_ASSERT(!OpenFileJob::parseDeeplinkUrl("kdrive://auth-desktop?code=1&state=2", request));
    }
}

void TestFileOpeningHandler::testShouldOpenParentFolder() {
    // shouldOpenParentFolder() inspects the item on disk: the asserted paths must exist for its on-disk checks to
    // run at all. Create real, non-executable files and assert against them.
    const LocalTemporaryDirectory tmpDir("TestFileOpeningHandler");
    const auto createEmptyFile = [](const SyncPath &path) { std::ofstream ofs(path); };

    // Files with an executable extension open the parent folder, whether or not they are executable.
    const SyncPath setupExePath = tmpDir.path() / "setup.exe";
    const SyncPath scriptShPath = tmpDir.path() / "script.sh";
    const SyncPath setupExeUpperCasePath = tmpDir.path() / "Setup.EXE"; // Extension check is case-insensitive.
    createEmptyFile(setupExePath);
    createEmptyFile(scriptShPath);
    createEmptyFile(setupExeUpperCasePath);

    CPPUNIT_ASSERT(FileOpeningHandler::shouldOpenParentFolder(setupExePath));
    CPPUNIT_ASSERT(FileOpeningHandler::shouldOpenParentFolder(scriptShPath));
    CPPUNIT_ASSERT(FileOpeningHandler::shouldOpenParentFolder(setupExeUpperCasePath));

    // Non-executable files without an executable extension do not open the parent folder.
    const SyncPath reportPath = tmpDir.path() / "report.docx";
    const SyncPath photoPath = tmpDir.path() / "photo.jpg";
    const SyncPath noExtensionPath = tmpDir.path() / "no_extension";
    createEmptyFile(reportPath);
    createEmptyFile(photoPath);
    createEmptyFile(noExtensionPath);

    CPPUNIT_ASSERT(!FileOpeningHandler::shouldOpenParentFolder(reportPath));
    CPPUNIT_ASSERT(!FileOpeningHandler::shouldOpenParentFolder(photoPath));
    CPPUNIT_ASSERT(!FileOpeningHandler::shouldOpenParentFolder(noExtensionPath));

#if defined(KD_MACOS) || defined(KD_LINUX)
    // An executable permission opens the parent folder whatever the extension is. Poco detects executables through
    // the file permissions on POSIX only: on Windows, it relies on the extension.
    const SyncPath executablePath = tmpDir.path() / "runnable.bin";
    createEmptyFile(executablePath);
    Poco::File file(executablePath.native());
    file.setExecutable();
    CPPUNIT_ASSERT(file.canExecute());

    CPPUNIT_ASSERT(FileOpeningHandler::shouldOpenParentFolder(executablePath));
#endif
}

} // namespace KDC

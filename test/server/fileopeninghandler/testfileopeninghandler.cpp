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
        FileOpeningHandler::Request request;
        CPPUNIT_ASSERT(FileOpeningHandler::parseDeeplinkUrl("kdrive://open/user/99/drive/1234/file/456", request));
        CPPUNIT_ASSERT_EQUAL(UserId(99), request.user.userId());
        CPPUNIT_ASSERT_EQUAL(DriveId(1234), request.driveId);
        CPPUNIT_ASSERT_EQUAL(std::string("456"), request.remoteNodeId);
    }

    {
        // Query items are ignored.
        FileOpeningHandler::Request request;
        CPPUNIT_ASSERT(FileOpeningHandler::parseDeeplinkUrl("kdrive://open/user/99/drive/1234/file/456?foo=bar", request));
        CPPUNIT_ASSERT_EQUAL(UserId(99), request.user.userId());
        CPPUNIT_ASSERT_EQUAL(DriveId(1234), request.driveId);
        CPPUNIT_ASSERT_EQUAL(std::string("456"), request.remoteNodeId);
    }

    {
        // Trailing slash.
        FileOpeningHandler::Request request;
        CPPUNIT_ASSERT(FileOpeningHandler::parseDeeplinkUrl("kdrive://open/user/99/drive/1234/file/456/", request));
        CPPUNIT_ASSERT_EQUAL(UserId(99), request.user.userId());
        CPPUNIT_ASSERT_EQUAL(DriveId(1234), request.driveId);
        CPPUNIT_ASSERT_EQUAL(std::string("456"), request.remoteNodeId);
    }

    {
        // Wrong number of path segments.
        FileOpeningHandler::Request request;
        CPPUNIT_ASSERT(!FileOpeningHandler::parseDeeplinkUrl("kdrive://open", request));
        CPPUNIT_ASSERT(!FileOpeningHandler::parseDeeplinkUrl("kdrive://open/user", request));
        CPPUNIT_ASSERT(!FileOpeningHandler::parseDeeplinkUrl("kdrive://open/user/99", request));
        CPPUNIT_ASSERT(!FileOpeningHandler::parseDeeplinkUrl("kdrive://open/user/99/drive", request));
        CPPUNIT_ASSERT(!FileOpeningHandler::parseDeeplinkUrl("kdrive://open/user/99/drive/1234", request));
        CPPUNIT_ASSERT(!FileOpeningHandler::parseDeeplinkUrl("kdrive://open/user/99/drive/1234/file", request));
        CPPUNIT_ASSERT(!FileOpeningHandler::parseDeeplinkUrl("kdrive://open/user/99/drive/1234/file/456/extra", request));
        CPPUNIT_ASSERT(!FileOpeningHandler::parseDeeplinkUrl("kdrive://open/drive/1234/file/456",
                                                             request)); // Legacy URL
                                                                        // format without the user id.
    }

    {
        // Unsupported resource kind.
        FileOpeningHandler::Request request;
        CPPUNIT_ASSERT(!FileOpeningHandler::parseDeeplinkUrl("kdrive://open/folder/99/drive/1234/file/456", request));
        CPPUNIT_ASSERT(!FileOpeningHandler::parseDeeplinkUrl("kdrive://open/user/99/folder/1234/file/456", request));
        CPPUNIT_ASSERT(!FileOpeningHandler::parseDeeplinkUrl("kdrive://open/user/99/drive/1234/folder/456", request));
    }

    {
        // Invalid user ids.
        FileOpeningHandler::Request request;
        CPPUNIT_ASSERT(!FileOpeningHandler::parseDeeplinkUrl("kdrive://open/user/abc/drive/1234/file/456", request));
        CPPUNIT_ASSERT(!FileOpeningHandler::parseDeeplinkUrl("kdrive://open/user/-1/drive/1234/file/456", request));
        CPPUNIT_ASSERT(!FileOpeningHandler::parseDeeplinkUrl("kdrive://open/user/0/drive/1234/file/456", request));
        CPPUNIT_ASSERT(!FileOpeningHandler::parseDeeplinkUrl("kdrive://open/user//drive/1234/file/456", request));
    }

    {
        // Invalid drive ids.
        FileOpeningHandler::Request request;
        CPPUNIT_ASSERT(!FileOpeningHandler::parseDeeplinkUrl("kdrive://open/user/99/drive/abc/file/456", request));
        CPPUNIT_ASSERT(!FileOpeningHandler::parseDeeplinkUrl("kdrive://open/user/99/drive/-1/file/456", request));
        CPPUNIT_ASSERT(!FileOpeningHandler::parseDeeplinkUrl("kdrive://open/user/99/drive/0/file/456", request));
        CPPUNIT_ASSERT(!FileOpeningHandler::parseDeeplinkUrl("kdrive://open/user/99/drive//file/456", request));
    }

    {
        // Invalid node ids.
        FileOpeningHandler::Request request;
        CPPUNIT_ASSERT(!FileOpeningHandler::parseDeeplinkUrl("kdrive://open/user/99/drive/1234/file", request));
        CPPUNIT_ASSERT(!FileOpeningHandler::parseDeeplinkUrl("kdrive://open/user/99/drive/1234/file/", request));
        CPPUNIT_ASSERT(
                !FileOpeningHandler::parseDeeplinkUrl("kdrive://open/user/99/drive/1234/file/%", request)); // Invalid
                                                                                                            // percent-encoding.
    }

    {
        // Not a file opening URL.
        FileOpeningHandler::Request request;
        CPPUNIT_ASSERT(!FileOpeningHandler::parseDeeplinkUrl("kdrive://auth-desktop?code=1&state=2", request));
    }
}

void TestFileOpeningHandler::testShouldOpenParentFolder() {
    CPPUNIT_ASSERT(FileOpeningHandler::shouldOpenParentFolder(SyncPath("tools/setup.exe")));
    CPPUNIT_ASSERT(FileOpeningHandler::shouldOpenParentFolder(SyncPath("script.sh")));
    CPPUNIT_ASSERT(FileOpeningHandler::shouldOpenParentFolder(SyncPath("Setup.EXE"))); // Extension check is case-insensitive.

    CPPUNIT_ASSERT(!FileOpeningHandler::shouldOpenParentFolder(SyncPath("Documents/report.docx")));
    CPPUNIT_ASSERT(!FileOpeningHandler::shouldOpenParentFolder(SyncPath("photo.jpg")));
    CPPUNIT_ASSERT(!FileOpeningHandler::shouldOpenParentFolder(SyncPath("no_extension")));
}

} // namespace KDC

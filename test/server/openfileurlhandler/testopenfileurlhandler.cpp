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

#include "testopenfileurlhandler.h"

#include "openfileurlhandler.h"

namespace KDC {

void TestOpenFileUrlHandler::setUp() {
    TestBase::start();
}

void TestOpenFileUrlHandler::tearDown() {
    TestBase::stop();
}

void TestOpenFileUrlHandler::testIsOpenFileUrl() {
    CPPUNIT_ASSERT(OpenFileUrlHandler::isOpenFileUrl("kdrive://open/drive/1234/file/456"));
    CPPUNIT_ASSERT(OpenFileUrlHandler::isOpenFileUrl("KDRIVE://OPEN/drive/1234/file/456")); // Scheme and host are
                                                                                            // case-insensitive.

    CPPUNIT_ASSERT(!OpenFileUrlHandler::isOpenFileUrl(""));
    CPPUNIT_ASSERT(!OpenFileUrlHandler::isOpenFileUrl("kdrive://auth-desktop?code=1&state=2")); // OAuth redirection.
    CPPUNIT_ASSERT(!OpenFileUrlHandler::isOpenFileUrl("kdrive://other/drive/1234/file/456"));
    CPPUNIT_ASSERT(!OpenFileUrlHandler::isOpenFileUrl("https://open/drive/1234/file/456"));
    CPPUNIT_ASSERT(!OpenFileUrlHandler::isOpenFileUrl("drive/1234/file/456"));
    CPPUNIT_ASSERT(!OpenFileUrlHandler::isOpenFileUrl("kdrive://open/drive/1234/file/%")); // Invalid percent-encoding.
}

void TestOpenFileUrlHandler::testParseUrl() {
    {
        OpenFileUrlHandler::Request request;
        CPPUNIT_ASSERT(OpenFileUrlHandler::parseUrl("kdrive://open/drive/1234/file/456", request));
        CPPUNIT_ASSERT_EQUAL(DriveId(1234), request.driveId);
        CPPUNIT_ASSERT_EQUAL(std::string("456"), request.nodeId);
    }

    {
        // Query items are ignored.
        OpenFileUrlHandler::Request request;
        CPPUNIT_ASSERT(OpenFileUrlHandler::parseUrl("kdrive://open/drive/1234/file/456?foo=bar", request));
        CPPUNIT_ASSERT_EQUAL(DriveId(1234), request.driveId);
        CPPUNIT_ASSERT_EQUAL(std::string("456"), request.nodeId);
    }

    {
        // Trailing slash.
        OpenFileUrlHandler::Request request;
        CPPUNIT_ASSERT(OpenFileUrlHandler::parseUrl("kdrive://open/drive/1234/file/456/", request));
        CPPUNIT_ASSERT_EQUAL(std::string("456"), request.nodeId);
    }

    {
        // Wrong number of path segments.
        OpenFileUrlHandler::Request request;
        CPPUNIT_ASSERT(!OpenFileUrlHandler::parseUrl("kdrive://open", request));
        CPPUNIT_ASSERT(!OpenFileUrlHandler::parseUrl("kdrive://open/drive", request));
        CPPUNIT_ASSERT(!OpenFileUrlHandler::parseUrl("kdrive://open/drive/1234", request));
        CPPUNIT_ASSERT(!OpenFileUrlHandler::parseUrl("kdrive://open/drive/1234/file", request));
        CPPUNIT_ASSERT(!OpenFileUrlHandler::parseUrl("kdrive://open/drive/1234/file/456/extra", request));
    }

    {
        // Unsupported resource kind.
        OpenFileUrlHandler::Request request;
        CPPUNIT_ASSERT(!OpenFileUrlHandler::parseUrl("kdrive://open/folder/1234/file/456", request));
        CPPUNIT_ASSERT(!OpenFileUrlHandler::parseUrl("kdrive://open/drive/1234/folder/456", request));
    }

    {
        // Invalid drive ids.
        OpenFileUrlHandler::Request request;
        CPPUNIT_ASSERT(!OpenFileUrlHandler::parseUrl("kdrive://open/drive/abc/file/456", request));
        CPPUNIT_ASSERT(!OpenFileUrlHandler::parseUrl("kdrive://open/drive/-1/file/456", request));
        CPPUNIT_ASSERT(!OpenFileUrlHandler::parseUrl("kdrive://open/drive/0/file/456", request));
        CPPUNIT_ASSERT(!OpenFileUrlHandler::parseUrl("kdrive://open/drive//file/456", request));
    }

    {
        // Invalid node ids.
        OpenFileUrlHandler::Request request;
        CPPUNIT_ASSERT(!OpenFileUrlHandler::parseUrl("kdrive://open/drive/1234/file", request));
        CPPUNIT_ASSERT(!OpenFileUrlHandler::parseUrl("kdrive://open/drive/1234/file/", request));
        CPPUNIT_ASSERT(!OpenFileUrlHandler::parseUrl("kdrive://open/drive/1234/file/%", request)); // Invalid percent-encoding.
    }

    {
        // Not a file opening URL.
        OpenFileUrlHandler::Request request;
        CPPUNIT_ASSERT(!OpenFileUrlHandler::parseUrl("kdrive://auth-desktop?code=1&state=2", request));
    }
}

void TestOpenFileUrlHandler::testShouldOpenParentFolder() {
    CPPUNIT_ASSERT(OpenFileUrlHandler::shouldOpenParentFolder(SyncPath("tools/setup.exe")));
    CPPUNIT_ASSERT(OpenFileUrlHandler::shouldOpenParentFolder(SyncPath("script.sh")));
    CPPUNIT_ASSERT(OpenFileUrlHandler::shouldOpenParentFolder(SyncPath("Setup.EXE"))); // Extension check is case-insensitive.

    CPPUNIT_ASSERT(!OpenFileUrlHandler::shouldOpenParentFolder(SyncPath("Documents/report.docx")));
    CPPUNIT_ASSERT(!OpenFileUrlHandler::shouldOpenParentFolder(SyncPath("photo.jpg")));
    CPPUNIT_ASSERT(!OpenFileUrlHandler::shouldOpenParentFolder(SyncPath("no_extension")));
}

} // namespace KDC

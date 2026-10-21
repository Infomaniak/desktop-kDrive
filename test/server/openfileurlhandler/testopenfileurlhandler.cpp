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

#include <QUrl>

namespace KDC {

void TestOpenFileUrlHandler::setUp() {
    TestBase::start();
}

void TestOpenFileUrlHandler::tearDown() {
    TestBase::stop();
}

void TestOpenFileUrlHandler::testIsOpenFileUrl() {
    CPPUNIT_ASSERT(OpenFileUrlHandler::isOpenFileUrl(QUrl("kdrive://open/drive/1234/file/456")));
    CPPUNIT_ASSERT(OpenFileUrlHandler::isOpenFileUrl(QUrl("KDRIVE://OPEN/drive/1234/file/456"))); // Scheme and host are
                                                                                                  // case-insensitive.

    CPPUNIT_ASSERT(!OpenFileUrlHandler::isOpenFileUrl(QUrl()));
    CPPUNIT_ASSERT(!OpenFileUrlHandler::isOpenFileUrl(QUrl("kdrive://auth-desktop?code=1&state=2"))); // OAuth redirection.
    CPPUNIT_ASSERT(!OpenFileUrlHandler::isOpenFileUrl(QUrl("kdrive://other/drive/1234/file/456")));
    CPPUNIT_ASSERT(!OpenFileUrlHandler::isOpenFileUrl(QUrl("https://open/drive/1234/file/456")));
    CPPUNIT_ASSERT(!OpenFileUrlHandler::isOpenFileUrl(QUrl("drive/1234/file/456")));
}

void TestOpenFileUrlHandler::testParseUrl() {
    {
        OpenFileUrlHandler::Request request;
        CPPUNIT_ASSERT(OpenFileUrlHandler::parseUrl(QUrl("kdrive://open/drive/1234/file/456"), request));
        CPPUNIT_ASSERT_EQUAL(DriveId(1234), request.driveId);
        CPPUNIT_ASSERT_EQUAL(std::string("456"), request.nodeId);
    }

    {
        // Query items are ignored.
        OpenFileUrlHandler::Request request;
        CPPUNIT_ASSERT(OpenFileUrlHandler::parseUrl(QUrl("kdrive://open/drive/1234/file/456?foo=bar"), request));
        CPPUNIT_ASSERT_EQUAL(DriveId(1234), request.driveId);
        CPPUNIT_ASSERT_EQUAL(std::string("456"), request.nodeId);
    }

    {
        // Trailing slash.
        OpenFileUrlHandler::Request request;
        CPPUNIT_ASSERT(OpenFileUrlHandler::parseUrl(QUrl("kdrive://open/drive/1234/file/456/"), request));
        CPPUNIT_ASSERT_EQUAL(std::string("456"), request.nodeId);
    }

    {
        // Wrong number of path segments.
        OpenFileUrlHandler::Request request;
        CPPUNIT_ASSERT(!OpenFileUrlHandler::parseUrl(QUrl("kdrive://open"), request));
        CPPUNIT_ASSERT(!OpenFileUrlHandler::parseUrl(QUrl("kdrive://open/drive"), request));
        CPPUNIT_ASSERT(!OpenFileUrlHandler::parseUrl(QUrl("kdrive://open/drive/1234"), request));
        CPPUNIT_ASSERT(!OpenFileUrlHandler::parseUrl(QUrl("kdrive://open/drive/1234/file"), request));
        CPPUNIT_ASSERT(!OpenFileUrlHandler::parseUrl(QUrl("kdrive://open/drive/1234/file/456/extra"), request));
    }

    {
        // Unsupported resource kind.
        OpenFileUrlHandler::Request request;
        CPPUNIT_ASSERT(!OpenFileUrlHandler::parseUrl(QUrl("kdrive://open/folder/1234/file/456"), request));
        CPPUNIT_ASSERT(!OpenFileUrlHandler::parseUrl(QUrl("kdrive://open/drive/1234/folder/456"), request));
    }

    {
        // Invalid drive ids.
        OpenFileUrlHandler::Request request;
        CPPUNIT_ASSERT(!OpenFileUrlHandler::parseUrl(QUrl("kdrive://open/drive/abc/file/456"), request));
        CPPUNIT_ASSERT(!OpenFileUrlHandler::parseUrl(QUrl("kdrive://open/drive/-1/file/456"), request));
        CPPUNIT_ASSERT(!OpenFileUrlHandler::parseUrl(QUrl("kdrive://open/drive/0/file/456"), request));
        CPPUNIT_ASSERT(!OpenFileUrlHandler::parseUrl(QUrl("kdrive://open/drive//file/456"), request));
    }

    {
        // Invalid node ids.
        OpenFileUrlHandler::Request request;
        CPPUNIT_ASSERT(!OpenFileUrlHandler::parseUrl(QUrl("kdrive://open/drive/1234/file"), request));
        CPPUNIT_ASSERT(!OpenFileUrlHandler::parseUrl(QUrl("kdrive://open/drive/1234/file/"), request));
    }

    {
        // Not a file opening URL.
        OpenFileUrlHandler::Request request;
        CPPUNIT_ASSERT(!OpenFileUrlHandler::parseUrl(QUrl("kdrive://auth-desktop?code=1&state=2"), request));
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

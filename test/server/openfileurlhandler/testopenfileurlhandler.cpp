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
    CPPUNIT_ASSERT(OpenFileUrlHandler::isOpenFileUrl(QUrl("kdrive://open/Documents/report.docx")));
    CPPUNIT_ASSERT(OpenFileUrlHandler::isOpenFileUrl(QUrl("kdrive://open/report.docx?driveId=123456")));
    CPPUNIT_ASSERT(OpenFileUrlHandler::isOpenFileUrl(QUrl("KDRIVE://OPEN/report.docx"))); // Scheme and host are case-insensitive.

    CPPUNIT_ASSERT(!OpenFileUrlHandler::isOpenFileUrl(QUrl()));
    CPPUNIT_ASSERT(!OpenFileUrlHandler::isOpenFileUrl(QUrl("kdrive://auth-desktop?code=1&state=2"))); // OAuth redirection.
    CPPUNIT_ASSERT(!OpenFileUrlHandler::isOpenFileUrl(QUrl("kdrive://other/report.docx")));
    CPPUNIT_ASSERT(!OpenFileUrlHandler::isOpenFileUrl(QUrl("https://open/report.docx")));
    CPPUNIT_ASSERT(!OpenFileUrlHandler::isOpenFileUrl(QUrl("report.docx")));
}

void TestOpenFileUrlHandler::testParseUrl() {
    {
        OpenFileUrlHandler::Request request;
        CPPUNIT_ASSERT(OpenFileUrlHandler::parseUrl(QUrl("kdrive://open/Documents/report.docx"), request));
        CPPUNIT_ASSERT(SyncPath("Documents/report.docx") == request.relativePath);
        CPPUNIT_ASSERT(!request.hasDriveId);
    }

    {
        // Percent-encoded characters are decoded.
        OpenFileUrlHandler::Request request;
        CPPUNIT_ASSERT(OpenFileUrlHandler::parseUrl(QUrl("kdrive://open/My%20folder/r%C3%A9sum%C3%A9.pdf"), request));
        CPPUNIT_ASSERT(SyncPath("My folder") / SyncPath(u8"résumé.pdf") == request.relativePath);
    }

    {
        OpenFileUrlHandler::Request request;
        CPPUNIT_ASSERT(OpenFileUrlHandler::parseUrl(QUrl("kdrive://open/report.docx?driveId=123456"), request));
        CPPUNIT_ASSERT(SyncPath("report.docx") == request.relativePath);
        CPPUNIT_ASSERT(request.hasDriveId);
        CPPUNIT_ASSERT_EQUAL(DriveId(123456), request.driveId);
    }

    {
        // Empty path.
        OpenFileUrlHandler::Request request;
        CPPUNIT_ASSERT(!OpenFileUrlHandler::parseUrl(QUrl("kdrive://open"), request));
        CPPUNIT_ASSERT(!OpenFileUrlHandler::parseUrl(QUrl("kdrive://open/"), request));
    }

    {
        // Directory traversal attempts.
        OpenFileUrlHandler::Request request;
        CPPUNIT_ASSERT(!OpenFileUrlHandler::parseUrl(QUrl("kdrive://open/../../etc/passwd"), request));
        CPPUNIT_ASSERT(!OpenFileUrlHandler::parseUrl(QUrl("kdrive://open/Documents/%2E%2E/secret.txt"), request));
    }

    {
        // Invalid drive ids.
        OpenFileUrlHandler::Request request;
        CPPUNIT_ASSERT(!OpenFileUrlHandler::parseUrl(QUrl("kdrive://open/report.docx?driveId=abc"), request));
        CPPUNIT_ASSERT(!OpenFileUrlHandler::parseUrl(QUrl("kdrive://open/report.docx?driveId=-1"), request));
    }

    {
        // Not a file opening URL.
        OpenFileUrlHandler::Request request;
        CPPUNIT_ASSERT(!OpenFileUrlHandler::parseUrl(QUrl("kdrive://auth-desktop?code=1&state=2"), request));
    }
}

void TestOpenFileUrlHandler::testIsRelativePathSafe() {
    CPPUNIT_ASSERT(OpenFileUrlHandler::isRelativePathSafe(SyncPath("report.docx")));
    CPPUNIT_ASSERT(OpenFileUrlHandler::isRelativePathSafe(SyncPath("Documents/report.docx")));

    CPPUNIT_ASSERT(!OpenFileUrlHandler::isRelativePathSafe(SyncPath()));
    CPPUNIT_ASSERT(!OpenFileUrlHandler::isRelativePathSafe(SyncPath("../report.docx")));
    CPPUNIT_ASSERT(!OpenFileUrlHandler::isRelativePathSafe(SyncPath("Documents/../../report.docx")));
    CPPUNIT_ASSERT(!OpenFileUrlHandler::isRelativePathSafe(SyncPath("./report.docx")));
#if defined(KD_WINDOWS)
    CPPUNIT_ASSERT(!OpenFileUrlHandler::isRelativePathSafe(SyncPath(LR"(C:\Windows\System32\cmd.exe)")));
#else
    CPPUNIT_ASSERT(!OpenFileUrlHandler::isRelativePathSafe(SyncPath("/etc/passwd")));
#endif
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

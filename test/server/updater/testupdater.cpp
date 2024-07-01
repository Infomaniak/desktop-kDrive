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

#include "testupdater.h"
#include "libsyncengine/requests/parameterscache.h"

#include "version.h"
#include "libcommonserver/log/log.h"
#include <log4cplus/loggingmacros.h>
#include <iostream>

using namespace CppUnit;

namespace KDC {

void TestUpdater::setUp() {
    _logger = Log::instance()->getLogger();
#ifdef __APPLE__
    _updater = static_cast<SparkleUpdater*>(UpdaterServer::instance());
#else
    _updater = static_cast<KDCUpdater*>(UpdaterServer::instance());
#endif
}

void TestUpdater::testUpdateInfoVersionParseString(void) {
    bool ok = false;
    QString xml =  // Well-formed XML
        "<kdriveclient>"
        "<version>1.2.3.4</version>"
        "<versionstring>1.2.3.4 Version test</versionstring>"
        "<web>test web</web>"
        "<downloadurl>https://download.storage.infomaniak.com/drive/desktopclient/kDrive-1.2.3.4.exe</downloadurl>"
        "</kdriveclient>";


    _updater->_updateInfo = UpdateInfo::parseString(xml, &ok);
    CPPUNIT_ASSERT(ok);
    CPPUNIT_ASSERT_EQUAL(std::string("1.2.3.4"), _updater->_updateInfo.version().toStdString());
    CPPUNIT_ASSERT_EQUAL(std::string("1.2.3.4 Version test"), _updater->_updateInfo.versionString().toStdString());
    CPPUNIT_ASSERT_EQUAL(std::string("test web"), _updater->_updateInfo.web().toStdString());
    CPPUNIT_ASSERT_EQUAL(std::string("https://download.storage.infomaniak.com/drive/desktopclient/kDrive-1.2.3.4.exe"),
                         _updater->_updateInfo.downloadUrl().toStdString());

    xml =  // Well-formed XML with empty values
        "<kdriveclient>"
        "<version></version>"
        "<versionstring></versionstring>"
        "<web></web>"
        "<downloadurl></downloadurl>"
        "</kdriveclient>";

    _updater->_updateInfo = UpdateInfo::parseString(xml, &ok);
    CPPUNIT_ASSERT(ok);
    CPPUNIT_ASSERT_EQUAL(std::string(""), _updater->_updateInfo.version().toStdString());
    CPPUNIT_ASSERT_EQUAL(std::string(""), _updater->_updateInfo.versionString().toStdString());
    CPPUNIT_ASSERT_EQUAL(std::string(""), _updater->_updateInfo.web().toStdString());
    CPPUNIT_ASSERT_EQUAL(std::string(""), _updater->_updateInfo.downloadUrl().toStdString());

    xml =  // Well-formed XML with missing values
        "<kdriveclient>"
        "</kdriveclient>";
    _updater->_updateInfo = UpdateInfo::parseString(xml, &ok);
    CPPUNIT_ASSERT(ok);
    CPPUNIT_ASSERT_EQUAL(std::string(""), _updater->_updateInfo.version().toStdString());
    CPPUNIT_ASSERT_EQUAL(std::string(""), _updater->_updateInfo.versionString().toStdString());
    CPPUNIT_ASSERT_EQUAL(std::string(""), _updater->_updateInfo.web().toStdString());
    CPPUNIT_ASSERT_EQUAL(std::string(""), _updater->_updateInfo.downloadUrl().toStdString());

    xml =  // Malformed XML
        "<kdriveclient>"
        "<version>"
        "<versionstring>"
        "</kdriveclient>";
    _updater->_updateInfo = UpdateInfo::parseString(xml, &ok);
    CPPUNIT_ASSERT(!ok);
    CPPUNIT_ASSERT_EQUAL(std::string(""), _updater->_updateInfo.version().toStdString());
    CPPUNIT_ASSERT_EQUAL(std::string(""), _updater->_updateInfo.versionString().toStdString());
    CPPUNIT_ASSERT_EQUAL(std::string(""), _updater->_updateInfo.web().toStdString());
    CPPUNIT_ASSERT_EQUAL(std::string(""), _updater->_updateInfo.downloadUrl().toStdString());
}

void TestUpdater::testIsKDCorSparkleUpdater(void) {
#ifdef __APPLE__
    CPPUNIT_ASSERT(_updater->isSparkleUpdater());
    CPPUNIT_ASSERT(!_updater->isKDCUpdater());
#else
    CPPUNIT_ASSERT(!_updater->isSparkleUpdater());
    CPPUNIT_ASSERT(_updater->isKDCUpdater());
#endif
}

void TestUpdater::testClientVersion(void) {
    QString version = UpdaterServer::clientVersion();
    CPPUNIT_ASSERT_EQUAL(std::string(KDRIVE_STRINGIFY(KDRIVE_VERSION_FULL)), version.toStdString());
}

void TestUpdater::testUpdateSucceeded(void) {
#ifdef  __APPLE__
    return true; // Not implemented 
#endif  //  __APPLE__

    ParametersCache::instance(true)->parameters().setUpdateTargetVersion("1");  // Target version is set to the current version
    CPPUNIT_ASSERT(_updater->updateSucceeded());

    ParametersCache::instance()->parameters().setUpdateTargetVersion("99.99.99");  // Target version is set to a newer version
    CPPUNIT_ASSERT(!_updater->updateSucceeded());
}

}  // namespace KDC

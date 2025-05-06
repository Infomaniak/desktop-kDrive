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

#include "testserverrequests.h"

#include "requests/serverrequests.h"
#include "requests/parameterscache.h"
#include "utility/types.h"
#include "libcommon/keychainmanager/keychainmanager.h"
#include "libparms/db/parmsdb.h"
#include "mocks/libcommonserver/db/mockdb.h"

#include "test_utility/remotetemporarydirectory.h"
#include "test_utility/testhelpers.h"

namespace KDC {

void TestServerRequests::setUp() {
    TestBase::start();
    const testhelpers::TestVariables testVariables;

    // Insert api token into keystore
    ApiToken apiToken;
    apiToken.setAccessToken(testVariables.apiToken);

    const std::string keychainKey("123");
    (void) KeyChainManager::instance(true);
    (void) KeyChainManager::instance()->writeToken(keychainKey, apiToken.reconstructJsonString());

    // Create parmsDb
    bool alreadyExists = false;
    const std::filesystem::path parmsDbPath = MockDb::makeDbName(alreadyExists);
    ParmsDb::instance(parmsDbPath, KDRIVE_VERSION_STRING, true, true);
    ParametersCache::instance()->parameters().setExtendedLog(true);

    // Insert user, account & drive
    const int userId(atoi(testVariables.userId.c_str()));
    const User user(1, userId, keychainKey);
    (void) ParmsDb::instance()->insertUser(user);

    const int accountId(atoi(testVariables.accountId.c_str()));
    const Account account(1, accountId, user.dbId());
    (void) ParmsDb::instance()->insertAccount(account);

    _driveDbId = 1;
    const int driveId = atoi(testVariables.driveId.c_str());
    const Drive drive(_driveDbId, driveId, account.dbId(), std::string(), 0, std::string());
    (void) ParmsDb::instance()->insertDrive(drive);
}

void TestServerRequests::tearDown() {
    ParmsDb::instance()->close();
    ParmsDb::reset();
    ParametersCache::reset();
    TestBase::stop();
}

void TestServerRequests::testFixProxyConfig() {
    ProxyConfig proxyConfig = ParametersCache::instance()->parameters().proxyConfig();
    proxyConfig.setType(ProxyType::Undefined);
    ParametersCache::instance()->parameters().setProxyConfig(proxyConfig);
    ParametersCache::instance()->save();

    CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, ServerRequests::fixProxyConfig());

    CPPUNIT_ASSERT_EQUAL(ProxyType::None, ParametersCache::instance()->parameters().proxyConfig().type());
}

void TestServerRequests::testGetPublicLink() {
    const RemoteTemporaryDirectory remoteTmpDir(_driveDbId, "1", "testGetPublicLink");

    // 1st call : sent POST request to generate the public share link
    QString url;
    CPPUNIT_ASSERT_EQUAL(ExitCode::Ok,
                         ServerRequests::getPublicLinkUrl(_driveDbId, QString::fromStdString(remoteTmpDir.id()), url));
    CPPUNIT_ASSERT(!url.isEmpty());
    // 2nd call : POST request will fail and a GET request should be sent to retreive exisiting share link
    url.clear();
    CPPUNIT_ASSERT_EQUAL(ExitCode::Ok,
                         ServerRequests::getPublicLinkUrl(_driveDbId, QString::fromStdString(remoteTmpDir.id()), url));
    CPPUNIT_ASSERT(!url.isEmpty());
}

void TestServerRequests::testSetExclusionTemplateList() {
    // Empty template list
    QList<ExclusionTemplateInfo> inputList;
    CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, ServerRequests::setExclusionTemplateList(true, inputList));
    CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, ServerRequests::getExclusionTemplateList(true, inputList));
    CPPUNIT_ASSERT(inputList.empty());

    CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, ServerRequests::setExclusionTemplateList(false, inputList));
    CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, ServerRequests::getExclusionTemplateList(false, inputList));
    CPPUNIT_ASSERT(inputList.empty());

    // The input default list contains a single template with a unique normalization (NFC = NFD)
    const QString templateWithSameNormalizations = "a";
    inputList.push_back({templateWithSameNormalizations});
    CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, ServerRequests::setExclusionTemplateList(true, inputList));
    QList<ExclusionTemplateInfo> outputList;
    CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, ServerRequests::getExclusionTemplateList(true, outputList));
    CPPUNIT_ASSERT_EQUAL(qsizetype(1), outputList.size());
    CPPUNIT_ASSERT(outputList.contains({templateWithSameNormalizations}));

    // The input default list contains a single template with a two distinct normalizations (NFC != NFD)
    const QString templateWithDifferentNormalizations = "é";
    inputList.clear();
    inputList.push_back({templateWithDifferentNormalizations});
    CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, ServerRequests::setExclusionTemplateList(true, inputList));
    CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, ServerRequests::getExclusionTemplateList(true, outputList));
    CPPUNIT_ASSERT_EQUAL(qsizetype(2), outputList.size());
    CPPUNIT_ASSERT(outputList.contains({templateWithDifferentNormalizations}));
    CPPUNIT_ASSERT(outputList.at(0).templ() != outputList.at(1).templ());

    // The input user list contains a single template with a two distinct normalizations (NFC != NFD)
    inputList.clear();
    inputList.push_back({templateWithDifferentNormalizations});
    CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, ServerRequests::setExclusionTemplateList(false, inputList));
    outputList.clear();
    CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, ServerRequests::getExclusionTemplateList(false, outputList));
    CPPUNIT_ASSERT_EQUAL(qsizetype(2), outputList.size());
    CPPUNIT_ASSERT(outputList.contains({templateWithDifferentNormalizations}));
    CPPUNIT_ASSERT(outputList.at(0).templ() != outputList.at(1).templ());
}

} // namespace KDC

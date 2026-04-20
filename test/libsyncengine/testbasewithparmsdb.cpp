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

#include "testbasewithparmsdb.h"
#include "test_utility/testhelpers.h"
#include "mocks/libcommonserver/db/mockdb.h"

#include "network/proxy.h"
#include "requests/parameterscache.h"

#include "libcommonserver/keychainmanager/keychainmanager.h"

#include "libparms/db/parmsdb.h"


namespace KDC {
void TestBaseWithParmsDb::initParmsDb() {
    const testhelpers::TestVariables testVariables;

    // Insert API token into keystore.
    _apiToken.setAccessToken(testVariables.apiToken);

    (void) KeyChainManager::instance(true);
    const std::string keychainKey("123");
    (void) KeyChainManager::instance()->writeToken(keychainKey, _apiToken.reconstructJsonString());

    // Create ParmsDb.
    (void) ParmsDb::instance(_localParmsDbTempDir.path() / MockDb::makeDbMockFileName(), KDRIVE_VERSION_STRING, true, true);
    ParametersCache::instance()->parameters().setExtendedLog(true);

    // Insert user, account & drive.
    const UserId userId(atoi(testVariables.userId.c_str()));
    _userDbId = UserDbId{1};
    User user(_userDbId, userId, keychainKey);
    (void) ParmsDb::instance()->insertUser(user);

    const AccountId accountId(atoi(testVariables.accountId.c_str()));
    Account account(AccountDbId{1}, accountId, user.dbId(), "account1");
    (void) ParmsDb::instance()->insertAccount(account);

    _driveDbId = 1;
    _driveId = atoi(testVariables.driveId.c_str());
    Drive drive(_driveDbId, _driveId, account.dbId(), std::string(), 0, std::string());
    (void) ParmsDb::instance()->insertDrive(drive);

    // Setup proxy
    Parameters parameters;
    bool found = false;
    if (ParmsDb::instance()->selectParameters(parameters, found) && found) {
        (void) Proxy::instance(parameters.proxyConfig());
    }
}
} // namespace KDC

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

#include "testparmsdb.h"
#include "libcommonserver/utility/asserts.h"
#include "libcommonserver/log/log.h"

#include <time.h>

using namespace CppUnit;

namespace KDC {

void TestParmsDb::setUp() {
    // Create a temp parmsDb
    bool alreadyExists;
    std::filesystem::path parmsDbPath = ParmsDb::makeDbName(alreadyExists, true);
    ParmsDb::instance(parmsDbPath, "3.6.1", true, true);
    ParmsDb::instance()->setAutoDelete(true);
}

void TestParmsDb::tearDown() {
    ParmsDb::reset();
}

void TestParmsDb::testParameters() {
    CPPUNIT_ASSERT(ParmsDb::instance()->exists());

    Parameters defaultParameters;
    Parameters parameters;
    bool found;
    CPPUNIT_ASSERT(ParmsDb::instance()->selectParameters(parameters, found) && found);
    CPPUNIT_ASSERT(parameters.language() == defaultParameters.language());
    CPPUNIT_ASSERT(parameters.monoIcons() == defaultParameters.monoIcons());
    CPPUNIT_ASSERT(parameters.autoStart() == defaultParameters.autoStart());
    CPPUNIT_ASSERT(parameters.moveToTrash() == defaultParameters.moveToTrash());
    CPPUNIT_ASSERT(parameters.notificationsDisabled() == defaultParameters.notificationsDisabled());
    CPPUNIT_ASSERT(parameters.useLog() == defaultParameters.useLog());
    CPPUNIT_ASSERT(parameters.logLevel() == defaultParameters.logLevel());
    CPPUNIT_ASSERT(parameters.purgeOldLogs() == defaultParameters.purgeOldLogs());
    CPPUNIT_ASSERT(parameters.syncHiddenFiles() == defaultParameters.syncHiddenFiles());
    CPPUNIT_ASSERT(parameters.proxyConfig().type() == defaultParameters.proxyConfig().type());
    CPPUNIT_ASSERT(parameters.proxyConfig().hostName() == defaultParameters.proxyConfig().hostName());
    CPPUNIT_ASSERT(parameters.proxyConfig().port() == defaultParameters.proxyConfig().port());
    CPPUNIT_ASSERT(parameters.proxyConfig().needsAuth() == defaultParameters.proxyConfig().needsAuth());
    CPPUNIT_ASSERT(parameters.proxyConfig().user() == defaultParameters.proxyConfig().user());
    CPPUNIT_ASSERT(parameters.proxyConfig().token() == defaultParameters.proxyConfig().token());
    CPPUNIT_ASSERT(parameters.useBigFolderSizeLimit() == defaultParameters.useBigFolderSizeLimit());
    CPPUNIT_ASSERT(parameters.bigFolderSizeLimit() == defaultParameters.bigFolderSizeLimit());
    CPPUNIT_ASSERT(parameters.darkTheme() == defaultParameters.darkTheme());
    CPPUNIT_ASSERT(parameters.showShortcuts() == defaultParameters.showShortcuts());
    CPPUNIT_ASSERT(parameters.dialogGeometry() == defaultParameters.dialogGeometry());

    Parameters parameters2;
    parameters2.setLanguage(LanguageFrench);
    parameters2.setMonoIcons(true);
    parameters2.setAutoStart(false);
    parameters2.setMoveToTrash(false);
    parameters2.setNotificationsDisabled(NotificationsDisabledAlways);
    parameters2.setUseLog(true);
    parameters2.setLogLevel(LogLevelWarning);
    parameters2.setPurgeOldLogs(true);
    parameters2.setSyncHiddenFiles(true);
    parameters2.setProxyConfig(ProxyConfig(ProxyTypeHTTP, "host name", 44444444, true, "user", "token"));
    parameters2.setUseBigFolderSizeLimit(true);
    parameters2.setBigFolderSizeLimit(1000);
    parameters2.setDarkTheme(true);
    std::string geometryStr("XXXXXXXXXX");
    parameters2.setDialogGeometry(
        std::shared_ptr<std::vector<char>>(new std::vector<char>(geometryStr.begin(), geometryStr.end())));
    CPPUNIT_ASSERT(ParmsDb::instance()->updateParameters(parameters2, found) && found);

    CPPUNIT_ASSERT(ParmsDb::instance()->selectParameters(parameters, found) && found);
    CPPUNIT_ASSERT(parameters.language() == parameters2.language());
    CPPUNIT_ASSERT(parameters.monoIcons() == parameters2.monoIcons());
    CPPUNIT_ASSERT(parameters.autoStart() == parameters2.autoStart());
    CPPUNIT_ASSERT(parameters.moveToTrash() == parameters2.moveToTrash());
    CPPUNIT_ASSERT(parameters.notificationsDisabled() == parameters2.notificationsDisabled());
    CPPUNIT_ASSERT(parameters.syncHiddenFiles() == parameters2.syncHiddenFiles());
    CPPUNIT_ASSERT(parameters.useLog() == parameters2.useLog());
    CPPUNIT_ASSERT(parameters.logLevel() == parameters2.logLevel());
    CPPUNIT_ASSERT(parameters.purgeOldLogs() == parameters2.purgeOldLogs());
    CPPUNIT_ASSERT(parameters.proxyConfig().type() == parameters2.proxyConfig().type());
    CPPUNIT_ASSERT(parameters.proxyConfig().hostName() == parameters2.proxyConfig().hostName());
    CPPUNIT_ASSERT(parameters.proxyConfig().port() == parameters2.proxyConfig().port());
    CPPUNIT_ASSERT(parameters.proxyConfig().needsAuth() == parameters2.proxyConfig().needsAuth());
    CPPUNIT_ASSERT(parameters.proxyConfig().user() == parameters2.proxyConfig().user());
    CPPUNIT_ASSERT(parameters.proxyConfig().token() == parameters2.proxyConfig().token());
    CPPUNIT_ASSERT(parameters.useBigFolderSizeLimit() == parameters2.useBigFolderSizeLimit());
    CPPUNIT_ASSERT(parameters.bigFolderSizeLimit() == parameters2.bigFolderSizeLimit());
    CPPUNIT_ASSERT(parameters.darkTheme() == parameters2.darkTheme());
    CPPUNIT_ASSERT(parameters.showShortcuts() == parameters2.showShortcuts());
    CPPUNIT_ASSERT(*parameters.dialogGeometry() == *parameters2.dialogGeometry());
}

void TestParmsDb::testUser() {
    User user1(1, 5555555, "123");
    User user2(2, 6666666, "456");
    User user3(3, 7777777, "789");

    CPPUNIT_ASSERT(ParmsDb::instance()->insertUser(user1));
    CPPUNIT_ASSERT(ParmsDb::instance()->insertUser(user2));
    CPPUNIT_ASSERT(ParmsDb::instance()->insertUser(user3));

    user3.setUserId(9999999);
    user3.setEmail("toto@titi.com");
    bool found;
    CPPUNIT_ASSERT(ParmsDb::instance()->updateUser(user3, found) && found);

    User user4;
    CPPUNIT_ASSERT(ParmsDb::instance()->selectUser(user3.dbId(), user4, found) && found);
    CPPUNIT_ASSERT(user4.dbId() == user3.dbId());
    CPPUNIT_ASSERT(user4.userId() == user3.userId());
    CPPUNIT_ASSERT(user4.email() == user3.email());

    std::vector<User> userList;
    CPPUNIT_ASSERT(ParmsDb::instance()->selectAllUsers(userList));
    CPPUNIT_ASSERT(userList.size() == 3);
    CPPUNIT_ASSERT(userList[0].dbId() == user1.dbId());
    CPPUNIT_ASSERT(userList[0].userId() == user1.userId());
    CPPUNIT_ASSERT(userList[0].userId() == user1.userId());

    CPPUNIT_ASSERT(ParmsDb::instance()->deleteUser(user3.dbId(), found) && found);
    CPPUNIT_ASSERT(ParmsDb::instance()->selectUser(user3.dbId(), user4, found) && !found);
}

void TestParmsDb::testAccount() {
    User user1(1, 5555555, "123");
    User user2(2, 6666666, "456");

    CPPUNIT_ASSERT(ParmsDb::instance()->insertUser(user1));
    CPPUNIT_ASSERT(ParmsDb::instance()->insertUser(user2));

    Account acc1(1, 12345678, user1.dbId());
    Account acc2(2, 23456789, user1.dbId());
    Account acc3(3, 34567890, user2.dbId());

    CPPUNIT_ASSERT(ParmsDb::instance()->insertAccount(acc1));
    CPPUNIT_ASSERT(ParmsDb::instance()->insertAccount(acc2));
    CPPUNIT_ASSERT(ParmsDb::instance()->insertAccount(acc3));

    Account acc4;
    bool found;
    CPPUNIT_ASSERT(ParmsDb::instance()->selectAccount(acc2.dbId(), acc4, found) && found);
    CPPUNIT_ASSERT(acc4.dbId() == acc2.dbId());
    CPPUNIT_ASSERT(acc4.accountId() == acc2.accountId());
    CPPUNIT_ASSERT(acc4.userDbId() == acc2.userDbId());

    std::vector<Account> accountList;
    CPPUNIT_ASSERT(ParmsDb::instance()->selectAllAccounts(user1.dbId(), accountList));
    CPPUNIT_ASSERT(accountList.size() == 2);
    CPPUNIT_ASSERT(accountList[0].dbId() == acc1.dbId());
    CPPUNIT_ASSERT(accountList[0].accountId() == acc1.accountId());
    CPPUNIT_ASSERT(accountList[0].userDbId() == acc1.userDbId());

    CPPUNIT_ASSERT(ParmsDb::instance()->deleteAccount(acc2.dbId(), found) && found);
    CPPUNIT_ASSERT(ParmsDb::instance()->selectAccount(acc2.dbId(), acc4, found) && !found);
}

void TestParmsDb::testDrive() {
    User user1(1, 5555555, "123");
    User user2(2, 6666666, "456");

    CPPUNIT_ASSERT(ParmsDb::instance()->insertUser(user1));
    CPPUNIT_ASSERT(ParmsDb::instance()->insertUser(user2));

    Account acc1(1, 12345678, user1.dbId());
    Account acc2(2, 23456789, user2.dbId());

    CPPUNIT_ASSERT(ParmsDb::instance()->insertAccount(acc1));
    CPPUNIT_ASSERT(ParmsDb::instance()->insertAccount(acc2));

    Drive drive1(1, 99999991, acc1.dbId(), "Drive 1", 2000000000, "#000000");
    Drive drive2(2, 99999992, acc1.dbId(), "Drive 2", 2000000000, "#000000");
    Drive drive3(3, 99999993, acc1.dbId(), "Drive 3", 2000000000, "#000000");

    CPPUNIT_ASSERT(ParmsDb::instance()->insertDrive(drive1));
    CPPUNIT_ASSERT(ParmsDb::instance()->insertDrive(drive2));
    CPPUNIT_ASSERT(ParmsDb::instance()->insertDrive(drive3));

    drive3.setDriveId(99999994);
    drive3.setAccountDbId(acc2.dbId());
    drive3.setName("Drive 3 bis");
    drive3.setSize(6000000000);
    drive3.setColor("#FFFFFF");
    bool found;
    CPPUNIT_ASSERT(ParmsDb::instance()->updateDrive(drive3, found) && found);

    Drive drive4;
    CPPUNIT_ASSERT(ParmsDb::instance()->selectDrive(drive3.dbId(), drive4, found) && found);
    CPPUNIT_ASSERT(drive4.dbId() == drive3.dbId());
    CPPUNIT_ASSERT(drive4.driveId() == drive3.driveId());
    CPPUNIT_ASSERT(drive4.accountDbId() == drive3.accountDbId());
    CPPUNIT_ASSERT(drive4.name() == drive3.name());
    CPPUNIT_ASSERT(drive4.size() == drive3.size());
    CPPUNIT_ASSERT(drive4.color() == drive3.color());

    std::vector<Drive> driveList;
    CPPUNIT_ASSERT(ParmsDb::instance()->selectAllDrives(acc1.dbId(), driveList));
    CPPUNIT_ASSERT(driveList.size() == 2);
    CPPUNIT_ASSERT(driveList[0].dbId() == drive1.dbId());
    CPPUNIT_ASSERT(driveList[0].driveId() == drive1.driveId());
    CPPUNIT_ASSERT(driveList[0].accountDbId() == drive1.accountDbId());
    CPPUNIT_ASSERT(driveList[0].name() == drive1.name());
    CPPUNIT_ASSERT(driveList[0].size() == drive1.size());
    CPPUNIT_ASSERT(driveList[0].color() == drive1.color());

    CPPUNIT_ASSERT(ParmsDb::instance()->deleteDrive(drive3.dbId(), found) && found);
    CPPUNIT_ASSERT(ParmsDb::instance()->selectDrive(drive3.dbId(), drive4, found) && !found);
}

void TestParmsDb::testSync() {
    User user1(1, 5555555, "123");

    CPPUNIT_ASSERT(ParmsDb::instance()->insertUser(user1));

    Account acc1(1, 12345678, user1.dbId());

    CPPUNIT_ASSERT(ParmsDb::instance()->insertAccount(acc1));

    Drive drive1(1, 99999991, acc1.dbId(), "Drive 1", 2000000000, "#000000");
    Drive drive2(2, 99999992, acc1.dbId(), "Drive 2", 2000000000, "#000000");

    CPPUNIT_ASSERT(ParmsDb::instance()->insertDrive(drive1));
    CPPUNIT_ASSERT(ParmsDb::instance()->insertDrive(drive2));

    Sync sync1(1, drive1.dbId(), "/Users/xxxxxx/kDrive1", "", "/Users/xxxxxx/Library/Application Support/kDrive/.syncyyyyyy.db");
    Sync sync2(2, drive1.dbId(), "/Users/xxxxxx/Pictures", "Pictures",
               "/Users/xxxxxx/Library/Application Support/kDrive/.synczzzzzz.db");

    CPPUNIT_ASSERT(ParmsDb::instance()->insertSync(sync1));
    CPPUNIT_ASSERT(ParmsDb::instance()->insertSync(sync2));

    sync2.setLocalPath("/Users/xxxxxx/Movies");
    sync2.setPaused(true);
    sync2.setNotificationsDisabled(true);
    bool found;
    CPPUNIT_ASSERT(ParmsDb::instance()->updateSync(sync2, found) && found);

    Sync sync;
    CPPUNIT_ASSERT(ParmsDb::instance()->selectSync(sync2.dbId(), sync, found) && found);
    CPPUNIT_ASSERT(sync.localPath() == sync2.localPath());
    CPPUNIT_ASSERT(sync.paused() == sync2.paused());
    CPPUNIT_ASSERT(sync.notificationsDisabled() == sync2.notificationsDisabled());

    std::vector<Sync> syncList;
    CPPUNIT_ASSERT(ParmsDb::instance()->selectAllSyncs(drive1.dbId(), syncList));
    CPPUNIT_ASSERT(syncList.size() == 2);
    CPPUNIT_ASSERT(syncList[0].localPath() == sync1.localPath());
    CPPUNIT_ASSERT(syncList[0].paused() == sync1.paused());
    CPPUNIT_ASSERT(syncList[0].notificationsDisabled() == sync1.notificationsDisabled());

    CPPUNIT_ASSERT(ParmsDb::instance()->deleteSync(sync2.dbId(), found) && found);
    CPPUNIT_ASSERT(ParmsDb::instance()->selectSync(sync2.dbId(), sync, found) && !found);
}

void TestParmsDb::testExclusionTemplate() {
    ExclusionTemplate exclusionTemplate1("template 1");
    ExclusionTemplate exclusionTemplate2("template 2");
    ExclusionTemplate exclusionTemplate3("template 3");

    bool constraintError;
    CPPUNIT_ASSERT(ParmsDb::instance()->insertExclusionTemplate(exclusionTemplate1, constraintError));
    CPPUNIT_ASSERT(ParmsDb::instance()->insertExclusionTemplate(exclusionTemplate2, constraintError));
    CPPUNIT_ASSERT(ParmsDb::instance()->insertExclusionTemplate(exclusionTemplate3, constraintError));

    exclusionTemplate3.setWarning(false);
    bool found;
    CPPUNIT_ASSERT(ParmsDb::instance()->updateExclusionTemplate(exclusionTemplate3, found) && found);

    std::vector<ExclusionTemplate> exclusionTemplateList;
    CPPUNIT_ASSERT(ParmsDb::instance()->selectAllExclusionTemplates(false, exclusionTemplateList));
    CPPUNIT_ASSERT(exclusionTemplateList.size() == 3);
    CPPUNIT_ASSERT(exclusionTemplateList[0].templ() == exclusionTemplate1.templ());
    CPPUNIT_ASSERT(exclusionTemplateList[0].warning() == exclusionTemplate1.warning());
    CPPUNIT_ASSERT(exclusionTemplateList[0].def() == exclusionTemplate1.def());
    CPPUNIT_ASSERT(exclusionTemplateList[0].deleted() == exclusionTemplate1.deleted());

    exclusionTemplateList.clear();
    CPPUNIT_ASSERT(ParmsDb::instance()->selectAllExclusionTemplates(true, exclusionTemplateList));
    CPPUNIT_ASSERT(exclusionTemplateList.size() > 0);

    CPPUNIT_ASSERT(ParmsDb::instance()->deleteExclusionTemplate(exclusionTemplate3.templ(), found) && found);
}

void TestParmsDb::testAppState(void) {
    bool found = true;
    std::string value;
    CPPUNIT_ASSERT(ParmsDb::instance()->selectAppState(AppStateKeyTest, value, found) && found);
    CPPUNIT_ASSERT_EQUAL("Test", value);

    CPPUNIT_ASSERT(ParmsDb::instance()->updateAppState(AppStateKeyTest, "value 1", found));
    CPPUNIT_ASSERT(ParmsDb::instance()->selectAppState(AppStateKeyTest, value, found) && found);
    CPPUNIT_ASSERT_EQUAL("value 1", value);

    CPPUNIT_ASSERT(ParmsDb::instance()->updateAppState(static_cast<AppStateKey>(9548215525211611), "value 2", found));
    CPPUNIT_ASSERT(!found);
}

#ifdef __APPLE__
void TestParmsDb::testExclusionApp() {
    ExclusionApp exclusionApp1("app id 1", "description 1");
    ExclusionApp exclusionApp2("app id 2", "description 2");
    ExclusionApp exclusionApp3("app id 3", "description 3");

    bool constraintError;
    CPPUNIT_ASSERT(ParmsDb::instance()->insertExclusionApp(exclusionApp1, constraintError));
    CPPUNIT_ASSERT(ParmsDb::instance()->insertExclusionApp(exclusionApp2, constraintError));
    CPPUNIT_ASSERT(ParmsDb::instance()->insertExclusionApp(exclusionApp3, constraintError));

    exclusionApp3.setDescription("description 3 new");
    bool found;
    CPPUNIT_ASSERT(ParmsDb::instance()->updateExclusionApp(exclusionApp3, found) && found);

    std::vector<ExclusionApp> exclusionAppList;
    CPPUNIT_ASSERT(ParmsDb::instance()->selectAllExclusionApps(false, exclusionAppList));
    CPPUNIT_ASSERT(exclusionAppList.size() == 3);
    CPPUNIT_ASSERT(exclusionAppList[0].appId() == exclusionApp1.appId());
    CPPUNIT_ASSERT(exclusionAppList[0].description() == exclusionApp1.description());
    CPPUNIT_ASSERT(exclusionAppList[0].def() == exclusionApp1.def());

    exclusionAppList.clear();
    CPPUNIT_ASSERT(ParmsDb::instance()->selectAllExclusionApps(true, exclusionAppList));
    CPPUNIT_ASSERT(exclusionAppList.size() > 0);

    CPPUNIT_ASSERT(ParmsDb::instance()->deleteExclusionApp(exclusionApp3.appId(), found) && found);
}
#endif

void TestParmsDb::testError() {
    // TOOD : créer le drive, sync et user
    Error error1("Fct1", ExitCodeDbError, ExitCauseDbAccessError);
    Error error2(1, "Worker1", ExitCodeDataError, ExitCauseSyncDirDoesntExist);
    Error error3(1, "local node 1", "remote node 1", NodeTypeFile, "/dir1/file1.1", ConflictTypeNone, InconsistencyTypeNone);

    CPPUNIT_ASSERT(ParmsDb::instance()->insertError(error1));
    // there is no sync, drive or account Fin the database
    CPPUNIT_ASSERT(!ParmsDb::instance()->insertError(error2));
    CPPUNIT_ASSERT(!ParmsDb::instance()->insertError(error3));
}

}  // namespace KDC

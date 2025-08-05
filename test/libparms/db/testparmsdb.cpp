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

#include "testparmsdb.h"
#include "test_utility/localtemporarydirectory.h"
#include "test_utility/testhelpers.h"

#include "mocks/libcommonserver/db/mockdb.h"

using namespace CppUnit;

namespace KDC {

void TestParmsDb::setUp() {
    TestBase::start();
    // Create a temp parmsDb
    bool alreadyExists = false;
    const std::filesystem::path parmsDbPath = _parmsDbTemporarDirectory.path() / MockDb::makeDbName(alreadyExists);
    (void) ParmsDb::instance(parmsDbPath, "3.6.1", false, true);
}

void TestParmsDb::tearDown() {
    ParmsDb::reset();
    TestBase::stop();
}

void TestParmsDb::testParameters() {
    CPPUNIT_ASSERT(ParmsDb::instance()->exists());

    Parameters defaultParameters;
    Parameters parameters;
    bool found = false;
    CPPUNIT_ASSERT(ParmsDb::instance()->selectParameters(parameters, found) && found);
    CPPUNIT_ASSERT(parameters.language() == defaultParameters.language());
    CPPUNIT_ASSERT(parameters.monoIcons() == defaultParameters.monoIcons());
    CPPUNIT_ASSERT(parameters.autoStart() == defaultParameters.autoStart());
    CPPUNIT_ASSERT(parameters.moveToTrash() == defaultParameters.moveToTrash());
    CPPUNIT_ASSERT(parameters.notificationsDisabled() == defaultParameters.notificationsDisabled());
    CPPUNIT_ASSERT(parameters.useLog() == defaultParameters.useLog());
    CPPUNIT_ASSERT(parameters.logLevel() == defaultParameters.logLevel());
    CPPUNIT_ASSERT(parameters.purgeOldLogs() == defaultParameters.purgeOldLogs());
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
    parameters2.setLanguage(Language::French);
    parameters2.setMonoIcons(true);
    parameters2.setAutoStart(false);
    parameters2.setMoveToTrash(false);
    parameters2.setNotificationsDisabled(NotificationsDisabled::Always);
    parameters2.setUseLog(true);
    parameters2.setLogLevel(LogLevel::Warning);
    parameters2.setPurgeOldLogs(true);
    parameters2.setProxyConfig(ProxyConfig(ProxyType::HTTP, "host name", 44444444, true, "user", "token"));
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

namespace {

struct SyncSetupData {
        std::array<Sync, 2> syncs;
        std::array<Drive, 2> drives;
};

SyncSetupData createSyncs() {
    const User user1(1, 5555555, "123");

    CPPUNIT_ASSERT(ParmsDb::instance()->insertUser(user1));

    const Account acc1(1, 12345678, user1.dbId());

    CPPUNIT_ASSERT(ParmsDb::instance()->insertAccount(acc1));

    const Drive drive1(1, 99999991, acc1.dbId(), "Drive 1", 2000000000, "#000000");
    const Drive drive2(2, 99999992, acc1.dbId(), "Drive 2", 2000000000, "#000000");

    CPPUNIT_ASSERT(ParmsDb::instance()->insertDrive(drive1));
    CPPUNIT_ASSERT(ParmsDb::instance()->insertDrive(drive2));

    Sync sync1(1, drive1.dbId(), "/Users/xxxxxx/kDrive1", "", "/Users/xxxxxx/Library/Application Support/kDrive/.syncyyyyyy.db");
    Sync sync2(2, drive1.dbId(), "/Users/xxxxxx/Pictures", "Pictures",
               "/Users/xxxxxx/Library/Application Support/kDrive/.synczzzzzz.db");

    sync2.setDbPath("/Users/me/Library/Application Support/kDrive/.parms.db");

    SyncSetupData data;
    data.syncs = {sync1, sync2};
    data.drives = {drive1, drive2};

    return data;
}
} // namespace

void TestParmsDb::testSync() {
    auto data = createSyncs();
    auto &sync1 = data.syncs.at(0);
    auto &sync2 = data.syncs.at(1);

    // Insert Sync
    {
        CPPUNIT_ASSERT(ParmsDb::instance()->insertSync(sync1));
        CPPUNIT_ASSERT(ParmsDb::instance()->insertSync(sync2));
    }
    // Update sync
    {
        sync2.setLocalPath("/Users/xxxxxx/Movies");
        sync2.setPaused(true);
        sync2.setNotificationsDisabled(true);
        bool syncIsFound = false;
        CPPUNIT_ASSERT(ParmsDb::instance()->updateSync(sync2, syncIsFound) && syncIsFound);
    }
    // Find sync by DB ID
    {
        Sync sync;
        bool syncIsFound = false;
        CPPUNIT_ASSERT(ParmsDb::instance()->selectSync(sync2.dbId(), sync, syncIsFound) && syncIsFound);

        CPPUNIT_ASSERT(sync.localPath() == sync2.localPath());
        CPPUNIT_ASSERT(sync.paused() == sync2.paused());
        CPPUNIT_ASSERT(sync.notificationsDisabled() == sync2.notificationsDisabled());
    }
    // Find sync by DB path
    {
        Sync sync;
        bool syncIsFound = false;
        CPPUNIT_ASSERT(ParmsDb::instance()->selectSync(sync2.dbPath(), sync, syncIsFound) && syncIsFound);
        CPPUNIT_ASSERT(sync.localPath() == sync2.localPath());
        CPPUNIT_ASSERT(sync.paused() == sync2.paused());
        CPPUNIT_ASSERT(sync.notificationsDisabled() == sync2.notificationsDisabled());
    }
    // Select all syncs
    {
        std::vector<Sync> syncList;
        const Drive &drive1 = data.drives.at(0);
        CPPUNIT_ASSERT(ParmsDb::instance()->selectAllSyncs(drive1.dbId(), syncList));
        CPPUNIT_ASSERT(syncList.size() == 2);
        CPPUNIT_ASSERT(syncList[0].localPath() == sync1.localPath());
        CPPUNIT_ASSERT(syncList[0].paused() == sync1.paused());
        CPPUNIT_ASSERT(syncList[0].notificationsDisabled() == sync1.notificationsDisabled());
    }
    // Delete sync
    {
        Sync sync;
        bool syncIsFound = false;
        CPPUNIT_ASSERT(ParmsDb::instance()->deleteSync(sync2.dbId(), syncIsFound) && syncIsFound);
        CPPUNIT_ASSERT(ParmsDb::instance()->selectSync(sync2.dbId(), sync, syncIsFound) && !syncIsFound);
    }
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
    CPPUNIT_ASSERT(!exclusionTemplateList.empty());

    CPPUNIT_ASSERT(ParmsDb::instance()->deleteExclusionTemplate(exclusionTemplate3.templ(), found) && found);
}

void TestParmsDb::testUpdateExclusionTemplates() {
    ExclusionTemplate exclusionTemplate1("template 1", false, true, false); // extra default template
    bool constraintError = false;
    CPPUNIT_ASSERT(ParmsDb::instance()->insertExclusionTemplate(exclusionTemplate1, constraintError));

    bool found = false;
    CPPUNIT_ASSERT(ParmsDb::instance()->deleteExclusionTemplate(".parms.db", found));

    ExclusionTemplate exclusionTemplate2(".parms.db"); // user template
    CPPUNIT_ASSERT(ParmsDb::instance()->insertExclusionTemplate(exclusionTemplate2, constraintError));
    ExclusionTemplate exclusionTemplate3("template 3"); // user template
    CPPUNIT_ASSERT(ParmsDb::instance()->insertExclusionTemplate(exclusionTemplate3, constraintError));

    // Update
    CPPUNIT_ASSERT(ParmsDb::instance()->updateExclusionTemplates());

    std::vector<ExclusionTemplate> dbDefaultExclusionTemplates;
    (void) ParmsDb::instance()->selectDefaultExclusionTemplates(dbDefaultExclusionTemplates);
    CPPUNIT_ASSERT(!dbDefaultExclusionTemplates.empty());

    std::vector<std::string> fileDefaultExclusionTemplates;
    const auto &excludeListFileName = Utility::getExcludedTemplateFilePath(true);
    ParmsDb::instance()->getDefaultExclusionTemplatesFromFile(excludeListFileName, fileDefaultExclusionTemplates);

    std::vector<ExclusionTemplate> dbUserExclusionTemplates;
    (void) ParmsDb::instance()->selectUserExclusionTemplates(dbUserExclusionTemplates);
    CPPUNIT_ASSERT_EQUAL(size_t{1}, dbUserExclusionTemplates.size());
    CPPUNIT_ASSERT_EQUAL(std::string{"template 3"}, dbUserExclusionTemplates.at(0).templ());

    std::set<std::string, std::less<>> fileDefaults(fileDefaultExclusionTemplates.begin(), fileDefaultExclusionTemplates.end());
    std::set<std::string, std::less<>> dbDefaults;
    (void) std::transform(dbDefaultExclusionTemplates.begin(), dbDefaultExclusionTemplates.end(),
                          std::inserter(dbDefaults, dbDefaults.begin()), [](const auto &t) { return t.templ(); });

    CPPUNIT_ASSERT(dbDefaults == fileDefaults);
}

// We simulate the absence of mandatory columns in a previous version of the database
// by deleting them.
bool TestParmsDb::deleteColumns() {
    int errId;
    std::string error;

    auto db = ParmsDb::instance();

    // Sync table

    if (!db->createAndPrepareRequest("delete_sync_local_node_id", "ALTER TABLE sync DROP localNodeId;")) return false;
    if (!db->queryExec("delete_sync_local_node_id", errId, error)) {
        db->queryFree("delete_sync_local_node_id");
        return db->sqlFail("delete_sync_local_node_id", error);
    }
    db->queryFree("delete_sync_local_node_id");


    // Parameters table

    if (!db->createAndPrepareRequest("delete_parameters_max_allowed_cpu", "ALTER TABLE parameters DROP maxAllowedCpu;"))
        return false;
    if (!db->queryExec("delete_parameters_max_allowed_cpu", errId, error)) {
        db->queryFree("delete_parameters_max_allowed_cpu");
        return db->sqlFail("delete_parameters_max_allowed_cpu", error);
    }
    db->queryFree("delete_parameters_upload_session_parallel_jobs");

    if (!db->createAndPrepareRequest("delete_parameters_upload_session_parallel_jobs",
                                     "ALTER TABLE parameters DROP uploadSessionParallelJobs;"))
        return false;
    if (!db->queryExec("delete_parameters_upload_session_parallel_jobs", errId, error)) {
        db->queryFree("delete_parameters_upload_session_parallel_jobs");
        return db->sqlFail("delete_parameters_upload_session_parallel_jobs", error);
    }
    db->queryFree("delete_parameters_upload_session_parallel_jobs");


    if (!db->createAndPrepareRequest("delete_parameters_job_pool_capacity_factor",
                                     "ALTER TABLE parameters DROP jobPoolCapacityFactor;"))
        return false;
    if (!db->queryExec("delete_parameters_job_pool_capacity_factor", errId, error)) {
        db->queryFree("delete_parameters_job_pool_capacity_factor");
        return db->sqlFail("delete_parameters_job_pool_capacity_factor", error);
    }
    db->queryFree("delete_parameters_job_pool_capacity_factor");


    return true;
}

void TestParmsDb::testUpgradeOfExclusionTemplates() {
    const SyncName nfcEncodedName = testhelpers::makeNfcSyncName();
    ExclusionTemplate exclusionTemplate1(SyncName2Str(nfcEncodedName + Str("/A/") + nfcEncodedName)); // user template
    bool constraintError = false;
    CPPUNIT_ASSERT(ParmsDb::instance()->insertExclusionTemplate(exclusionTemplate1, constraintError));

    ExclusionTemplate exclusionTemplate2("o"); // user template
    CPPUNIT_ASSERT(ParmsDb::instance()->insertExclusionTemplate(exclusionTemplate2, constraintError));

    CPPUNIT_ASSERT(deleteColumns()); // Missing columns should be automatically restored before any SELECT request.

    const std::filesystem::path parmsDbPath = ParmsDb::instance()->dbPath();
    ParmsDb::reset();
    (void) ParmsDb::instance(parmsDbPath, "3.7.2", true, true);

    std::vector<ExclusionTemplate> dbUserExclusionTemplates;
    CPPUNIT_ASSERT(ParmsDb::instance()->selectUserExclusionTemplates(dbUserExclusionTemplates));
    CPPUNIT_ASSERT_EQUAL(size_t{5}, dbUserExclusionTemplates.size());

    const SyncName nfdEncodedName = testhelpers::makeNfdSyncName();

    StrSet expectedTemplateSet;
    (void) expectedTemplateSet.emplace("o");
    for (const auto &name1: {nfcEncodedName, nfdEncodedName}) {
        for (const auto &name2: {nfcEncodedName, nfdEncodedName})
            (void) expectedTemplateSet.emplace(SyncName2Str(name1 + CommonUtility::preferredPathSeparator() + Str("A") +
                                                            CommonUtility::preferredPathSeparator() + name2));
    }

    StrSet actualTemplateSet;
    for (const auto &template_: dbUserExclusionTemplates) {
        (void) actualTemplateSet.emplace(template_.templ());
    }

    CPPUNIT_ASSERT(expectedTemplateSet == actualTemplateSet);
    CPPUNIT_ASSERT(dbUserExclusionTemplates.at(4).templ() == "o");
}

void TestParmsDb::testUpgrade() {
    const SyncName nfcEncodedName = testhelpers::makeNfcSyncName();
    ExclusionTemplate exclusionTemplate1(SyncName2Str(nfcEncodedName + Str("/A/") + nfcEncodedName)); // user template
    bool constraintError = false;
    CPPUNIT_ASSERT(ParmsDb::instance()->insertExclusionTemplate(exclusionTemplate1, constraintError));

    ExclusionTemplate exclusionTemplate2("o"); // user template
    CPPUNIT_ASSERT(ParmsDb::instance()->insertExclusionTemplate(exclusionTemplate2, constraintError));

    const std::filesystem::path parmsDbPath = ParmsDb::instance()->dbPath();
    ParmsDb::reset();
    (void) ParmsDb::instance(parmsDbPath, "3.7.2", true, true);

    std::vector<ExclusionTemplate> dbUserExclusionTemplates;
    CPPUNIT_ASSERT(ParmsDb::instance()->selectUserExclusionTemplates(dbUserExclusionTemplates));
    CPPUNIT_ASSERT_EQUAL(size_t{5}, dbUserExclusionTemplates.size());

    const SyncName nfdEncodedName = testhelpers::makeNfdSyncName();

    StrSet expectedTemplateSet;
    (void) expectedTemplateSet.emplace("o");
    for (const auto &name1: {nfcEncodedName, nfdEncodedName}) {
        for (const auto &name2: {nfcEncodedName, nfdEncodedName})
            (void) expectedTemplateSet.emplace(SyncName2Str(name1 + CommonUtility::preferredPathSeparator() + Str("A") +
                                                            CommonUtility::preferredPathSeparator() + name2));
    }

    StrSet actualTemplateSet;
    for (const auto &template_: dbUserExclusionTemplates) {
        (void) actualTemplateSet.emplace(template_.templ());
    }

    CPPUNIT_ASSERT(expectedTemplateSet == actualTemplateSet);
    CPPUNIT_ASSERT(dbUserExclusionTemplates.at(4).templ() == "o");
}

void TestParmsDb::testAppState(void) {
    bool found = true;
    // Empty string values are not allowed (must use APP_STATE_DEFAULT_IS_EMPTY)
    CPPUNIT_ASSERT(!ParmsDb::instance()->insertAppState(AppStateKey::LogUploadState, std::string{}));
    CPPUNIT_ASSERT(ParmsDb::instance()->insertAppState(AppStateKey::LogUploadState, "__DEFAULT_IS_EMPTY__"));

    CPPUNIT_ASSERT(ParmsDb::instance()->updateAppState(AppStateKey::Unknown, std::string("value"),
                                                       found)); // Test for unknown key (not in db)
    CPPUNIT_ASSERT(!found);

    AppStateValue value = ""; // Test for unknown key (not in db)
    CPPUNIT_ASSERT(ParmsDb::instance()->selectAppState(AppStateKey::Unknown, value, found));
    CPPUNIT_ASSERT(!found);

    // Test for int value
    CPPUNIT_ASSERT(ParmsDb::instance()->updateAppState(static_cast<AppStateKey>(0), 10, found));
    CPPUNIT_ASSERT(found);

    AppStateValue valueIntRes = 0;
    CPPUNIT_ASSERT(ParmsDb::instance()->selectAppState(static_cast<AppStateKey>(0), valueIntRes, found));
    CPPUNIT_ASSERT(found);
    CPPUNIT_ASSERT_EQUAL(10, std::get<int>(valueIntRes));

    // Test for string value
    CPPUNIT_ASSERT(ParmsDb::instance()->updateAppState(static_cast<AppStateKey>(0), std::string("test"), found));
    CPPUNIT_ASSERT(found);

    AppStateValue valueStrRes = ""; // Test for string value
    CPPUNIT_ASSERT(ParmsDb::instance()->selectAppState(static_cast<AppStateKey>(0), valueStrRes, found));
    CPPUNIT_ASSERT(found);
    CPPUNIT_ASSERT_EQUAL(std::string("test"), std::get<std::string>(valueStrRes));

    // Test for LogUploadState value
    CPPUNIT_ASSERT(ParmsDb::instance()->updateAppState(static_cast<AppStateKey>(0), LogUploadState::None, found));
    CPPUNIT_ASSERT(found);

    AppStateValue valueLogUploadStateRes = LogUploadState::None; // Test for LogUploadState value
    CPPUNIT_ASSERT(ParmsDb::instance()->selectAppState(static_cast<AppStateKey>(0), valueLogUploadStateRes, found));
    CPPUNIT_ASSERT(found);
    CPPUNIT_ASSERT_EQUAL(LogUploadState::None, std::get<LogUploadState>(valueLogUploadStateRes));

    int i = 0;
    while (true) {
        AppStateKey key = static_cast<AppStateKey>(i); // Test for all known keys
        if (key == AppStateKey::Unknown) {
            CPPUNIT_ASSERT_EQUAL(AppStateKey::EnumEnd, fromInt<AppStateKey>(i + 1));
            break;
        }
        AppStateValue valueRes = "";
        CPPUNIT_ASSERT(ParmsDb::instance()->selectAppState(key, valueRes, found) && found);
        CPPUNIT_ASSERT(ParmsDb::instance()->updateAppState(key, std::string("value"), found));
        CPPUNIT_ASSERT(ParmsDb::instance()->selectAppState(key, valueRes, found) && found);
        CPPUNIT_ASSERT_EQUAL(std::string("value"), std::get<std::string>(valueRes));
        i++;
    };

    // Test add defaut value if and only if value is empty
    CPPUNIT_ASSERT(ParmsDb::instance()->insertAppState(AppStateKey::Unknown, "test1", true));
    CPPUNIT_ASSERT(ParmsDb::instance()->selectAppState(AppStateKey::Unknown, value, found) && found);
    CPPUNIT_ASSERT_EQUAL(std::string("test1"), std::get<std::string>(value));
    CPPUNIT_ASSERT(ParmsDb::instance()->insertAppState(AppStateKey::Unknown, "test2", true));
    CPPUNIT_ASSERT(ParmsDb::instance()->selectAppState(AppStateKey::Unknown, value, found) && found);
    CPPUNIT_ASSERT_EQUAL(std::string("test1"), std::get<std::string>(value));
}

#if defined(KD_MACOS)
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
    CPPUNIT_ASSERT(!exclusionAppList.empty());

    CPPUNIT_ASSERT(ParmsDb::instance()->deleteExclusionApp(exclusionApp3.appId(), found) && found);
}
#endif

void TestParmsDb::testError() {
    // TOOD : créer le drive, sync et user
    Error error1("Fct1", ExitCode::DbError, ExitCause::DbAccessError);
    Error error2(1, "Worker1", ExitCode::DataError, ExitCause::SyncDirAccessError);
    Error error3(1, "local node 1", "remote node 1", NodeType::File, "/dir1/file1.1", ConflictType::None,
                 InconsistencyType::None);

    CPPUNIT_ASSERT(ParmsDb::instance()->insertError(error1));
    // there is no sync, drive or account Fin the database
    CPPUNIT_ASSERT(!ParmsDb::instance()->insertError(error2));
    CPPUNIT_ASSERT(!ParmsDb::instance()->insertError(error3));

    {
        Error error("Fct", {ExitCode::DbError, ExitCause::DbAccessError});
        CPPUNIT_ASSERT_EQUAL(ExitCode::DbError, error.exitCode());
        CPPUNIT_ASSERT_EQUAL(ExitCause::DbAccessError, error.exitCause());
    }

    {
        Error error(1, "Worker", {ExitCode::DataError, ExitCause::SyncDirAccessError});
        CPPUNIT_ASSERT_EQUAL(ExitCode::DataError, error.exitCode());
        CPPUNIT_ASSERT_EQUAL(ExitCause::SyncDirAccessError, error.exitCause());
    }
}

#if defined(KD_WINDOWS)
void TestParmsDb::testUpgradeOfShortPathNames() {
    LocalTemporaryDirectory temporaryDirectory("testUpgrade");
    std::vector<SyncPath> syncDbLongPaths(3, SyncPath{});
    std::vector<SyncPath> syncDbShortPaths(syncDbLongPaths.size(), SyncPath{});
    for (auto i = 0; i < syncDbLongPaths.size(); ++i) {
        SyncName syncDbName = L".sync_" + std::to_wstring(i + 1) + L".db";
        syncDbLongPaths[i] = temporaryDirectory.path() / syncDbName;
        std::ofstream ofs(syncDbLongPaths[i]);
        auto ioError = IoError::Success;
        IoHelper::getShortPathName(syncDbLongPaths[i], syncDbShortPaths[i], ioError);
    }

    std::vector<Sync> syncList;
    ParmsDb::instance()->selectAllSyncs(syncList);
    CPPUNIT_ASSERT(syncList.empty());

    const User user(1, 5555555, "123");
    ParmsDb::instance()->insertUser(user);
    const Account acc(1, 12345678, user.dbId());
    ParmsDb::instance()->insertAccount(acc);
    const Drive drive(1, 99999991, acc.dbId(), "Drive 1", 2000000000, "#000000");
    ParmsDb::instance()->insertDrive(drive);

    Sync syncWithLongPath;
    syncWithLongPath.setDbPath(syncDbLongPaths[0]);
    syncWithLongPath.setDriveDbId(1);
    syncWithLongPath.setDbId(1);
    ParmsDb::instance()->insertSync(syncWithLongPath);

    for (auto i = 1; i < syncDbLongPaths.size(); ++i) {
        Sync sync;
        sync.setDbPath(syncDbShortPaths[i]);
        sync.setDriveDbId(1);
        sync.setDbId(i + 1);
        ParmsDb::instance()->insertSync(sync);
    }

    // We simulate the absence of mandatory columns in previous version of the database
    // by deleting them.
    // Missing columns should be automatically restored before any SELECT request.
    CPPUNIT_ASSERT(deleteColumns());

    const std::filesystem::path parmsDbPath = ParmsDb::instance()->dbPath();
    ParmsDb::reset();
    (void) ParmsDb::instance(parmsDbPath, "3.7.2", true, true);

    ParmsDb::instance()->selectAllSyncs(syncList);
    CPPUNIT_ASSERT_EQUAL(size_t(3), syncList.size());
    for (auto i = 0; i < syncDbLongPaths.size(); ++i) {
        CPPUNIT_ASSERT_EQUAL(syncDbLongPaths[i], syncList[i].dbPath());
    }
}
#endif

} // namespace KDC

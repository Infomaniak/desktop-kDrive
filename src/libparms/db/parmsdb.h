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

#pragma once

#include "libparms/parmslib.h"
#include "parameters.h"
#include "user.h"
#include "account.h"
#include "drive.h"
#include "sync.h"
#include "exclusiontemplate.h"
#if defined(KD_MACOS)
#include "exclusionapp.h"
#endif
#include "error.h"
#include "migrationselectivesync.h"
#include "libcommonserver/db/db.h"

namespace KDC {

class PARMS_EXPORT ParmsDb : public Db {
    public:
        static std::shared_ptr<ParmsDb> instance(const std::filesystem::path &dbPath = std::filesystem::path(),
                                                 const std::string &version = std::string(), bool autoDelete = false,
                                                 bool test = false);

        std::string dbType() const override { return "Parms"; }

        static void reset();

        ParmsDb(ParmsDb const &) = delete;
        void operator=(ParmsDb const &) = delete;

        bool create(bool &retry) override;
        bool prepare() override;
        bool upgrade(const std::string &fromVersion, const std::string &toVersion) override;

        bool initData();

        bool updateParameters(const Parameters &parameters, bool &found);
        bool selectParameters(Parameters &parameters, bool &found);

        bool insertUser(const User &user);
        bool updateUser(const User &user, bool &found);
        bool deleteUser(UserDbId dbId, bool &found);
        bool selectUser(UserDbId dbId, User &user, bool &found);
        bool selectUserByUserId(UserId userId, User &user, bool &found);
        bool selectUserFromAccountDbId(AccountDbId accountDbId, User &user, bool &found);
        bool selectUserFromDriveDbId(DriveDbId driveDbId, User &user, bool &found);
        bool selectAllUsers(std::vector<User> &userList);
        bool selectLastConnectedUser(User &user, bool &found);
        bool getNewUserDbId(UserDbId &dbId);

        bool insertAccount(const Account &account);
        bool updateAccount(const Account &account, bool &found);
        bool deleteAccount(AccountDbId dbId, bool &found);
        bool selectAccount(AccountDbId dbId, Account &account, bool &found);
        bool selectAllAccounts(std::vector<Account> &accountList);
        bool selectAllAccounts(UserDbId userDbId, std::vector<Account> &accountList);
        bool accountFromUserDbIdAndAccountId(UserDbId userDbId, AccountId accountId, Account &account, bool &found);
        bool getNewAccountDbId(AccountDbId &dbId);

        bool insertDrive(const Drive &drive);
        bool updateDrive(const Drive &drive, bool &found);
        bool deleteDrive(DriveDbId dbId, bool &found);
        bool selectDrive(DriveDbId dbId, Drive &drive, bool &found);
        bool selectDriveByDriveId(DriveId driveId, Drive &drive, bool &found);
        bool selectAllDrives(std::vector<Drive> &driveList);
        bool selectAllDrives(AccountDbId accountDbId, std::vector<Drive> &driveList);
        bool driveDbId(AccountDbId accountDbId, DriveId driveId, DriveDbId &dbId);
        bool getNewDriveDbId(DriveDbId &dbId);

        bool insertSync(const Sync &sync);
        bool updateSync(const Sync &sync, bool &found);
        bool setSyncPaused(SyncDbId dbId, bool value, bool &found);
        bool setSyncHasFullyCompleted(SyncDbId dbId, bool value, bool &found);
        bool deleteSync(SyncDbId dbId, bool &found);
        bool selectSync(SyncDbId dbId, Sync &sync, bool &found);
        bool selectSync(const SyncPath &syncDbPath, Sync &sync, bool &found);
        bool selectAllSyncs(std::vector<Sync> &syncList);
        bool selectAllSyncs(DriveDbId driveDbId, std::vector<Sync> &syncList);
        bool getNewSyncDbId(SyncDbId &dbId);

        bool insertExclusionTemplate(const ExclusionTemplate &exclusionTemplate, bool &constraintError);
        bool updateExclusionTemplate(const ExclusionTemplate &exclusionTemplate, bool &found);
        bool deleteExclusionTemplate(const std::string &template_, bool &found);
        bool selectAllExclusionTemplates(std::vector<ExclusionTemplate> &exclusionTemplateList);
        bool selectDefaultExclusionTemplates(std::vector<ExclusionTemplate> &exclusionTemplateList) {
            return selectAllExclusionTemplates(true, exclusionTemplateList);
        };
        bool selectUserExclusionTemplates(std::vector<ExclusionTemplate> &exclusionTemplateList) {
            return selectAllExclusionTemplates(false, exclusionTemplateList);
        };
        bool updateAllExclusionTemplates(bool defaultTemplate, const std::vector<ExclusionTemplate> &exclusionTemplateList);
        bool updateUserExclusionTemplates(const std::vector<ExclusionTemplate> &exclusionTemplateList) {
            return updateAllExclusionTemplates(false, exclusionTemplateList);
        };

#if defined(KD_MACOS)
        bool insertExclusionApp(const ExclusionApp &exclusionApp, bool &constraintError);
        bool updateExclusionApp(const ExclusionApp &exclusionApp, bool &found);
        bool deleteExclusionApp(const std::string &appId, bool &found);
        bool selectAllExclusionApps(std::vector<ExclusionApp> &exclusionAppList);
        bool selectAllExclusionApps(bool def, std::vector<ExclusionApp> &exclusionAppList);
        bool updateAllExclusionApps(bool def, const std::vector<ExclusionApp> &exclusionAppList);
#endif

        bool insertError(Error &err);
        bool updateError(const Error &err, bool &found);
        bool deleteAllErrorsByExitCode(ExitCode exitCode);
        bool deleteAllErrorsByExitCause(ExitCause exitCause);
        bool selectAllErrors(ErrorLevel level, SyncDbId syncDbId, int limit, std::vector<Error> &errs);
        bool selectAllErrors(int limit, std::vector<Error> &errs);
        bool selectError(ErrorDbId dbId, Error &error, bool &found);
        bool selectConflicts(const SyncDbId syncDbId, ConflictType filter, std::vector<Error> &errs);
        bool deleteErrors(ErrorLevel level);
        bool deleteError(ErrorDbId dbId, bool &found);

        bool insertMigrationSelectiveSync(const MigrationSelectiveSync &migrationSelectiveSync);
        bool selectAllMigrationSelectiveSync(std::vector<MigrationSelectiveSync> &migrationSelectiveSyncList);

        bool selectAppState(AppStateKey key, AppStateValue &value, bool &found);
        bool updateAppState(AppStateKey key, const AppStateValue &value, bool &found); // update or insert

    private:
        friend class TestParmsDb;
        bool _test{false};

        static std::shared_ptr<ParmsDb> _instance;

        ParmsDb(const std::filesystem::path &dbPath, const std::string &version, bool autoDelete, bool test);

        bool upgradeTables();
        bool insertDefaultParameters();
        bool insertDefaultAppState();
        bool insertAppState(AppStateKey key, const std::string &value, bool updateOnlyIfEmpty = false);
        bool updateExclusionTemplates();

        bool createAppState();
        bool prepareAppState();

        void fillSyncWithQueryResult(Sync &sync, const char *requestId);

        bool selectAllExclusionTemplates(bool defaultTemplate, std::vector<ExclusionTemplate> &exclusionTemplateList);

        bool getDefaultExclusionTemplatesFromFile(const SyncPath &syncExcludeListPath,
                                                  std::vector<std::string> &fileDefaultExclusionTemplates);
        bool insertUserTemplateNormalizations(const std::string &fromVersion);

#if defined(KD_MACOS)
        bool updateExclusionApps();
#endif

#if defined(KD_WINDOWS)
        bool replaceShortDbPathsWithLongPaths();
#endif
        bool enableSentryAndMatomo();

#
#
};
} // namespace KDC

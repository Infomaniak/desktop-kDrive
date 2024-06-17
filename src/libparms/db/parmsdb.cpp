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

#include "parmsdb.h"
#include "libcommon/utility/utility.h"
#include "libcommonserver/utility/asserts.h"
#include "libcommonserver/utility/utility.h"
#include "utility/utility.h"

#include <3rdparty/sqlite3/sqlite3.h>

#include <fstream>
#include <string>

//
// parameters
//
#define CREATE_PARAMETERS_TABLE_ID "create_parameters"
#define CREATE_PARAMETERS_TABLE              \
    "CREATE TABLE IF NOT EXISTS parameters(" \
    "language INTEGER,"                      \
    "monoIcons INTEGER,"                     \
    "autoStart INTEGER,"                     \
    "moveToTrash INTEGER,"                   \
    "notifications INTEGER,"                 \
    "uselog INTEGER,"                        \
    "logLevel INTEGER,"                      \
    "purgeOldLogs INTEGER,"                  \
    "syncHiddenFiles INTEGER,"               \
    "proxyType INTEGER,"                     \
    "proxyHostName TEXT,"                    \
    "proxyPort INTEGER,"                     \
    "proxyNeedsAuth INTEGER,"                \
    "proxyUser TEXT,"                        \
    "proxyToken TEXT,"                       \
    "useBigFolderSizeLimit INTEGER,"         \
    "bigFolderSizeLimit INTEGER,"            \
    "darkTheme INTEGER,"                     \
    "showShortcuts INTEGER,"                 \
    "updateFileAvailable TEXT,"              \
    "updateTargetVersion TEXT,"              \
    "updateTargetVersionString TEXT,"        \
    "autoUpdateAttempted INTEGER,"           \
    "seenVersion TEXT,"                      \
    "dialogGeometry BLOB,"                   \
    "extendedLog INTEGER,"                   \
    "maxAllowedCpu INTEGER,"                 \
    "uploadSessionParallelJobs INTEGER,"     \
    "jobPoolCapacityFactor INTEGER);"

#define INSERT_PARAMETERS_REQUEST_ID "insert_parameters"
#define INSERT_PARAMETERS_REQUEST                                                                                             \
    "INSERT INTO parameters (language, monoIcons, autoStart, moveToTrash, notifications, useLog, logLevel, purgeOldLogs, "    \
    "syncHiddenFiles, proxyType, proxyHostName, proxyPort, proxyNeedsAuth, proxyUser, proxyToken, useBigFolderSizeLimit, "    \
    "bigFolderSizeLimit, darkTheme, showShortcuts, updateFileAvailable, updateTargetVersion, updateTargetVersionString, "     \
    "autoUpdateAttempted, seenVersion, dialogGeometry, extendedLog, maxAllowedCpu, uploadSessionParallelJobs, "               \
    "jobPoolCapacityFactor) "                                                                                                 \
    "VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, ?12, ?13, ?14, ?15, ?16, ?17, ?18, ?19, ?20, ?21, ?22, ?23, ?24, " \
    "?25, ?26, ?27, ?28, ?29);"

#define UPDATE_PARAMETERS_REQUEST_ID "update_parameters"
#define UPDATE_PARAMETERS_REQUEST                                                                                               \
    "UPDATE parameters SET language=?1, monoIcons=?2, autoStart=?3, moveToTrash=?4, notifications=?5, useLog=?6, logLevel=?7, " \
    "purgeOldLogs=?8, "                                                                                                         \
    "syncHiddenFiles=?9, proxyType=?10, proxyHostName=?11, proxyPort=?12, proxyNeedsAuth=?13, proxyUser=?14, proxyToken=?15, "  \
    "useBigFolderSizeLimit=?16, "                                                                                               \
    "bigFolderSizeLimit=?17, darkTheme=?18, showShortcuts=?19, updateFileAvailable=?20, updateTargetVersion=?21, "              \
    "updateTargetVersionString=?22, "                                                                                           \
    "autoUpdateAttempted=?23, seenVersion=?24, dialogGeometry=?25, extendedLog=?26, maxAllowedCpu=?27, "                        \
    "uploadSessionParallelJobs=?28, jobPoolCapacityFactor=?29;"

#define SELECT_PARAMETERS_REQUEST_ID "select_parameters"
#define SELECT_PARAMETERS_REQUEST                                                                                          \
    "SELECT language, monoIcons, autoStart, moveToTrash, notifications, useLog, logLevel, purgeOldLogs, "                  \
    "syncHiddenFiles, proxyType, proxyHostName, proxyPort, proxyNeedsAuth, proxyUser, proxyToken, useBigFolderSizeLimit, " \
    "bigFolderSizeLimit, darkTheme, showShortcuts, updateFileAvailable, updateTargetVersion, updateTargetVersionString, "  \
    "autoUpdateAttempted, seenVersion, dialogGeometry, extendedLog, maxAllowedCpu, uploadSessionParallelJobs, "            \
    "jobPoolCapacityFactor "                                                                                               \
    "FROM parameters;"

#define ALTER_PARAMETERS_ADD_MAX_ALLOWED_CPU_REQUEST_ID "alter_parameters_add_max_allowed_cpu"
#define ALTER_PARAMETERS_ADD_MAX_ALLOWED_CPU_REQUEST "ALTER TABLE parameters ADD COLUMN maxAllowedCpu INTEGER;"

#define ALTER_PARAMETERS_ADD_UPLOAD_SESSION_PARALLEL_JOBS_REQUEST_ID "alter_parameters_add_upload_session_parallel_jobs"
#define ALTER_PARAMETERS_ADD_UPLOAD_SESSION_PARALLEL_JOBS_REQUEST \
    "ALTER TABLE parameters ADD COLUMN uploadSessionParallelJobs INTEGER;"

#define ALTER_PARAMETERS_ADD_JOB_POOL_CAPACITY_FACTOR_REQUEST_ID "alter_parameters_add_job_pool_capacity_factor"
#define ALTER_PARAMETERS_ADD_JOB_POOL_CAPACITY_FACTOR_REQUEST "ALTER TABLE parameters ADD COLUMN jobPoolCapacityFactor INTEGER;"

#define UPDATE_PARAMETERS_JOB_REQUEST_ID "update_parameters_job"
#define UPDATE_PARAMETERS_JOB_REQUEST "UPDATE parameters SET uploadSessionParallelJobs=?1, jobPoolCapacityFactor=?2;"

//
// user
//
#define CREATE_USER_TABLE_ID "create_user"
#define CREATE_USER_TABLE              \
    "CREATE TABLE IF NOT EXISTS user(" \
    "dbId INTEGER PRIMARY KEY,"        \
    "userId INTEGER UNIQUE,"           \
    "keychainKey TEXT,"                \
    "name TEXT,"                       \
    "email TEXT,"                      \
    "avatarUrl TEXT,"                  \
    "avatar BLOB, "                    \
    "toMigrate INTEGER) "              \
    "WITHOUT ROWID;"

#define INSERT_USER_REQUEST_ID "insert_user"
#define INSERT_USER_REQUEST                                                                    \
    "INSERT INTO user (dbId, userId, keychainKey, name, email, avatarUrl, avatar, toMigrate) " \
    "VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8);"

#define UPDATE_USER_REQUEST_ID "update_user"
#define UPDATE_USER_REQUEST                                                                                \
    "UPDATE user SET userId=?1, keychainKey=?2, name=?3, email=?4, avatarUrl=?5, avatar=?6, toMigrate=?7 " \
    "WHERE dbId=?8;"

#define DELETE_USER_REQUEST_ID "delete_user"
#define DELETE_USER_REQUEST \
    "DELETE FROM user "     \
    "WHERE dbId=?1;"

#define SELECT_USER_REQUEST_ID "select_user"
#define SELECT_USER_REQUEST                                                            \
    "SELECT userId, keychainKey, name, email, avatarUrl, avatar, toMigrate FROM user " \
    "WHERE dbId=?1;"

#define SELECT_USER_BY_USERID_REQUEST_ID "select_user_by_userid"
#define SELECT_USER_BY_USERID_REQUEST                                                \
    "SELECT dbId, keychainKey, name, email, avatarUrl, avatar, toMigrate FROM user " \
    "WHERE userId=?1;"

#define SELECT_ALL_USERS_REQUEST_ID "select_users"
#define SELECT_ALL_USERS_REQUEST                                                             \
    "SELECT dbId, userId, keychainKey, name, email, avatarUrl, avatar, toMigrate FROM user " \
    "ORDER BY dbId;"

//
// account
//
#define CREATE_ACCOUNT_TABLE_ID "create_account"
#define CREATE_ACCOUNT_TABLE                                                               \
    "CREATE TABLE IF NOT EXISTS account("                                                  \
    "dbId INTEGER PRIMARY KEY,"                                                            \
    "accountId INTEGER,"                                                                   \
    "userDbId INTEGER, "                                                                   \
    "FOREIGN KEY (userDbId) REFERENCES user(dbId) ON DELETE CASCADE ON UPDATE NO ACTION) " \
    "WITHOUT ROWID;"

#define INSERT_ACCOUNT_REQUEST_ID "insert_account"
#define INSERT_ACCOUNT_REQUEST                         \
    "INSERT INTO account (dbId, accountId, userDbId) " \
    "VALUES (?1, ?2, ?3);"

#define UPDATE_ACCOUNT_REQUEST_ID "update_account"
#define UPDATE_ACCOUNT_REQUEST                      \
    "UPDATE account SET accountId=?1, userDbId=?2 " \
    "WHERE dbId=?3;"

#define DELETE_ACCOUNT_REQUEST_ID "delete_account"
#define DELETE_ACCOUNT_REQUEST \
    "DELETE FROM account "     \
    "WHERE dbId=?1;"

#define SELECT_ACCOUNT_REQUEST_ID "select_account"
#define SELECT_ACCOUNT_REQUEST                 \
    "SELECT accountId, userDbId FROM account " \
    "WHERE dbId=?1;"

#define SELECT_ALL_ACCOUNTS_REQUEST_ID "select_accounts"
#define SELECT_ALL_ACCOUNTS_REQUEST                  \
    "SELECT dbId, accountId, userDbId FROM account " \
    "ORDER BY dbId;"

#define SELECT_ALL_ACCOUNTS_BY_USER_REQUEST_ID "select_accounts_by_user"
#define SELECT_ALL_ACCOUNTS_BY_USER_REQUEST \
    "SELECT dbId, accountId FROM account "  \
    "WHERE userDbId=?1 "                    \
    "ORDER BY dbId;"

//
// drive
//
#define CREATE_DRIVE_TABLE_ID "create_drive"
#define CREATE_DRIVE_TABLE                                                                       \
    "CREATE TABLE IF NOT EXISTS drive("                                                          \
    "dbId INTEGER PRIMARY KEY,"                                                                  \
    "driveId INTEGER,"                                                                           \
    "accountDbId INTEGER,"                                                                       \
    "driveName TEXT, "                                                                           \
    "size INTEGER, "                                                                             \
    "color TEXT, "                                                                               \
    "notifications INTEGER, "                                                                    \
    "admin INTEGER, "                                                                            \
    "FOREIGN KEY (accountDbId) REFERENCES account(dbId) ON DELETE CASCADE ON UPDATE NO ACTION) " \
    "WITHOUT ROWID;"

#define INSERT_DRIVE_REQUEST_ID "insert_drive"
#define INSERT_DRIVE_REQUEST                                                                        \
    "INSERT INTO drive (dbId, driveId, accountDbId, driveName, size, color, notifications, admin) " \
    "VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8);"

#define UPDATE_DRIVE_REQUEST_ID "update_drive"
#define UPDATE_DRIVE_REQUEST                                                                                    \
    "UPDATE drive SET driveId=?1, accountDbId=?2, driveName=?3, size=?4, color=?5, notifications=?6, admin=?7 " \
    "WHERE dbId=?8;"

#define DELETE_DRIVE_REQUEST_ID "delete_drive"
#define DELETE_DRIVE_REQUEST \
    "DELETE FROM drive "     \
    "WHERE dbId=?1;"

#define SELECT_DRIVE_REQUEST_ID "select_drive"
#define SELECT_DRIVE_REQUEST                                                                \
    "SELECT driveId, accountDbId, driveName, size, color, notifications, admin FROM drive " \
    "WHERE dbId=?1;"

#define SELECT_DRIVE_BY_DRIVEID_REQUEST_ID "select_drive_by_driveid"
#define SELECT_DRIVE_BY_DRIVEID_REQUEST                                                  \
    "SELECT dbId, accountDbId, driveName, size, color, notifications, admin FROM drive " \
    "WHERE driveId=?1;"

#define SELECT_ALL_DRIVES_REQUEST_ID "select_drives"
#define SELECT_ALL_DRIVES_REQUEST                                                                 \
    "SELECT dbId, driveId, accountDbId, driveName, size, color, notifications, admin FROM drive " \
    "ORDER BY dbId;"

#define SELECT_ALL_DRIVES_BY_ACCOUNT_REQUEST_ID "select_drives_by_account"
#define SELECT_ALL_DRIVES_BY_ACCOUNT_REQUEST                                         \
    "SELECT dbId, driveId, driveName, size, color, notifications, admin FROM drive " \
    "WHERE accountDbId=?1 "                                                          \
    "ORDER BY dbId;"

//
// sync
//
#define CREATE_SYNC_TABLE_ID "create_sync"
#define CREATE_SYNC_TABLE                                                                    \
    "CREATE TABLE IF NOT EXISTS sync("                                                       \
    "dbId INTEGER PRIMARY KEY,"                                                              \
    "driveDbId INTEGER,"                                                                     \
    "localPath TEXT,"                                                                        \
    "targetPath TEXT,"                                                                       \
    "targetNodeId TEXT,"                                                                     \
    "paused INTEGER,"                                                                        \
    "supportVfs INTEGER,"                                                                    \
    "virtualFileMode INTEGER,"                                                               \
    "dbPath TEXT,"                                                                           \
    "notificationsDisabled INTEGER,"                                                         \
    "hasFullyCompleted INTEGER,"                                                             \
    "navigationPaneClsid TEXT,"                                                              \
    "listingCursor TEXT,"                                                                    \
    "listingCursorTimestamp INTEGER,"                                                        \
    "FOREIGN KEY (driveDbId) REFERENCES drive(dbId) ON DELETE CASCADE ON UPDATE NO ACTION) " \
    "WITHOUT ROWID;"

#define INSERT_SYNC_REQUEST_ID "insert_sync"
#define INSERT_SYNC_REQUEST                                                                                                 \
    "INSERT INTO sync (dbId, driveDbId, localPath, targetPath, targetNodeId, dbPath, paused, supportVfs, virtualFileMode, " \
    "notificationsDisabled, hasFullyCompleted, navigationPaneClsid, listingCursor, listingCursorTimestamp) "                \
    "VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, ?12, ?13, ?14);"

#define UPDATE_SYNC_REQUEST_ID "update_sync"
#define UPDATE_SYNC_REQUEST                                                                                             \
    "UPDATE sync SET driveDbId=?1, localPath=?2, targetPath=?3, targetNodeId=?4, dbPath=?5, paused=?6, supportVfs=?7, " \
    "virtualFileMode=?8, notificationsDisabled=?9, hasFullyCompleted=?10, navigationPaneClsid=?11, listingCursor=?12, " \
    "listingCursorTimestamp=?13 "                                                                                       \
    "WHERE dbId=?14;"

#define UPDATE_SYNC_PAUSED_REQUEST_ID "update_sync_paused"
#define UPDATE_SYNC_PAUSED_REQUEST \
    "UPDATE sync SET paused=?1 "   \
    "WHERE dbId=?2;"

#define UPDATE_SYNC_HASFULLYCOMPLETED_REQUEST_ID "update_sync_hasfullycompleted"
#define UPDATE_SYNC_HASFULLYCOMPLETED_REQUEST \
    "UPDATE sync SET hasFullyCompleted=?1 "   \
    "WHERE dbId=?2;"

#define DELETE_SYNC_REQUEST_ID "delete_sync"
#define DELETE_SYNC_REQUEST \
    "DELETE FROM sync "     \
    "WHERE dbId=?1;"

#define SELECT_SYNC_REQUEST_ID "select_sync"
#define SELECT_SYNC_REQUEST                                                                                           \
    "SELECT driveDbId, localPath, targetPath, targetNodeId, dbPath, paused, supportVfs, virtualFileMode, "            \
    "notificationsDisabled, hasFullyCompleted, navigationPaneClsid, listingCursor, listingCursorTimestamp FROM sync " \
    "WHERE dbId=?1;"

#define SELECT_ALL_SYNCS_REQUEST_ID "select_syncs"
#define SELECT_ALL_SYNCS_REQUEST                                                                                      \
    "SELECT dbId, driveDbId, localPath, targetPath, targetNodeId, dbPath, paused, supportVfs, virtualFileMode, "      \
    "notificationsDisabled, hasFullyCompleted, navigationPaneClsid, listingCursor, listingCursorTimestamp FROM sync " \
    "ORDER BY dbId;"

#define SELECT_ALL_SYNCS_BY_DRIVE_REQUEST_ID "select_syncs_by_drive"
#define SELECT_ALL_SYNCS_BY_DRIVE_REQUEST                                                                                    \
    "SELECT dbId, localPath, targetPath, targetNodeId, dbPath, paused, supportVfs, virtualFileMode, notificationsDisabled, " \
    "hasFullyCompleted, navigationPaneClsid, listingCursor, listingCursorTimestamp FROM sync "                               \
    "WHERE driveDbId=?1 "                                                                                                    \
    "ORDER BY dbId;"

//
// exclusion_template
//
#define CREATE_EXCLUSION_TEMPLATE_TABLE_ID "create_exclusion_template"
#define CREATE_EXCLUSION_TEMPLATE_TABLE              \
    "CREATE TABLE IF NOT EXISTS exclusion_template(" \
    "template TEXT PRIMARY KEY,"                     \
    "warning INTEGER,"                               \
    "def INTEGER,"                                   \
    "deleted INTEGER);"

#define INSERT_EXCLUSION_TEMPLATE_REQUEST_ID "insert_exclusion_template"
#define INSERT_EXCLUSION_TEMPLATE_REQUEST                               \
    "INSERT INTO exclusion_template (template, warning, def, deleted) " \
    "VALUES (?1, ?2, ?3, ?4);"

#define UPDATE_EXCLUSION_TEMPLATE_REQUEST_ID "update_exclusion_template"
#define UPDATE_EXCLUSION_TEMPLATE_REQUEST                           \
    "UPDATE exclusion_template SET warning=?1, def=?2, deleted=?3 " \
    "WHERE template=?4;"

#define DELETE_EXCLUSION_TEMPLATE_REQUEST_ID "delete_exclusion_template"
#define DELETE_EXCLUSION_TEMPLATE_REQUEST \
    "DELETE FROM exclusion_template "     \
    "WHERE template=?1;"

#define DELETE_ALL_EXCLUSION_TEMPLATE_BY_DEF_REQUEST_ID "delete_exclusion_template_by_def"
#define DELETE_ALL_EXCLUSION_TEMPLATE_BY_DEF_REQUEST \
    "DELETE FROM exclusion_template "                \
    "WHERE def=?1;"

#define SELECT_ALL_EXCLUSION_TEMPLATE_REQUEST_ID "select_exclusion_templates"
#define SELECT_ALL_EXCLUSION_TEMPLATE_REQUEST "SELECT template, warning, def, deleted FROM exclusion_template;"

#define SELECT_ALL_EXCLUSION_TEMPLATE_BY_DEF_REQUEST_ID "select_exclusion_templates_by_def"
#define SELECT_ALL_EXCLUSION_TEMPLATE_BY_DEF_REQUEST             \
    "SELECT template, warning, deleted FROM exclusion_template " \
    "WHERE def=?1;"

//
// exclusion_app
//
#ifdef __APPLE__
#define CREATE_EXCLUSION_APP_TABLE_ID "create_exclusion_app"
#define CREATE_EXCLUSION_APP_TABLE              \
    "CREATE TABLE IF NOT EXISTS exclusion_app(" \
    "appId TEXT PRIMARY KEY,"                   \
    "description TEXT,"                         \
    "def INTEGER);"

#define INSERT_EXCLUSION_APP_REQUEST_ID "insert_exclusion_app"
#define INSERT_EXCLUSION_APP_REQUEST                       \
    "INSERT INTO exclusion_app (appId, description, def) " \
    "VALUES (?1, ?2, ?3);"

#define UPDATE_EXCLUSION_APP_REQUEST_ID "update_exclusion_app"
#define UPDATE_EXCLUSION_APP_REQUEST                   \
    "UPDATE exclusion_app SET description=?1, def=?2 " \
    "WHERE appId=?3;"

#define DELETE_EXCLUSION_APP_REQUEST_ID "delete_exclusion_app"
#define DELETE_EXCLUSION_APP_REQUEST \
    "DELETE FROM exclusion_app "     \
    "WHERE appId=?1;"

#define DELETE_ALL_EXCLUSION_APP_BY_DEF_REQUEST_ID "delete_exclusion_app_by_def"
#define DELETE_ALL_EXCLUSION_APP_BY_DEF_REQUEST \
    "DELETE FROM exclusion_app "                \
    "WHERE def=?1;"

#define SELECT_ALL_EXCLUSION_APP_REQUEST_ID "select_exclusion_apps"
#define SELECT_ALL_EXCLUSION_APP_REQUEST "SELECT appId, description, def FROM exclusion_app;"

#define SELECT_ALL_EXCLUSION_APP_BY_DEF_REQUEST_ID "select_exclusion_apps_by_def"
#define SELECT_ALL_EXCLUSION_APP_BY_DEF_REQUEST     \
    "SELECT appId, description FROM exclusion_app " \
    "WHERE def=?1;"
#endif

//
// error
//

#define CREATE_ERROR_TABLE_ID "create_error"
#define CREATE_ERROR_TABLE                   \
    "CREATE TABLE IF NOT EXISTS error("      \
    "dbId INTEGER PRIMARY KEY,"              \
    "time INTEGER,"                          \
    "level INTEGER,"     /* Server level */  \
    "functionName TEXT," /* SyncPal level */ \
    "syncDbId INTEGER,"                      \
    "workerName TEXT,"                       \
    "exitCode INTEGER,"                      \
    "exitCause INTEGER," /* Node level */    \
    "localNodeId TEXT,"                      \
    "remoteNodeId TEXT,"                     \
    "nodeType INTEGER,"                      \
    "path TEXT,"                             \
    "status INTEGER,"                        \
    "conflictType INTEGER,"                  \
    "inconsistencyType INTEGER,"             \
    "cancelType INTEGER,"                    \
    "destinationPath TEXT, "                 \
    "FOREIGN KEY (syncDbId) REFERENCES sync(dbId) ON DELETE CASCADE ON UPDATE NO ACTION);"

#define INSERT_ERROR_REQUEST_ID "insert_error"
#define INSERT_ERROR_REQUEST                                                                                            \
    "INSERT INTO error (time, level, "                                                                                  \
    "functionName, syncDbId, workerName, exitCode, exitCause, "                                                         \
    "localNodeId, remoteNodeId, nodeType, path, status, conflictType, inconsistencyType, cancelType, destinationPath) " \
    "VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, ?12, ?13, ?14, ?15, ?16);"

#define UPDATE_ERROR_REQUEST_ID "update_error"
#define UPDATE_ERROR_REQUEST    \
    "UPDATE error SET time=?1 " \
    "WHERE dbId=?2;"

#define DELETE_ALL_ERROR_BY_EXITCODE_REQUEST_ID "delete_error_by_exitcode"
#define DELETE_ALL_ERROR_BY_EXITCODE_REQUEST \
    "DELETE FROM error "                     \
    "WHERE exitCode=?1;"

#define DELETE_ALL_ERROR_BY_ExitCauseREQUEST_ID "delete_error_by_exitcause"
#define DELETE_ALL_ERROR_BY_ExitCauseREQUEST \
    "DELETE FROM error "                      \
    "WHERE exitCause=?1;"

#define DELETE_ALL_ERROR_BY_LEVEL_REQUEST_ID "delete_error_by_level"
#define DELETE_ALL_ERROR_BY_LEVEL_REQUEST \
    "DELETE FROM error "                  \
    "WHERE level=?1;"

#define DELETE_ERROR_BY_DBID_REQUEST_ID "delete_error_by_dbid"
#define DELETE_ERROR_BY_DBID_REQUEST \
    "DELETE FROM error "             \
    "WHERE dbId=?1;"

#define SELECT_ALL_ERROR_BY_LEVEL_AND_SYNCDBID_REQUEST_ID "select_error_by_level_and_syncdbid"
#define SELECT_ALL_ERROR_BY_LEVEL_AND_SYNCDBID_REQUEST                                                                      \
    "SELECT dbId, time, "                                                                                                   \
    "functionName, workerName, exitCode, exitCause, "                                                                       \
    "localNodeId, remoteNodeId, nodeType, path, status, conflictType, inconsistencyType, cancelType, destinationPath FROM " \
    "error "                                                                                                                \
    "WHERE level=?1 AND ((?2=0 AND syncDbId IS NULL) OR syncDbId=?2) "                                                      \
    "ORDER BY time "                                                                                                        \
    "LIMIT ?3;"

#define SELECT_ALL_CONFLICTS_BY_SYNCDBID_REQUEST_ID "select_all_conflicts_by_syncdbid"
#define SELECT_ALL_CONFLICTS_BY_SYNCDBID_REQUEST                                                                            \
    "SELECT dbId, time, "                                                                                                   \
    "functionName, workerName, exitCode, exitCause, "                                                                       \
    "localNodeId, remoteNodeId, nodeType, path, status, conflictType, inconsistencyType, cancelType, destinationPath FROM " \
    "error "                                                                                                                \
    "WHERE syncDbId=?1 AND conflictType <> 0;"

#define SELECT_FILTERED_CONFLICTS_BY_SYNCDBID_REQUEST_ID "select_filtered_conflicts_by_syncdbid"
#define SELECT_FILTERED_CONFLICTS_BY_SYNCDBID_REQUEST                                                                       \
    "SELECT dbId, time, "                                                                                                   \
    "functionName, workerName, exitCode, exitCause, "                                                                       \
    "localNodeId, remoteNodeId, nodeType, path, status, conflictType, inconsistencyType, cancelType, destinationPath FROM " \
    "error "                                                                                                                \
    "WHERE syncDbId=?1 AND conflictType=?2;"

//
// migration_selectivesync
//
#define CREATE_MIGRATION_SELECTIVESYNC_TABLE_ID "create_migration_selectivesync"
#define CREATE_MIGRATION_SELECTIVESYNC_TABLE              \
    "CREATE TABLE IF NOT EXISTS migration_selectivesync(" \
    "syncDbId INTEGER,"                                   \
    "path TEXT,"                                          \
    "type INTEGER);"

#define INSERT_MIGRATION_SELECTIVESYNC_REQUEST_ID "insert_migration_selectivesync"
#define INSERT_MIGRATION_SELECTIVESYNC_REQUEST                    \
    "INSERT INTO migration_selectivesync (syncDbId, path, type) " \
    "VALUES (?1, ?2, ?3);"

#define SELECT_ALL_MIGRATION_SELECTIVESYNC_REQUEST_ID "select_migration_selectivesync"
#define SELECT_ALL_MIGRATION_SELECTIVESYNC_REQUEST "SELECT syncDbId, path, type FROM migration_selectivesync;"

namespace KDC {

std::shared_ptr<ParmsDb> ParmsDb::_instance = nullptr;

std::shared_ptr<ParmsDb> ParmsDb::instance(const std::filesystem::path &dbPath, const std::string &version,
                                           bool autoDelete /*= false*/, bool test /*= false*/) {
    if (_instance == nullptr) {
        if (dbPath.empty()) {
            throw std::runtime_error("ParmsDb must be initialized!");
        } else {
            _instance = std::shared_ptr<ParmsDb>(new ParmsDb(dbPath, version, autoDelete, test));
        }
    }

    return _instance;
}

void ParmsDb::reset() {
    if (_instance) {
        _instance = nullptr;
    }
}

ParmsDb::ParmsDb(const std::filesystem::path &dbPath, const std::string &version, bool autoDelete, bool test)
    : Db(dbPath), _test(test) {
    setAutoDelete(autoDelete);

    if (!checkConnect(version)) {
        throw std::runtime_error("Cannot open DB!");
        return;
    }

    LOG_INFO(_logger, "ParmsDb initialization done");
}

bool ParmsDb::insertDefaultParameters() {
    const std::scoped_lock lock(_mutex);

    bool found = false;
    if (!queryNext(SELECT_PARAMETERS_REQUEST_ID, found)) {
        LOG_WARN(_logger, "Error getting query result: " << SELECT_PARAMETERS_REQUEST_ID);
        return false;
    }
    if (found) {
        return true;
    }

    Parameters parameters;

    ProxyConfig proxyConfig(parameters.proxyConfig());
    proxyConfig.setType(ProxyType::None);
    parameters.setProxyConfig(proxyConfig);

    int errId = 0;
    std::string error;

    ASSERT(queryResetAndClearBindings(INSERT_PARAMETERS_REQUEST_ID));
    ASSERT(queryBindValue(INSERT_PARAMETERS_REQUEST_ID, 1, static_cast<int>(parameters.language())));
    ASSERT(queryBindValue(INSERT_PARAMETERS_REQUEST_ID, 2, parameters.monoIcons()));
    ASSERT(queryBindValue(INSERT_PARAMETERS_REQUEST_ID, 3, parameters.autoStart()));
    ASSERT(queryBindValue(INSERT_PARAMETERS_REQUEST_ID, 4, parameters.moveToTrash()));
    ASSERT(queryBindValue(INSERT_PARAMETERS_REQUEST_ID, 5, static_cast<int>(parameters.notificationsDisabled())));
    ASSERT(queryBindValue(INSERT_PARAMETERS_REQUEST_ID, 6, parameters.useLog()));
    ASSERT(queryBindValue(INSERT_PARAMETERS_REQUEST_ID, 7, static_cast<int>(parameters.logLevel())));
    ASSERT(queryBindValue(INSERT_PARAMETERS_REQUEST_ID, 8, parameters.purgeOldLogs()));
    ASSERT(queryBindValue(INSERT_PARAMETERS_REQUEST_ID, 9, parameters.syncHiddenFiles()));
    ASSERT(queryBindValue(INSERT_PARAMETERS_REQUEST_ID, 10, static_cast<int>(parameters.proxyConfig().type())));
    ASSERT(queryBindValue(INSERT_PARAMETERS_REQUEST_ID, 11, parameters.proxyConfig().hostName()));
    ASSERT(queryBindValue(INSERT_PARAMETERS_REQUEST_ID, 12, parameters.proxyConfig().port()));
    ASSERT(queryBindValue(INSERT_PARAMETERS_REQUEST_ID, 13, parameters.proxyConfig().needsAuth()));
    ASSERT(queryBindValue(INSERT_PARAMETERS_REQUEST_ID, 14, parameters.proxyConfig().user()));
    ASSERT(queryBindValue(INSERT_PARAMETERS_REQUEST_ID, 15, parameters.proxyConfig().token()));
    ASSERT(queryBindValue(INSERT_PARAMETERS_REQUEST_ID, 16, parameters.useBigFolderSizeLimit()));
    ASSERT(queryBindValue(INSERT_PARAMETERS_REQUEST_ID, 17, parameters.bigFolderSizeLimit()));
    ASSERT(queryBindValue(INSERT_PARAMETERS_REQUEST_ID, 18, parameters.darkTheme()));
    ASSERT(queryBindValue(INSERT_PARAMETERS_REQUEST_ID, 19, parameters.showShortcuts()));
    ASSERT(queryBindValue(INSERT_PARAMETERS_REQUEST_ID, 20, parameters.updateFileAvailable()));
    ASSERT(queryBindValue(INSERT_PARAMETERS_REQUEST_ID, 21, parameters.updateTargetVersion()));
    ASSERT(queryBindValue(INSERT_PARAMETERS_REQUEST_ID, 22, parameters.updateTargetVersionString()));
    ASSERT(queryBindValue(INSERT_PARAMETERS_REQUEST_ID, 23, parameters.autoUpdateAttempted()));
    ASSERT(queryBindValue(INSERT_PARAMETERS_REQUEST_ID, 24, parameters.seenVersion()));
    ASSERT(queryBindValue(INSERT_PARAMETERS_REQUEST_ID, 25, parameters.dialogGeometry()));
    ASSERT(queryBindValue(INSERT_PARAMETERS_REQUEST_ID, 26, static_cast<int>(parameters.extendedLog())));
    ASSERT(queryBindValue(INSERT_PARAMETERS_REQUEST_ID, 27, parameters.maxAllowedCpu()));
    ASSERT(queryBindValue(INSERT_PARAMETERS_REQUEST_ID, 28, parameters.uploadSessionParallelJobs()));
    ASSERT(queryBindValue(INSERT_PARAMETERS_REQUEST_ID, 29, parameters.jobPoolCapacityFactor()));

    if (!queryExec(INSERT_PARAMETERS_REQUEST_ID, errId, error)) {
        LOG_WARN(_logger, "Error running query: " << INSERT_PARAMETERS_REQUEST_ID);
        return false;
    }

    return true;
}

bool ParmsDb::updateExclusionTemplates() {
    // Load default exclusion templates in DB
    std::vector<ExclusionTemplate> exclusionTemplateDbList;
    if (!selectAllExclusionTemplates(true, exclusionTemplateDbList)) {
        LOG_WARN(_logger, "Error in selectAllExclusionTemplates");
        return false;
    }

    // Load default exclusion templates in configuration file
    std::vector<std::string> exclusionTemplateFileList;
    SyncName t = Utility::getExcludedTemplateFilePath(_test);
    std::ifstream exclusionFile(Utility::getExcludedTemplateFilePath(_test));
    if (exclusionFile.is_open()) {
        std::string line;
        while (std::getline(exclusionFile, line)) {
            // Remove end of line
            if (line.size() > 0 && line[line.size() - 1] == '\n') {
                line.pop_back();
            }
            if (line.size() > 0 && line[line.size() - 1] == '\r') {
                line.pop_back();
            }

            exclusionTemplateFileList.push_back(line);
        }
    } else {
        LOGW_WARN(_logger,
                  L"Cannot open exclusion templates file " << SyncName2WStr(Utility::getExcludedTemplateFilePath(_test)).c_str());
        return false;
    }

    for (const auto &templDb : exclusionTemplateDbList) {
        bool exists = false;
        for (const auto &templFile : exclusionTemplateFileList) {
            if (templFile == templDb.templ()) {
                exists = true;
                break;
            }
        }
        if (!exists) {
            // Remove DB template
            bool found;
            if (!deleteExclusionTemplate(templDb.templ(), found)) {
                LOG_WARN(_logger, "Error in deleteExclusionTemplate");
                return false;
            }
        }
    }

    std::vector<ExclusionTemplate> exclusionTemplateUserDbList;
    if (!selectAllExclusionTemplates(false, exclusionTemplateUserDbList)) {
        LOG_WARN(_logger, "Error in selectAllExclusionTemplates");
        return false;
    }

    for (const auto &templFile : exclusionTemplateFileList) {
        bool exists = false;
        for (const auto &templDb : exclusionTemplateDbList) {
            if (templDb.templ() == templFile) {
                exists = true;
                break;
            }
        }
        if (!exists) {
            for (const auto &userTempDb : exclusionTemplateUserDbList) {
                if (templFile == userTempDb.templ()) {
                    bool found = false;
                    if (!deleteExclusionTemplate(userTempDb.templ(), found)) {
                        LOG_WARN(_logger, "Error in deleteExclusionTemplate");
                        return false;
                    }
                    break;
                }
            }
            // Add template in DB
            bool constraintError;
            if (!insertExclusionTemplate(ExclusionTemplate(templFile, false, true, false), constraintError)) {
                LOG_WARN(_logger, "Error in insertExclusionTemplate");
                return false;
            }
        }
    }
    return true;
}

#ifdef __APPLE__
bool ParmsDb::updateExclusionApps() {
    // Load exclusion apps in DB
    std::vector<ExclusionApp> exclusionAppDbList;
    if (!selectAllExclusionApps(exclusionAppDbList)) {
        LOG_WARN(_logger, "Error in selectAllExclusionApps");
        return false;
    }

    // Load exclusion app in configuration file
    std::vector<std::pair<std::string, std::string>> exclusionAppFileList;
    std::ifstream exclusionFile(Utility::getExcludedAppFilePath(_test));
    if (exclusionFile.is_open()) {
        std::string line;
        while (std::getline(exclusionFile, line)) {
            // Remove end of line
            if (line.size() > 0 && line[line.size() - 1] == '\n') {
                line.pop_back();
            }
            if (line.size() > 0 && line[line.size() - 1] == '\r') {
                line.pop_back();
            }

            size_t pos = line.find(';');
            std::string appId = line.substr(0, pos);
            std::string descr = line.substr(pos + 1);
            exclusionAppFileList.push_back(std::make_pair(appId, descr));
        }
    } else {
        LOG_WARN(_logger, "Cannot open exclusion app file");
        return false;
    }

    for (const auto &templDb : exclusionAppDbList) {
        if (templDb.def() == false) {
            continue;
        }

        bool exists = false;
        for (const auto &templFile : exclusionAppFileList) {
            if (templFile.first == templDb.appId()) {
                exists = true;
                break;
            }
        }
        if (!exists) {
            // Remove app exclusion in DB
            bool found;
            if (!deleteExclusionApp(templDb.appId(), found)) {
                LOG_WARN(_logger, "Error in deleteExclusionApp");
                return false;
            }
        }
    }

    for (const auto &templFile : exclusionAppFileList) {
        bool exists = false;
        bool def = false;
        for (const auto &templDb : exclusionAppDbList) {
            if (templDb.appId() == templFile.first) {
                exists = true;
                def = templDb.def();
                break;
            }
        }
        if (exists && !def) {
            // Remove app exclusion in DB
            bool found;
            if (!deleteExclusionApp(templFile.first, found)) {
                LOG_WARN(_logger, "Error in deleteExclusionApp");
                return false;
            }
        }

        if (!exists || !def) {
            // Add app exclusion in DB
            bool constraintError;
            if (!insertExclusionApp(ExclusionApp(templFile.first, templFile.second, true), constraintError)) {
                LOG_WARN(_logger, "Error in insertExclusionApp");
                return false;
            }
        }
    }
    return true;
}
#endif

bool ParmsDb::create(bool &retry) {
    int errId;
    std::string error;

    // Parameters
    ASSERT(queryCreate(CREATE_PARAMETERS_TABLE_ID));
    if (!queryPrepare(CREATE_PARAMETERS_TABLE_ID, CREATE_PARAMETERS_TABLE, false, errId, error)) {
        queryFree(CREATE_PARAMETERS_TABLE_ID);
        return sqlFail(CREATE_PARAMETERS_TABLE_ID, error);
    }
    if (!queryExec(CREATE_PARAMETERS_TABLE_ID, errId, error)) {
        // In certain situations the io error can be avoided by switching
        // to the DELETE journal mode
        if (_journalMode != "DELETE" && errId == SQLITE_IOERR && extendedErrorCode() == SQLITE_IOERR_SHMMAP) {
            LOG_WARN(_logger, "IO error SHMMAP on table creation, attempting with DELETE journal mode");
            _journalMode = "DELETE";
            queryFree(CREATE_PARAMETERS_TABLE_ID);
            retry = true;
            return false;
        }

        return sqlFail(CREATE_PARAMETERS_TABLE_ID, error);
    }
    queryFree(CREATE_PARAMETERS_TABLE_ID);

    // User
    ASSERT(queryCreate(CREATE_USER_TABLE_ID));
    if (!queryPrepare(CREATE_USER_TABLE_ID, CREATE_USER_TABLE, false, errId, error)) {
        queryFree(CREATE_USER_TABLE_ID);
        return sqlFail(CREATE_USER_TABLE_ID, error);
    }
    if (!queryExec(CREATE_USER_TABLE_ID, errId, error)) {
        queryFree(CREATE_USER_TABLE_ID);
        return sqlFail(CREATE_USER_TABLE_ID, error);
    }
    queryFree(CREATE_USER_TABLE_ID);

    // Account
    ASSERT(queryCreate(CREATE_ACCOUNT_TABLE_ID));
    if (!queryPrepare(CREATE_ACCOUNT_TABLE_ID, CREATE_ACCOUNT_TABLE, false, errId, error)) {
        queryFree(CREATE_ACCOUNT_TABLE_ID);
        return sqlFail(CREATE_ACCOUNT_TABLE_ID, error);
    }
    if (!queryExec(CREATE_ACCOUNT_TABLE_ID, errId, error)) {
        queryFree(CREATE_ACCOUNT_TABLE_ID);
        return sqlFail(CREATE_ACCOUNT_TABLE_ID, error);
    }
    queryFree(CREATE_ACCOUNT_TABLE_ID);

    // Drive
    ASSERT(queryCreate(CREATE_DRIVE_TABLE_ID));
    if (!queryPrepare(CREATE_DRIVE_TABLE_ID, CREATE_DRIVE_TABLE, false, errId, error)) {
        queryFree(CREATE_DRIVE_TABLE_ID);
        return sqlFail(CREATE_DRIVE_TABLE_ID, error);
    }
    if (!queryExec(CREATE_DRIVE_TABLE_ID, errId, error)) {
        queryFree(CREATE_DRIVE_TABLE_ID);
        return sqlFail(CREATE_DRIVE_TABLE_ID, error);
    }
    queryFree(CREATE_DRIVE_TABLE_ID);

    // Sync
    ASSERT(queryCreate(CREATE_SYNC_TABLE_ID));
    if (!queryPrepare(CREATE_SYNC_TABLE_ID, CREATE_SYNC_TABLE, false, errId, error)) {
        queryFree(CREATE_SYNC_TABLE_ID);
        return sqlFail(CREATE_SYNC_TABLE_ID, error);
    }
    if (!queryExec(CREATE_SYNC_TABLE_ID, errId, error)) {
        queryFree(CREATE_SYNC_TABLE_ID);
        return sqlFail(CREATE_SYNC_TABLE_ID, error);
    }
    queryFree(CREATE_SYNC_TABLE_ID);

    // Exclusion Template
    ASSERT(queryCreate(CREATE_EXCLUSION_TEMPLATE_TABLE_ID));
    if (!queryPrepare(CREATE_EXCLUSION_TEMPLATE_TABLE_ID, CREATE_EXCLUSION_TEMPLATE_TABLE, false, errId, error)) {
        queryFree(CREATE_EXCLUSION_TEMPLATE_TABLE_ID);
        return sqlFail(CREATE_EXCLUSION_TEMPLATE_TABLE_ID, error);
    }
    if (!queryExec(CREATE_EXCLUSION_TEMPLATE_TABLE_ID, errId, error)) {
        queryFree(CREATE_EXCLUSION_TEMPLATE_TABLE_ID);
        return sqlFail(CREATE_EXCLUSION_TEMPLATE_TABLE_ID, error);
    }
    queryFree(CREATE_EXCLUSION_TEMPLATE_TABLE_ID);

#ifdef __APPLE__
    // Exclusion App
    ASSERT(queryCreate(CREATE_EXCLUSION_APP_TABLE_ID));
    if (!queryPrepare(CREATE_EXCLUSION_APP_TABLE_ID, CREATE_EXCLUSION_APP_TABLE, false, errId, error)) {
        queryFree(CREATE_EXCLUSION_APP_TABLE_ID);
        return sqlFail(CREATE_EXCLUSION_APP_TABLE_ID, error);
    }
    if (!queryExec(CREATE_EXCLUSION_APP_TABLE_ID, errId, error)) {
        queryFree(CREATE_EXCLUSION_APP_TABLE_ID);
        return sqlFail(CREATE_EXCLUSION_APP_TABLE_ID, error);
    }
    queryFree(CREATE_EXCLUSION_APP_TABLE_ID);
#endif

    // Error
    ASSERT(queryCreate(CREATE_ERROR_TABLE_ID));
    if (!queryPrepare(CREATE_ERROR_TABLE_ID, CREATE_ERROR_TABLE, false, errId, error)) {
        queryFree(CREATE_ERROR_TABLE_ID);
        return sqlFail(CREATE_ERROR_TABLE_ID, error);
    }
    if (!queryExec(CREATE_ERROR_TABLE_ID, errId, error)) {
        queryFree(CREATE_ERROR_TABLE_ID);
        return sqlFail(CREATE_ERROR_TABLE_ID, error);
    }
    queryFree(CREATE_ERROR_TABLE_ID);
    
    // app state
    if (!createAppState()) {
        LOG_WARN(_logger, "Error in createAppState");
        return false;
    }

    // Migration old selectivesync table
    ASSERT(queryCreate(CREATE_MIGRATION_SELECTIVESYNC_TABLE_ID));
    if (!queryPrepare(CREATE_MIGRATION_SELECTIVESYNC_TABLE_ID, CREATE_MIGRATION_SELECTIVESYNC_TABLE, false, errId, error)) {
        queryFree(CREATE_MIGRATION_SELECTIVESYNC_TABLE_ID);
        return sqlFail(CREATE_MIGRATION_SELECTIVESYNC_TABLE_ID, error);
    }
    if (!queryExec(CREATE_MIGRATION_SELECTIVESYNC_TABLE_ID, errId, error)) {
        queryFree(CREATE_MIGRATION_SELECTIVESYNC_TABLE_ID);
        return sqlFail(CREATE_MIGRATION_SELECTIVESYNC_TABLE_ID, error);
    }
    queryFree(CREATE_MIGRATION_SELECTIVESYNC_TABLE_ID);


    return true;
}

bool ParmsDb::prepare() {
    int errId;
    std::string error;

    // Parameters
    ASSERT(queryCreate(INSERT_PARAMETERS_REQUEST_ID));
    if (!queryPrepare(INSERT_PARAMETERS_REQUEST_ID, INSERT_PARAMETERS_REQUEST, false, errId, error)) {
        queryFree(INSERT_PARAMETERS_REQUEST_ID);
        return sqlFail(INSERT_PARAMETERS_REQUEST_ID, error);
    }

    ASSERT(queryCreate(UPDATE_PARAMETERS_REQUEST_ID));
    if (!queryPrepare(UPDATE_PARAMETERS_REQUEST_ID, UPDATE_PARAMETERS_REQUEST, false, errId, error)) {
        queryFree(UPDATE_PARAMETERS_REQUEST_ID);
        return sqlFail(UPDATE_PARAMETERS_REQUEST_ID, error);
    }

    ASSERT(queryCreate(SELECT_PARAMETERS_REQUEST_ID));
    if (!queryPrepare(SELECT_PARAMETERS_REQUEST_ID, SELECT_PARAMETERS_REQUEST, false, errId, error)) {
        queryFree(SELECT_PARAMETERS_REQUEST_ID);
        return sqlFail(SELECT_PARAMETERS_REQUEST_ID, error);
    }

    // User
    ASSERT(queryCreate(INSERT_USER_REQUEST_ID));
    if (!queryPrepare(INSERT_USER_REQUEST_ID, INSERT_USER_REQUEST, false, errId, error)) {
        queryFree(INSERT_USER_REQUEST_ID);
        return sqlFail(INSERT_USER_REQUEST_ID, error);
    }

    ASSERT(queryCreate(UPDATE_USER_REQUEST_ID));
    if (!queryPrepare(UPDATE_USER_REQUEST_ID, UPDATE_USER_REQUEST, false, errId, error)) {
        queryFree(UPDATE_USER_REQUEST_ID);
        return sqlFail(UPDATE_USER_REQUEST_ID, error);
    }

    ASSERT(queryCreate(DELETE_USER_REQUEST_ID));
    if (!queryPrepare(DELETE_USER_REQUEST_ID, DELETE_USER_REQUEST, false, errId, error)) {
        queryFree(DELETE_USER_REQUEST_ID);
        return sqlFail(DELETE_USER_REQUEST_ID, error);
    }

    ASSERT(queryCreate(SELECT_USER_REQUEST_ID));
    if (!queryPrepare(SELECT_USER_REQUEST_ID, SELECT_USER_REQUEST, false, errId, error)) {
        queryFree(SELECT_USER_REQUEST_ID);
        return sqlFail(SELECT_USER_REQUEST_ID, error);
    }

    ASSERT(queryCreate(SELECT_USER_BY_USERID_REQUEST_ID));
    if (!queryPrepare(SELECT_USER_BY_USERID_REQUEST_ID, SELECT_USER_BY_USERID_REQUEST, false, errId, error)) {
        queryFree(SELECT_USER_BY_USERID_REQUEST_ID);
        return sqlFail(SELECT_USER_BY_USERID_REQUEST_ID, error);
    }

    ASSERT(queryCreate(SELECT_ALL_USERS_REQUEST_ID));
    if (!queryPrepare(SELECT_ALL_USERS_REQUEST_ID, SELECT_ALL_USERS_REQUEST, false, errId, error)) {
        queryFree(SELECT_ALL_USERS_REQUEST_ID);
        return sqlFail(SELECT_ALL_USERS_REQUEST_ID, error);
    }

    // Account
    ASSERT(queryCreate(INSERT_ACCOUNT_REQUEST_ID));
    if (!queryPrepare(INSERT_ACCOUNT_REQUEST_ID, INSERT_ACCOUNT_REQUEST, false, errId, error)) {
        queryFree(INSERT_ACCOUNT_REQUEST_ID);
        return sqlFail(INSERT_ACCOUNT_REQUEST_ID, error);
    }

    ASSERT(queryCreate(UPDATE_ACCOUNT_REQUEST_ID));
    if (!queryPrepare(UPDATE_ACCOUNT_REQUEST_ID, UPDATE_ACCOUNT_REQUEST, false, errId, error)) {
        queryFree(UPDATE_ACCOUNT_REQUEST_ID);
        return sqlFail(UPDATE_ACCOUNT_REQUEST_ID, error);
    }

    ASSERT(queryCreate(DELETE_ACCOUNT_REQUEST_ID));
    if (!queryPrepare(DELETE_ACCOUNT_REQUEST_ID, DELETE_ACCOUNT_REQUEST, false, errId, error)) {
        queryFree(DELETE_ACCOUNT_REQUEST_ID);
        return sqlFail(DELETE_ACCOUNT_REQUEST_ID, error);
    }

    ASSERT(queryCreate(SELECT_ACCOUNT_REQUEST_ID));
    if (!queryPrepare(SELECT_ACCOUNT_REQUEST_ID, SELECT_ACCOUNT_REQUEST, false, errId, error)) {
        queryFree(SELECT_ACCOUNT_REQUEST_ID);
        return sqlFail(SELECT_ACCOUNT_REQUEST_ID, error);
    }

    ASSERT(queryCreate(SELECT_ALL_ACCOUNTS_REQUEST_ID));
    if (!queryPrepare(SELECT_ALL_ACCOUNTS_REQUEST_ID, SELECT_ALL_ACCOUNTS_REQUEST, false, errId, error)) {
        queryFree(SELECT_ALL_ACCOUNTS_REQUEST_ID);
        return sqlFail(SELECT_ALL_ACCOUNTS_REQUEST_ID, error);
    }

    ASSERT(queryCreate(SELECT_ALL_ACCOUNTS_BY_USER_REQUEST_ID));
    if (!queryPrepare(SELECT_ALL_ACCOUNTS_BY_USER_REQUEST_ID, SELECT_ALL_ACCOUNTS_BY_USER_REQUEST, false, errId, error)) {
        queryFree(SELECT_ALL_ACCOUNTS_BY_USER_REQUEST_ID);
        return sqlFail(SELECT_ALL_ACCOUNTS_BY_USER_REQUEST_ID, error);
    }

    // Drive
    ASSERT(queryCreate(INSERT_DRIVE_REQUEST_ID));
    if (!queryPrepare(INSERT_DRIVE_REQUEST_ID, INSERT_DRIVE_REQUEST, false, errId, error)) {
        queryFree(INSERT_DRIVE_REQUEST_ID);
        return sqlFail(INSERT_DRIVE_REQUEST_ID, error);
    }

    ASSERT(queryCreate(UPDATE_DRIVE_REQUEST_ID));
    if (!queryPrepare(UPDATE_DRIVE_REQUEST_ID, UPDATE_DRIVE_REQUEST, false, errId, error)) {
        queryFree(UPDATE_DRIVE_REQUEST_ID);
        return sqlFail(UPDATE_DRIVE_REQUEST_ID, error);
    }

    ASSERT(queryCreate(DELETE_DRIVE_REQUEST_ID));
    if (!queryPrepare(DELETE_DRIVE_REQUEST_ID, DELETE_DRIVE_REQUEST, false, errId, error)) {
        queryFree(DELETE_DRIVE_REQUEST_ID);
        return sqlFail(DELETE_DRIVE_REQUEST_ID, error);
    }

    ASSERT(queryCreate(SELECT_DRIVE_REQUEST_ID));
    if (!queryPrepare(SELECT_DRIVE_REQUEST_ID, SELECT_DRIVE_REQUEST, false, errId, error)) {
        queryFree(SELECT_DRIVE_REQUEST_ID);
        return sqlFail(SELECT_DRIVE_REQUEST_ID, error);
    }

    ASSERT(queryCreate(SELECT_DRIVE_BY_DRIVEID_REQUEST_ID));
    if (!queryPrepare(SELECT_DRIVE_BY_DRIVEID_REQUEST_ID, SELECT_DRIVE_BY_DRIVEID_REQUEST, false, errId, error)) {
        queryFree(SELECT_DRIVE_BY_DRIVEID_REQUEST_ID);
        return sqlFail(SELECT_DRIVE_BY_DRIVEID_REQUEST_ID, error);
    }

    ASSERT(queryCreate(SELECT_ALL_DRIVES_REQUEST_ID));
    if (!queryPrepare(SELECT_ALL_DRIVES_REQUEST_ID, SELECT_ALL_DRIVES_REQUEST, false, errId, error)) {
        queryFree(SELECT_ALL_DRIVES_REQUEST_ID);
        return sqlFail(SELECT_ALL_DRIVES_REQUEST_ID, error);
    }

    ASSERT(queryCreate(SELECT_ALL_DRIVES_BY_ACCOUNT_REQUEST_ID));
    if (!queryPrepare(SELECT_ALL_DRIVES_BY_ACCOUNT_REQUEST_ID, SELECT_ALL_DRIVES_BY_ACCOUNT_REQUEST, false, errId, error)) {
        queryFree(SELECT_ALL_DRIVES_BY_ACCOUNT_REQUEST_ID);
        return sqlFail(SELECT_ALL_DRIVES_BY_ACCOUNT_REQUEST_ID, error);
    }

    // Sync
    ASSERT(queryCreate(INSERT_SYNC_REQUEST_ID));
    if (!queryPrepare(INSERT_SYNC_REQUEST_ID, INSERT_SYNC_REQUEST, false, errId, error)) {
        queryFree(INSERT_SYNC_REQUEST_ID);
        return sqlFail(INSERT_SYNC_REQUEST_ID, error);
    }

    ASSERT(queryCreate(UPDATE_SYNC_REQUEST_ID));
    if (!queryPrepare(UPDATE_SYNC_REQUEST_ID, UPDATE_SYNC_REQUEST, false, errId, error)) {
        queryFree(UPDATE_SYNC_REQUEST_ID);
        return sqlFail(UPDATE_SYNC_REQUEST_ID, error);
    }

    ASSERT(queryCreate(UPDATE_SYNC_PAUSED_REQUEST_ID));
    if (!queryPrepare(UPDATE_SYNC_PAUSED_REQUEST_ID, UPDATE_SYNC_PAUSED_REQUEST, false, errId, error)) {
        queryFree(UPDATE_SYNC_PAUSED_REQUEST_ID);
        return sqlFail(UPDATE_SYNC_PAUSED_REQUEST_ID, error);
    }

    ASSERT(queryCreate(UPDATE_SYNC_HASFULLYCOMPLETED_REQUEST_ID));
    if (!queryPrepare(UPDATE_SYNC_HASFULLYCOMPLETED_REQUEST_ID, UPDATE_SYNC_HASFULLYCOMPLETED_REQUEST, false, errId, error)) {
        queryFree(UPDATE_SYNC_HASFULLYCOMPLETED_REQUEST_ID);
        return sqlFail(UPDATE_SYNC_HASFULLYCOMPLETED_REQUEST_ID, error);
    }

    ASSERT(queryCreate(DELETE_SYNC_REQUEST_ID));
    if (!queryPrepare(DELETE_SYNC_REQUEST_ID, DELETE_SYNC_REQUEST, false, errId, error)) {
        queryFree(DELETE_SYNC_REQUEST_ID);
        return sqlFail(DELETE_SYNC_REQUEST_ID, error);
    }

    ASSERT(queryCreate(SELECT_SYNC_REQUEST_ID));
    if (!queryPrepare(SELECT_SYNC_REQUEST_ID, SELECT_SYNC_REQUEST, false, errId, error)) {
        queryFree(SELECT_SYNC_REQUEST_ID);
        return sqlFail(SELECT_SYNC_REQUEST_ID, error);
    }

    ASSERT(queryCreate(SELECT_ALL_SYNCS_REQUEST_ID));
    if (!queryPrepare(SELECT_ALL_SYNCS_REQUEST_ID, SELECT_ALL_SYNCS_REQUEST, false, errId, error)) {
        queryFree(SELECT_ALL_SYNCS_REQUEST_ID);
        return sqlFail(SELECT_ALL_SYNCS_REQUEST_ID, error);
    }

    ASSERT(queryCreate(SELECT_ALL_SYNCS_BY_DRIVE_REQUEST_ID));
    if (!queryPrepare(SELECT_ALL_SYNCS_BY_DRIVE_REQUEST_ID, SELECT_ALL_SYNCS_BY_DRIVE_REQUEST, false, errId, error)) {
        queryFree(SELECT_ALL_SYNCS_BY_DRIVE_REQUEST_ID);
        return sqlFail(SELECT_ALL_SYNCS_BY_DRIVE_REQUEST_ID, error);
    }

    // Exclusion Template
    ASSERT(queryCreate(INSERT_EXCLUSION_TEMPLATE_REQUEST_ID));
    if (!queryPrepare(INSERT_EXCLUSION_TEMPLATE_REQUEST_ID, INSERT_EXCLUSION_TEMPLATE_REQUEST, false, errId, error)) {
        queryFree(INSERT_EXCLUSION_TEMPLATE_REQUEST_ID);
        return sqlFail(INSERT_EXCLUSION_TEMPLATE_REQUEST_ID, error);
    }

    ASSERT(queryCreate(UPDATE_EXCLUSION_TEMPLATE_REQUEST_ID));
    if (!queryPrepare(UPDATE_EXCLUSION_TEMPLATE_REQUEST_ID, UPDATE_EXCLUSION_TEMPLATE_REQUEST, false, errId, error)) {
        queryFree(UPDATE_EXCLUSION_TEMPLATE_REQUEST_ID);
        return sqlFail(UPDATE_EXCLUSION_TEMPLATE_REQUEST_ID, error);
    }

    ASSERT(queryCreate(DELETE_EXCLUSION_TEMPLATE_REQUEST_ID));
    if (!queryPrepare(DELETE_EXCLUSION_TEMPLATE_REQUEST_ID, DELETE_EXCLUSION_TEMPLATE_REQUEST, false, errId, error)) {
        queryFree(DELETE_EXCLUSION_TEMPLATE_REQUEST_ID);
        return sqlFail(DELETE_EXCLUSION_TEMPLATE_REQUEST_ID, error);
    }

    ASSERT(queryCreate(DELETE_ALL_EXCLUSION_TEMPLATE_BY_DEF_REQUEST_ID));
    if (!queryPrepare(DELETE_ALL_EXCLUSION_TEMPLATE_BY_DEF_REQUEST_ID, DELETE_ALL_EXCLUSION_TEMPLATE_BY_DEF_REQUEST, false, errId,
                      error)) {
        queryFree(DELETE_ALL_EXCLUSION_TEMPLATE_BY_DEF_REQUEST_ID);
        return sqlFail(DELETE_ALL_EXCLUSION_TEMPLATE_BY_DEF_REQUEST_ID, error);
    }

    ASSERT(queryCreate(SELECT_ALL_EXCLUSION_TEMPLATE_REQUEST_ID));
    if (!queryPrepare(SELECT_ALL_EXCLUSION_TEMPLATE_REQUEST_ID, SELECT_ALL_EXCLUSION_TEMPLATE_REQUEST, false, errId, error)) {
        queryFree(SELECT_ALL_EXCLUSION_TEMPLATE_REQUEST_ID);
        return sqlFail(SELECT_ALL_EXCLUSION_TEMPLATE_REQUEST_ID, error);
    }

    ASSERT(queryCreate(SELECT_ALL_EXCLUSION_TEMPLATE_BY_DEF_REQUEST_ID));
    if (!queryPrepare(SELECT_ALL_EXCLUSION_TEMPLATE_BY_DEF_REQUEST_ID, SELECT_ALL_EXCLUSION_TEMPLATE_BY_DEF_REQUEST, false, errId,
                      error)) {
        queryFree(SELECT_ALL_EXCLUSION_TEMPLATE_BY_DEF_REQUEST_ID);
        return sqlFail(SELECT_ALL_EXCLUSION_TEMPLATE_BY_DEF_REQUEST_ID, error);
    }

#ifdef __APPLE__
    // Exclusion App
    ASSERT(queryCreate(INSERT_EXCLUSION_APP_REQUEST_ID));
    if (!queryPrepare(INSERT_EXCLUSION_APP_REQUEST_ID, INSERT_EXCLUSION_APP_REQUEST, false, errId, error)) {
        queryFree(INSERT_EXCLUSION_APP_REQUEST_ID);
        return sqlFail(INSERT_EXCLUSION_APP_REQUEST_ID, error);
    }

    ASSERT(queryCreate(UPDATE_EXCLUSION_APP_REQUEST_ID));
    if (!queryPrepare(UPDATE_EXCLUSION_APP_REQUEST_ID, UPDATE_EXCLUSION_APP_REQUEST, false, errId, error)) {
        queryFree(UPDATE_EXCLUSION_APP_REQUEST_ID);
        return sqlFail(UPDATE_EXCLUSION_APP_REQUEST_ID, error);
    }

    ASSERT(queryCreate(DELETE_EXCLUSION_APP_REQUEST_ID));
    if (!queryPrepare(DELETE_EXCLUSION_APP_REQUEST_ID, DELETE_EXCLUSION_APP_REQUEST, false, errId, error)) {
        queryFree(DELETE_EXCLUSION_APP_REQUEST_ID);
        return sqlFail(DELETE_EXCLUSION_APP_REQUEST_ID, error);
    }

    ASSERT(queryCreate(DELETE_ALL_EXCLUSION_APP_BY_DEF_REQUEST_ID));
    if (!queryPrepare(DELETE_ALL_EXCLUSION_APP_BY_DEF_REQUEST_ID, DELETE_ALL_EXCLUSION_APP_BY_DEF_REQUEST, false, errId, error)) {
        queryFree(DELETE_ALL_EXCLUSION_APP_BY_DEF_REQUEST_ID);
        return sqlFail(DELETE_ALL_EXCLUSION_APP_BY_DEF_REQUEST_ID, error);
    }

    ASSERT(queryCreate(SELECT_ALL_EXCLUSION_APP_REQUEST_ID));
    if (!queryPrepare(SELECT_ALL_EXCLUSION_APP_REQUEST_ID, SELECT_ALL_EXCLUSION_APP_REQUEST, false, errId, error)) {
        queryFree(SELECT_ALL_EXCLUSION_APP_REQUEST_ID);
        return sqlFail(SELECT_ALL_EXCLUSION_APP_REQUEST_ID, error);
    }

    ASSERT(queryCreate(SELECT_ALL_EXCLUSION_APP_BY_DEF_REQUEST_ID));
    if (!queryPrepare(SELECT_ALL_EXCLUSION_APP_BY_DEF_REQUEST_ID, SELECT_ALL_EXCLUSION_APP_BY_DEF_REQUEST, false, errId, error)) {
        queryFree(SELECT_ALL_EXCLUSION_APP_BY_DEF_REQUEST_ID);
        return sqlFail(SELECT_ALL_EXCLUSION_APP_BY_DEF_REQUEST_ID, error);
    }
#endif

    // Error
    ASSERT(queryCreate(INSERT_ERROR_REQUEST_ID));
    if (!queryPrepare(INSERT_ERROR_REQUEST_ID, INSERT_ERROR_REQUEST, false, errId, error)) {
        queryFree(INSERT_ERROR_REQUEST_ID);
        return sqlFail(INSERT_ERROR_REQUEST_ID, error);
    }

    ASSERT(queryCreate(UPDATE_ERROR_REQUEST_ID));
    if (!queryPrepare(UPDATE_ERROR_REQUEST_ID, UPDATE_ERROR_REQUEST, false, errId, error)) {
        queryFree(UPDATE_ERROR_REQUEST_ID);
        return sqlFail(UPDATE_ERROR_REQUEST_ID, error);
    }

    ASSERT(queryCreate(DELETE_ALL_ERROR_BY_EXITCODE_REQUEST_ID));
    if (!queryPrepare(DELETE_ALL_ERROR_BY_EXITCODE_REQUEST_ID, DELETE_ALL_ERROR_BY_EXITCODE_REQUEST, false, errId, error)) {
        queryFree(DELETE_ALL_ERROR_BY_EXITCODE_REQUEST_ID);
        return sqlFail(DELETE_ALL_ERROR_BY_EXITCODE_REQUEST_ID, error);
    }

    ASSERT(queryCreate(DELETE_ALL_ERROR_BY_ExitCauseREQUEST_ID));
    if (!queryPrepare(DELETE_ALL_ERROR_BY_ExitCauseREQUEST_ID, DELETE_ALL_ERROR_BY_ExitCauseREQUEST, false, errId, error)) {
        queryFree(DELETE_ALL_ERROR_BY_ExitCauseREQUEST_ID);
        return sqlFail(DELETE_ALL_ERROR_BY_ExitCauseREQUEST_ID, error);
    }

    ASSERT(queryCreate(DELETE_ALL_ERROR_BY_LEVEL_REQUEST_ID));
    if (!queryPrepare(DELETE_ALL_ERROR_BY_LEVEL_REQUEST_ID, DELETE_ALL_ERROR_BY_LEVEL_REQUEST, false, errId, error)) {
        queryFree(DELETE_ALL_ERROR_BY_LEVEL_REQUEST_ID);
        return sqlFail(DELETE_ALL_ERROR_BY_LEVEL_REQUEST_ID, error);
    }

    ASSERT(queryCreate(DELETE_ERROR_BY_DBID_REQUEST_ID));
    if (!queryPrepare(DELETE_ERROR_BY_DBID_REQUEST_ID, DELETE_ERROR_BY_DBID_REQUEST, false, errId, error)) {
        queryFree(DELETE_ERROR_BY_DBID_REQUEST_ID);
        return sqlFail(DELETE_ERROR_BY_DBID_REQUEST_ID, error);
    }

    ASSERT(queryCreate(SELECT_ALL_ERROR_BY_LEVEL_AND_SYNCDBID_REQUEST_ID));
    if (!queryPrepare(SELECT_ALL_ERROR_BY_LEVEL_AND_SYNCDBID_REQUEST_ID, SELECT_ALL_ERROR_BY_LEVEL_AND_SYNCDBID_REQUEST, false,
                      errId, error)) {
        queryFree(SELECT_ALL_ERROR_BY_LEVEL_AND_SYNCDBID_REQUEST_ID);
        return sqlFail(SELECT_ALL_ERROR_BY_LEVEL_AND_SYNCDBID_REQUEST_ID, error);
    }

    ASSERT(queryCreate(SELECT_ALL_CONFLICTS_BY_SYNCDBID_REQUEST_ID));
    if (!queryPrepare(SELECT_ALL_CONFLICTS_BY_SYNCDBID_REQUEST_ID, SELECT_ALL_CONFLICTS_BY_SYNCDBID_REQUEST, false, errId,
                      error)) {
        queryFree(SELECT_ALL_CONFLICTS_BY_SYNCDBID_REQUEST_ID);
        return sqlFail(SELECT_ALL_CONFLICTS_BY_SYNCDBID_REQUEST_ID, error);
    }

    ASSERT(queryCreate(SELECT_FILTERED_CONFLICTS_BY_SYNCDBID_REQUEST_ID));
    if (!queryPrepare(SELECT_FILTERED_CONFLICTS_BY_SYNCDBID_REQUEST_ID, SELECT_FILTERED_CONFLICTS_BY_SYNCDBID_REQUEST, false,
                      errId, error)) {
        queryFree(SELECT_FILTERED_CONFLICTS_BY_SYNCDBID_REQUEST_ID);
        return sqlFail(SELECT_FILTERED_CONFLICTS_BY_SYNCDBID_REQUEST_ID, error);
    }

    // Migration old selectivesync table
    ASSERT(queryCreate(INSERT_MIGRATION_SELECTIVESYNC_REQUEST_ID));
    if (!queryPrepare(INSERT_MIGRATION_SELECTIVESYNC_REQUEST_ID, INSERT_MIGRATION_SELECTIVESYNC_REQUEST, false, errId, error)) {
        queryFree(INSERT_MIGRATION_SELECTIVESYNC_REQUEST_ID);
        return sqlFail(INSERT_MIGRATION_SELECTIVESYNC_REQUEST_ID, error);
    }

    ASSERT(queryCreate(SELECT_ALL_MIGRATION_SELECTIVESYNC_REQUEST_ID));
    if (!queryPrepare(SELECT_ALL_MIGRATION_SELECTIVESYNC_REQUEST_ID, SELECT_ALL_MIGRATION_SELECTIVESYNC_REQUEST, false, errId,
                      error)) {
        queryFree(SELECT_ALL_MIGRATION_SELECTIVESYNC_REQUEST_ID);
        return sqlFail(SELECT_ALL_MIGRATION_SELECTIVESYNC_REQUEST_ID, error);
    }
    
    // App state
    if (!prepareAppState()) {
        LOG_WARN(_logger, "Error in prepareAppState");
        return false;
    }

    if (!initData()) {
        LOG_WARN(_logger, "Error in initParameters");
        return false;
    }

    return true;
}

bool ParmsDb::upgrade(const std::string &fromVersion, const std::string & /*toVersion*/) {
    int errId;
    std::string error;

    std::string dbFromVersionNumber = CommonUtility::dbVersionNumber(fromVersion);
    if (CommonUtility::isVersionLower(dbFromVersionNumber, "3.4.9")) {
        LOG_DEBUG(_logger, "Upgrade < 3.4.9 DB");

        ASSERT(queryCreate(ALTER_PARAMETERS_ADD_MAX_ALLOWED_CPU_REQUEST_ID));
        if (!queryPrepare(ALTER_PARAMETERS_ADD_MAX_ALLOWED_CPU_REQUEST_ID, ALTER_PARAMETERS_ADD_MAX_ALLOWED_CPU_REQUEST, false,
                          errId, error)) {
            queryFree(ALTER_PARAMETERS_ADD_MAX_ALLOWED_CPU_REQUEST_ID);
            return sqlFail(ALTER_PARAMETERS_ADD_MAX_ALLOWED_CPU_REQUEST_ID, error);
        }
        if (!queryExec(ALTER_PARAMETERS_ADD_MAX_ALLOWED_CPU_REQUEST_ID, errId, error)) {
            queryFree(ALTER_PARAMETERS_ADD_MAX_ALLOWED_CPU_REQUEST_ID);
            return sqlFail(ALTER_PARAMETERS_ADD_MAX_ALLOWED_CPU_REQUEST_ID, error);
        }
        queryFree(ALTER_PARAMETERS_ADD_MAX_ALLOWED_CPU_REQUEST_ID);
    }

    // TODO: Version to update if this code is not delivered in 3.5.8
    if (CommonUtility::isVersionLower(dbFromVersionNumber, "3.5.8")) {
        LOG_DEBUG(_logger, "Upgrade < 3.5.8 DB");

        ASSERT(queryCreate(ALTER_PARAMETERS_ADD_UPLOAD_SESSION_PARALLEL_JOBS_REQUEST_ID));
        if (!queryPrepare(ALTER_PARAMETERS_ADD_UPLOAD_SESSION_PARALLEL_JOBS_REQUEST_ID,
                          ALTER_PARAMETERS_ADD_UPLOAD_SESSION_PARALLEL_JOBS_REQUEST, false, errId, error)) {
            queryFree(ALTER_PARAMETERS_ADD_UPLOAD_SESSION_PARALLEL_JOBS_REQUEST_ID);
            return sqlFail(ALTER_PARAMETERS_ADD_UPLOAD_SESSION_PARALLEL_JOBS_REQUEST_ID, error);
        }
        if (!queryExec(ALTER_PARAMETERS_ADD_UPLOAD_SESSION_PARALLEL_JOBS_REQUEST_ID, errId, error)) {
            queryFree(ALTER_PARAMETERS_ADD_UPLOAD_SESSION_PARALLEL_JOBS_REQUEST_ID);
            return sqlFail(ALTER_PARAMETERS_ADD_UPLOAD_SESSION_PARALLEL_JOBS_REQUEST_ID, error);
        }
        queryFree(ALTER_PARAMETERS_ADD_UPLOAD_SESSION_PARALLEL_JOBS_REQUEST_ID);

        ASSERT(queryCreate(ALTER_PARAMETERS_ADD_JOB_POOL_CAPACITY_FACTOR_REQUEST_ID));
        if (!queryPrepare(ALTER_PARAMETERS_ADD_JOB_POOL_CAPACITY_FACTOR_REQUEST_ID,
                          ALTER_PARAMETERS_ADD_JOB_POOL_CAPACITY_FACTOR_REQUEST, false, errId, error)) {
            queryFree(ALTER_PARAMETERS_ADD_JOB_POOL_CAPACITY_FACTOR_REQUEST_ID);
            return sqlFail(ALTER_PARAMETERS_ADD_JOB_POOL_CAPACITY_FACTOR_REQUEST_ID, error);
        }
        if (!queryExec(ALTER_PARAMETERS_ADD_JOB_POOL_CAPACITY_FACTOR_REQUEST_ID, errId, error)) {
            queryFree(ALTER_PARAMETERS_ADD_JOB_POOL_CAPACITY_FACTOR_REQUEST_ID);
            return sqlFail(ALTER_PARAMETERS_ADD_JOB_POOL_CAPACITY_FACTOR_REQUEST_ID, error);
        }
        queryFree(ALTER_PARAMETERS_ADD_JOB_POOL_CAPACITY_FACTOR_REQUEST_ID);

        ASSERT(queryCreate(UPDATE_PARAMETERS_JOB_REQUEST_ID));
        if (!queryPrepare(UPDATE_PARAMETERS_JOB_REQUEST_ID, UPDATE_PARAMETERS_JOB_REQUEST, false, errId, error)) {
            queryFree(UPDATE_PARAMETERS_JOB_REQUEST_ID);
            return sqlFail(UPDATE_PARAMETERS_JOB_REQUEST_ID, error);
        }
        ASSERT(queryResetAndClearBindings(UPDATE_PARAMETERS_JOB_REQUEST_ID));
        ASSERT(queryBindValue(UPDATE_PARAMETERS_JOB_REQUEST_ID, 1, Parameters::_uploadSessionParallelJobsDefault));
        ASSERT(queryBindValue(UPDATE_PARAMETERS_JOB_REQUEST_ID, 2, Parameters::_jobPoolCapacityFactorDefault));
        if (!queryExec(UPDATE_PARAMETERS_JOB_REQUEST_ID, errId, error)) {
            queryFree(UPDATE_PARAMETERS_JOB_REQUEST_ID);
            return sqlFail(UPDATE_PARAMETERS_JOB_REQUEST_ID, error);
        }
        if (numRowsAffected() != 1) {
            queryFree(UPDATE_PARAMETERS_JOB_REQUEST_ID);
            return false;
        }
        queryFree(UPDATE_PARAMETERS_JOB_REQUEST_ID);
    }

    if (CommonUtility::isVersionLower(dbFromVersionNumber, "3.6.1")) {
        LOG_DEBUG(_logger, "Upgrade < 3.6.1 DB");
        if (!createAppState()) {
            LOG_WARN(_logger, "Error in createAppState");
            return false;
        }
    }

    return true;
}

bool ParmsDb::initData() {
    // Clear Server and SyncPal level errors
    if (!deleteErrors(ErrorLevel::Server)) {
        LOG_WARN(_logger, "Error in clearErrors");
        return false;
    }

    if (!deleteErrors(ErrorLevel::SyncPal)) {
        LOG_WARN(_logger, "Error in clearErrors");
        return false;
    }

    // Insert default values for parameters
    if (!insertDefaultParameters()) {
        LOG_WARN(_logger, "Error in insertDefaultParameters");
        return false;
    }

    if (!insertDefaultAppState()) {
        LOG_WARN(_logger, "Error in insertDefaultAppState");
        return false;
    }


    // Update exclusion templates
    if (!updateExclusionTemplates()) {
        LOG_WARN(_logger, "Error in updateExclusionTemplates");
        return false;
    }

#ifdef __APPLE__
    // Update exclusion apps
    if (!updateExclusionApps()) {
        LOG_WARN(_logger, "Error in updateExclusionApps");
        return false;
    }
#endif

    return true;
}

bool ParmsDb::updateParameters(const Parameters &parameters, bool &found) {
    const std::scoped_lock lock(_mutex);

    int errId;
    std::string error;

    ASSERT(queryResetAndClearBindings(UPDATE_PARAMETERS_REQUEST_ID));
    ASSERT(queryBindValue(UPDATE_PARAMETERS_REQUEST_ID, 1, static_cast<int>(parameters.language())));
    ASSERT(queryBindValue(UPDATE_PARAMETERS_REQUEST_ID, 2, parameters.monoIcons()));
    ASSERT(queryBindValue(UPDATE_PARAMETERS_REQUEST_ID, 3, parameters.autoStart()));
    ASSERT(queryBindValue(UPDATE_PARAMETERS_REQUEST_ID, 4, parameters.moveToTrash()));
    ASSERT(queryBindValue(UPDATE_PARAMETERS_REQUEST_ID, 5, static_cast<int>(parameters.notificationsDisabled())));
    ASSERT(queryBindValue(UPDATE_PARAMETERS_REQUEST_ID, 6, parameters.useLog()));
    ASSERT(queryBindValue(UPDATE_PARAMETERS_REQUEST_ID, 7, static_cast<int>(parameters.logLevel())));
    ASSERT(queryBindValue(UPDATE_PARAMETERS_REQUEST_ID, 8, parameters.purgeOldLogs()));
    ASSERT(queryBindValue(UPDATE_PARAMETERS_REQUEST_ID, 9, parameters.syncHiddenFiles()));
    ASSERT(queryBindValue(UPDATE_PARAMETERS_REQUEST_ID, 10, static_cast<int>(parameters.proxyConfig().type())));
    ASSERT(queryBindValue(UPDATE_PARAMETERS_REQUEST_ID, 11, parameters.proxyConfig().hostName()));
    ASSERT(queryBindValue(UPDATE_PARAMETERS_REQUEST_ID, 12, parameters.proxyConfig().port()));
    ASSERT(queryBindValue(UPDATE_PARAMETERS_REQUEST_ID, 13, parameters.proxyConfig().needsAuth()));
    ASSERT(queryBindValue(UPDATE_PARAMETERS_REQUEST_ID, 14, parameters.proxyConfig().user()));
    ASSERT(queryBindValue(UPDATE_PARAMETERS_REQUEST_ID, 15, parameters.proxyConfig().token()));
    ASSERT(queryBindValue(UPDATE_PARAMETERS_REQUEST_ID, 16, parameters.useBigFolderSizeLimit()));
    ASSERT(queryBindValue(UPDATE_PARAMETERS_REQUEST_ID, 17, parameters.bigFolderSizeLimit()));
    ASSERT(queryBindValue(UPDATE_PARAMETERS_REQUEST_ID, 18, parameters.darkTheme()));
    ASSERT(queryBindValue(UPDATE_PARAMETERS_REQUEST_ID, 19, parameters.showShortcuts()));
    ASSERT(queryBindValue(UPDATE_PARAMETERS_REQUEST_ID, 20, parameters.updateFileAvailable()));
    ASSERT(queryBindValue(UPDATE_PARAMETERS_REQUEST_ID, 21, parameters.updateTargetVersion()));
    ASSERT(queryBindValue(UPDATE_PARAMETERS_REQUEST_ID, 22, parameters.updateTargetVersionString()));
    ASSERT(queryBindValue(UPDATE_PARAMETERS_REQUEST_ID, 23, parameters.autoUpdateAttempted()));
    ASSERT(queryBindValue(UPDATE_PARAMETERS_REQUEST_ID, 24, parameters.seenVersion()));
    ASSERT(queryBindValue(UPDATE_PARAMETERS_REQUEST_ID, 25, parameters.dialogGeometry()));
    ASSERT(queryBindValue(UPDATE_PARAMETERS_REQUEST_ID, 26, static_cast<int>(parameters.extendedLog())));
    ASSERT(queryBindValue(UPDATE_PARAMETERS_REQUEST_ID, 27, parameters.maxAllowedCpu()));
    ASSERT(queryBindValue(UPDATE_PARAMETERS_REQUEST_ID, 28, parameters.uploadSessionParallelJobs()));
    ASSERT(queryBindValue(UPDATE_PARAMETERS_REQUEST_ID, 29, parameters.jobPoolCapacityFactor()));

    if (!queryExec(UPDATE_PARAMETERS_REQUEST_ID, errId, error)) {
        LOG_WARN(_logger, "Error running query: " << UPDATE_PARAMETERS_REQUEST_ID);
        return false;
    }
    if (numRowsAffected() == 1) {
        found = true;
    } else {
        LOG_WARN(_logger, "Error running query: " << UPDATE_PARAMETERS_REQUEST_ID << " - num rows affected != 1");
        found = false;
    }

    return true;
}

bool ParmsDb::selectParameters(Parameters &parameters, bool &found) {
    const std::scoped_lock lock(_mutex);

    ASSERT(queryResetAndClearBindings(SELECT_PARAMETERS_REQUEST_ID));
    if (!queryNext(SELECT_PARAMETERS_REQUEST_ID, found)) {
        LOG_WARN(_logger, "Error getting query result: " << SELECT_PARAMETERS_REQUEST_ID);
        return false;
    }
    if (!found) {
        return true;
    }

    int intResult;
    ASSERT(queryIntValue(SELECT_PARAMETERS_REQUEST_ID, 0, intResult));
    parameters.setLanguage(static_cast<Language>(intResult));

    ASSERT(queryIntValue(SELECT_PARAMETERS_REQUEST_ID, 1, intResult));
    parameters.setMonoIcons(intResult);

    ASSERT(queryIntValue(SELECT_PARAMETERS_REQUEST_ID, 2, intResult));
    parameters.setAutoStart(intResult);

    ASSERT(queryIntValue(SELECT_PARAMETERS_REQUEST_ID, 3, intResult));
    parameters.setMoveToTrash(intResult);

    ASSERT(queryIntValue(SELECT_PARAMETERS_REQUEST_ID, 4, intResult));
    parameters.setNotificationsDisabled(static_cast<NotificationsDisabled>(intResult));

    ASSERT(queryIntValue(SELECT_PARAMETERS_REQUEST_ID, 5, intResult));
    parameters.setUseLog(intResult);

    ASSERT(queryIntValue(SELECT_PARAMETERS_REQUEST_ID, 6, intResult));
    parameters.setLogLevel(static_cast<LogLevel>(intResult));

    ASSERT(queryIntValue(SELECT_PARAMETERS_REQUEST_ID, 7, intResult));
    parameters.setPurgeOldLogs(intResult);

    ASSERT(queryIntValue(SELECT_PARAMETERS_REQUEST_ID, 8, intResult));
    parameters.setSyncHiddenFiles(intResult);

    ProxyType type;
    std::string hostName;
    int port;
    bool needsAuth;
    std::string user;
    std::string token;
    ASSERT(queryIntValue(SELECT_PARAMETERS_REQUEST_ID, 9, intResult));
    type = static_cast<ProxyType>(intResult);
    ASSERT(queryStringValue(SELECT_PARAMETERS_REQUEST_ID, 10, hostName));
    ASSERT(queryIntValue(SELECT_PARAMETERS_REQUEST_ID, 11, port));
    ASSERT(queryIntValue(SELECT_PARAMETERS_REQUEST_ID, 12, intResult));
    needsAuth = static_cast<bool>(intResult);
    ASSERT(queryStringValue(SELECT_PARAMETERS_REQUEST_ID, 13, user));
    ASSERT(queryStringValue(SELECT_PARAMETERS_REQUEST_ID, 14, token));
    parameters.setProxyConfig(ProxyConfig(type, hostName, port, needsAuth, user, token));

    bool useBigFolderSizeLimit;
    ASSERT(queryIntValue(SELECT_PARAMETERS_REQUEST_ID, 15, intResult));
    useBigFolderSizeLimit = static_cast<bool>(intResult);
    parameters.setUseBigFolderSizeLimit(useBigFolderSizeLimit);

    int64_t int64Result;
    ASSERT(queryInt64Value(SELECT_PARAMETERS_REQUEST_ID, 16, int64Result));
    parameters.setBigFolderSizeLimit(int64Result);

    ASSERT(queryIntValue(SELECT_PARAMETERS_REQUEST_ID, 17, intResult));
    parameters.setDarkTheme(intResult);

    ASSERT(queryIntValue(SELECT_PARAMETERS_REQUEST_ID, 18, intResult));
    parameters.setShowShortcuts(intResult);

    std::string strResult;
    ASSERT(queryStringValue(SELECT_PARAMETERS_REQUEST_ID, 19, strResult));
    parameters.setUpdateFileAvailable(strResult);

    ASSERT(queryStringValue(SELECT_PARAMETERS_REQUEST_ID, 20, strResult));
    parameters.setUpdateTargetVersion(strResult);

    ASSERT(queryStringValue(SELECT_PARAMETERS_REQUEST_ID, 21, strResult));
    parameters.setUpdateTargetVersionString(strResult);

    ASSERT(queryIntValue(SELECT_PARAMETERS_REQUEST_ID, 22, intResult));
    parameters.setAutoUpdateAttempted(intResult);

    ASSERT(queryStringValue(SELECT_PARAMETERS_REQUEST_ID, 23, strResult));
    parameters.setSeenVersion(strResult);

    std::shared_ptr<std::vector<char>> blobResult;
    ASSERT(queryBlobValue(SELECT_PARAMETERS_REQUEST_ID, 24, blobResult));
    parameters.setDialogGeometry(blobResult);

    ASSERT(queryIntValue(SELECT_PARAMETERS_REQUEST_ID, 25, intResult));
    parameters.setExtendedLog(intResult);

    ASSERT(queryIntValue(SELECT_PARAMETERS_REQUEST_ID, 26, intResult));
    parameters.setMaxAllowedCpu(intResult);

    ASSERT(queryIntValue(SELECT_PARAMETERS_REQUEST_ID, 27, intResult));
    parameters.setUploadSessionParallelJobs(intResult);

    ASSERT(queryIntValue(SELECT_PARAMETERS_REQUEST_ID, 28, intResult));
    parameters.setJobPoolCapacityFactor(intResult);

    ASSERT(queryResetAndClearBindings(SELECT_PARAMETERS_REQUEST_ID));

    return true;
}

bool ParmsDb::insertUser(const User &user) {
    const std::scoped_lock lock(_mutex);

    int errId;
    std::string error;

    ASSERT(queryResetAndClearBindings(INSERT_USER_REQUEST_ID));
    ASSERT(queryBindValue(INSERT_USER_REQUEST_ID, 1, user.dbId()));
    ASSERT(queryBindValue(INSERT_USER_REQUEST_ID, 2, user.userId()));
    ASSERT(queryBindValue(INSERT_USER_REQUEST_ID, 3, user.keychainKey()));
    ASSERT(queryBindValue(INSERT_USER_REQUEST_ID, 4, user.name()));
    ASSERT(queryBindValue(INSERT_USER_REQUEST_ID, 5, user.email()));
    ASSERT(queryBindValue(INSERT_USER_REQUEST_ID, 6, user.avatarUrl()));
    ASSERT(queryBindValue(INSERT_USER_REQUEST_ID, 7, user.avatar()));
    ASSERT(queryBindValue(INSERT_USER_REQUEST_ID, 8, user.toMigrate()));
    if (!queryExec(INSERT_USER_REQUEST_ID, errId, error)) {
        LOG_WARN(_logger, "Error running query: " << INSERT_USER_REQUEST_ID);
        return false;
    }

    return true;
}

bool ParmsDb::updateUser(const User &user, bool &found) {
    const std::scoped_lock lock(_mutex);

    int errId;
    std::string error;

    ASSERT(queryResetAndClearBindings(UPDATE_USER_REQUEST_ID));
    ASSERT(queryBindValue(UPDATE_USER_REQUEST_ID, 1, user.userId()));
    ASSERT(queryBindValue(UPDATE_USER_REQUEST_ID, 2, user.keychainKey()));
    ASSERT(queryBindValue(UPDATE_USER_REQUEST_ID, 3, user.name()));
    ASSERT(queryBindValue(UPDATE_USER_REQUEST_ID, 4, user.email()));
    ASSERT(queryBindValue(UPDATE_USER_REQUEST_ID, 5, user.avatarUrl()));
    ASSERT(queryBindValue(UPDATE_USER_REQUEST_ID, 6, user.avatar()));
    ASSERT(queryBindValue(UPDATE_USER_REQUEST_ID, 7, user.toMigrate()));
    ASSERT(queryBindValue(UPDATE_USER_REQUEST_ID, 8, user.dbId()));
    if (!queryExec(UPDATE_USER_REQUEST_ID, errId, error)) {
        LOG_WARN(_logger, "Error running query: " << UPDATE_USER_REQUEST_ID);
        return false;
    }
    if (numRowsAffected() == 1) {
        found = true;
    } else {
        LOG_WARN(_logger, "Error running query: " << UPDATE_USER_REQUEST_ID << " - num rows affected != 1");
        found = false;
    }

    return true;
}

bool ParmsDb::deleteUser(int dbId, bool &found) {
    const std::scoped_lock lock(_mutex);

    int errId;
    std::string error;

    ASSERT(queryResetAndClearBindings(DELETE_USER_REQUEST_ID));
    ASSERT(queryBindValue(DELETE_USER_REQUEST_ID, 1, dbId));
    if (!queryExec(DELETE_USER_REQUEST_ID, errId, error)) {
        LOG_WARN(_logger, "Error running query: " << DELETE_USER_REQUEST_ID);
        return false;
    }
    if (numRowsAffected() == 1) {
        found = true;
    } else {
        LOG_WARN(_logger, "Error running query: " << DELETE_USER_REQUEST_ID << " - num rows affected != 1");
        found = false;
    }

    return true;
}

bool ParmsDb::selectUser(int dbId, User &user, bool &found) {
    const std::scoped_lock lock(_mutex);

    ASSERT(queryResetAndClearBindings(SELECT_USER_REQUEST_ID));
    ASSERT(queryBindValue(SELECT_USER_REQUEST_ID, 1, dbId));
    if (!queryNext(SELECT_USER_REQUEST_ID, found)) {
        LOG_WARN(_logger, "Error getting query result: " << SELECT_USER_REQUEST_ID);
        return false;
    }
    if (!found) {
        return true;
    }

    user.setDbId(dbId);

    int intResult;
    ASSERT(queryIntValue(SELECT_USER_REQUEST_ID, 0, intResult));
    user.setUserId(intResult);

    std::string strResult;
    ASSERT(queryStringValue(SELECT_USER_REQUEST_ID, 1, strResult));
    user.setKeychainKey(strResult);

    ASSERT(queryStringValue(SELECT_USER_REQUEST_ID, 2, strResult));
    user.setName(strResult);

    ASSERT(queryStringValue(SELECT_USER_REQUEST_ID, 3, strResult));
    user.setEmail(strResult);

    ASSERT(queryStringValue(SELECT_USER_REQUEST_ID, 4, strResult));
    user.setAvatarUrl(strResult);

    std::shared_ptr<std::vector<char>> blobResult;
    ASSERT(queryBlobValue(SELECT_USER_REQUEST_ID, 5, blobResult));
    user.setAvatar(blobResult);

    ASSERT(queryIntValue(SELECT_USER_REQUEST_ID, 6, intResult));
    user.setToMigrate(static_cast<bool>(intResult));

    ASSERT(queryResetAndClearBindings(SELECT_USER_REQUEST_ID));

    return true;
}

bool ParmsDb::selectUserByUserId(int userId, User &user, bool &found) {
    const std::scoped_lock lock(_mutex);

    ASSERT(queryResetAndClearBindings(SELECT_USER_BY_USERID_REQUEST_ID));
    ASSERT(queryBindValue(SELECT_USER_BY_USERID_REQUEST_ID, 1, userId));
    if (!queryNext(SELECT_USER_BY_USERID_REQUEST_ID, found)) {
        LOG_WARN(_logger, "Error getting query result: " << SELECT_USER_BY_USERID_REQUEST_ID);
        return false;
    }
    if (!found) {
        return true;
    }

    user.setUserId(userId);

    int intResult;
    ASSERT(queryIntValue(SELECT_USER_BY_USERID_REQUEST_ID, 0, intResult));
    user.setDbId(intResult);

    std::string strResult;
    ASSERT(queryStringValue(SELECT_USER_BY_USERID_REQUEST_ID, 1, strResult));
    user.setKeychainKey(strResult);

    ASSERT(queryStringValue(SELECT_USER_BY_USERID_REQUEST_ID, 2, strResult));
    user.setName(strResult);

    ASSERT(queryStringValue(SELECT_USER_BY_USERID_REQUEST_ID, 3, strResult));
    user.setEmail(strResult);

    ASSERT(queryStringValue(SELECT_USER_BY_USERID_REQUEST_ID, 4, strResult));
    user.setAvatarUrl(strResult);

    std::shared_ptr<std::vector<char>> blobResult;
    ASSERT(queryBlobValue(SELECT_USER_BY_USERID_REQUEST_ID, 5, blobResult));
    user.setAvatar(blobResult);

    ASSERT(queryIntValue(SELECT_USER_BY_USERID_REQUEST_ID, 6, intResult));
    user.setToMigrate(static_cast<bool>(intResult));

    ASSERT(queryResetAndClearBindings(SELECT_USER_BY_USERID_REQUEST_ID));

    return true;
}

bool ParmsDb::selectUserFromAccountDbId(int dbId, User &user, bool &found) {
    Account account;
    if (!ParmsDb::instance()->selectAccount(dbId, account, found)) {
        LOG_WARN(_logger, "Error in ParmsDb::selectAccount");
        return false;
    }
    if (!found) {
        LOG_WARN(_logger, "Account not found for accountDbId=" << dbId);
        return false;
    }

    return selectUser(account.userDbId(), user, found);
}

bool ParmsDb::selectUserFromDriveDbId(int dbId, User &user, bool &found) {
    Drive drive;
    if (!ParmsDb::instance()->selectDrive(dbId, drive, found)) {
        LOG_WARN(_logger, "Error in ParmsDb::selectDrive");
        return false;
    }
    if (!found) {
        LOG_WARN(_logger, "Drive not found for driveDbId=" << dbId);
        return false;
    }

    return selectUserFromAccountDbId(drive.accountDbId(), user, found);
}

bool ParmsDb::selectAllUsers(std::vector<User> &userList) {
    const std::scoped_lock lock(_mutex);

    userList.clear();

    ASSERT(queryResetAndClearBindings(SELECT_ALL_USERS_REQUEST_ID));

    bool found;
    for (;;) {
        if (!queryNext(SELECT_ALL_USERS_REQUEST_ID, found)) {
            LOGW_WARN(_logger, L"Error getting query result: " << SELECT_ALL_USERS_REQUEST_ID);
            return false;
        }
        if (!found) {
            break;
        }

        int id;
        ASSERT(queryIntValue(SELECT_ALL_USERS_REQUEST_ID, 0, id));
        int userId;
        ASSERT(queryIntValue(SELECT_ALL_USERS_REQUEST_ID, 1, userId));
        std::string keychainKey;
        ASSERT(queryStringValue(SELECT_ALL_USERS_REQUEST_ID, 2, keychainKey));
        std::string name;
        ASSERT(queryStringValue(SELECT_ALL_USERS_REQUEST_ID, 3, name));
        std::string email;
        ASSERT(queryStringValue(SELECT_ALL_USERS_REQUEST_ID, 4, email));
        std::string avatarUrl;
        ASSERT(queryStringValue(SELECT_ALL_USERS_REQUEST_ID, 5, avatarUrl));
        std::shared_ptr<std::vector<char>> avatar;
        ASSERT(queryBlobValue(SELECT_ALL_USERS_REQUEST_ID, 6, avatar));
        int toMigrate;
        ASSERT(queryIntValue(SELECT_ALL_USERS_REQUEST_ID, 7, toMigrate));

        userList.push_back(User(id, userId, keychainKey, name, email, avatarUrl, avatar, static_cast<bool>(toMigrate)));
    }
    ASSERT(queryResetAndClearBindings(SELECT_ALL_USERS_REQUEST_ID));

    return true;
}

bool ParmsDb::getNewUserDbId(int &dbId) {
    std::vector<User> userList;
    if (!selectAllUsers(userList)) {
        LOG_WARN(_logger, "Error in selectAllUsers");
        return false;
    }

    dbId = 1;
    for (const User &user : userList) {
        // NB: userList is sorted by dbId
        if (user.dbId() > dbId) {
            break;
        }
        dbId++;
    }

    return true;
}

bool ParmsDb::insertAccount(const Account &account) {
    const std::scoped_lock lock(_mutex);

    int errId;
    std::string error;

    ASSERT(queryResetAndClearBindings(INSERT_ACCOUNT_REQUEST_ID));
    ASSERT(queryBindValue(INSERT_ACCOUNT_REQUEST_ID, 1, account.dbId()));
    ASSERT(queryBindValue(INSERT_ACCOUNT_REQUEST_ID, 2, account.accountId()));
    ASSERT(queryBindValue(INSERT_ACCOUNT_REQUEST_ID, 3, account.userDbId()));
    if (!queryExec(INSERT_ACCOUNT_REQUEST_ID, errId, error)) {
        LOG_WARN(_logger, "Error running query: " << INSERT_ACCOUNT_REQUEST_ID);
        return false;
    }

    return true;
}

bool ParmsDb::updateAccount(const Account &account, bool &found) {
    const std::scoped_lock lock(_mutex);

    int errId;
    std::string error;

    ASSERT(queryResetAndClearBindings(UPDATE_ACCOUNT_REQUEST_ID));
    ASSERT(queryBindValue(UPDATE_ACCOUNT_REQUEST_ID, 1, account.accountId()));
    ASSERT(queryBindValue(UPDATE_ACCOUNT_REQUEST_ID, 2, account.userDbId()));
    ASSERT(queryBindValue(UPDATE_ACCOUNT_REQUEST_ID, 3, account.dbId()));
    if (!queryExec(UPDATE_ACCOUNT_REQUEST_ID, errId, error)) {
        LOG_WARN(_logger, "Error running query: " << UPDATE_ACCOUNT_REQUEST_ID);
        return false;
    }
    if (numRowsAffected() == 1) {
        found = true;
    } else {
        LOG_WARN(_logger, "Error running query: " << UPDATE_ACCOUNT_REQUEST_ID << " - num rows affected != 1");
        found = false;
    }

    return true;
}

bool ParmsDb::deleteAccount(int dbId, bool &found) {
    const std::scoped_lock lock(_mutex);

    int errId;
    std::string error;

    ASSERT(queryResetAndClearBindings(DELETE_ACCOUNT_REQUEST_ID));
    ASSERT(queryBindValue(DELETE_ACCOUNT_REQUEST_ID, 1, dbId));
    if (!queryExec(DELETE_ACCOUNT_REQUEST_ID, errId, error)) {
        LOG_WARN(_logger, "Error running query: " << DELETE_ACCOUNT_REQUEST_ID);
        return false;
    }
    if (numRowsAffected() == 1) {
        found = true;
    } else {
        LOG_WARN(_logger, "Error running query: " << DELETE_ACCOUNT_REQUEST_ID << " - num rows affected != 1");
        found = false;
    }

    return true;
}

bool ParmsDb::selectAccount(int dbId, Account &account, bool &found) {
    const std::scoped_lock lock(_mutex);

    ASSERT(queryResetAndClearBindings(SELECT_ACCOUNT_REQUEST_ID));
    ASSERT(queryBindValue(SELECT_ACCOUNT_REQUEST_ID, 1, dbId));
    if (!queryNext(SELECT_ACCOUNT_REQUEST_ID, found)) {
        LOG_WARN(_logger, "Error getting query result: " << SELECT_ACCOUNT_REQUEST_ID);
        return false;
    }
    if (!found) {
        return true;
    }

    account.setDbId(dbId);

    int intResult;
    ASSERT(queryIntValue(SELECT_ACCOUNT_REQUEST_ID, 0, intResult));
    account.setAccountId(intResult);

    ASSERT(queryIntValue(SELECT_ACCOUNT_REQUEST_ID, 1, intResult));
    account.setUserDbId(intResult);

    ASSERT(queryResetAndClearBindings(SELECT_ACCOUNT_REQUEST_ID));

    return true;
}

bool ParmsDb::selectAllAccounts(std::vector<Account> &accountList) {
    const std::scoped_lock lock(_mutex);

    accountList.clear();

    ASSERT(queryResetAndClearBindings(SELECT_ALL_ACCOUNTS_REQUEST_ID));

    bool found;
    for (;;) {
        if (!queryNext(SELECT_ALL_ACCOUNTS_REQUEST_ID, found)) {
            LOGW_WARN(_logger, L"Error getting query result: " << SELECT_ALL_ACCOUNTS_REQUEST_ID);
            return false;
        }
        if (!found) {
            break;
        }

        int id;
        ASSERT(queryIntValue(SELECT_ALL_ACCOUNTS_REQUEST_ID, 0, id));
        int accountId;
        ASSERT(queryIntValue(SELECT_ALL_ACCOUNTS_REQUEST_ID, 1, accountId));
        int userDbId;
        ASSERT(queryIntValue(SELECT_ALL_ACCOUNTS_REQUEST_ID, 2, userDbId));

        accountList.push_back(Account(id, accountId, userDbId));
    }
    ASSERT(queryResetAndClearBindings(SELECT_ALL_ACCOUNTS_REQUEST_ID));

    return true;
}

bool ParmsDb::selectAllAccounts(int userDbId, std::vector<Account> &accountList) {
    const std::scoped_lock lock(_mutex);

    accountList.clear();

    ASSERT(queryResetAndClearBindings(SELECT_ALL_ACCOUNTS_BY_USER_REQUEST_ID));
    ASSERT(queryBindValue(SELECT_ALL_ACCOUNTS_BY_USER_REQUEST_ID, 1, userDbId));

    bool found;
    for (;;) {
        if (!queryNext(SELECT_ALL_ACCOUNTS_BY_USER_REQUEST_ID, found)) {
            LOGW_WARN(_logger, L"Error getting query result: " << SELECT_ALL_ACCOUNTS_BY_USER_REQUEST_ID);
            return false;
        }
        if (!found) {
            break;
        }

        int id;
        ASSERT(queryIntValue(SELECT_ALL_ACCOUNTS_BY_USER_REQUEST_ID, 0, id));
        int accountId;
        ASSERT(queryIntValue(SELECT_ALL_ACCOUNTS_BY_USER_REQUEST_ID, 1, accountId));

        accountList.push_back(Account(id, accountId, userDbId));
    }
    ASSERT(queryResetAndClearBindings(SELECT_ALL_ACCOUNTS_BY_USER_REQUEST_ID));

    return true;
}

bool ParmsDb::accountDbId(int userDbId, int accountId, int &dbId) {
    std::vector<Account> accountList;
    if (!selectAllAccounts(userDbId, accountList)) {
        LOG_WARN(_logger, "Error in selectAllAccounts");
        return false;
    }

    dbId = 0;
    for (const Account &account : accountList) {
        if (account.accountId() == accountId) {
            dbId = account.dbId();
            break;
        }
    }

    return true;
}

bool ParmsDb::getNewAccountDbId(int &dbId) {
    std::vector<Account> accountList;
    if (!selectAllAccounts(accountList)) {
        LOG_WARN(_logger, "Error in selectAllAccounts");
        return false;
    }

    dbId = 1;
    for (const Account &account : accountList) {
        // NB: accountList is sorted by dbId
        if (account.dbId() > dbId) {
            break;
        }
        dbId++;
    }

    return true;
}

bool ParmsDb::insertDrive(const Drive &drive) {
    const std::scoped_lock lock(_mutex);

    int errId;
    std::string error;

    ASSERT(queryResetAndClearBindings(INSERT_DRIVE_REQUEST_ID));
    ASSERT(queryBindValue(INSERT_DRIVE_REQUEST_ID, 1, drive.dbId()));
    ASSERT(queryBindValue(INSERT_DRIVE_REQUEST_ID, 2, drive.driveId()));
    ASSERT(queryBindValue(INSERT_DRIVE_REQUEST_ID, 3, drive.accountDbId()));
    ASSERT(queryBindValue(INSERT_DRIVE_REQUEST_ID, 4, drive.name()));
    ASSERT(queryBindValue(INSERT_DRIVE_REQUEST_ID, 5, drive.size()));
    ASSERT(queryBindValue(INSERT_DRIVE_REQUEST_ID, 6, drive.color()));
    ASSERT(queryBindValue(INSERT_DRIVE_REQUEST_ID, 7, drive.notifications()));
    ASSERT(queryBindValue(INSERT_DRIVE_REQUEST_ID, 8, drive.admin()));
    if (!queryExec(INSERT_DRIVE_REQUEST_ID, errId, error)) {
        LOG_WARN(_logger, "Error running query: " << INSERT_DRIVE_REQUEST_ID);
        return false;
    }

    return true;
}

bool ParmsDb::updateDrive(const Drive &drive, bool &found) {
    const std::scoped_lock lock(_mutex);

    int errId;
    std::string error;

    ASSERT(queryResetAndClearBindings(UPDATE_DRIVE_REQUEST_ID));
    ASSERT(queryBindValue(UPDATE_DRIVE_REQUEST_ID, 1, drive.driveId()));
    ASSERT(queryBindValue(UPDATE_DRIVE_REQUEST_ID, 2, drive.accountDbId()));
    ASSERT(queryBindValue(UPDATE_DRIVE_REQUEST_ID, 3, drive.name()));
    ASSERT(queryBindValue(UPDATE_DRIVE_REQUEST_ID, 4, drive.size()));
    ASSERT(queryBindValue(UPDATE_DRIVE_REQUEST_ID, 5, drive.color()));
    ASSERT(queryBindValue(UPDATE_DRIVE_REQUEST_ID, 6, drive.notifications()));
    ASSERT(queryBindValue(UPDATE_DRIVE_REQUEST_ID, 7, drive.admin()));
    ASSERT(queryBindValue(UPDATE_DRIVE_REQUEST_ID, 8, drive.dbId()));
    if (!queryExec(UPDATE_DRIVE_REQUEST_ID, errId, error)) {
        LOG_WARN(_logger, "Error running query: " << UPDATE_DRIVE_REQUEST_ID);
        return false;
    }
    if (numRowsAffected() == 1) {
        found = true;
    } else {
        LOG_WARN(_logger, "Error running query: " << UPDATE_DRIVE_REQUEST_ID << " - num rows affected != 1");
        found = false;
    }

    return true;
}

bool ParmsDb::deleteDrive(int dbId, bool &found) {
    const std::scoped_lock lock(_mutex);

    int errId;
    std::string error;

    ASSERT(queryResetAndClearBindings(DELETE_DRIVE_REQUEST_ID));
    ASSERT(queryBindValue(DELETE_DRIVE_REQUEST_ID, 1, dbId));
    if (!queryExec(DELETE_DRIVE_REQUEST_ID, errId, error)) {
        LOG_WARN(_logger, "Error running query: " << DELETE_DRIVE_REQUEST_ID);
        return false;
    }
    if (numRowsAffected() == 1) {
        found = true;
    } else {
        LOG_WARN(_logger, "Error running query: " << DELETE_DRIVE_REQUEST_ID << " - num rows affected != 1");
        found = false;
    }

    return true;
}

bool ParmsDb::selectDrive(int dbId, Drive &drive, bool &found) {
    const std::scoped_lock lock(_mutex);

    ASSERT(queryResetAndClearBindings(SELECT_DRIVE_REQUEST_ID));
    ASSERT(queryBindValue(SELECT_DRIVE_REQUEST_ID, 1, dbId));
    if (!queryNext(SELECT_DRIVE_REQUEST_ID, found)) {
        LOG_WARN(_logger, "Error getting query result: " << SELECT_DRIVE_REQUEST_ID);
        return false;
    }
    if (!found) {
        return true;
    }

    drive.setDbId(dbId);

    int intResult;
    ASSERT(queryIntValue(SELECT_DRIVE_REQUEST_ID, 0, intResult));
    drive.setDriveId(intResult);

    ASSERT(queryIntValue(SELECT_DRIVE_REQUEST_ID, 1, intResult));
    drive.setAccountDbId(intResult);

    std::string strResult;
    ASSERT(queryStringValue(SELECT_DRIVE_REQUEST_ID, 2, strResult));
    drive.setName(strResult);

    int64_t int64Result;
    ASSERT(queryInt64Value(SELECT_DRIVE_REQUEST_ID, 3, int64Result));
    drive.setSize(int64Result);

    ASSERT(queryStringValue(SELECT_DRIVE_REQUEST_ID, 4, strResult));
    drive.setColor(strResult);

    ASSERT(queryIntValue(SELECT_DRIVE_REQUEST_ID, 5, intResult));
    drive.setNotifications(static_cast<bool>(intResult));

    ASSERT(queryResetAndClearBindings(SELECT_DRIVE_REQUEST_ID));

    return true;
}

bool ParmsDb::selectDriveByDriveId(int driveId, Drive &drive, bool &found) {
    const std::scoped_lock lock(_mutex);

    ASSERT(queryResetAndClearBindings(SELECT_DRIVE_BY_DRIVEID_REQUEST_ID));
    ASSERT(queryBindValue(SELECT_DRIVE_BY_DRIVEID_REQUEST_ID, 1, driveId));
    if (!queryNext(SELECT_DRIVE_BY_DRIVEID_REQUEST_ID, found)) {
        LOG_WARN(_logger, "Error getting query result: " << SELECT_DRIVE_BY_DRIVEID_REQUEST_ID);
        return false;
    }
    if (!found) {
        return true;
    }

    drive.setDriveId(driveId);

    int intResult;
    ASSERT(queryIntValue(SELECT_DRIVE_BY_DRIVEID_REQUEST_ID, 0, intResult));
    drive.setDbId(intResult);

    ASSERT(queryIntValue(SELECT_DRIVE_BY_DRIVEID_REQUEST_ID, 1, intResult));
    drive.setAccountDbId(intResult);

    std::string strResult;
    ASSERT(queryStringValue(SELECT_DRIVE_BY_DRIVEID_REQUEST_ID, 2, strResult));
    drive.setName(strResult);

    int64_t int64Result;
    ASSERT(queryInt64Value(SELECT_DRIVE_BY_DRIVEID_REQUEST_ID, 3, int64Result));
    drive.setSize(int64Result);

    ASSERT(queryStringValue(SELECT_DRIVE_BY_DRIVEID_REQUEST_ID, 4, strResult));
    drive.setColor(strResult);

    ASSERT(queryIntValue(SELECT_DRIVE_BY_DRIVEID_REQUEST_ID, 5, intResult));
    drive.setNotifications(static_cast<bool>(intResult));

    ASSERT(queryResetAndClearBindings(SELECT_DRIVE_BY_DRIVEID_REQUEST_ID));

    return true;
}

bool ParmsDb::selectAllDrives(std::vector<Drive> &driveList) {
    const std::scoped_lock lock(_mutex);

    driveList.clear();

    ASSERT(queryResetAndClearBindings(SELECT_ALL_DRIVES_REQUEST_ID));

    bool found;
    for (;;) {
        if (!queryNext(SELECT_ALL_DRIVES_REQUEST_ID, found)) {
            LOG_WARN(_logger, "Error getting query result: " << SELECT_ALL_DRIVES_REQUEST_ID);
            return false;
        }
        if (!found) {
            break;
        }

        int id;
        ASSERT(queryIntValue(SELECT_ALL_DRIVES_REQUEST_ID, 0, id));
        int driveId;
        ASSERT(queryIntValue(SELECT_ALL_DRIVES_REQUEST_ID, 1, driveId));
        int accountDbId;
        ASSERT(queryIntValue(SELECT_ALL_DRIVES_REQUEST_ID, 2, accountDbId));
        std::string driveName;
        ASSERT(queryStringValue(SELECT_ALL_DRIVES_REQUEST_ID, 3, driveName));
        int64_t size;
        ASSERT(queryInt64Value(SELECT_ALL_DRIVES_REQUEST_ID, 4, size));
        std::string color;
        ASSERT(queryStringValue(SELECT_ALL_DRIVES_REQUEST_ID, 5, color));
        int notifications;
        ASSERT(queryIntValue(SELECT_ALL_DRIVES_REQUEST_ID, 6, notifications));
        int admin;
        ASSERT(queryIntValue(SELECT_ALL_DRIVES_REQUEST_ID, 7, admin));

        driveList.push_back(
            Drive(id, driveId, accountDbId, driveName, size, color, static_cast<bool>(notifications), static_cast<bool>(admin)));
    }
    ASSERT(queryResetAndClearBindings(SELECT_ALL_DRIVES_REQUEST_ID));

    return true;
}

bool ParmsDb::selectAllDrives(int accountDbId, std::vector<Drive> &driveList) {
    const std::scoped_lock lock(_mutex);

    driveList.clear();

    ASSERT(queryResetAndClearBindings(SELECT_ALL_DRIVES_BY_ACCOUNT_REQUEST_ID));
    ASSERT(queryBindValue(SELECT_ALL_DRIVES_BY_ACCOUNT_REQUEST_ID, 1, accountDbId));

    bool found;
    for (;;) {
        if (!queryNext(SELECT_ALL_DRIVES_BY_ACCOUNT_REQUEST_ID, found)) {
            LOG_WARN(_logger, "Error getting query result: " << SELECT_ALL_DRIVES_BY_ACCOUNT_REQUEST_ID);
            return false;
        }
        if (!found) {
            break;
        }

        int id;
        ASSERT(queryIntValue(SELECT_ALL_DRIVES_BY_ACCOUNT_REQUEST_ID, 0, id));
        int driveId;
        ASSERT(queryIntValue(SELECT_ALL_DRIVES_BY_ACCOUNT_REQUEST_ID, 1, driveId));
        std::string driveName;
        ASSERT(queryStringValue(SELECT_ALL_DRIVES_BY_ACCOUNT_REQUEST_ID, 2, driveName));
        int64_t size;
        ASSERT(queryInt64Value(SELECT_ALL_DRIVES_BY_ACCOUNT_REQUEST_ID, 3, size));
        std::string color;
        ASSERT(queryStringValue(SELECT_ALL_DRIVES_BY_ACCOUNT_REQUEST_ID, 4, color));
        int notifications;
        ASSERT(queryIntValue(SELECT_ALL_DRIVES_BY_ACCOUNT_REQUEST_ID, 5, notifications));
        int admin;
        ASSERT(queryIntValue(SELECT_ALL_DRIVES_BY_ACCOUNT_REQUEST_ID, 6, admin));

        driveList.push_back(
            Drive(id, driveId, accountDbId, driveName, size, color, static_cast<bool>(notifications), static_cast<bool>(admin)));
    }
    ASSERT(queryResetAndClearBindings(SELECT_ALL_DRIVES_BY_ACCOUNT_REQUEST_ID));

    return true;
}

bool ParmsDb::driveDbId(int accountDbId, int driveId, int &dbId) {
    std::vector<Drive> driveList;
    if (!selectAllDrives(accountDbId, driveList)) {
        LOG_WARN(_logger, "Error in selectAllDrives");
        return false;
    }

    dbId = 0;
    for (const Drive &drive : driveList) {
        if (drive.driveId() == driveId) {
            dbId = drive.dbId();
            break;
        }
    }

    return true;
}

bool ParmsDb::getNewDriveDbId(int &dbId) {
    std::vector<Drive> driveList;
    if (!selectAllDrives(driveList)) {
        LOG_WARN(_logger, "Error in selectAllDrives");
        return false;
    }

    dbId = 1;
    for (const Drive &drive : driveList) {
        // NB: driveList is sorted by dbId
        if (drive.dbId() > dbId) {
            break;
        }
        dbId++;
    }

    return true;
}

bool ParmsDb::insertSync(const Sync &sync) {
    const std::scoped_lock lock(_mutex);

    std::string listingCursor;
    int64_t listingCursorTimestamp;
    sync.listingCursor(listingCursor, listingCursorTimestamp);

    // Insert sync record
    int errId;
    std::string error;

    ASSERT(queryResetAndClearBindings(INSERT_SYNC_REQUEST_ID));
    ASSERT(queryBindValue(INSERT_SYNC_REQUEST_ID, 1, sync.dbId()));
    ASSERT(queryBindValue(INSERT_SYNC_REQUEST_ID, 2, sync.driveDbId()));
    ASSERT(queryBindValue(INSERT_SYNC_REQUEST_ID, 3, sync.localPath().native()));
    ASSERT(queryBindValue(INSERT_SYNC_REQUEST_ID, 4, sync.targetPath().native()));
    ASSERT(queryBindValue(INSERT_SYNC_REQUEST_ID, 5, sync.targetNodeId()));
    ASSERT(queryBindValue(INSERT_SYNC_REQUEST_ID, 6, sync.dbPath().native()));
    ASSERT(queryBindValue(INSERT_SYNC_REQUEST_ID, 7, static_cast<int>(sync.paused())));
    ASSERT(queryBindValue(INSERT_SYNC_REQUEST_ID, 8, static_cast<int>(sync.supportVfs())));
    ASSERT(queryBindValue(INSERT_SYNC_REQUEST_ID, 9, static_cast<int>(sync.virtualFileMode())));
    ASSERT(queryBindValue(INSERT_SYNC_REQUEST_ID, 10, static_cast<int>(sync.notificationsDisabled())));
    ASSERT(queryBindValue(INSERT_SYNC_REQUEST_ID, 11, static_cast<int>(sync.hasFullyCompleted())));
    ASSERT(queryBindValue(INSERT_SYNC_REQUEST_ID, 12, sync.navigationPaneClsid()));
    ASSERT(queryBindValue(INSERT_SYNC_REQUEST_ID, 13, listingCursor));
    ASSERT(queryBindValue(INSERT_SYNC_REQUEST_ID, 14, listingCursorTimestamp));
    if (!queryExec(INSERT_SYNC_REQUEST_ID, errId, error)) {
        LOG_WARN(_logger, "Error running query: " << INSERT_SYNC_REQUEST_ID);
        return false;
    }

    return true;
}

bool ParmsDb::updateSync(const Sync &sync, bool &found) {
    const std::scoped_lock lock(_mutex);

    std::string listingCursor;
    int64_t listingCursorTimestamp;
    sync.listingCursor(listingCursor, listingCursorTimestamp);

    int errId;
    std::string error;

    ASSERT(queryResetAndClearBindings(UPDATE_SYNC_REQUEST_ID));
    ASSERT(queryBindValue(UPDATE_SYNC_REQUEST_ID, 1, sync.driveDbId()));
    ASSERT(queryBindValue(UPDATE_SYNC_REQUEST_ID, 2, sync.localPath().native()));
    ASSERT(queryBindValue(UPDATE_SYNC_REQUEST_ID, 3, sync.targetPath().native()));
    ASSERT(queryBindValue(UPDATE_SYNC_REQUEST_ID, 4, sync.targetNodeId()));
    ASSERT(queryBindValue(UPDATE_SYNC_REQUEST_ID, 5, sync.dbPath().native()));
    ASSERT(queryBindValue(UPDATE_SYNC_REQUEST_ID, 6, static_cast<int>(sync.paused())));
    ASSERT(queryBindValue(UPDATE_SYNC_REQUEST_ID, 7, static_cast<int>(sync.supportVfs())));
    ASSERT(queryBindValue(UPDATE_SYNC_REQUEST_ID, 8, static_cast<int>(sync.virtualFileMode())));
    ASSERT(queryBindValue(UPDATE_SYNC_REQUEST_ID, 9, static_cast<int>(sync.notificationsDisabled())));
    ASSERT(queryBindValue(UPDATE_SYNC_REQUEST_ID, 10, static_cast<int>(sync.hasFullyCompleted())));
    ASSERT(queryBindValue(UPDATE_SYNC_REQUEST_ID, 11, sync.navigationPaneClsid()));
    ASSERT(queryBindValue(UPDATE_SYNC_REQUEST_ID, 12, listingCursor));
    ASSERT(queryBindValue(UPDATE_SYNC_REQUEST_ID, 13, listingCursorTimestamp));
    ASSERT(queryBindValue(UPDATE_SYNC_REQUEST_ID, 14, sync.dbId()));
    if (!queryExec(UPDATE_SYNC_REQUEST_ID, errId, error)) {
        LOG_WARN(_logger, "Error running query: " << UPDATE_SYNC_REQUEST_ID);
        return false;
    }
    if (numRowsAffected() == 1) {
        found = true;
    } else {
        LOG_WARN(_logger, "Error running query: " << UPDATE_SYNC_REQUEST_ID << " - num rows affected != 1");
        found = false;
    }

    return true;
}

bool ParmsDb::setSyncPaused(int dbId, bool value, bool &found) {
    const std::scoped_lock lock(_mutex);

    int errId;
    std::string error;

    ASSERT(queryResetAndClearBindings(UPDATE_SYNC_PAUSED_REQUEST_ID));
    ASSERT(queryBindValue(UPDATE_SYNC_PAUSED_REQUEST_ID, 1, value));
    ASSERT(queryBindValue(UPDATE_SYNC_PAUSED_REQUEST_ID, 2, dbId));
    if (!queryExec(UPDATE_SYNC_PAUSED_REQUEST_ID, errId, error)) {
        LOG_WARN(_logger, "Error running query: " << UPDATE_SYNC_PAUSED_REQUEST_ID);
        return false;
    }
    if (numRowsAffected() == 1) {
        found = true;
    } else {
        LOG_WARN(_logger, "Error running query: " << UPDATE_SYNC_PAUSED_REQUEST_ID << " - num rows affected != 1");
        found = false;
    }

    return true;
}

bool ParmsDb::setSyncHasFullyCompleted(int dbId, bool value, bool &found) {
    const std::scoped_lock lock(_mutex);

    int errId;
    std::string error;

    ASSERT(queryResetAndClearBindings(UPDATE_SYNC_HASFULLYCOMPLETED_REQUEST_ID));
    ASSERT(queryBindValue(UPDATE_SYNC_HASFULLYCOMPLETED_REQUEST_ID, 1, value));
    ASSERT(queryBindValue(UPDATE_SYNC_HASFULLYCOMPLETED_REQUEST_ID, 2, dbId));
    if (!queryExec(UPDATE_SYNC_HASFULLYCOMPLETED_REQUEST_ID, errId, error)) {
        LOG_WARN(_logger, "Error running query: " << UPDATE_SYNC_HASFULLYCOMPLETED_REQUEST_ID);
        return false;
    }
    if (numRowsAffected() == 1) {
        found = true;
    } else {
        LOG_WARN(_logger, "Error running query: " << UPDATE_SYNC_HASFULLYCOMPLETED_REQUEST_ID << " - num rows affected != 1");
        found = false;
    }

    return true;
}

bool ParmsDb::deleteSync(int dbId, bool &found) {
    const std::scoped_lock lock(_mutex);

    int errId;
    std::string error;

    ASSERT(queryResetAndClearBindings(DELETE_SYNC_REQUEST_ID));
    ASSERT(queryBindValue(DELETE_SYNC_REQUEST_ID, 1, dbId));
    if (!queryExec(DELETE_SYNC_REQUEST_ID, errId, error)) {
        LOG_WARN(_logger, "Error running query: " << DELETE_SYNC_REQUEST_ID);
        return false;
    }
    if (numRowsAffected() == 1) {
        found = true;
    } else {
        LOG_WARN(_logger, "Error running query: " << DELETE_SYNC_REQUEST_ID << " - num rows affected != 1");
        found = false;
    }

    return true;
}

bool ParmsDb::selectSync(int dbId, Sync &sync, bool &found) {
    const std::scoped_lock lock(_mutex);

    ASSERT(queryResetAndClearBindings(SELECT_SYNC_REQUEST_ID));
    ASSERT(queryBindValue(SELECT_SYNC_REQUEST_ID, 1, dbId));
    if (!queryNext(SELECT_SYNC_REQUEST_ID, found)) {
        LOG_WARN(_logger, "Error getting query result: " << SELECT_SYNC_REQUEST_ID);
        return false;
    }
    if (!found) {
        return true;
    }

    sync.setDbId(dbId);

    int intResult;
    ASSERT(queryIntValue(SELECT_SYNC_REQUEST_ID, 0, intResult));
    sync.setDriveDbId(intResult);

    SyncName syncNameResult;
    ASSERT(querySyncNameValue(SELECT_SYNC_REQUEST_ID, 1, syncNameResult));
    sync.setLocalPath(SyncPath(syncNameResult));

    ASSERT(querySyncNameValue(SELECT_SYNC_REQUEST_ID, 2, syncNameResult));
    sync.setTargetPath(SyncPath(syncNameResult));

    std::string strResult;
    ASSERT(queryStringValue(SELECT_SYNC_REQUEST_ID, 3, strResult));
    sync.setTargetNodeId(strResult);

    ASSERT(querySyncNameValue(SELECT_SYNC_REQUEST_ID, 4, syncNameResult));
    sync.setDbPath(SyncPath(syncNameResult));

    ASSERT(queryIntValue(SELECT_SYNC_REQUEST_ID, 5, intResult));
    sync.setPaused(static_cast<bool>(intResult));

    ASSERT(queryIntValue(SELECT_SYNC_REQUEST_ID, 6, intResult));
    sync.setSupportVfs(static_cast<bool>(intResult));

    ASSERT(queryIntValue(SELECT_SYNC_REQUEST_ID, 7, intResult));
    sync.setVirtualFileMode(static_cast<VirtualFileMode>(intResult));

    ASSERT(queryIntValue(SELECT_SYNC_REQUEST_ID, 8, intResult));
    sync.setNotificationsDisabled(static_cast<bool>(intResult));

    ASSERT(queryIntValue(SELECT_SYNC_REQUEST_ID, 9, intResult));
    sync.setHasFullyCompleted(static_cast<bool>(intResult));

    ASSERT(queryStringValue(SELECT_SYNC_REQUEST_ID, 10, strResult));
    sync.setNavigationPaneClsid(strResult);

    ASSERT(queryStringValue(SELECT_SYNC_REQUEST_ID, 11, strResult));

    int64_t int64Result;
    ASSERT(queryInt64Value(SELECT_SYNC_REQUEST_ID, 12, int64Result));
    sync.setListingCursor(strResult, int64Result);

    ASSERT(queryResetAndClearBindings(SELECT_SYNC_REQUEST_ID));

    return true;
}

bool ParmsDb::selectAllSyncs(std::vector<Sync> &syncList) {
    const std::scoped_lock lock(_mutex);

    syncList.clear();

    ASSERT(queryResetAndClearBindings(SELECT_ALL_SYNCS_REQUEST_ID));
    bool found;
    for (;;) {
        if (!queryNext(SELECT_ALL_SYNCS_REQUEST_ID, found)) {
            LOG_WARN(_logger, "Error getting query result: " << SELECT_ALL_SYNCS_REQUEST_ID);
            return false;
        }
        if (!found) {
            break;
        }

        int id;
        ASSERT(queryIntValue(SELECT_ALL_SYNCS_REQUEST_ID, 0, id));
        int driveDbId;
        ASSERT(queryIntValue(SELECT_ALL_SYNCS_REQUEST_ID, 1, driveDbId));
        SyncName localPath;
        ASSERT(querySyncNameValue(SELECT_ALL_SYNCS_REQUEST_ID, 2, localPath));
        SyncName targetPath;
        ASSERT(querySyncNameValue(SELECT_ALL_SYNCS_REQUEST_ID, 3, targetPath));
        std::string targetNodeId;
        ASSERT(queryStringValue(SELECT_ALL_SYNCS_REQUEST_ID, 4, targetNodeId));
        SyncName dbPath;
        ASSERT(querySyncNameValue(SELECT_ALL_SYNCS_REQUEST_ID, 5, dbPath));
        int paused;
        ASSERT(queryIntValue(SELECT_ALL_SYNCS_REQUEST_ID, 6, paused));
        int supportVfs;
        ASSERT(queryIntValue(SELECT_ALL_SYNCS_REQUEST_ID, 7, supportVfs));
        int virtualFileMode;
        ASSERT(queryIntValue(SELECT_ALL_SYNCS_REQUEST_ID, 8, virtualFileMode));
        int notificationsDisabled;
        ASSERT(queryIntValue(SELECT_ALL_SYNCS_REQUEST_ID, 9, notificationsDisabled));
        int hasFullyCompleted;
        ASSERT(queryIntValue(SELECT_ALL_SYNCS_REQUEST_ID, 10, hasFullyCompleted));
        std::string navigationPaneClsid;
        ASSERT(queryStringValue(SELECT_ALL_SYNCS_REQUEST_ID, 11, navigationPaneClsid));
        std::string listingCursor;
        ASSERT(queryStringValue(SELECT_ALL_SYNCS_REQUEST_ID, 12, listingCursor));
        int64_t listingCursorTimestamp;
        ASSERT(queryInt64Value(SELECT_ALL_SYNCS_REQUEST_ID, 13, listingCursorTimestamp));

        syncList.push_back(Sync(id, driveDbId, SyncPath(localPath), SyncPath(targetPath), targetNodeId, static_cast<bool>(paused),
                                static_cast<bool>(supportVfs), static_cast<VirtualFileMode>(virtualFileMode),
                                static_cast<bool>(notificationsDisabled), SyncPath(dbPath), static_cast<bool>(hasFullyCompleted),
                                navigationPaneClsid, listingCursor, listingCursorTimestamp));
    }
    ASSERT(queryResetAndClearBindings(SELECT_ALL_SYNCS_REQUEST_ID));

    return true;
}

bool ParmsDb::selectAllSyncs(int driveDbId, std::vector<Sync> &syncList) {
    const std::scoped_lock lock(_mutex);

    syncList.clear();

    ASSERT(queryResetAndClearBindings(SELECT_ALL_SYNCS_BY_DRIVE_REQUEST_ID));
    ASSERT(queryBindValue(SELECT_ALL_SYNCS_BY_DRIVE_REQUEST_ID, 1, driveDbId));
    bool found;
    for (;;) {
        if (!queryNext(SELECT_ALL_SYNCS_BY_DRIVE_REQUEST_ID, found)) {
            LOG_WARN(_logger, "Error getting query result: " << SELECT_ALL_SYNCS_BY_DRIVE_REQUEST_ID);
            return false;
        }
        if (!found) {
            break;
        }

        int id;
        ASSERT(queryIntValue(SELECT_ALL_SYNCS_BY_DRIVE_REQUEST_ID, 0, id));
        SyncName localPath;
        ASSERT(querySyncNameValue(SELECT_ALL_SYNCS_BY_DRIVE_REQUEST_ID, 1, localPath));
        SyncName targetPath;
        ASSERT(querySyncNameValue(SELECT_ALL_SYNCS_BY_DRIVE_REQUEST_ID, 2, targetPath));
        std::string targetNodeId;
        ASSERT(queryStringValue(SELECT_ALL_SYNCS_BY_DRIVE_REQUEST_ID, 3, targetNodeId));
        SyncName dbPath;
        ASSERT(querySyncNameValue(SELECT_ALL_SYNCS_BY_DRIVE_REQUEST_ID, 4, dbPath));
        int paused;
        ASSERT(queryIntValue(SELECT_ALL_SYNCS_BY_DRIVE_REQUEST_ID, 5, paused));
        int supportVfs;
        ASSERT(queryIntValue(SELECT_ALL_SYNCS_BY_DRIVE_REQUEST_ID, 6, supportVfs));
        int virtualFileMode;
        ASSERT(queryIntValue(SELECT_ALL_SYNCS_BY_DRIVE_REQUEST_ID, 7, virtualFileMode));
        int notificationsDisabled;
        ASSERT(queryIntValue(SELECT_ALL_SYNCS_BY_DRIVE_REQUEST_ID, 8, notificationsDisabled));
        int hasFullyCompleted;
        ASSERT(queryIntValue(SELECT_ALL_SYNCS_BY_DRIVE_REQUEST_ID, 9, hasFullyCompleted));
        std::string navigationPaneClsid;
        ASSERT(queryStringValue(SELECT_ALL_SYNCS_BY_DRIVE_REQUEST_ID, 10, navigationPaneClsid));
        std::string listingCursor;
        ASSERT(queryStringValue(SELECT_ALL_SYNCS_BY_DRIVE_REQUEST_ID, 11, listingCursor));
        int64_t listingCursorTimestamp;
        ASSERT(queryInt64Value(SELECT_ALL_SYNCS_BY_DRIVE_REQUEST_ID, 12, listingCursorTimestamp));

        syncList.push_back(Sync(id, driveDbId, SyncPath(localPath), SyncPath(targetPath), targetNodeId, static_cast<bool>(paused),
                                static_cast<bool>(supportVfs), static_cast<VirtualFileMode>(virtualFileMode),
                                static_cast<bool>(notificationsDisabled), SyncPath(dbPath), static_cast<bool>(hasFullyCompleted),
                                navigationPaneClsid, listingCursor, listingCursorTimestamp));
    }
    ASSERT(queryResetAndClearBindings(SELECT_ALL_SYNCS_BY_DRIVE_REQUEST_ID));

    return true;
}

bool ParmsDb::getNewSyncDbId(int &dbId) {
    std::vector<Sync> syncList;
    if (!selectAllSyncs(syncList)) {
        LOG_WARN(_logger, "Error in selectAllSyncs");
        return false;
    }

    dbId = 1;
    for (const Sync &sync : syncList) {
        // NB: syncList is sorted by dbId
        if (sync.dbId() > dbId) {
            break;
        }
        dbId++;
    }

    return true;
}

bool ParmsDb::insertExclusionTemplate(const ExclusionTemplate &exclusionTemplate, bool &constraintError) {
    const std::scoped_lock lock(_mutex);

    // Insert exclusion template record
    int errId;
    std::string error;

    ASSERT(queryResetAndClearBindings(INSERT_EXCLUSION_TEMPLATE_REQUEST_ID));
    ASSERT(queryBindValue(INSERT_EXCLUSION_TEMPLATE_REQUEST_ID, 1, exclusionTemplate.templ()));
    ASSERT(queryBindValue(INSERT_EXCLUSION_TEMPLATE_REQUEST_ID, 2, exclusionTemplate.warning()));
    ASSERT(queryBindValue(INSERT_EXCLUSION_TEMPLATE_REQUEST_ID, 3, exclusionTemplate.def()));
    ASSERT(queryBindValue(INSERT_EXCLUSION_TEMPLATE_REQUEST_ID, 4, exclusionTemplate.deleted()));
    if (!queryExec(INSERT_EXCLUSION_TEMPLATE_REQUEST_ID, errId, error)) {
        LOG_WARN(_logger, "Error running query: " << INSERT_EXCLUSION_TEMPLATE_REQUEST_ID);
        constraintError = (errId == SQLITE_CONSTRAINT);
        return false;
    }

    return true;
}

bool ParmsDb::updateExclusionTemplate(const ExclusionTemplate &exclusionTemplate, bool &found) {
    const std::scoped_lock lock(_mutex);

    int errId;
    std::string error;

    ASSERT(queryResetAndClearBindings(UPDATE_EXCLUSION_TEMPLATE_REQUEST_ID));
    ASSERT(queryBindValue(UPDATE_EXCLUSION_TEMPLATE_REQUEST_ID, 1, exclusionTemplate.warning()));
    ASSERT(queryBindValue(UPDATE_EXCLUSION_TEMPLATE_REQUEST_ID, 2, exclusionTemplate.def()));
    ASSERT(queryBindValue(UPDATE_EXCLUSION_TEMPLATE_REQUEST_ID, 3, exclusionTemplate.deleted()));
    ASSERT(queryBindValue(UPDATE_EXCLUSION_TEMPLATE_REQUEST_ID, 4, exclusionTemplate.templ()));
    if (!queryExec(UPDATE_EXCLUSION_TEMPLATE_REQUEST_ID, errId, error)) {
        LOG_WARN(_logger, "Error running query: " << UPDATE_EXCLUSION_TEMPLATE_REQUEST_ID);
        return false;
    }
    if (numRowsAffected() == 1) {
        found = true;
    } else {
        LOG_WARN(_logger, "Error running query: " << UPDATE_EXCLUSION_TEMPLATE_REQUEST_ID << " - num rows affected != 1");
        found = false;
    }

    return true;
}

bool ParmsDb::deleteExclusionTemplate(const std::string &templ, bool &found) {
    const std::scoped_lock lock(_mutex);

    int errId;
    std::string error;

    ASSERT(queryResetAndClearBindings(DELETE_EXCLUSION_TEMPLATE_REQUEST_ID));
    ASSERT(queryBindValue(DELETE_EXCLUSION_TEMPLATE_REQUEST_ID, 1, templ));
    if (!queryExec(DELETE_EXCLUSION_TEMPLATE_REQUEST_ID, errId, error)) {
        LOG_WARN(_logger, "Error running query: " << DELETE_EXCLUSION_TEMPLATE_REQUEST_ID);
        return false;
    }
    if (numRowsAffected() == 1) {
        found = true;
    } else {
        LOG_WARN(_logger, "Error running query: " << DELETE_EXCLUSION_TEMPLATE_REQUEST_ID << " - num rows affected != 1");
        found = false;
    }

    return true;
}

bool ParmsDb::selectAllExclusionTemplates(std::vector<ExclusionTemplate> &exclusionTemplateList) {
    const std::scoped_lock lock(_mutex);

    exclusionTemplateList.clear();

    ASSERT(queryResetAndClearBindings(SELECT_ALL_EXCLUSION_TEMPLATE_REQUEST_ID));
    bool found;
    for (;;) {
        if (!queryNext(SELECT_ALL_EXCLUSION_TEMPLATE_REQUEST_ID, found)) {
            LOG_WARN(_logger, "Error getting query result: " << SELECT_ALL_EXCLUSION_TEMPLATE_REQUEST_ID);
            return false;
        }
        if (!found) {
            break;
        }

        std::string templ;
        ASSERT(queryStringValue(SELECT_ALL_EXCLUSION_TEMPLATE_REQUEST_ID, 0, templ));
        int warning;
        ASSERT(queryIntValue(SELECT_ALL_EXCLUSION_TEMPLATE_REQUEST_ID, 1, warning));
        int def;
        ASSERT(queryIntValue(SELECT_ALL_EXCLUSION_TEMPLATE_REQUEST_ID, 2, def));
        int deleted;
        ASSERT(queryIntValue(SELECT_ALL_EXCLUSION_TEMPLATE_REQUEST_ID, 3, deleted));

        exclusionTemplateList.push_back(
            ExclusionTemplate(templ, static_cast<bool>(warning), static_cast<bool>(def), static_cast<bool>(deleted)));
    }
    ASSERT(queryResetAndClearBindings(SELECT_ALL_EXCLUSION_TEMPLATE_REQUEST_ID));

    return true;
}

bool ParmsDb::selectAllExclusionTemplates(bool def, std::vector<ExclusionTemplate> &exclusionTemplateList) {
    const std::scoped_lock lock(_mutex);

    exclusionTemplateList.clear();

    ASSERT(queryResetAndClearBindings(SELECT_ALL_EXCLUSION_TEMPLATE_BY_DEF_REQUEST_ID));
    ASSERT(queryBindValue(SELECT_ALL_EXCLUSION_TEMPLATE_BY_DEF_REQUEST_ID, 1, def));
    bool found;
    for (;;) {
        if (!queryNext(SELECT_ALL_EXCLUSION_TEMPLATE_BY_DEF_REQUEST_ID, found)) {
            LOG_WARN(_logger, "Error getting query result: " << SELECT_ALL_EXCLUSION_TEMPLATE_BY_DEF_REQUEST_ID);
            return false;
        }
        if (!found) {
            break;
        }

        std::string templ;
        ASSERT(queryStringValue(SELECT_ALL_EXCLUSION_TEMPLATE_BY_DEF_REQUEST_ID, 0, templ));
        int warning;
        ASSERT(queryIntValue(SELECT_ALL_EXCLUSION_TEMPLATE_BY_DEF_REQUEST_ID, 1, warning));
        int deleted;
        ASSERT(queryIntValue(SELECT_ALL_EXCLUSION_TEMPLATE_BY_DEF_REQUEST_ID, 2, deleted));

        exclusionTemplateList.push_back(ExclusionTemplate(templ, static_cast<bool>(warning), def, static_cast<bool>(deleted)));
    }
    ASSERT(queryResetAndClearBindings(SELECT_ALL_EXCLUSION_TEMPLATE_BY_DEF_REQUEST_ID));

    return true;
}

bool ParmsDb::updateAllExclusionTemplates(bool def, const std::vector<ExclusionTemplate> &exclusionTemplateList) {
    const std::scoped_lock lock(_mutex);

    int errId;
    std::string error;

    startTransaction();

    // Delete existing ExclusionTemplates
    ASSERT(queryResetAndClearBindings(DELETE_ALL_EXCLUSION_TEMPLATE_BY_DEF_REQUEST_ID));
    ASSERT(queryBindValue(DELETE_ALL_EXCLUSION_TEMPLATE_BY_DEF_REQUEST_ID, 1, def));
    if (!queryExec(DELETE_ALL_EXCLUSION_TEMPLATE_BY_DEF_REQUEST_ID, errId, error)) {
        LOG_WARN(_logger, "Error running query: " << DELETE_ALL_EXCLUSION_TEMPLATE_BY_DEF_REQUEST_ID);
        rollbackTransaction();
        return false;
    }

    // Insert new ExclusionTemplates
    for (const ExclusionTemplate &exclusionTemplate : exclusionTemplateList) {
        ASSERT(queryResetAndClearBindings(INSERT_EXCLUSION_TEMPLATE_REQUEST_ID));
        ASSERT(queryBindValue(INSERT_EXCLUSION_TEMPLATE_REQUEST_ID, 1, exclusionTemplate.templ()));
        ASSERT(queryBindValue(INSERT_EXCLUSION_TEMPLATE_REQUEST_ID, 2, exclusionTemplate.warning()));
        ASSERT(queryBindValue(INSERT_EXCLUSION_TEMPLATE_REQUEST_ID, 3, exclusionTemplate.def()));
        ASSERT(queryBindValue(INSERT_EXCLUSION_TEMPLATE_REQUEST_ID, 4, exclusionTemplate.deleted()));
        if (!queryExec(INSERT_EXCLUSION_TEMPLATE_REQUEST_ID, errId, error)) {
            LOG_WARN(_logger, "Error running query: " << INSERT_EXCLUSION_TEMPLATE_REQUEST_ID);
            rollbackTransaction();
            return false;
        }
    }

    commitTransaction();

    return true;
}

#ifdef __APPLE__
bool ParmsDb::insertExclusionApp(const ExclusionApp &exclusionApp, bool &constraintError) {
    const std::scoped_lock lock(_mutex);

    int errId;
    std::string error;

    ASSERT(queryResetAndClearBindings(INSERT_EXCLUSION_APP_REQUEST_ID));
    ASSERT(queryBindValue(INSERT_EXCLUSION_APP_REQUEST_ID, 1, exclusionApp.appId()));
    ASSERT(queryBindValue(INSERT_EXCLUSION_APP_REQUEST_ID, 2, exclusionApp.description()));
    ASSERT(queryBindValue(INSERT_EXCLUSION_APP_REQUEST_ID, 3, exclusionApp.def()));
    if (!queryExec(INSERT_EXCLUSION_APP_REQUEST_ID, errId, error)) {
        LOG_WARN(_logger, "Error running query: " << INSERT_EXCLUSION_APP_REQUEST_ID);
        constraintError = (errId == SQLITE_CONSTRAINT);
        return false;
    }

    return true;
}

bool ParmsDb::updateExclusionApp(const ExclusionApp &exclusionApp, bool &found) {
    const std::scoped_lock lock(_mutex);

    int errId;
    std::string error;

    ASSERT(queryResetAndClearBindings(UPDATE_EXCLUSION_APP_REQUEST_ID));
    ASSERT(queryBindValue(UPDATE_EXCLUSION_APP_REQUEST_ID, 1, exclusionApp.description()));
    ASSERT(queryBindValue(UPDATE_EXCLUSION_APP_REQUEST_ID, 2, exclusionApp.def()));
    ASSERT(queryBindValue(UPDATE_EXCLUSION_APP_REQUEST_ID, 3, exclusionApp.appId()));
    if (!queryExec(UPDATE_EXCLUSION_APP_REQUEST_ID, errId, error)) {
        LOG_WARN(_logger, "Error running query: " << UPDATE_EXCLUSION_APP_REQUEST_ID);
        return false;
    }
    if (numRowsAffected() == 1) {
        found = true;
    } else {
        LOG_WARN(_logger, "Error running query: " << UPDATE_EXCLUSION_APP_REQUEST_ID << " - num rows affected != 1");
        found = false;
    }

    return true;
}

bool ParmsDb::deleteExclusionApp(const std::string &appId, bool &found) {
    const std::scoped_lock lock(_mutex);

    int errId;
    std::string error;

    ASSERT(queryResetAndClearBindings(DELETE_EXCLUSION_APP_REQUEST_ID));
    ASSERT(queryBindValue(DELETE_EXCLUSION_APP_REQUEST_ID, 1, appId));
    if (!queryExec(DELETE_EXCLUSION_APP_REQUEST_ID, errId, error)) {
        LOG_WARN(_logger, "Error running query: " << DELETE_EXCLUSION_APP_REQUEST_ID);
        return false;
    }
    if (numRowsAffected() == 1) {
        found = true;
    } else {
        LOG_WARN(_logger, "Error running query: " << DELETE_EXCLUSION_APP_REQUEST_ID << " - num rows affected != 1");
        found = false;
    }

    return true;
}

bool ParmsDb::selectAllExclusionApps(std::vector<ExclusionApp> &exclusionAppList) {
    const std::scoped_lock lock(_mutex);

    exclusionAppList.clear();

    ASSERT(queryResetAndClearBindings(SELECT_ALL_EXCLUSION_APP_REQUEST_ID));
    bool found;
    for (;;) {
        if (!queryNext(SELECT_ALL_EXCLUSION_APP_REQUEST_ID, found)) {
            LOG_WARN(_logger, "Error getting query result: " << SELECT_ALL_EXCLUSION_APP_REQUEST_ID);
            return false;
        }
        if (!found) {
            break;
        }

        std::string appId;
        ASSERT(queryStringValue(SELECT_ALL_EXCLUSION_APP_REQUEST_ID, 0, appId));
        std::string description;
        ASSERT(queryStringValue(SELECT_ALL_EXCLUSION_APP_REQUEST_ID, 1, description));
        int def;
        ASSERT(queryIntValue(SELECT_ALL_EXCLUSION_APP_REQUEST_ID, 2, def));

        exclusionAppList.push_back(ExclusionApp(appId, description, static_cast<bool>(def)));
    }
    ASSERT(queryResetAndClearBindings(SELECT_ALL_EXCLUSION_APP_REQUEST_ID));

    return true;
}

bool ParmsDb::selectAllExclusionApps(bool def, std::vector<ExclusionApp> &exclusionAppList) {
    const std::scoped_lock lock(_mutex);

    exclusionAppList.clear();

    ASSERT(queryResetAndClearBindings(SELECT_ALL_EXCLUSION_APP_BY_DEF_REQUEST_ID));
    ASSERT(queryBindValue(SELECT_ALL_EXCLUSION_APP_BY_DEF_REQUEST_ID, 1, def));
    bool found;
    for (;;) {
        if (!queryNext(SELECT_ALL_EXCLUSION_APP_BY_DEF_REQUEST_ID, found)) {
            LOG_WARN(_logger, "Error getting query result: " << SELECT_ALL_EXCLUSION_APP_REQUEST_ID);
            return false;
        }
        if (!found) {
            break;
        }

        std::string appId;
        ASSERT(queryStringValue(SELECT_ALL_EXCLUSION_APP_BY_DEF_REQUEST_ID, 0, appId));
        std::string description;
        ASSERT(queryStringValue(SELECT_ALL_EXCLUSION_APP_BY_DEF_REQUEST_ID, 1, description));

        exclusionAppList.push_back(ExclusionApp(appId, description, def));
    }
    ASSERT(queryResetAndClearBindings(SELECT_ALL_EXCLUSION_APP_BY_DEF_REQUEST_ID));

    return true;
}

bool ParmsDb::updateAllExclusionApps(bool def, const std::vector<ExclusionApp> &exclusionAppList) {
    const std::scoped_lock lock(_mutex);

    int errId;
    std::string error;

    startTransaction();

    // Delete existing ExclusionApps
    ASSERT(queryResetAndClearBindings(DELETE_ALL_EXCLUSION_APP_BY_DEF_REQUEST_ID));
    ASSERT(queryBindValue(DELETE_ALL_EXCLUSION_APP_BY_DEF_REQUEST_ID, 1, def));
    if (!queryExec(DELETE_ALL_EXCLUSION_APP_BY_DEF_REQUEST_ID, errId, error)) {
        LOG_WARN(_logger, "Error running query: " << DELETE_ALL_EXCLUSION_APP_BY_DEF_REQUEST_ID);
        rollbackTransaction();
        return false;
    }

    // Insert new ExclusionApps
    for (const ExclusionApp &exclusionApp : exclusionAppList) {
        ASSERT(queryResetAndClearBindings(INSERT_EXCLUSION_APP_REQUEST_ID));
        ASSERT(queryBindValue(INSERT_EXCLUSION_APP_REQUEST_ID, 1, exclusionApp.appId()));
        ASSERT(queryBindValue(INSERT_EXCLUSION_APP_REQUEST_ID, 2, exclusionApp.description()));
        ASSERT(queryBindValue(INSERT_EXCLUSION_APP_REQUEST_ID, 3, exclusionApp.def()));
        if (!queryExec(INSERT_EXCLUSION_APP_REQUEST_ID, errId, error)) {
            LOG_WARN(_logger, "Error running query: " << INSERT_EXCLUSION_APP_REQUEST_ID);
            rollbackTransaction();
            return false;
        }
    }

    commitTransaction();

    return true;
}
#endif

bool ParmsDb::insertError(const Error &err) {
    const std::scoped_lock lock(_mutex);

    int errId;
    std::string error;

    ASSERT(queryResetAndClearBindings(INSERT_ERROR_REQUEST_ID));
    ASSERT(queryBindValue(INSERT_ERROR_REQUEST_ID, 1, err.time()));
    ASSERT(queryBindValue(INSERT_ERROR_REQUEST_ID, 2, enumClassToInt(err.level())));
    ASSERT(queryBindValue(INSERT_ERROR_REQUEST_ID, 3, err.functionName()));
    ASSERT(queryBindValue(INSERT_ERROR_REQUEST_ID, 4, err.syncDbId() ? dbtype(err.syncDbId()) : std::monostate()));
    ASSERT(queryBindValue(INSERT_ERROR_REQUEST_ID, 5, err.workerName()));
    ASSERT(queryBindValue(INSERT_ERROR_REQUEST_ID, 6, enumClassToInt(err.exitCode())));
    ASSERT(queryBindValue(INSERT_ERROR_REQUEST_ID, 7, enumClassToInt(err.exitCause())));
    ASSERT(queryBindValue(INSERT_ERROR_REQUEST_ID, 8, err.localNodeId()));
    ASSERT(queryBindValue(INSERT_ERROR_REQUEST_ID, 9, err.remoteNodeId()));
    ASSERT(queryBindValue(INSERT_ERROR_REQUEST_ID, 10, enumClassToInt(err.nodeType())));
    ASSERT(queryBindValue(INSERT_ERROR_REQUEST_ID, 11, err.path().native()));
    ASSERT(queryBindValue(INSERT_ERROR_REQUEST_ID, 12, 0));  // TODO : Not used anymore
    ASSERT(queryBindValue(INSERT_ERROR_REQUEST_ID, 13, enumClassToInt(err.conflictType())));
    ASSERT(queryBindValue(INSERT_ERROR_REQUEST_ID, 14, enumClassToInt(err.inconsistencyType())));
    ASSERT(queryBindValue(INSERT_ERROR_REQUEST_ID, 15, enumClassToInt(err.cancelType())));
    ASSERT(queryBindValue(INSERT_ERROR_REQUEST_ID, 16, err.destinationPath()));
    if (!queryExec(INSERT_ERROR_REQUEST_ID, errId, error)) {
        LOG_WARN(_logger, "Error running query: " << INSERT_ERROR_REQUEST_ID);
        return false;
    }

    return true;
}

bool ParmsDb::updateError(const Error &err, bool &found) {
    const std::scoped_lock lock(_mutex);

    int errId;
    std::string error;

    ASSERT(queryResetAndClearBindings(UPDATE_ERROR_REQUEST_ID));
    ASSERT(queryBindValue(UPDATE_ERROR_REQUEST_ID, 1, err.time()));
    ASSERT(queryBindValue(UPDATE_ERROR_REQUEST_ID, 2, err.dbId()));
    if (!queryExec(UPDATE_ERROR_REQUEST_ID, errId, error)) {
        LOG_WARN(_logger, "Error running query: " << UPDATE_ERROR_REQUEST_ID);
        return false;
    }
    if (numRowsAffected() == 1) {
        found = true;
    } else {
        LOG_WARN(_logger, "Error running query: " << UPDATE_ERROR_REQUEST_ID << " - num rows affected != 1");
        found = false;
    }

    return true;
}

bool ParmsDb::deleteAllErrorsByExitCode(ExitCode exitCode) {
    const std::scoped_lock lock(_mutex);

    int errId;
    std::string error;

    ASSERT(queryResetAndClearBindings(DELETE_ALL_ERROR_BY_EXITCODE_REQUEST_ID));
    ASSERT(queryBindValue(DELETE_ALL_ERROR_BY_EXITCODE_REQUEST_ID, 1, static_cast<int>(exitCode)));
    if (!queryExec(DELETE_ALL_ERROR_BY_EXITCODE_REQUEST_ID, errId, error)) {
        LOG_WARN(_logger, "Error running query: " << DELETE_ALL_ERROR_BY_EXITCODE_REQUEST_ID);
        return false;
    }

    return true;
}

bool ParmsDb::deleteAllErrorsByExitCause(ExitCause exitCause) {
    const std::scoped_lock lock(_mutex);

    int errId;
    std::string error;

    ASSERT(queryResetAndClearBindings(DELETE_ALL_ERROR_BY_ExitCauseREQUEST_ID));
    ASSERT(queryBindValue(DELETE_ALL_ERROR_BY_ExitCauseREQUEST_ID, 1, static_cast<int>(exitCause)));
    if (!queryExec(DELETE_ALL_ERROR_BY_ExitCauseREQUEST_ID, errId, error)) {
        LOG_WARN(_logger, "Error running query: " << DELETE_ALL_ERROR_BY_ExitCauseREQUEST_ID);
        return false;
    }

    return true;
}

bool ParmsDb::selectAllErrors(ErrorLevel level, int syncDbId, int limit, std::vector<Error> &errs) {
    const std::scoped_lock lock(_mutex);

    ASSERT(queryResetAndClearBindings(SELECT_ALL_ERROR_BY_LEVEL_AND_SYNCDBID_REQUEST_ID));
    ASSERT(queryBindValue(SELECT_ALL_ERROR_BY_LEVEL_AND_SYNCDBID_REQUEST_ID, 1, enumClassToInt(level)));
    ASSERT(queryBindValue(SELECT_ALL_ERROR_BY_LEVEL_AND_SYNCDBID_REQUEST_ID, 2, syncDbId));
    ASSERT(queryBindValue(SELECT_ALL_ERROR_BY_LEVEL_AND_SYNCDBID_REQUEST_ID, 3, limit));
    bool found;
    for (;;) {
        if (!queryNext(SELECT_ALL_ERROR_BY_LEVEL_AND_SYNCDBID_REQUEST_ID, found)) {
            LOG_WARN(_logger, "Error getting query result: " << SELECT_ALL_ERROR_BY_LEVEL_AND_SYNCDBID_REQUEST_ID);
            return false;
        }
        if (!found) {
            break;
        }

        int64_t dbId;
        ASSERT(queryInt64Value(SELECT_ALL_ERROR_BY_LEVEL_AND_SYNCDBID_REQUEST_ID, 0, dbId));
        int64_t time;
        ASSERT(queryInt64Value(SELECT_ALL_ERROR_BY_LEVEL_AND_SYNCDBID_REQUEST_ID, 1, time));
        std::string functionName;
        ASSERT(queryStringValue(SELECT_ALL_ERROR_BY_LEVEL_AND_SYNCDBID_REQUEST_ID, 2, functionName));
        std::string workerName;
        ASSERT(queryStringValue(SELECT_ALL_ERROR_BY_LEVEL_AND_SYNCDBID_REQUEST_ID, 3, workerName));
        int exitCode;
        ASSERT(queryIntValue(SELECT_ALL_ERROR_BY_LEVEL_AND_SYNCDBID_REQUEST_ID, 4, exitCode));
        int exitCause;
        ASSERT(queryIntValue(SELECT_ALL_ERROR_BY_LEVEL_AND_SYNCDBID_REQUEST_ID, 5, exitCause));
        std::string localNodeId;
        ASSERT(queryStringValue(SELECT_ALL_ERROR_BY_LEVEL_AND_SYNCDBID_REQUEST_ID, 6, localNodeId));
        std::string remoteNodeId;
        ASSERT(queryStringValue(SELECT_ALL_ERROR_BY_LEVEL_AND_SYNCDBID_REQUEST_ID, 7, remoteNodeId));
        int nodeType;
        ASSERT(queryIntValue(SELECT_ALL_ERROR_BY_LEVEL_AND_SYNCDBID_REQUEST_ID, 8, nodeType));
        SyncName path;
        ASSERT(querySyncNameValue(SELECT_ALL_ERROR_BY_LEVEL_AND_SYNCDBID_REQUEST_ID, 9, path));
        int status;
        ASSERT(queryIntValue(SELECT_ALL_ERROR_BY_LEVEL_AND_SYNCDBID_REQUEST_ID, 10, status));
        int conflictType;
        ASSERT(queryIntValue(SELECT_ALL_ERROR_BY_LEVEL_AND_SYNCDBID_REQUEST_ID, 11, conflictType));
        int inconsistencyType;
        ASSERT(queryIntValue(SELECT_ALL_ERROR_BY_LEVEL_AND_SYNCDBID_REQUEST_ID, 12, inconsistencyType));
        int cancelType;
        ASSERT(queryIntValue(SELECT_ALL_ERROR_BY_LEVEL_AND_SYNCDBID_REQUEST_ID, 13, cancelType));
        SyncName destinationPath;
        ASSERT(querySyncNameValue(SELECT_ALL_ERROR_BY_LEVEL_AND_SYNCDBID_REQUEST_ID, 14, destinationPath));

        errs.push_back(Error(dbId, time, static_cast<ErrorLevel>(level), functionName, syncDbId, workerName,
                             static_cast<ExitCode>(exitCode), static_cast<ExitCause>(exitCause), static_cast<NodeId>(localNodeId),
                             static_cast<NodeId>(remoteNodeId), static_cast<NodeType>(nodeType), static_cast<SyncPath>(path),
                             static_cast<ConflictType>(conflictType), static_cast<InconsistencyType>(inconsistencyType),
                             static_cast<CancelType>(cancelType), static_cast<SyncPath>(destinationPath)));
    }
    ASSERT(queryResetAndClearBindings(SELECT_ALL_ERROR_BY_LEVEL_AND_SYNCDBID_REQUEST_ID));

    return true;
}

bool ParmsDb::selectConflicts(int syncDbId, ConflictType filter, std::vector<Error> &errs) {
    const std::scoped_lock lock(_mutex);

    std::string requestId = (filter == ConflictType::None ? SELECT_ALL_CONFLICTS_BY_SYNCDBID_REQUEST_ID
                                                        : SELECT_FILTERED_CONFLICTS_BY_SYNCDBID_REQUEST_ID);

    ASSERT(queryResetAndClearBindings(requestId));
    ASSERT(queryBindValue(requestId, 1, syncDbId));
    ASSERT(queryBindValue(requestId, 2, std::to_string(enumClassToInt(filter))));

    bool found = false;
    for (;;) {
        if (!queryNext(requestId, found)) {
            LOG_WARN(_logger, "Error getting query result: " << requestId.c_str());
            return false;
        }
        if (!found) {
            break;
        }

        int64_t dbId = 0;
        ASSERT(queryInt64Value(requestId, 0, dbId));
        int64_t time = 0;
        ASSERT(queryInt64Value(requestId, 1, time));
        std::string functionName;
        ASSERT(queryStringValue(requestId, 2, functionName));
        std::string workerName;
        ASSERT(queryStringValue(requestId, 3, workerName));
        int exitCode = 0;
        ASSERT(queryIntValue(requestId, 4, exitCode));
        int exitCause = 0;
        ASSERT(queryIntValue(requestId, 5, exitCause));
        std::string localNodeId;
        ASSERT(queryStringValue(requestId, 6, localNodeId));
        std::string remoteNodeId;
        ASSERT(queryStringValue(requestId, 7, remoteNodeId));
        int nodeType = 0;
        ASSERT(queryIntValue(requestId, 8, nodeType));
        SyncName path;
        ASSERT(querySyncNameValue(requestId, 9, path));
        int status = 0;
        ASSERT(queryIntValue(requestId, 10, status));
        int conflictType = 0;
        ASSERT(queryIntValue(requestId, 11, conflictType));
        int inconsistencyType = 0;
        ASSERT(queryIntValue(requestId, 12, inconsistencyType));
        int cancelType = 0;
        ASSERT(queryIntValue(requestId, 13, cancelType));
        SyncName destinationPath;
        ASSERT(querySyncNameValue(requestId, 14, destinationPath));

        errs.push_back(Error(dbId, time, ErrorLevel::Node, functionName, syncDbId, workerName, static_cast<ExitCode>(exitCode),
                             static_cast<ExitCause>(exitCause), static_cast<NodeId>(localNodeId),
                             static_cast<NodeId>(remoteNodeId), static_cast<NodeType>(nodeType), static_cast<SyncPath>(path),
                             static_cast<ConflictType>(conflictType), static_cast<InconsistencyType>(inconsistencyType),
                             static_cast<CancelType>(cancelType), static_cast<SyncPath>(destinationPath)));
    }
    ASSERT(queryResetAndClearBindings(requestId));

    return true;
}

bool ParmsDb::deleteErrors(ErrorLevel level) {
    const std::scoped_lock lock(_mutex);

    int errId;
    std::string error;

    ASSERT(queryResetAndClearBindings(DELETE_ALL_ERROR_BY_LEVEL_REQUEST_ID));
    ASSERT(queryBindValue(DELETE_ALL_ERROR_BY_LEVEL_REQUEST_ID, 1, enumClassToInt(level)));
    if (!queryExec(DELETE_ALL_ERROR_BY_LEVEL_REQUEST_ID, errId, error)) {
        LOG_WARN(_logger, "Error running query: " << DELETE_ALL_ERROR_BY_LEVEL_REQUEST_ID);
        return false;
    }

    return true;
}

bool ParmsDb::deleteError(int dbId, bool &found) {
    const std::scoped_lock lock(_mutex);

    int errId;
    std::string error;

    ASSERT(queryResetAndClearBindings(DELETE_ERROR_BY_DBID_REQUEST_ID));
    ASSERT(queryBindValue(DELETE_ERROR_BY_DBID_REQUEST_ID, 1, dbId));
    if (!queryExec(DELETE_ERROR_BY_DBID_REQUEST_ID, errId, error)) {
        LOG_WARN(_logger, "Error running query: " << DELETE_ERROR_BY_DBID_REQUEST_ID);
        return false;
    }
    if (numRowsAffected() == 1) {
        found = true;
    } else {
        LOG_WARN(_logger, "Error running query: " << DELETE_ERROR_BY_DBID_REQUEST_ID << " - num rows affected != 1");
        found = false;
    }

    return true;
}

bool ParmsDb::insertMigrationSelectiveSync(const MigrationSelectiveSync &migrationSelectiveSync) {
    const std::scoped_lock lock(_mutex);

    int errId;
    std::string error;

    ASSERT(queryResetAndClearBindings(INSERT_MIGRATION_SELECTIVESYNC_REQUEST_ID));
    ASSERT(queryBindValue(INSERT_MIGRATION_SELECTIVESYNC_REQUEST_ID, 1, migrationSelectiveSync.syncDbId()));
    ASSERT(queryBindValue(INSERT_MIGRATION_SELECTIVESYNC_REQUEST_ID, 2, migrationSelectiveSync.path().native()));
    ASSERT(queryBindValue(INSERT_MIGRATION_SELECTIVESYNC_REQUEST_ID, 3, migrationSelectiveSync.type()));
    if (!queryExec(INSERT_MIGRATION_SELECTIVESYNC_REQUEST_ID, errId, error)) {
        LOG_WARN(_logger, "Error running query: " << INSERT_MIGRATION_SELECTIVESYNC_REQUEST_ID);
        return false;
    }

    return true;
}

bool ParmsDb::selectAllMigrationSelectiveSync(std::vector<MigrationSelectiveSync> &migrationSelectiveSyncList) {
    const std::scoped_lock lock(_mutex);

    migrationSelectiveSyncList.clear();

    ASSERT(queryResetAndClearBindings(SELECT_ALL_MIGRATION_SELECTIVESYNC_REQUEST_ID));
    bool found;
    for (;;) {
        if (!queryNext(SELECT_ALL_MIGRATION_SELECTIVESYNC_REQUEST_ID, found)) {
            LOG_WARN(_logger, "Error getting query result: " << SELECT_ALL_MIGRATION_SELECTIVESYNC_REQUEST_ID);
            return false;
        }
        if (!found) {
            break;
        }

        int syncDbId;
        ASSERT(queryIntValue(SELECT_ALL_MIGRATION_SELECTIVESYNC_REQUEST_ID, 0, syncDbId));

        SyncName path;
        ASSERT(querySyncNameValue(SELECT_ALL_MIGRATION_SELECTIVESYNC_REQUEST_ID, 1, path));

        int type;
        ASSERT(queryIntValue(SELECT_ALL_MIGRATION_SELECTIVESYNC_REQUEST_ID, 2, type));

        migrationSelectiveSyncList.push_back(MigrationSelectiveSync(syncDbId, SyncPath(path), type));
    }
    ASSERT(queryResetAndClearBindings(SELECT_ALL_MIGRATION_SELECTIVESYNC_REQUEST_ID));

    return true;
}
}  // namespace KDC

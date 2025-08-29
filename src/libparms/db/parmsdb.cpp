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

#include "parmsdb.h"


#include "libcommon/utility/utility.h"
#include "libcommon/utility/logiffail.h"

#include "libcommonserver/io/iohelper.h"
#include "libcommonserver/utility/utility.h"

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
    "jobPoolCapacityFactor INTEGER,"         \
    "distributionChannel INTEGER"            \
    ");"

#define INSERT_PARAMETERS_REQUEST_ID "insert_parameters"
#define INSERT_PARAMETERS_REQUEST                                                                                             \
    "INSERT INTO parameters (language, monoIcons, autoStart, moveToTrash, notifications, useLog, logLevel, purgeOldLogs, "    \
    "syncHiddenFiles, proxyType, proxyHostName, proxyPort, proxyNeedsAuth, proxyUser, proxyToken, useBigFolderSizeLimit, "    \
    "bigFolderSizeLimit, darkTheme, showShortcuts, updateFileAvailable, updateTargetVersion, updateTargetVersionString, "     \
    "autoUpdateAttempted, seenVersion, dialogGeometry, extendedLog, maxAllowedCpu, uploadSessionParallelJobs, "               \
    "jobPoolCapacityFactor, distributionChannel) "                                                                            \
    "VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, ?12, ?13, ?14, ?15, ?16, ?17, ?18, ?19, ?20, ?21, ?22, ?23, ?24, " \
    "?25, ?26, ?27, ?28, ?29, ?30);"

#define UPDATE_PARAMETERS_REQUEST_ID "update_parameters"
#define UPDATE_PARAMETERS_REQUEST                                                                                               \
    "UPDATE parameters SET language=?1, monoIcons=?2, autoStart=?3, moveToTrash=?4, notifications=?5, useLog=?6, logLevel=?7, " \
    "purgeOldLogs=?8, "                                                                                                         \
    "syncHiddenFiles=?9, proxyType=?10, proxyHostName=?11, proxyPort=?12, proxyNeedsAuth=?13, proxyUser=?14, proxyToken=?15, "  \
    "useBigFolderSizeLimit=?16, "                                                                                               \
    "bigFolderSizeLimit=?17, darkTheme=?18, showShortcuts=?19, updateFileAvailable=?20, updateTargetVersion=?21, "              \
    "updateTargetVersionString=?22, "                                                                                           \
    "autoUpdateAttempted=?23, seenVersion=?24, dialogGeometry=?25, extendedLog=?26, maxAllowedCpu=?27, "                        \
    "uploadSessionParallelJobs=?28, jobPoolCapacityFactor=?29, distributionChannel=?30;"

#define SELECT_PARAMETERS_REQUEST_ID "select_parameters"
#define SELECT_PARAMETERS_REQUEST                                                                                          \
    "SELECT language, monoIcons, autoStart, moveToTrash, notifications, useLog, logLevel, purgeOldLogs, "                  \
    "syncHiddenFiles, proxyType, proxyHostName, proxyPort, proxyNeedsAuth, proxyUser, proxyToken, useBigFolderSizeLimit, " \
    "bigFolderSizeLimit, darkTheme, showShortcuts, updateFileAvailable, updateTargetVersion, updateTargetVersionString, "  \
    "autoUpdateAttempted, seenVersion, dialogGeometry, extendedLog, maxAllowedCpu, uploadSessionParallelJobs, "            \
    "jobPoolCapacityFactor, distributionChannel "                                                                          \
    "FROM parameters;"

#define UPDATE_PARAMETERS_JOB_REQUEST_ID "update_parameters_job"
#define UPDATE_PARAMETERS_JOB_REQUEST "UPDATE parameters SET uploadSessionParallelJobs=?1, jobPoolCapacityFactor=?2;"

#define ALTER_PARAMETERS_ADD_DISTRIBUTION_CHANNEL_REQUEST_ID "alter_parameters_add_distribution"
#define ALTER_PARAMETERS_ADD_DISTRIBUTION_CHANNEL_REQUEST "ALTER TABLE parameters ADD COLUMN distributionChannel INTEGER;"

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

#define SELECT_LAST_CONNECTED_USER_REQUEST_ID "select_last_connected_user"
#define SELECT_LAST_CONNECTED_USER_REQUEST \
    "SELECT dbId, userId, keychainKey, name, email, avatarUrl, avatar, toMigrate FROM user ORDER BY dbId DESC LIMIT 1;"
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
    "localNodeId TEXT,"                                                                      \
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
#define INSERT_SYNC_REQUEST                                                                                             \
    "INSERT INTO sync (dbId, driveDbId, localPath, localNodeId, targetPath, targetNodeId, dbPath, paused, supportVfs, " \
    "virtualFileMode, "                                                                                                 \
    "notificationsDisabled, hasFullyCompleted, navigationPaneClsid, listingCursor, listingCursorTimestamp) "            \
    "VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, ?12, ?13, ?14, ?15);"

#define UPDATE_SYNC_REQUEST_ID "update_sync"
#define UPDATE_SYNC_REQUEST                                                                                                \
    "UPDATE sync SET driveDbId=?1, localPath=?2, localNodeId = ?3, targetPath=?4, targetNodeId=?5, dbPath=?6, paused=?7, " \
    "supportVfs=?8, "                                                                                                      \
    "virtualFileMode=?9, notificationsDisabled=?10, hasFullyCompleted=?11, navigationPaneClsid=?12, listingCursor=?13, "   \
    "listingCursorTimestamp=?14 "                                                                                          \
    "WHERE dbId=?15;"

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
#define SELECT_SYNC_REQUEST                                                                                                   \
    "SELECT dbId, driveDbId, localPath, localNodeId, targetPath, targetNodeId, dbPath, paused, supportVfs, virtualFileMode, " \
    "notificationsDisabled, hasFullyCompleted, navigationPaneClsid, listingCursor, listingCursorTimestamp FROM sync "         \
    "WHERE dbId=?1;"

#define SELECT_SYNC_BY_PATH_REQUEST_ID "select_sync_by_path"
#define SELECT_SYNC_BY_PATH_REQUEST                                                                                           \
    "SELECT dbId, driveDbId, localPath, localNodeId, targetPath, targetNodeId, dbPath, paused, supportVfs, virtualFileMode, " \
    "notificationsDisabled, hasFullyCompleted, navigationPaneClsid, listingCursor, listingCursorTimestamp FROM sync "         \
    "WHERE dbPath=?1;"


#define SELECT_ALL_SYNCS_REQUEST_ID "select_syncs"
#define SELECT_ALL_SYNCS_REQUEST                                                                                              \
    "SELECT dbId, driveDbId, localPath, localNodeId, targetPath, targetNodeId, dbPath, paused, supportVfs, virtualFileMode, " \
    "notificationsDisabled, hasFullyCompleted, navigationPaneClsid, listingCursor, listingCursorTimestamp FROM sync "         \
    "ORDER BY dbId;"

#define SELECT_ALL_SYNCS_BY_DRIVE_REQUEST_ID "select_syncs_by_drive"
#define SELECT_ALL_SYNCS_BY_DRIVE_REQUEST                                                                          \
    "SELECT dbId, localPath, localNodeId, targetPath, targetNodeId, dbPath, paused, supportVfs, virtualFileMode, " \
    "notificationsDisabled, "                                                                                      \
    "hasFullyCompleted, navigationPaneClsid, listingCursor, listingCursorTimestamp FROM sync "                     \
    "WHERE driveDbId=?1 "                                                                                          \
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
#if defined(KD_MACOS)
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
    "level INTEGER," /* Server level */      \
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
#define UPDATE_ERROR_REQUEST             \
    "UPDATE error SET time=?1, path=?2 " \
    "WHERE dbId=?3;"

#define DELETE_ALL_ERROR_BY_EXITCODE_REQUEST_ID "delete_error_by_exitcode"
#define DELETE_ALL_ERROR_BY_EXITCODE_REQUEST \
    "DELETE FROM error "                     \
    "WHERE exitCode=?1;"

#define DELETE_ALL_ERROR_BY_EXITCAUSEREQUEST_ID "delete_error_by_exitcause"
#define DELETE_ALL_ERROR_BY_EXITCAUSEREQUEST \
    "DELETE FROM error "                     \
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

// Check column existance
#define CHECK_COLUMN_EXISTENCE_REQUEST_ID "check_column_existence"
#define CHECK_COLUMN_EXISTENCE_REQUEST "SELECT COUNT(*) AS CNTREC FROM pragma_table_info('?1') WHERE name='?2';"

namespace KDC {

std::shared_ptr<ParmsDb> ParmsDb::_instance = nullptr;

std::shared_ptr<ParmsDb> ParmsDb::instance(const std::filesystem::path &dbPath, const std::string &version,
                                           bool autoDelete /*= false*/, bool test /*= false*/) {
    if (_instance == nullptr) {
        if (dbPath.empty()) {
            assert(false);
            return nullptr;
        }

        try {
            _instance = std::shared_ptr<ParmsDb>(new ParmsDb(dbPath, version, autoDelete, test));
        } catch (...) {
            return nullptr;
        }

        if (!_instance->init(version)) {
            _instance.reset();
            return nullptr;
        }
    }

    return _instance;
}

void ParmsDb::reset() {
    if (_instance) {
        _instance = nullptr;
    }
}

ParmsDb::ParmsDb(const std::filesystem::path &dbPath, const std::string &version, bool autoDelete, bool test) :
    Db(dbPath),
    _test(test) {
    setAutoDelete(autoDelete);

    if (!checkConnect(version)) {
        throw std::runtime_error("Cannot open DB!");
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

    LOG_IF_FAIL(queryResetAndClearBindings(INSERT_PARAMETERS_REQUEST_ID));
    LOG_IF_FAIL(queryBindValue(INSERT_PARAMETERS_REQUEST_ID, 1, static_cast<int>(parameters.language())));
    LOG_IF_FAIL(queryBindValue(INSERT_PARAMETERS_REQUEST_ID, 2, parameters.monoIcons()));
    LOG_IF_FAIL(queryBindValue(INSERT_PARAMETERS_REQUEST_ID, 3, parameters.autoStart()));
    LOG_IF_FAIL(queryBindValue(INSERT_PARAMETERS_REQUEST_ID, 4, parameters.moveToTrash()));
    LOG_IF_FAIL(queryBindValue(INSERT_PARAMETERS_REQUEST_ID, 5, static_cast<int>(parameters.notificationsDisabled())));
    LOG_IF_FAIL(queryBindValue(INSERT_PARAMETERS_REQUEST_ID, 6, parameters.useLog()));
    LOG_IF_FAIL(queryBindValue(INSERT_PARAMETERS_REQUEST_ID, 7, static_cast<int>(parameters.logLevel())));
    LOG_IF_FAIL(queryBindValue(INSERT_PARAMETERS_REQUEST_ID, 8, parameters.purgeOldLogs()));
    LOG_IF_FAIL(queryBindValue(INSERT_PARAMETERS_REQUEST_ID, 9, 0)); // Sync hidden files : not used anymore
    LOG_IF_FAIL(queryBindValue(INSERT_PARAMETERS_REQUEST_ID, 10, static_cast<int>(parameters.proxyConfig().type())));
    LOG_IF_FAIL(queryBindValue(INSERT_PARAMETERS_REQUEST_ID, 11, parameters.proxyConfig().hostName()));
    LOG_IF_FAIL(queryBindValue(INSERT_PARAMETERS_REQUEST_ID, 12, parameters.proxyConfig().port()));
    LOG_IF_FAIL(queryBindValue(INSERT_PARAMETERS_REQUEST_ID, 13, parameters.proxyConfig().needsAuth()));
    LOG_IF_FAIL(queryBindValue(INSERT_PARAMETERS_REQUEST_ID, 14, parameters.proxyConfig().user()));
    LOG_IF_FAIL(queryBindValue(INSERT_PARAMETERS_REQUEST_ID, 15, parameters.proxyConfig().token()));
    LOG_IF_FAIL(queryBindValue(INSERT_PARAMETERS_REQUEST_ID, 16, parameters.useBigFolderSizeLimit()));
    LOG_IF_FAIL(queryBindValue(INSERT_PARAMETERS_REQUEST_ID, 17, parameters.bigFolderSizeLimit()));
    LOG_IF_FAIL(queryBindValue(INSERT_PARAMETERS_REQUEST_ID, 18, parameters.darkTheme()));
    LOG_IF_FAIL(queryBindValue(INSERT_PARAMETERS_REQUEST_ID, 19, parameters.showShortcuts()));
    LOG_IF_FAIL(queryBindValue(INSERT_PARAMETERS_REQUEST_ID, 20, parameters.updateFileAvailable()));
    LOG_IF_FAIL(queryBindValue(INSERT_PARAMETERS_REQUEST_ID, 21, parameters.updateTargetVersion()));
    LOG_IF_FAIL(queryBindValue(INSERT_PARAMETERS_REQUEST_ID, 22, parameters.updateTargetVersionString()));
    LOG_IF_FAIL(queryBindValue(INSERT_PARAMETERS_REQUEST_ID, 23, parameters.autoUpdateAttempted()));
    LOG_IF_FAIL(queryBindValue(INSERT_PARAMETERS_REQUEST_ID, 24, parameters.seenVersion()));
    LOG_IF_FAIL(queryBindValue(INSERT_PARAMETERS_REQUEST_ID, 25, parameters.dialogGeometry()));
    LOG_IF_FAIL(queryBindValue(INSERT_PARAMETERS_REQUEST_ID, 26, static_cast<int>(_test ? true : parameters.extendedLog())));
    LOG_IF_FAIL(queryBindValue(INSERT_PARAMETERS_REQUEST_ID, 27, parameters.maxAllowedCpu()));
    LOG_IF_FAIL(queryBindValue(INSERT_PARAMETERS_REQUEST_ID, 28, parameters.uploadSessionParallelJobs()));
    LOG_IF_FAIL(queryBindValue(INSERT_PARAMETERS_REQUEST_ID, 29, 0));
    LOG_IF_FAIL(queryBindValue(INSERT_PARAMETERS_REQUEST_ID, 30, static_cast<int>(parameters.distributionChannel())));

    if (!queryExec(INSERT_PARAMETERS_REQUEST_ID, errId, error)) {
        LOG_WARN(_logger, "Error running query: " << INSERT_PARAMETERS_REQUEST_ID);
        return false;
    }

    return true;
}


bool ParmsDb::getDefaultExclusionTemplatesFromFile(const SyncPath &syncExcludeListPath,
                                                   std::vector<std::string> &fileDefaultExclusionTemplates) {
    fileDefaultExclusionTemplates.clear();

    if (std::ifstream exclusionFile(syncExcludeListPath); exclusionFile.is_open()) {
        std::string line;
        while (std::getline(exclusionFile, line)) {
            // Remove end of line
            if (!line.empty() && line.back() == '\n') {
                line.pop_back();
            }
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }

            (void) fileDefaultExclusionTemplates.emplace_back(line);
        }
        return true;
    }
    return false;
}

namespace {
std::vector<ExclusionTemplate>::const_iterator findTemplateString(const std::vector<ExclusionTemplate> &templates,
                                                                  const std::string &templateString) {
    return std::find_if(templates.cbegin(), templates.cend(),
                        [&templateString](const ExclusionTemplate &t) { return t.templ() == templateString; });
}
} // namespace

bool ParmsDb::updateExclusionTemplates() {
    // Load default exclusion templates from DB
    std::vector<ExclusionTemplate> dbDefaultExclusionTemplates;
    if (!selectDefaultExclusionTemplates(dbDefaultExclusionTemplates)) {
        LOG_WARN(_logger, "Error in selectAllExclusionTemplates");
        return false;
    }

    // Load default exclusion templates from the template configuration file
    std::vector<std::string> fileDefaultExclusionTemplates;
    if (const auto &excludeListFileName = Utility::getExcludedTemplateFilePath(_test);
        !getDefaultExclusionTemplatesFromFile(excludeListFileName.c_str(), fileDefaultExclusionTemplates)) {
        LOGW_WARN(_logger, L"Cannot open exclusion templates file " << Utility::formatSyncName(excludeListFileName));
        return false;
    }

    for (const auto &defaultTemplateFromDb: dbDefaultExclusionTemplates) {
        if (const auto it = std::find(fileDefaultExclusionTemplates.cbegin(), fileDefaultExclusionTemplates.cend(),
                                      defaultTemplateFromDb.templ());
            it == fileDefaultExclusionTemplates.cend()) {
            // Remove DB template
            bool found = false;
            if (!deleteExclusionTemplate(defaultTemplateFromDb.templ(), found)) {
                LOG_WARN(_logger, "Error in deleteExclusionTemplate");
                return false;
            }
        }
    }

    std::vector<ExclusionTemplate> dbUserExclusionTemplates;
    if (!selectUserExclusionTemplates(dbUserExclusionTemplates)) {
        LOG_WARN(_logger, "Error in selectAllExclusionTemplates");
        return false;
    }

    for (const auto &templateFromFile: fileDefaultExclusionTemplates) {
        if (const auto it = findTemplateString(dbDefaultExclusionTemplates, templateFromFile);
            it != dbDefaultExclusionTemplates.cend())
            continue;

        if (const auto it = findTemplateString(dbUserExclusionTemplates, templateFromFile);
            it != dbUserExclusionTemplates.cend()) {
            if (bool found = false; !deleteExclusionTemplate(it->templ(), found)) {
                LOG_WARN(_logger, "Error in deleteExclusionTemplate");
                return false;
            }
        }

        // Add template as a DB default template.
        if (bool constraintError = false;
            !insertExclusionTemplate(ExclusionTemplate(templateFromFile, false, true, false), constraintError)) {
            LOG_WARN(_logger, "Error in insertExclusionTemplate");
            return false;
        }
    }

    return true;
}

namespace {
void logIfTemplateNormalizationFails(log4cplus::Logger &logger, const SyncName &template_, bool &normalizationHasFailed) {
    normalizationHasFailed = false;

    SyncName nfcNormalizedName;
    const bool nfcSuccess = CommonUtility::normalizedSyncName(template_, nfcNormalizedName, UnicodeNormalization::NFC);
    if (!nfcSuccess) {
        LOGW_WARN(logger, L"Error in CommonUtility::normalizedSyncName. Failed to NFC-normalize the template '"
                                  << SyncName2WStr(template_) << L"'.");
    }

    SyncName nfdNormalizedName;
    const bool nfdSuccess = CommonUtility::normalizedSyncName(template_, nfdNormalizedName, UnicodeNormalization::NFD);
    if (!nfdSuccess) {
        LOGW_WARN(logger, L"Error in CommonUtility::normalizedSyncName. Failed to NFD-normalize the template '"
                                  << SyncName2WStr(template_) << L"'.");
    }

    if (!nfcSuccess || !nfdSuccess) {
        normalizationHasFailed = true;
        LOG_WARN(logger,
                 "File exclusion based on user templates may fail to exclude file names depending on their normalizations.");
    }
}
} // namespace

bool ParmsDb::insertUserTemplateNormalizations(const std::string &fromVersion) {
    if (!CommonUtility::isVersionLower(fromVersion, "3.7.2")) {
        return true;
    }

    LOG_INFO(_logger, "Inserting the normalizations of user exclusion file patterns.");

    if (!createAndPrepareRequest(SELECT_ALL_EXCLUSION_TEMPLATE_BY_DEF_REQUEST_ID, SELECT_ALL_EXCLUSION_TEMPLATE_BY_DEF_REQUEST))
        return false;

    std::vector<ExclusionTemplate> dbUserExclusionTemplates;
    const bool successfulSelection = selectUserExclusionTemplates(dbUserExclusionTemplates);
    queryFree(SELECT_ALL_EXCLUSION_TEMPLATE_BY_DEF_REQUEST_ID);
    if (!successfulSelection) {
        LOG_WARN(_logger, "Error in selectAllExclusionTemplates");
        return false;
    }

    std::vector<ExclusionTemplate> dbUserExclusionTemplatesOutput;
    StrSet dbUserExclusionUniqueStrings;
    for (const auto &userTemplate: dbUserExclusionTemplates) {
        const SyncName &templateName = Str2SyncName(userTemplate.templ());
        bool normalizationHasFailed = false;
        logIfTemplateNormalizationFails(_logger, templateName, normalizationHasFailed);

        if (normalizationHasFailed) continue;

        const auto normalizedTemplateNames = CommonUtility::computePathNormalizations(templateName);
        for (const auto &normalizedName: normalizedTemplateNames) {
            const std::string &normalizedStr = SyncName2Str(normalizedName);
            if (const auto &[_, successfulInsertion] = dbUserExclusionUniqueStrings.emplace(normalizedStr); successfulInsertion) {
                (void) dbUserExclusionTemplatesOutput.emplace_back(normalizedStr);
            }
        }
    }

    LOG_INFO(_logger, "Normalizations prepared for updates.");

    if (!createAndPrepareRequest(DELETE_ALL_EXCLUSION_TEMPLATE_BY_DEF_REQUEST_ID, DELETE_ALL_EXCLUSION_TEMPLATE_BY_DEF_REQUEST))
        return false;

    if (!createAndPrepareRequest(INSERT_EXCLUSION_TEMPLATE_REQUEST_ID, INSERT_EXCLUSION_TEMPLATE_REQUEST)) return false;

    const bool result = updateUserExclusionTemplates(dbUserExclusionTemplatesOutput);

    queryFree(DELETE_ALL_EXCLUSION_TEMPLATE_BY_DEF_REQUEST_ID);
    queryFree(INSERT_EXCLUSION_TEMPLATE_REQUEST_ID);

    return result;
}

#if defined(KD_MACOS)
bool ParmsDb::updateExclusionApps() {
    // Load exclusion apps in DB
    std::vector<ExclusionApp> exclusionAppDbList;
    if (!selectAllExclusionApps(exclusionAppDbList)) {
        LOG_WARN(_logger, "Error in selectAllExclusionApps");
        return false;
    }

    // Load exclusion app in configuration file
    std::vector<std::pair<std::string, std::string>> exclusionAppFileList;
    std::ifstream exclusionFile(Utility::getExcludedAppFilePath(_test).c_str());
    if (exclusionFile.is_open()) {
        std::string line;
        while (std::getline(exclusionFile, line)) {
            // Remove end of line
            if (!line.empty() && line.back() == '\n') {
                line.pop_back();
            }
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }

            size_t pos = line.find(';');
            std::string appId = line.substr(0, pos);
            std::string description = line.substr(pos + 1);
            (void) exclusionAppFileList.emplace_back(std::make_pair(appId, description));
        }
    } else {
        LOGW_WARN(_logger,
                  L"Cannot open exclusion app file with " << Utility::formatSyncPath(Utility::getExcludedAppFilePath(_test)));
        return false;
    }

    for (const auto &templDb: exclusionAppDbList) {
        if (!templDb.def()) continue;

        bool exists = false;
        for (const auto &templFile: exclusionAppFileList) {
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

    for (const auto &templFile: exclusionAppFileList) {
        bool exists = false;
        bool def = false;
        for (const auto &templDb: exclusionAppDbList) {
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
    if (!createAndPrepareRequest(CREATE_PARAMETERS_TABLE_ID, CREATE_PARAMETERS_TABLE)) return false;
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
    if (!createAndPrepareRequest(CREATE_USER_TABLE_ID, CREATE_USER_TABLE)) return false;
    if (!queryExec(CREATE_USER_TABLE_ID, errId, error)) {
        queryFree(CREATE_USER_TABLE_ID);
        return sqlFail(CREATE_USER_TABLE_ID, error);
    }
    queryFree(CREATE_USER_TABLE_ID);

    // Account
    if (!createAndPrepareRequest(CREATE_ACCOUNT_TABLE_ID, CREATE_ACCOUNT_TABLE)) return false;
    if (!queryExec(CREATE_ACCOUNT_TABLE_ID, errId, error)) {
        queryFree(CREATE_ACCOUNT_TABLE_ID);
        return sqlFail(CREATE_ACCOUNT_TABLE_ID, error);
    }
    queryFree(CREATE_ACCOUNT_TABLE_ID);

    // Drive
    if (!createAndPrepareRequest(CREATE_DRIVE_TABLE_ID, CREATE_DRIVE_TABLE)) return false;
    if (!queryExec(CREATE_DRIVE_TABLE_ID, errId, error)) {
        queryFree(CREATE_DRIVE_TABLE_ID);
        return sqlFail(CREATE_DRIVE_TABLE_ID, error);
    }
    queryFree(CREATE_DRIVE_TABLE_ID);

    // Sync
    if (!createAndPrepareRequest(CREATE_SYNC_TABLE_ID, CREATE_SYNC_TABLE)) return false;
    if (!queryExec(CREATE_SYNC_TABLE_ID, errId, error)) {
        queryFree(CREATE_SYNC_TABLE_ID);
        return sqlFail(CREATE_SYNC_TABLE_ID, error);
    }
    queryFree(CREATE_SYNC_TABLE_ID);

    // Exclusion Template
    if (!createAndPrepareRequest(CREATE_EXCLUSION_TEMPLATE_TABLE_ID, CREATE_EXCLUSION_TEMPLATE_TABLE)) return false;
    if (!queryExec(CREATE_EXCLUSION_TEMPLATE_TABLE_ID, errId, error)) {
        queryFree(CREATE_EXCLUSION_TEMPLATE_TABLE_ID);
        return sqlFail(CREATE_EXCLUSION_TEMPLATE_TABLE_ID, error);
    }
    queryFree(CREATE_EXCLUSION_TEMPLATE_TABLE_ID);

#if defined(KD_MACOS)
    // Exclusion App
    if (!createAndPrepareRequest(CREATE_EXCLUSION_APP_TABLE_ID, CREATE_EXCLUSION_APP_TABLE)) return false;
    if (!queryExec(CREATE_EXCLUSION_APP_TABLE_ID, errId, error)) {
        queryFree(CREATE_EXCLUSION_APP_TABLE_ID);
        return sqlFail(CREATE_EXCLUSION_APP_TABLE_ID, error);
    }
    queryFree(CREATE_EXCLUSION_APP_TABLE_ID);
#endif

    // Error
    if (!createAndPrepareRequest(CREATE_ERROR_TABLE_ID, CREATE_ERROR_TABLE)) return false;
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
    if (!createAndPrepareRequest(CREATE_MIGRATION_SELECTIVESYNC_TABLE_ID, CREATE_MIGRATION_SELECTIVESYNC_TABLE)) return false;
    if (!queryExec(CREATE_MIGRATION_SELECTIVESYNC_TABLE_ID, errId, error)) {
        queryFree(CREATE_MIGRATION_SELECTIVESYNC_TABLE_ID);
        return sqlFail(CREATE_MIGRATION_SELECTIVESYNC_TABLE_ID, error);
    }
    queryFree(CREATE_MIGRATION_SELECTIVESYNC_TABLE_ID);

    return true;
}

bool ParmsDb::prepare() {
    // Parameters
    if (!createAndPrepareRequest(INSERT_PARAMETERS_REQUEST_ID, INSERT_PARAMETERS_REQUEST)) return false;
    if (!createAndPrepareRequest(UPDATE_PARAMETERS_REQUEST_ID, UPDATE_PARAMETERS_REQUEST)) return false;
    if (!createAndPrepareRequest(SELECT_PARAMETERS_REQUEST_ID, SELECT_PARAMETERS_REQUEST)) return false;
    // User
    if (!createAndPrepareRequest(INSERT_USER_REQUEST_ID, INSERT_USER_REQUEST)) return false;
    if (!createAndPrepareRequest(UPDATE_USER_REQUEST_ID, UPDATE_USER_REQUEST)) return false;
    if (!createAndPrepareRequest(DELETE_USER_REQUEST_ID, DELETE_USER_REQUEST)) return false;
    if (!createAndPrepareRequest(SELECT_USER_REQUEST_ID, SELECT_USER_REQUEST)) return false;
    if (!createAndPrepareRequest(SELECT_USER_BY_USERID_REQUEST_ID, SELECT_USER_BY_USERID_REQUEST)) return false;
    if (!createAndPrepareRequest(SELECT_ALL_USERS_REQUEST_ID, SELECT_ALL_USERS_REQUEST)) return false;
    if (!createAndPrepareRequest(SELECT_LAST_CONNECTED_USER_REQUEST_ID, SELECT_LAST_CONNECTED_USER_REQUEST)) return false;
    // Account
    if (!createAndPrepareRequest(INSERT_ACCOUNT_REQUEST_ID, INSERT_ACCOUNT_REQUEST)) return false;
    if (!createAndPrepareRequest(UPDATE_ACCOUNT_REQUEST_ID, UPDATE_ACCOUNT_REQUEST)) return false;
    if (!createAndPrepareRequest(DELETE_ACCOUNT_REQUEST_ID, DELETE_ACCOUNT_REQUEST)) return false;
    if (!createAndPrepareRequest(SELECT_ACCOUNT_REQUEST_ID, SELECT_ACCOUNT_REQUEST)) return false;
    if (!createAndPrepareRequest(SELECT_ALL_ACCOUNTS_REQUEST_ID, SELECT_ALL_ACCOUNTS_REQUEST)) return false;
    if (!createAndPrepareRequest(SELECT_ALL_ACCOUNTS_BY_USER_REQUEST_ID, SELECT_ALL_ACCOUNTS_BY_USER_REQUEST)) return false;
    // Drive
    if (!createAndPrepareRequest(INSERT_DRIVE_REQUEST_ID, INSERT_DRIVE_REQUEST)) return false;
    if (!createAndPrepareRequest(UPDATE_DRIVE_REQUEST_ID, UPDATE_DRIVE_REQUEST)) return false;
    if (!createAndPrepareRequest(DELETE_DRIVE_REQUEST_ID, DELETE_DRIVE_REQUEST)) return false;
    if (!createAndPrepareRequest(SELECT_DRIVE_REQUEST_ID, SELECT_DRIVE_REQUEST)) return false;
    if (!createAndPrepareRequest(SELECT_DRIVE_BY_DRIVEID_REQUEST_ID, SELECT_DRIVE_BY_DRIVEID_REQUEST)) return false;
    if (!createAndPrepareRequest(SELECT_ALL_DRIVES_REQUEST_ID, SELECT_ALL_DRIVES_REQUEST)) return false;
    if (!createAndPrepareRequest(SELECT_ALL_DRIVES_BY_ACCOUNT_REQUEST_ID, SELECT_ALL_DRIVES_BY_ACCOUNT_REQUEST)) return false;
    // Sync
    if (!createAndPrepareRequest(INSERT_SYNC_REQUEST_ID, INSERT_SYNC_REQUEST)) return false;
    if (!createAndPrepareRequest(UPDATE_SYNC_REQUEST_ID, UPDATE_SYNC_REQUEST)) return false;
    if (!createAndPrepareRequest(UPDATE_SYNC_PAUSED_REQUEST_ID, UPDATE_SYNC_PAUSED_REQUEST)) return false;
    if (!createAndPrepareRequest(UPDATE_SYNC_HASFULLYCOMPLETED_REQUEST_ID, UPDATE_SYNC_HASFULLYCOMPLETED_REQUEST)) return false;
    if (!createAndPrepareRequest(DELETE_SYNC_REQUEST_ID, DELETE_SYNC_REQUEST)) return false;
    if (!createAndPrepareRequest(SELECT_SYNC_REQUEST_ID, SELECT_SYNC_REQUEST)) return false;
    if (!createAndPrepareRequest(SELECT_SYNC_BY_PATH_REQUEST_ID, SELECT_SYNC_BY_PATH_REQUEST)) return false;
    if (!createAndPrepareRequest(SELECT_ALL_SYNCS_REQUEST_ID, SELECT_ALL_SYNCS_REQUEST)) return false;
    if (!createAndPrepareRequest(SELECT_ALL_SYNCS_BY_DRIVE_REQUEST_ID, SELECT_ALL_SYNCS_BY_DRIVE_REQUEST)) return false;
    // Exclusion Template
    if (!createAndPrepareRequest(INSERT_EXCLUSION_TEMPLATE_REQUEST_ID, INSERT_EXCLUSION_TEMPLATE_REQUEST)) return false;
    if (!createAndPrepareRequest(UPDATE_EXCLUSION_TEMPLATE_REQUEST_ID, UPDATE_EXCLUSION_TEMPLATE_REQUEST)) return false;
    if (!createAndPrepareRequest(DELETE_EXCLUSION_TEMPLATE_REQUEST_ID, DELETE_EXCLUSION_TEMPLATE_REQUEST)) return false;
    if (!createAndPrepareRequest(DELETE_ALL_EXCLUSION_TEMPLATE_BY_DEF_REQUEST_ID, DELETE_ALL_EXCLUSION_TEMPLATE_BY_DEF_REQUEST))
        return false;
    if (!createAndPrepareRequest(SELECT_ALL_EXCLUSION_TEMPLATE_REQUEST_ID, SELECT_ALL_EXCLUSION_TEMPLATE_REQUEST)) return false;
    if (!createAndPrepareRequest(SELECT_ALL_EXCLUSION_TEMPLATE_BY_DEF_REQUEST_ID, SELECT_ALL_EXCLUSION_TEMPLATE_BY_DEF_REQUEST))
        return false;
#if defined(KD_MACOS)
    // Exclusion App
    if (!createAndPrepareRequest(INSERT_EXCLUSION_APP_REQUEST_ID, INSERT_EXCLUSION_APP_REQUEST)) return false;
    if (!createAndPrepareRequest(UPDATE_EXCLUSION_APP_REQUEST_ID, UPDATE_EXCLUSION_APP_REQUEST)) return false;
    if (!createAndPrepareRequest(DELETE_EXCLUSION_APP_REQUEST_ID, DELETE_EXCLUSION_APP_REQUEST)) return false;
    if (!createAndPrepareRequest(DELETE_ALL_EXCLUSION_APP_BY_DEF_REQUEST_ID, DELETE_ALL_EXCLUSION_APP_BY_DEF_REQUEST))
        return false;
    if (!createAndPrepareRequest(SELECT_ALL_EXCLUSION_APP_REQUEST_ID, SELECT_ALL_EXCLUSION_APP_REQUEST)) return false;
    if (!createAndPrepareRequest(SELECT_ALL_EXCLUSION_APP_BY_DEF_REQUEST_ID, SELECT_ALL_EXCLUSION_APP_BY_DEF_REQUEST))
        return false;
#endif
    // Error
    if (!createAndPrepareRequest(INSERT_ERROR_REQUEST_ID, INSERT_ERROR_REQUEST)) return false;
    if (!createAndPrepareRequest(UPDATE_ERROR_REQUEST_ID, UPDATE_ERROR_REQUEST)) return false;
    if (!createAndPrepareRequest(DELETE_ALL_ERROR_BY_EXITCODE_REQUEST_ID, DELETE_ALL_ERROR_BY_EXITCODE_REQUEST)) return false;
    if (!createAndPrepareRequest(DELETE_ALL_ERROR_BY_EXITCAUSEREQUEST_ID, DELETE_ALL_ERROR_BY_EXITCAUSEREQUEST)) return false;
    if (!createAndPrepareRequest(DELETE_ALL_ERROR_BY_LEVEL_REQUEST_ID, DELETE_ALL_ERROR_BY_LEVEL_REQUEST)) return false;
    if (!createAndPrepareRequest(DELETE_ERROR_BY_DBID_REQUEST_ID, DELETE_ERROR_BY_DBID_REQUEST)) return false;
    if (!createAndPrepareRequest(SELECT_ALL_ERROR_BY_LEVEL_AND_SYNCDBID_REQUEST_ID,
                                 SELECT_ALL_ERROR_BY_LEVEL_AND_SYNCDBID_REQUEST))
        return false;
    if (!createAndPrepareRequest(SELECT_ALL_CONFLICTS_BY_SYNCDBID_REQUEST_ID, SELECT_ALL_CONFLICTS_BY_SYNCDBID_REQUEST))
        return false;
    if (!createAndPrepareRequest(SELECT_FILTERED_CONFLICTS_BY_SYNCDBID_REQUEST_ID, SELECT_FILTERED_CONFLICTS_BY_SYNCDBID_REQUEST))
        return false;
    // Migration old selectivesync table
    if (!createAndPrepareRequest(INSERT_MIGRATION_SELECTIVESYNC_REQUEST_ID, INSERT_MIGRATION_SELECTIVESYNC_REQUEST)) return false;
    if (!createAndPrepareRequest(SELECT_ALL_MIGRATION_SELECTIVESYNC_REQUEST_ID, SELECT_ALL_MIGRATION_SELECTIVESYNC_REQUEST))
        return false;
    // App state
    if (!prepareAppState()) {
        LOG_WARN(_logger, "Error in createAndPrepareRequestAppState");
        return false;
    }

    if (!initData()) {
        LOG_WARN(_logger, "Error in initParameters");
        return false;
    }

    return true;
}

bool ParmsDb::upgradeTables() {
    const std::string tableName = "parameters";
    std::string columnName = "maxAllowedCpu";
    if (!addIntegerColumnIfMissing(tableName, columnName)) {
        return false;
    }

    bool updateParameters = false;
    columnName = "uploadSessionParallelJobs";
    if (!addIntegerColumnIfMissing(tableName, columnName, &updateParameters)) {
        return false;
    }

    columnName = "jobPoolCapacityFactor";
    if (!addIntegerColumnIfMissing(tableName, columnName, &updateParameters)) {
        return false;
    }

    if (updateParameters) {
        int errId = 0;
        std::string error;

        if (!createAndPrepareRequest(UPDATE_PARAMETERS_JOB_REQUEST_ID, UPDATE_PARAMETERS_JOB_REQUEST)) return false;
        LOG_IF_FAIL(queryResetAndClearBindings(UPDATE_PARAMETERS_JOB_REQUEST_ID));
        LOG_IF_FAIL(queryBindValue(UPDATE_PARAMETERS_JOB_REQUEST_ID, 1, Parameters::_uploadSessionParallelJobsDefault));
        LOG_IF_FAIL(queryBindValue(UPDATE_PARAMETERS_JOB_REQUEST_ID, 2, 0));
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

    columnName = "distributionChannel";
    if (!addIntegerColumnIfMissing(tableName, columnName)) {
        return false;
    }

    bool exist = false;
    if (!tableExists("app_state", exist)) return false;
    if (!exist) {
        if (!createAppState()) {
            LOG_WARN(_logger, "Error in createAppState");
            return false;
        }
    }

    // Add localNodeId to sync table
    if (!addTextColumnIfMissing("sync", "localNodeId")) {
        return false;
    }

    LOG_INFO(_logger, "Columns upgrade in " << dbType() << " successfully completed.");

    return true;
}

bool ParmsDb::upgrade(const std::string &fromVersion, const std::string &toVersion) {
    LOG_INFO(_logger, "Apply generic upgrade fixes to " << dbType() << " DB version " << fromVersion);
    if (!upgradeTables()) {
        LOG_WARN(_logger, "Failed to insert missing columns.");
        return false;
    }

    if (CommonUtility::isVersionLower(fromVersion, toVersion)) {
        LOG_INFO(_logger, "Upgrade " << dbType() << " DB from " << fromVersion << " to " << toVersion);
        if (!insertUserTemplateNormalizations(fromVersion)) {
            LOG_WARN(_logger, "Insertion of the normalizations of user exclusion file patterns has failed.");
            return false;
        }
#if defined(KD_WINDOWS)
        if (!replaceShortDbPathsWithLongPaths()) {
            LOG_WARN(_logger, "Failed to replace short DB paths with long ones.");
        }
#endif
    }
    LOG_INFO(_logger, "Upgrade " << dbType() << " successfully completed.");

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

#if defined(KD_MACOS)
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

    LOG_IF_FAIL(queryResetAndClearBindings(UPDATE_PARAMETERS_REQUEST_ID));
    LOG_IF_FAIL(queryBindValue(UPDATE_PARAMETERS_REQUEST_ID, 1, static_cast<int>(parameters.language())));
    LOG_IF_FAIL(queryBindValue(UPDATE_PARAMETERS_REQUEST_ID, 2, parameters.monoIcons()));
    LOG_IF_FAIL(queryBindValue(UPDATE_PARAMETERS_REQUEST_ID, 3, parameters.autoStart()));
    LOG_IF_FAIL(queryBindValue(UPDATE_PARAMETERS_REQUEST_ID, 4, parameters.moveToTrash()));
    LOG_IF_FAIL(queryBindValue(UPDATE_PARAMETERS_REQUEST_ID, 5, static_cast<int>(parameters.notificationsDisabled())));
    LOG_IF_FAIL(queryBindValue(UPDATE_PARAMETERS_REQUEST_ID, 6, parameters.useLog()));
    LOG_IF_FAIL(queryBindValue(UPDATE_PARAMETERS_REQUEST_ID, 7, static_cast<int>(parameters.logLevel())));
    LOG_IF_FAIL(queryBindValue(UPDATE_PARAMETERS_REQUEST_ID, 8, parameters.purgeOldLogs()));
    LOG_IF_FAIL(queryBindValue(UPDATE_PARAMETERS_REQUEST_ID, 9, 0)); // Sync hidden files : not used anymore
    LOG_IF_FAIL(queryBindValue(UPDATE_PARAMETERS_REQUEST_ID, 10, static_cast<int>(parameters.proxyConfig().type())));
    LOG_IF_FAIL(queryBindValue(UPDATE_PARAMETERS_REQUEST_ID, 11, parameters.proxyConfig().hostName()));
    LOG_IF_FAIL(queryBindValue(UPDATE_PARAMETERS_REQUEST_ID, 12, parameters.proxyConfig().port()));
    LOG_IF_FAIL(queryBindValue(UPDATE_PARAMETERS_REQUEST_ID, 13, parameters.proxyConfig().needsAuth()));
    LOG_IF_FAIL(queryBindValue(UPDATE_PARAMETERS_REQUEST_ID, 14, parameters.proxyConfig().user()));
    LOG_IF_FAIL(queryBindValue(UPDATE_PARAMETERS_REQUEST_ID, 15, parameters.proxyConfig().token()));
    LOG_IF_FAIL(queryBindValue(UPDATE_PARAMETERS_REQUEST_ID, 16, parameters.useBigFolderSizeLimit()));
    LOG_IF_FAIL(queryBindValue(UPDATE_PARAMETERS_REQUEST_ID, 17, parameters.bigFolderSizeLimit()));
    LOG_IF_FAIL(queryBindValue(UPDATE_PARAMETERS_REQUEST_ID, 18, parameters.darkTheme()));
    LOG_IF_FAIL(queryBindValue(UPDATE_PARAMETERS_REQUEST_ID, 19, parameters.showShortcuts()));
    LOG_IF_FAIL(queryBindValue(UPDATE_PARAMETERS_REQUEST_ID, 20, parameters.updateFileAvailable()));
    LOG_IF_FAIL(queryBindValue(UPDATE_PARAMETERS_REQUEST_ID, 21, parameters.updateTargetVersion()));
    LOG_IF_FAIL(queryBindValue(UPDATE_PARAMETERS_REQUEST_ID, 22, parameters.updateTargetVersionString()));
    LOG_IF_FAIL(queryBindValue(UPDATE_PARAMETERS_REQUEST_ID, 23, parameters.autoUpdateAttempted()));
    LOG_IF_FAIL(queryBindValue(UPDATE_PARAMETERS_REQUEST_ID, 24, parameters.seenVersion()));
    LOG_IF_FAIL(queryBindValue(UPDATE_PARAMETERS_REQUEST_ID, 25, parameters.dialogGeometry()));
    LOG_IF_FAIL(queryBindValue(UPDATE_PARAMETERS_REQUEST_ID, 26, static_cast<int>(parameters.extendedLog())));
    LOG_IF_FAIL(queryBindValue(UPDATE_PARAMETERS_REQUEST_ID, 27, parameters.maxAllowedCpu()));
    LOG_IF_FAIL(queryBindValue(UPDATE_PARAMETERS_REQUEST_ID, 28, parameters.uploadSessionParallelJobs()));
    LOG_IF_FAIL(queryBindValue(UPDATE_PARAMETERS_REQUEST_ID, 29, 0));
    LOG_IF_FAIL(queryBindValue(UPDATE_PARAMETERS_REQUEST_ID, 30, static_cast<int>(parameters.distributionChannel())));

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

    LOG_IF_FAIL(queryResetAndClearBindings(SELECT_PARAMETERS_REQUEST_ID));
    if (!queryNext(SELECT_PARAMETERS_REQUEST_ID, found)) {
        LOG_WARN(_logger, "Error getting query result: " << SELECT_PARAMETERS_REQUEST_ID);
        return false;
    }
    if (!found) {
        return true;
    }

    int intResult;
    LOG_IF_FAIL(queryIntValue(SELECT_PARAMETERS_REQUEST_ID, 0, intResult));
    parameters.setLanguage(static_cast<Language>(intResult));

    LOG_IF_FAIL(queryIntValue(SELECT_PARAMETERS_REQUEST_ID, 1, intResult));
    parameters.setMonoIcons(intResult);

    LOG_IF_FAIL(queryIntValue(SELECT_PARAMETERS_REQUEST_ID, 2, intResult));
    parameters.setAutoStart(intResult);

    LOG_IF_FAIL(queryIntValue(SELECT_PARAMETERS_REQUEST_ID, 3, intResult));
    parameters.setMoveToTrash(intResult);

    LOG_IF_FAIL(queryIntValue(SELECT_PARAMETERS_REQUEST_ID, 4, intResult));
    parameters.setNotificationsDisabled(static_cast<NotificationsDisabled>(intResult));

    LOG_IF_FAIL(queryIntValue(SELECT_PARAMETERS_REQUEST_ID, 5, intResult));
    parameters.setUseLog(intResult);

    LOG_IF_FAIL(queryIntValue(SELECT_PARAMETERS_REQUEST_ID, 6, intResult));
    parameters.setLogLevel(static_cast<LogLevel>(intResult));

    LOG_IF_FAIL(queryIntValue(SELECT_PARAMETERS_REQUEST_ID, 7, intResult));
    parameters.setPurgeOldLogs(intResult);

    // Sync hidden files (8): not used anymore

    auto proxyType = ProxyType::Undefined;
    std::string hostName;
    int port = 0;
    bool needsAuth = false;
    std::string user;
    std::string token;
    LOG_IF_FAIL(queryIntValue(SELECT_PARAMETERS_REQUEST_ID, 9, intResult));
    proxyType = static_cast<ProxyType>(intResult);
    LOG_IF_FAIL(queryStringValue(SELECT_PARAMETERS_REQUEST_ID, 10, hostName));
    LOG_IF_FAIL(queryIntValue(SELECT_PARAMETERS_REQUEST_ID, 11, port));
    LOG_IF_FAIL(queryIntValue(SELECT_PARAMETERS_REQUEST_ID, 12, intResult));
    needsAuth = static_cast<bool>(intResult);
    LOG_IF_FAIL(queryStringValue(SELECT_PARAMETERS_REQUEST_ID, 13, user));
    LOG_IF_FAIL(queryStringValue(SELECT_PARAMETERS_REQUEST_ID, 14, token));
    parameters.setProxyConfig(ProxyConfig(proxyType, hostName, port, needsAuth, user, token));

    bool useBigFolderSizeLimit = false;
    LOG_IF_FAIL(queryIntValue(SELECT_PARAMETERS_REQUEST_ID, 15, intResult));
    useBigFolderSizeLimit = static_cast<bool>(intResult);
    parameters.setUseBigFolderSizeLimit(useBigFolderSizeLimit);

    int64_t int64Result = 0;
    LOG_IF_FAIL(queryInt64Value(SELECT_PARAMETERS_REQUEST_ID, 16, int64Result));
    parameters.setBigFolderSizeLimit(int64Result);

    LOG_IF_FAIL(queryIntValue(SELECT_PARAMETERS_REQUEST_ID, 17, intResult));
    parameters.setDarkTheme(intResult);

    LOG_IF_FAIL(queryIntValue(SELECT_PARAMETERS_REQUEST_ID, 18, intResult));
    parameters.setShowShortcuts(intResult);

    std::string strResult;
    LOG_IF_FAIL(queryStringValue(SELECT_PARAMETERS_REQUEST_ID, 19, strResult));
    parameters.setUpdateFileAvailable(strResult);

    LOG_IF_FAIL(queryStringValue(SELECT_PARAMETERS_REQUEST_ID, 20, strResult));
    parameters.setUpdateTargetVersion(strResult);

    LOG_IF_FAIL(queryStringValue(SELECT_PARAMETERS_REQUEST_ID, 21, strResult));
    parameters.setUpdateTargetVersionString(strResult);

    LOG_IF_FAIL(queryIntValue(SELECT_PARAMETERS_REQUEST_ID, 22, intResult));
    parameters.setAutoUpdateAttempted(intResult);

    LOG_IF_FAIL(queryStringValue(SELECT_PARAMETERS_REQUEST_ID, 23, strResult));
    parameters.setSeenVersion(strResult);

    std::shared_ptr<std::vector<char>> blobResult;
    LOG_IF_FAIL(queryBlobValue(SELECT_PARAMETERS_REQUEST_ID, 24, blobResult));
    parameters.setDialogGeometry(blobResult);

    LOG_IF_FAIL(queryIntValue(SELECT_PARAMETERS_REQUEST_ID, 25, intResult));
    parameters.setExtendedLog(intResult);

    LOG_IF_FAIL(queryIntValue(SELECT_PARAMETERS_REQUEST_ID, 26, intResult));
    parameters.setMaxAllowedCpu(intResult);

    LOG_IF_FAIL(queryIntValue(SELECT_PARAMETERS_REQUEST_ID, 27, intResult));
    parameters.setUploadSessionParallelJobs(intResult);

    // Job pool capacity factor (28): not used anymore

    LOG_IF_FAIL(queryIntValue(SELECT_PARAMETERS_REQUEST_ID, 29, intResult));
    parameters.setDistributionChannel(static_cast<VersionChannel>(intResult));

    LOG_IF_FAIL(queryResetAndClearBindings(SELECT_PARAMETERS_REQUEST_ID));

    return true;
}

bool ParmsDb::insertUser(const User &user) {
    const std::scoped_lock lock(_mutex);

    int errId;
    std::string error;

    LOG_IF_FAIL(queryResetAndClearBindings(INSERT_USER_REQUEST_ID));
    LOG_IF_FAIL(queryBindValue(INSERT_USER_REQUEST_ID, 1, user.dbId()));
    LOG_IF_FAIL(queryBindValue(INSERT_USER_REQUEST_ID, 2, user.userId()));
    LOG_IF_FAIL(queryBindValue(INSERT_USER_REQUEST_ID, 3, user.keychainKey()));
    LOG_IF_FAIL(queryBindValue(INSERT_USER_REQUEST_ID, 4, user.name()));
    LOG_IF_FAIL(queryBindValue(INSERT_USER_REQUEST_ID, 5, user.email()));
    LOG_IF_FAIL(queryBindValue(INSERT_USER_REQUEST_ID, 6, user.avatarUrl()));
    LOG_IF_FAIL(queryBindValue(INSERT_USER_REQUEST_ID, 7, user.avatar()));
    LOG_IF_FAIL(queryBindValue(INSERT_USER_REQUEST_ID, 8, user.toMigrate()));
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

    LOG_IF_FAIL(queryResetAndClearBindings(UPDATE_USER_REQUEST_ID));
    LOG_IF_FAIL(queryBindValue(UPDATE_USER_REQUEST_ID, 1, user.userId()));
    LOG_IF_FAIL(queryBindValue(UPDATE_USER_REQUEST_ID, 2, user.keychainKey()));
    LOG_IF_FAIL(queryBindValue(UPDATE_USER_REQUEST_ID, 3, user.name()));
    LOG_IF_FAIL(queryBindValue(UPDATE_USER_REQUEST_ID, 4, user.email()));
    LOG_IF_FAIL(queryBindValue(UPDATE_USER_REQUEST_ID, 5, user.avatarUrl()));
    LOG_IF_FAIL(queryBindValue(UPDATE_USER_REQUEST_ID, 6, user.avatar()));
    LOG_IF_FAIL(queryBindValue(UPDATE_USER_REQUEST_ID, 7, user.toMigrate()));
    LOG_IF_FAIL(queryBindValue(UPDATE_USER_REQUEST_ID, 8, user.dbId()));
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

    LOG_IF_FAIL(queryResetAndClearBindings(DELETE_USER_REQUEST_ID));
    LOG_IF_FAIL(queryBindValue(DELETE_USER_REQUEST_ID, 1, dbId));
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

    LOG_IF_FAIL(queryResetAndClearBindings(SELECT_USER_REQUEST_ID));
    LOG_IF_FAIL(queryBindValue(SELECT_USER_REQUEST_ID, 1, dbId));
    if (!queryNext(SELECT_USER_REQUEST_ID, found)) {
        LOG_WARN(_logger, "Error getting query result: " << SELECT_USER_REQUEST_ID);
        return false;
    }
    if (!found) {
        return true;
    }

    user.setDbId(dbId);

    int intResult;
    LOG_IF_FAIL(queryIntValue(SELECT_USER_REQUEST_ID, 0, intResult));
    user.setUserId(intResult);

    std::string strResult;
    LOG_IF_FAIL(queryStringValue(SELECT_USER_REQUEST_ID, 1, strResult));
    user.setKeychainKey(strResult);

    LOG_IF_FAIL(queryStringValue(SELECT_USER_REQUEST_ID, 2, strResult));
    user.setName(strResult);

    LOG_IF_FAIL(queryStringValue(SELECT_USER_REQUEST_ID, 3, strResult));
    user.setEmail(strResult);

    LOG_IF_FAIL(queryStringValue(SELECT_USER_REQUEST_ID, 4, strResult));
    user.setAvatarUrl(strResult);

    std::shared_ptr<std::vector<char>> blobResult;
    LOG_IF_FAIL(queryBlobValue(SELECT_USER_REQUEST_ID, 5, blobResult));
    user.setAvatar(blobResult);

    LOG_IF_FAIL(queryIntValue(SELECT_USER_REQUEST_ID, 6, intResult));
    user.setToMigrate(static_cast<bool>(intResult));

    LOG_IF_FAIL(queryResetAndClearBindings(SELECT_USER_REQUEST_ID));

    return true;
}

bool ParmsDb::selectUserByUserId(int userId, User &user, bool &found) {
    const std::scoped_lock lock(_mutex);

    LOG_IF_FAIL(queryResetAndClearBindings(SELECT_USER_BY_USERID_REQUEST_ID));
    LOG_IF_FAIL(queryBindValue(SELECT_USER_BY_USERID_REQUEST_ID, 1, userId));
    if (!queryNext(SELECT_USER_BY_USERID_REQUEST_ID, found)) {
        LOG_WARN(_logger, "Error getting query result: " << SELECT_USER_BY_USERID_REQUEST_ID);
        return false;
    }
    if (!found) {
        return true;
    }

    user.setUserId(userId);

    int intResult;
    LOG_IF_FAIL(queryIntValue(SELECT_USER_BY_USERID_REQUEST_ID, 0, intResult));
    user.setDbId(intResult);

    std::string strResult;
    LOG_IF_FAIL(queryStringValue(SELECT_USER_BY_USERID_REQUEST_ID, 1, strResult));
    user.setKeychainKey(strResult);

    LOG_IF_FAIL(queryStringValue(SELECT_USER_BY_USERID_REQUEST_ID, 2, strResult));
    user.setName(strResult);

    LOG_IF_FAIL(queryStringValue(SELECT_USER_BY_USERID_REQUEST_ID, 3, strResult));
    user.setEmail(strResult);

    LOG_IF_FAIL(queryStringValue(SELECT_USER_BY_USERID_REQUEST_ID, 4, strResult));
    user.setAvatarUrl(strResult);

    std::shared_ptr<std::vector<char>> blobResult;
    LOG_IF_FAIL(queryBlobValue(SELECT_USER_BY_USERID_REQUEST_ID, 5, blobResult));
    user.setAvatar(blobResult);

    LOG_IF_FAIL(queryIntValue(SELECT_USER_BY_USERID_REQUEST_ID, 6, intResult));
    user.setToMigrate(static_cast<bool>(intResult));

    LOG_IF_FAIL(queryResetAndClearBindings(SELECT_USER_BY_USERID_REQUEST_ID));

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

bool ParmsDb::selectLastConnectedUser(User &user, bool &found) {
    const std::scoped_lock lock(_mutex);

    LOG_IF_FAIL(queryResetAndClearBindings(SELECT_LAST_CONNECTED_USER_REQUEST_ID));
    if (!queryNext(SELECT_LAST_CONNECTED_USER_REQUEST_ID, found)) {
        LOG_WARN(_logger, "Error getting query result: " << SELECT_LAST_CONNECTED_USER_REQUEST_ID);
        return false;
    }
    if (!found) {
        return true;
    }

    int intResult;
    LOG_IF_FAIL(queryIntValue(SELECT_LAST_CONNECTED_USER_REQUEST_ID, 0, intResult));
    user.setDbId(intResult);

    LOG_IF_FAIL(queryIntValue(SELECT_LAST_CONNECTED_USER_REQUEST_ID, 1, intResult));
    user.setUserId(intResult);

    std::string strResult;
    LOG_IF_FAIL(queryStringValue(SELECT_LAST_CONNECTED_USER_REQUEST_ID, 2, strResult));
    user.setKeychainKey(strResult);

    LOG_IF_FAIL(queryStringValue(SELECT_LAST_CONNECTED_USER_REQUEST_ID, 3, strResult));
    user.setName(strResult);

    LOG_IF_FAIL(queryStringValue(SELECT_LAST_CONNECTED_USER_REQUEST_ID, 4, strResult));
    user.setEmail(strResult);

    LOG_IF_FAIL(queryStringValue(SELECT_LAST_CONNECTED_USER_REQUEST_ID, 5, strResult));
    user.setAvatarUrl(strResult);

    std::shared_ptr<std::vector<char>> blobResult;
    LOG_IF_FAIL(queryBlobValue(SELECT_LAST_CONNECTED_USER_REQUEST_ID, 6, blobResult));
    user.setAvatar(blobResult);

    LOG_IF_FAIL(queryIntValue(SELECT_LAST_CONNECTED_USER_REQUEST_ID, 7, intResult));
    user.setToMigrate(static_cast<bool>(intResult));

    LOG_IF_FAIL(queryResetAndClearBindings(SELECT_LAST_CONNECTED_USER_REQUEST_ID));

    return true;
}

bool ParmsDb::selectAllUsers(std::vector<User> &userList) {
    const std::scoped_lock lock(_mutex);

    userList.clear();

    LOG_IF_FAIL(queryResetAndClearBindings(SELECT_ALL_USERS_REQUEST_ID));

    bool found;
    for (;;) {
        if (!queryNext(SELECT_ALL_USERS_REQUEST_ID, found)) {
            LOG_WARN(_logger, "Error getting query result: " << SELECT_ALL_USERS_REQUEST_ID);
            return false;
        }
        if (!found) {
            break;
        }

        int id;
        LOG_IF_FAIL(queryIntValue(SELECT_ALL_USERS_REQUEST_ID, 0, id));
        int userId;
        LOG_IF_FAIL(queryIntValue(SELECT_ALL_USERS_REQUEST_ID, 1, userId));
        std::string keychainKey;
        LOG_IF_FAIL(queryStringValue(SELECT_ALL_USERS_REQUEST_ID, 2, keychainKey));
        std::string name;
        LOG_IF_FAIL(queryStringValue(SELECT_ALL_USERS_REQUEST_ID, 3, name));
        std::string email;
        LOG_IF_FAIL(queryStringValue(SELECT_ALL_USERS_REQUEST_ID, 4, email));
        std::string avatarUrl;
        LOG_IF_FAIL(queryStringValue(SELECT_ALL_USERS_REQUEST_ID, 5, avatarUrl));
        std::shared_ptr<std::vector<char>> avatar;
        LOG_IF_FAIL(queryBlobValue(SELECT_ALL_USERS_REQUEST_ID, 6, avatar));
        int toMigrate;
        LOG_IF_FAIL(queryIntValue(SELECT_ALL_USERS_REQUEST_ID, 7, toMigrate));

        userList.push_back(User(id, userId, keychainKey, name, email, avatarUrl, avatar, static_cast<bool>(toMigrate)));
    }
    LOG_IF_FAIL(queryResetAndClearBindings(SELECT_ALL_USERS_REQUEST_ID));

    return true;
}

bool ParmsDb::getNewUserDbId(int &dbId) {
    std::vector<User> userList;
    if (!selectAllUsers(userList)) {
        LOG_WARN(_logger, "Error in selectAllUsers");
        return false;
    }

    dbId = 1;
    for (const User &user: userList) {
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

    LOG_IF_FAIL(queryResetAndClearBindings(INSERT_ACCOUNT_REQUEST_ID));
    LOG_IF_FAIL(queryBindValue(INSERT_ACCOUNT_REQUEST_ID, 1, account.dbId()));
    LOG_IF_FAIL(queryBindValue(INSERT_ACCOUNT_REQUEST_ID, 2, account.accountId()));
    LOG_IF_FAIL(queryBindValue(INSERT_ACCOUNT_REQUEST_ID, 3, account.userDbId()));
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

    LOG_IF_FAIL(queryResetAndClearBindings(UPDATE_ACCOUNT_REQUEST_ID));
    LOG_IF_FAIL(queryBindValue(UPDATE_ACCOUNT_REQUEST_ID, 1, account.accountId()));
    LOG_IF_FAIL(queryBindValue(UPDATE_ACCOUNT_REQUEST_ID, 2, account.userDbId()));
    LOG_IF_FAIL(queryBindValue(UPDATE_ACCOUNT_REQUEST_ID, 3, account.dbId()));
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

    LOG_IF_FAIL(queryResetAndClearBindings(DELETE_ACCOUNT_REQUEST_ID));
    LOG_IF_FAIL(queryBindValue(DELETE_ACCOUNT_REQUEST_ID, 1, dbId));
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

    LOG_IF_FAIL(queryResetAndClearBindings(SELECT_ACCOUNT_REQUEST_ID));
    LOG_IF_FAIL(queryBindValue(SELECT_ACCOUNT_REQUEST_ID, 1, dbId));
    if (!queryNext(SELECT_ACCOUNT_REQUEST_ID, found)) {
        LOG_WARN(_logger, "Error getting query result: " << SELECT_ACCOUNT_REQUEST_ID);
        return false;
    }
    if (!found) {
        return true;
    }

    account.setDbId(dbId);

    int intResult;
    LOG_IF_FAIL(queryIntValue(SELECT_ACCOUNT_REQUEST_ID, 0, intResult));
    account.setAccountId(intResult);

    LOG_IF_FAIL(queryIntValue(SELECT_ACCOUNT_REQUEST_ID, 1, intResult));
    account.setUserDbId(intResult);

    LOG_IF_FAIL(queryResetAndClearBindings(SELECT_ACCOUNT_REQUEST_ID));

    return true;
}

bool ParmsDb::selectAllAccounts(std::vector<Account> &accountList) {
    const std::scoped_lock lock(_mutex);

    accountList.clear();

    LOG_IF_FAIL(queryResetAndClearBindings(SELECT_ALL_ACCOUNTS_REQUEST_ID));

    bool found;
    for (;;) {
        if (!queryNext(SELECT_ALL_ACCOUNTS_REQUEST_ID, found)) {
            LOG_WARN(_logger, "Error getting query result: " << SELECT_ALL_ACCOUNTS_REQUEST_ID);
            return false;
        }
        if (!found) {
            break;
        }

        int id;
        LOG_IF_FAIL(queryIntValue(SELECT_ALL_ACCOUNTS_REQUEST_ID, 0, id));
        int accountId;
        LOG_IF_FAIL(queryIntValue(SELECT_ALL_ACCOUNTS_REQUEST_ID, 1, accountId));
        int userDbId;
        LOG_IF_FAIL(queryIntValue(SELECT_ALL_ACCOUNTS_REQUEST_ID, 2, userDbId));

        accountList.push_back(Account(id, accountId, userDbId));
    }
    LOG_IF_FAIL(queryResetAndClearBindings(SELECT_ALL_ACCOUNTS_REQUEST_ID));

    return true;
}

bool ParmsDb::selectAllAccounts(int userDbId, std::vector<Account> &accountList) {
    const std::scoped_lock lock(_mutex);

    accountList.clear();

    LOG_IF_FAIL(queryResetAndClearBindings(SELECT_ALL_ACCOUNTS_BY_USER_REQUEST_ID));
    LOG_IF_FAIL(queryBindValue(SELECT_ALL_ACCOUNTS_BY_USER_REQUEST_ID, 1, userDbId));

    bool found;
    for (;;) {
        if (!queryNext(SELECT_ALL_ACCOUNTS_BY_USER_REQUEST_ID, found)) {
            LOG_WARN(_logger, "Error getting query result: " << SELECT_ALL_ACCOUNTS_BY_USER_REQUEST_ID);
            return false;
        }
        if (!found) {
            break;
        }

        int id;
        LOG_IF_FAIL(queryIntValue(SELECT_ALL_ACCOUNTS_BY_USER_REQUEST_ID, 0, id));
        int accountId;
        LOG_IF_FAIL(queryIntValue(SELECT_ALL_ACCOUNTS_BY_USER_REQUEST_ID, 1, accountId));

        accountList.push_back(Account(id, accountId, userDbId));
    }
    LOG_IF_FAIL(queryResetAndClearBindings(SELECT_ALL_ACCOUNTS_BY_USER_REQUEST_ID));

    return true;
}

bool ParmsDb::accountDbId(int userDbId, int accountId, int &dbId) {
    std::vector<Account> accountList;
    if (!selectAllAccounts(userDbId, accountList)) {
        LOG_WARN(_logger, "Error in selectAllAccounts");
        return false;
    }

    dbId = 0;
    for (const Account &account: accountList) {
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
    for (const Account &account: accountList) {
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

    LOG_IF_FAIL(queryResetAndClearBindings(INSERT_DRIVE_REQUEST_ID));
    LOG_IF_FAIL(queryBindValue(INSERT_DRIVE_REQUEST_ID, 1, drive.dbId()));
    LOG_IF_FAIL(queryBindValue(INSERT_DRIVE_REQUEST_ID, 2, drive.driveId()));
    LOG_IF_FAIL(queryBindValue(INSERT_DRIVE_REQUEST_ID, 3, drive.accountDbId()));
    LOG_IF_FAIL(queryBindValue(INSERT_DRIVE_REQUEST_ID, 4, drive.name()));
    LOG_IF_FAIL(queryBindValue(INSERT_DRIVE_REQUEST_ID, 5, drive.size()));
    LOG_IF_FAIL(queryBindValue(INSERT_DRIVE_REQUEST_ID, 6, drive.color()));
    LOG_IF_FAIL(queryBindValue(INSERT_DRIVE_REQUEST_ID, 7, drive.notifications()));
    LOG_IF_FAIL(queryBindValue(INSERT_DRIVE_REQUEST_ID, 8, drive.admin()));
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

    LOG_IF_FAIL(queryResetAndClearBindings(UPDATE_DRIVE_REQUEST_ID));
    LOG_IF_FAIL(queryBindValue(UPDATE_DRIVE_REQUEST_ID, 1, drive.driveId()));
    LOG_IF_FAIL(queryBindValue(UPDATE_DRIVE_REQUEST_ID, 2, drive.accountDbId()));
    LOG_IF_FAIL(queryBindValue(UPDATE_DRIVE_REQUEST_ID, 3, drive.name()));
    LOG_IF_FAIL(queryBindValue(UPDATE_DRIVE_REQUEST_ID, 4, drive.size()));
    LOG_IF_FAIL(queryBindValue(UPDATE_DRIVE_REQUEST_ID, 5, drive.color()));
    LOG_IF_FAIL(queryBindValue(UPDATE_DRIVE_REQUEST_ID, 6, drive.notifications()));
    LOG_IF_FAIL(queryBindValue(UPDATE_DRIVE_REQUEST_ID, 7, drive.admin()));
    LOG_IF_FAIL(queryBindValue(UPDATE_DRIVE_REQUEST_ID, 8, drive.dbId()));
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

    LOG_IF_FAIL(queryResetAndClearBindings(DELETE_DRIVE_REQUEST_ID));
    LOG_IF_FAIL(queryBindValue(DELETE_DRIVE_REQUEST_ID, 1, dbId));
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

    LOG_IF_FAIL(queryResetAndClearBindings(SELECT_DRIVE_REQUEST_ID));
    LOG_IF_FAIL(queryBindValue(SELECT_DRIVE_REQUEST_ID, 1, dbId));
    if (!queryNext(SELECT_DRIVE_REQUEST_ID, found)) {
        LOG_WARN(_logger, "Error getting query result: " << SELECT_DRIVE_REQUEST_ID);
        return false;
    }
    if (!found) {
        return true;
    }

    drive.setDbId(dbId);

    int intResult;
    LOG_IF_FAIL(queryIntValue(SELECT_DRIVE_REQUEST_ID, 0, intResult));
    drive.setDriveId(intResult);

    LOG_IF_FAIL(queryIntValue(SELECT_DRIVE_REQUEST_ID, 1, intResult));
    drive.setAccountDbId(intResult);

    std::string strResult;
    LOG_IF_FAIL(queryStringValue(SELECT_DRIVE_REQUEST_ID, 2, strResult));
    drive.setName(strResult);

    int64_t int64Result;
    LOG_IF_FAIL(queryInt64Value(SELECT_DRIVE_REQUEST_ID, 3, int64Result));
    drive.setSize(int64Result);

    LOG_IF_FAIL(queryStringValue(SELECT_DRIVE_REQUEST_ID, 4, strResult));
    drive.setColor(strResult);

    LOG_IF_FAIL(queryIntValue(SELECT_DRIVE_REQUEST_ID, 5, intResult));
    drive.setNotifications(static_cast<bool>(intResult));

    LOG_IF_FAIL(queryResetAndClearBindings(SELECT_DRIVE_REQUEST_ID));

    return true;
}

bool ParmsDb::selectDriveByDriveId(int driveId, Drive &drive, bool &found) {
    const std::scoped_lock lock(_mutex);

    LOG_IF_FAIL(queryResetAndClearBindings(SELECT_DRIVE_BY_DRIVEID_REQUEST_ID));
    LOG_IF_FAIL(queryBindValue(SELECT_DRIVE_BY_DRIVEID_REQUEST_ID, 1, driveId));
    if (!queryNext(SELECT_DRIVE_BY_DRIVEID_REQUEST_ID, found)) {
        LOG_WARN(_logger, "Error getting query result: " << SELECT_DRIVE_BY_DRIVEID_REQUEST_ID);
        return false;
    }
    if (!found) {
        return true;
    }

    drive.setDriveId(driveId);

    int intResult;
    LOG_IF_FAIL(queryIntValue(SELECT_DRIVE_BY_DRIVEID_REQUEST_ID, 0, intResult));
    drive.setDbId(intResult);

    LOG_IF_FAIL(queryIntValue(SELECT_DRIVE_BY_DRIVEID_REQUEST_ID, 1, intResult));
    drive.setAccountDbId(intResult);

    std::string strResult;
    LOG_IF_FAIL(queryStringValue(SELECT_DRIVE_BY_DRIVEID_REQUEST_ID, 2, strResult));
    drive.setName(strResult);

    int64_t int64Result;
    LOG_IF_FAIL(queryInt64Value(SELECT_DRIVE_BY_DRIVEID_REQUEST_ID, 3, int64Result));
    drive.setSize(int64Result);

    LOG_IF_FAIL(queryStringValue(SELECT_DRIVE_BY_DRIVEID_REQUEST_ID, 4, strResult));
    drive.setColor(strResult);

    LOG_IF_FAIL(queryIntValue(SELECT_DRIVE_BY_DRIVEID_REQUEST_ID, 5, intResult));
    drive.setNotifications(static_cast<bool>(intResult));

    LOG_IF_FAIL(queryResetAndClearBindings(SELECT_DRIVE_BY_DRIVEID_REQUEST_ID));

    return true;
}

bool ParmsDb::selectAllDrives(std::vector<Drive> &driveList) {
    const std::scoped_lock lock(_mutex);

    driveList.clear();

    LOG_IF_FAIL(queryResetAndClearBindings(SELECT_ALL_DRIVES_REQUEST_ID));

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
        LOG_IF_FAIL(queryIntValue(SELECT_ALL_DRIVES_REQUEST_ID, 0, id));
        int driveId;
        LOG_IF_FAIL(queryIntValue(SELECT_ALL_DRIVES_REQUEST_ID, 1, driveId));
        int accountDbId;
        LOG_IF_FAIL(queryIntValue(SELECT_ALL_DRIVES_REQUEST_ID, 2, accountDbId));
        std::string driveName;
        LOG_IF_FAIL(queryStringValue(SELECT_ALL_DRIVES_REQUEST_ID, 3, driveName));
        int64_t size;
        LOG_IF_FAIL(queryInt64Value(SELECT_ALL_DRIVES_REQUEST_ID, 4, size));
        std::string color;
        LOG_IF_FAIL(queryStringValue(SELECT_ALL_DRIVES_REQUEST_ID, 5, color));
        int notifications;
        LOG_IF_FAIL(queryIntValue(SELECT_ALL_DRIVES_REQUEST_ID, 6, notifications));
        int admin;
        LOG_IF_FAIL(queryIntValue(SELECT_ALL_DRIVES_REQUEST_ID, 7, admin));

        driveList.push_back(Drive(id, driveId, accountDbId, driveName, size, color, static_cast<bool>(notifications),
                                  static_cast<bool>(admin)));
    }
    LOG_IF_FAIL(queryResetAndClearBindings(SELECT_ALL_DRIVES_REQUEST_ID));

    return true;
}

bool ParmsDb::selectAllDrives(int accountDbId, std::vector<Drive> &driveList) {
    const std::scoped_lock lock(_mutex);

    driveList.clear();

    LOG_IF_FAIL(queryResetAndClearBindings(SELECT_ALL_DRIVES_BY_ACCOUNT_REQUEST_ID));
    LOG_IF_FAIL(queryBindValue(SELECT_ALL_DRIVES_BY_ACCOUNT_REQUEST_ID, 1, accountDbId));

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
        LOG_IF_FAIL(queryIntValue(SELECT_ALL_DRIVES_BY_ACCOUNT_REQUEST_ID, 0, id));
        int driveId;
        LOG_IF_FAIL(queryIntValue(SELECT_ALL_DRIVES_BY_ACCOUNT_REQUEST_ID, 1, driveId));
        std::string driveName;
        LOG_IF_FAIL(queryStringValue(SELECT_ALL_DRIVES_BY_ACCOUNT_REQUEST_ID, 2, driveName));
        int64_t size;
        LOG_IF_FAIL(queryInt64Value(SELECT_ALL_DRIVES_BY_ACCOUNT_REQUEST_ID, 3, size));
        std::string color;
        LOG_IF_FAIL(queryStringValue(SELECT_ALL_DRIVES_BY_ACCOUNT_REQUEST_ID, 4, color));
        int notifications;
        LOG_IF_FAIL(queryIntValue(SELECT_ALL_DRIVES_BY_ACCOUNT_REQUEST_ID, 5, notifications));
        int admin;
        LOG_IF_FAIL(queryIntValue(SELECT_ALL_DRIVES_BY_ACCOUNT_REQUEST_ID, 6, admin));

        (void) driveList.emplace_back(Drive(id, driveId, accountDbId, driveName, size, color, static_cast<bool>(notifications),
                                            static_cast<bool>(admin)));
    }
    LOG_IF_FAIL(queryResetAndClearBindings(SELECT_ALL_DRIVES_BY_ACCOUNT_REQUEST_ID));

    return true;
}

bool ParmsDb::driveDbId(int accountDbId, int driveId, int &dbId) {
    std::vector<Drive> driveList;
    if (!selectAllDrives(accountDbId, driveList)) {
        LOG_WARN(_logger, "Error in selectAllDrives");
        return false;
    }

    dbId = 0;
    for (const Drive &drive: driveList) {
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
    for (const Drive &drive: driveList) {
        // NB: driveList is sorted by dbId
        if (drive.dbId() > dbId) {
            break;
        }
        dbId++;
    }

    return true;
}

bool ParmsDb::insertSync(const Sync &sync) {
    const char *requestId = INSERT_SYNC_REQUEST_ID;

    const std::scoped_lock lock(_mutex);

    std::string listingCursor;
    int64_t listingCursorTimestamp{0};
    sync.listingCursor(listingCursor, listingCursorTimestamp);

    // Insert sync record
    LOG_IF_FAIL(queryResetAndClearBindings(requestId));
    LOG_IF_FAIL(queryBindValue(requestId, 1, sync.dbId()));
    LOG_IF_FAIL(queryBindValue(requestId, 2, sync.driveDbId()));
    LOG_IF_FAIL(queryBindValue(requestId, 3, sync.localPath().native()));
    LOG_IF_FAIL(queryBindValue(requestId, 4, sync.localNodeId()));
    LOG_IF_FAIL(queryBindValue(requestId, 5, sync.targetPath().native()));
    LOG_IF_FAIL(queryBindValue(requestId, 6, sync.targetNodeId()));
    LOG_IF_FAIL(queryBindValue(requestId, 7, sync.dbPath().native()));
    LOG_IF_FAIL(queryBindValue(requestId, 8, static_cast<int>(sync.paused())));
    LOG_IF_FAIL(queryBindValue(requestId, 9, static_cast<int>(sync.supportVfs())));
    LOG_IF_FAIL(queryBindValue(requestId, 10, static_cast<int>(sync.virtualFileMode())));
    LOG_IF_FAIL(queryBindValue(requestId, 11, static_cast<int>(sync.notificationsDisabled())));
    LOG_IF_FAIL(queryBindValue(requestId, 12, static_cast<int>(sync.hasFullyCompleted())));
    LOG_IF_FAIL(queryBindValue(requestId, 13, sync.navigationPaneClsid()));
    LOG_IF_FAIL(queryBindValue(requestId, 14, listingCursor));
    LOG_IF_FAIL(queryBindValue(requestId, 15, listingCursorTimestamp));

    int errId = -1;
    std::string error;
    if (!queryExec(requestId, errId, error)) {
        LOG_WARN(_logger, "Error running query: " << requestId);
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

    LOG_IF_FAIL(queryResetAndClearBindings(UPDATE_SYNC_REQUEST_ID));
    LOG_IF_FAIL(queryBindValue(UPDATE_SYNC_REQUEST_ID, 1, sync.driveDbId()));
    LOG_IF_FAIL(queryBindValue(UPDATE_SYNC_REQUEST_ID, 2, sync.localPath().native()));
    LOG_IF_FAIL(queryBindValue(UPDATE_SYNC_REQUEST_ID, 3, sync.localNodeId()));
    LOG_IF_FAIL(queryBindValue(UPDATE_SYNC_REQUEST_ID, 4, sync.targetPath().native()));
    LOG_IF_FAIL(queryBindValue(UPDATE_SYNC_REQUEST_ID, 5, sync.targetNodeId()));
    LOG_IF_FAIL(queryBindValue(UPDATE_SYNC_REQUEST_ID, 6, sync.dbPath().native()));
    LOG_IF_FAIL(queryBindValue(UPDATE_SYNC_REQUEST_ID, 7, static_cast<int>(sync.paused())));
    LOG_IF_FAIL(queryBindValue(UPDATE_SYNC_REQUEST_ID, 8, static_cast<int>(sync.supportVfs())));
    LOG_IF_FAIL(queryBindValue(UPDATE_SYNC_REQUEST_ID, 9, static_cast<int>(sync.virtualFileMode())));
    LOG_IF_FAIL(queryBindValue(UPDATE_SYNC_REQUEST_ID, 10, static_cast<int>(sync.notificationsDisabled())));
    LOG_IF_FAIL(queryBindValue(UPDATE_SYNC_REQUEST_ID, 11, static_cast<int>(sync.hasFullyCompleted())));
    LOG_IF_FAIL(queryBindValue(UPDATE_SYNC_REQUEST_ID, 12, sync.navigationPaneClsid()));
    LOG_IF_FAIL(queryBindValue(UPDATE_SYNC_REQUEST_ID, 13, listingCursor));
    LOG_IF_FAIL(queryBindValue(UPDATE_SYNC_REQUEST_ID, 14, listingCursorTimestamp));
    LOG_IF_FAIL(queryBindValue(UPDATE_SYNC_REQUEST_ID, 15, sync.dbId()));
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

    LOG_IF_FAIL(queryResetAndClearBindings(UPDATE_SYNC_PAUSED_REQUEST_ID));
    LOG_IF_FAIL(queryBindValue(UPDATE_SYNC_PAUSED_REQUEST_ID, 1, value));
    LOG_IF_FAIL(queryBindValue(UPDATE_SYNC_PAUSED_REQUEST_ID, 2, dbId));
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

    LOG_IF_FAIL(queryResetAndClearBindings(UPDATE_SYNC_HASFULLYCOMPLETED_REQUEST_ID));
    LOG_IF_FAIL(queryBindValue(UPDATE_SYNC_HASFULLYCOMPLETED_REQUEST_ID, 1, value));
    LOG_IF_FAIL(queryBindValue(UPDATE_SYNC_HASFULLYCOMPLETED_REQUEST_ID, 2, dbId));
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

    LOG_IF_FAIL(queryResetAndClearBindings(DELETE_SYNC_REQUEST_ID));
    LOG_IF_FAIL(queryBindValue(DELETE_SYNC_REQUEST_ID, 1, dbId));
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

void ParmsDb::fillSyncWithQueryResult(Sync &sync, const char *requestId) {
    assert(std::string(requestId) == std::string(SELECT_SYNC_BY_PATH_REQUEST_ID) ||
           std::string(requestId) == std::string(SELECT_SYNC_REQUEST_ID));

    int intResult = -1;
    LOG_IF_FAIL(queryIntValue(requestId, 0, intResult));
    sync.setDbId(intResult);

    LOG_IF_FAIL(queryIntValue(requestId, 1, intResult));
    sync.setDriveDbId(intResult);

    SyncName syncNameResult;
    LOG_IF_FAIL(querySyncNameValue(requestId, 2, syncNameResult));
    sync.setLocalPath(SyncPath(syncNameResult));

    std::string strResult;
    LOG_IF_FAIL(queryStringValue(requestId, 3, strResult));
    sync.setLocalNodeId(strResult);

    LOG_IF_FAIL(querySyncNameValue(requestId, 4, syncNameResult));
    sync.setTargetPath(SyncPath(syncNameResult));

    LOG_IF_FAIL(queryStringValue(requestId, 5, strResult));
    sync.setTargetNodeId(strResult);

    LOG_IF_FAIL(querySyncNameValue(requestId, 6, syncNameResult));
    sync.setDbPath(SyncPath(syncNameResult));

    LOG_IF_FAIL(queryIntValue(requestId, 7, intResult));
    sync.setPaused(static_cast<bool>(intResult));

    LOG_IF_FAIL(queryIntValue(requestId, 8, intResult));
    sync.setSupportVfs(static_cast<bool>(intResult));

    LOG_IF_FAIL(queryIntValue(requestId, 9, intResult));
    sync.setVirtualFileMode(static_cast<VirtualFileMode>(intResult));

    LOG_IF_FAIL(queryIntValue(requestId, 10, intResult));
    sync.setNotificationsDisabled(static_cast<bool>(intResult));

    LOG_IF_FAIL(queryIntValue(requestId, 11, intResult));
    sync.setHasFullyCompleted(static_cast<bool>(intResult));

    LOG_IF_FAIL(queryStringValue(requestId, 12, strResult));
    sync.setNavigationPaneClsid(strResult);

    LOG_IF_FAIL(queryStringValue(requestId, 13, strResult));

    int64_t int64Result;
    LOG_IF_FAIL(queryInt64Value(requestId, 14, int64Result));
    sync.setListingCursor(strResult, int64Result);
}

bool ParmsDb::selectSync(const SyncPath &syncDbPath, Sync &sync, bool &found) {
    static const char *requestId = SELECT_SYNC_BY_PATH_REQUEST_ID;

    const std::scoped_lock lock(_mutex);

    LOG_IF_FAIL(queryResetAndClearBindings(requestId));
    LOG_IF_FAIL(queryBindValue(requestId, 1, syncDbPath));
    if (!queryNext(requestId, found)) {
        LOG_WARN(_logger, "Error getting query result: " << requestId);
        return false;
    }

    if (!found) return true;


    fillSyncWithQueryResult(sync, requestId);

    LOG_IF_FAIL(queryResetAndClearBindings(requestId));

    return true;
}

bool ParmsDb::selectSync(int dbId, Sync &sync, bool &found) {
    static const char *requestId = SELECT_SYNC_REQUEST_ID;

    const std::scoped_lock lock(_mutex);

    LOG_IF_FAIL(queryResetAndClearBindings(requestId));
    LOG_IF_FAIL(queryBindValue(requestId, 1, dbId));
    if (!queryNext(requestId, found)) {
        LOG_WARN(_logger, "Error getting query result: " << requestId);
        return false;
    }

    if (!found) return true;


    fillSyncWithQueryResult(sync, requestId);

    LOG_IF_FAIL(queryResetAndClearBindings(requestId));

    return true;
}

bool ParmsDb::selectAllSyncs(std::vector<Sync> &syncList) {
    const std::scoped_lock lock(_mutex);

    syncList.clear();

    LOG_IF_FAIL(queryResetAndClearBindings(SELECT_ALL_SYNCS_REQUEST_ID));
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
        LOG_IF_FAIL(queryIntValue(SELECT_ALL_SYNCS_REQUEST_ID, 0, id));
        int driveDbId;
        LOG_IF_FAIL(queryIntValue(SELECT_ALL_SYNCS_REQUEST_ID, 1, driveDbId));
        SyncName localPath;
        LOG_IF_FAIL(querySyncNameValue(SELECT_ALL_SYNCS_REQUEST_ID, 2, localPath));
        std::string localNodeId;
        LOG_IF_FAIL(queryStringValue(SELECT_ALL_SYNCS_REQUEST_ID, 3, localNodeId));
        SyncName targetPath;
        LOG_IF_FAIL(querySyncNameValue(SELECT_ALL_SYNCS_REQUEST_ID, 4, targetPath));
        std::string targetNodeId;
        LOG_IF_FAIL(queryStringValue(SELECT_ALL_SYNCS_REQUEST_ID, 5, targetNodeId));
        SyncName dbPath;
        LOG_IF_FAIL(querySyncNameValue(SELECT_ALL_SYNCS_REQUEST_ID, 6, dbPath));
        int paused;
        LOG_IF_FAIL(queryIntValue(SELECT_ALL_SYNCS_REQUEST_ID, 7, paused));
        int supportVfs;
        LOG_IF_FAIL(queryIntValue(SELECT_ALL_SYNCS_REQUEST_ID, 8, supportVfs));
        int virtualFileMode;
        LOG_IF_FAIL(queryIntValue(SELECT_ALL_SYNCS_REQUEST_ID, 9, virtualFileMode));
        int notificationsDisabled;
        LOG_IF_FAIL(queryIntValue(SELECT_ALL_SYNCS_REQUEST_ID, 10, notificationsDisabled));
        int hasFullyCompleted;
        LOG_IF_FAIL(queryIntValue(SELECT_ALL_SYNCS_REQUEST_ID, 11, hasFullyCompleted));
        std::string navigationPaneClsid;
        LOG_IF_FAIL(queryStringValue(SELECT_ALL_SYNCS_REQUEST_ID, 12, navigationPaneClsid));
        std::string listingCursor;
        LOG_IF_FAIL(queryStringValue(SELECT_ALL_SYNCS_REQUEST_ID, 13, listingCursor));
        int64_t listingCursorTimestamp;
        LOG_IF_FAIL(queryInt64Value(SELECT_ALL_SYNCS_REQUEST_ID, 14, listingCursorTimestamp));

        syncList.push_back(Sync(id, driveDbId, SyncPath(localPath), localNodeId, SyncPath(targetPath), targetNodeId,
                                static_cast<bool>(paused), static_cast<bool>(supportVfs),
                                static_cast<VirtualFileMode>(virtualFileMode), static_cast<bool>(notificationsDisabled),
                                SyncPath(dbPath), static_cast<bool>(hasFullyCompleted), navigationPaneClsid, listingCursor,
                                listingCursorTimestamp));
    }
    LOG_IF_FAIL(queryResetAndClearBindings(SELECT_ALL_SYNCS_REQUEST_ID));

    return true;
}

bool ParmsDb::selectAllSyncs(int driveDbId, std::vector<Sync> &syncList) {
    const std::scoped_lock lock(_mutex);

    syncList.clear();

    LOG_IF_FAIL(queryResetAndClearBindings(SELECT_ALL_SYNCS_BY_DRIVE_REQUEST_ID));
    LOG_IF_FAIL(queryBindValue(SELECT_ALL_SYNCS_BY_DRIVE_REQUEST_ID, 1, driveDbId));
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
        LOG_IF_FAIL(queryIntValue(SELECT_ALL_SYNCS_BY_DRIVE_REQUEST_ID, 0, id));
        SyncName localPath;
        LOG_IF_FAIL(querySyncNameValue(SELECT_ALL_SYNCS_BY_DRIVE_REQUEST_ID, 1, localPath));
        std::string localNodeId;
        LOG_IF_FAIL(queryStringValue(SELECT_ALL_SYNCS_BY_DRIVE_REQUEST_ID, 2, localNodeId));
        SyncName targetPath;
        LOG_IF_FAIL(querySyncNameValue(SELECT_ALL_SYNCS_BY_DRIVE_REQUEST_ID, 3, targetPath));
        std::string targetNodeId;
        LOG_IF_FAIL(queryStringValue(SELECT_ALL_SYNCS_BY_DRIVE_REQUEST_ID, 4, targetNodeId));
        SyncName dbPath;
        LOG_IF_FAIL(querySyncNameValue(SELECT_ALL_SYNCS_BY_DRIVE_REQUEST_ID, 5, dbPath));
        int paused;
        LOG_IF_FAIL(queryIntValue(SELECT_ALL_SYNCS_BY_DRIVE_REQUEST_ID, 6, paused));
        int supportVfs;
        LOG_IF_FAIL(queryIntValue(SELECT_ALL_SYNCS_BY_DRIVE_REQUEST_ID, 7, supportVfs));
        int virtualFileMode;
        LOG_IF_FAIL(queryIntValue(SELECT_ALL_SYNCS_BY_DRIVE_REQUEST_ID, 8, virtualFileMode));
        int notificationsDisabled;
        LOG_IF_FAIL(queryIntValue(SELECT_ALL_SYNCS_BY_DRIVE_REQUEST_ID, 9, notificationsDisabled));
        int hasFullyCompleted;
        LOG_IF_FAIL(queryIntValue(SELECT_ALL_SYNCS_BY_DRIVE_REQUEST_ID, 10, hasFullyCompleted));
        std::string navigationPaneClsid;
        LOG_IF_FAIL(queryStringValue(SELECT_ALL_SYNCS_BY_DRIVE_REQUEST_ID, 11, navigationPaneClsid));
        std::string listingCursor;
        LOG_IF_FAIL(queryStringValue(SELECT_ALL_SYNCS_BY_DRIVE_REQUEST_ID, 12, listingCursor));
        int64_t listingCursorTimestamp;
        LOG_IF_FAIL(queryInt64Value(SELECT_ALL_SYNCS_BY_DRIVE_REQUEST_ID, 13, listingCursorTimestamp));

        syncList.push_back(Sync(id, driveDbId, SyncPath(localPath), localNodeId, SyncPath(targetPath), targetNodeId,
                                static_cast<bool>(paused), static_cast<bool>(supportVfs),
                                static_cast<VirtualFileMode>(virtualFileMode), static_cast<bool>(notificationsDisabled),
                                SyncPath(dbPath), static_cast<bool>(hasFullyCompleted), navigationPaneClsid, listingCursor,
                                listingCursorTimestamp));
    }
    LOG_IF_FAIL(queryResetAndClearBindings(SELECT_ALL_SYNCS_BY_DRIVE_REQUEST_ID));

    return true;
}

bool ParmsDb::getNewSyncDbId(int &dbId) {
    std::vector<Sync> syncList;
    if (!selectAllSyncs(syncList)) {
        LOG_WARN(_logger, "Error in selectAllSyncs");
        return false;
    }

    dbId = 1;
    for (const Sync &sync: syncList) {
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

    LOG_IF_FAIL(queryResetAndClearBindings(INSERT_EXCLUSION_TEMPLATE_REQUEST_ID));
    LOG_IF_FAIL(queryBindValue(INSERT_EXCLUSION_TEMPLATE_REQUEST_ID, 1, exclusionTemplate.templ()));
    LOG_IF_FAIL(queryBindValue(INSERT_EXCLUSION_TEMPLATE_REQUEST_ID, 2, exclusionTemplate.warning()));
    LOG_IF_FAIL(queryBindValue(INSERT_EXCLUSION_TEMPLATE_REQUEST_ID, 3, exclusionTemplate.def()));
    LOG_IF_FAIL(queryBindValue(INSERT_EXCLUSION_TEMPLATE_REQUEST_ID, 4, exclusionTemplate.deleted()));
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

    LOG_IF_FAIL(queryResetAndClearBindings(UPDATE_EXCLUSION_TEMPLATE_REQUEST_ID));
    LOG_IF_FAIL(queryBindValue(UPDATE_EXCLUSION_TEMPLATE_REQUEST_ID, 1, exclusionTemplate.warning()));
    LOG_IF_FAIL(queryBindValue(UPDATE_EXCLUSION_TEMPLATE_REQUEST_ID, 2, exclusionTemplate.def()));
    LOG_IF_FAIL(queryBindValue(UPDATE_EXCLUSION_TEMPLATE_REQUEST_ID, 3, exclusionTemplate.deleted()));
    LOG_IF_FAIL(queryBindValue(UPDATE_EXCLUSION_TEMPLATE_REQUEST_ID, 4, exclusionTemplate.templ()));
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

    LOG_IF_FAIL(queryResetAndClearBindings(DELETE_EXCLUSION_TEMPLATE_REQUEST_ID));
    LOG_IF_FAIL(queryBindValue(DELETE_EXCLUSION_TEMPLATE_REQUEST_ID, 1, templ));
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

    LOG_IF_FAIL(queryResetAndClearBindings(SELECT_ALL_EXCLUSION_TEMPLATE_REQUEST_ID));
    bool found;
    for (;;) {
        if (!queryNext(SELECT_ALL_EXCLUSION_TEMPLATE_REQUEST_ID, found)) {
            LOG_WARN(_logger, "Error getting query result: " << SELECT_ALL_EXCLUSION_TEMPLATE_REQUEST_ID);
            return false;
        }
        if (!found) {
            break;
        }

        std::string template_;
        LOG_IF_FAIL(queryStringValue(SELECT_ALL_EXCLUSION_TEMPLATE_REQUEST_ID, 0, template_));
        int warning;
        LOG_IF_FAIL(queryIntValue(SELECT_ALL_EXCLUSION_TEMPLATE_REQUEST_ID, 1, warning));
        int def;
        LOG_IF_FAIL(queryIntValue(SELECT_ALL_EXCLUSION_TEMPLATE_REQUEST_ID, 2, def));
        int deleted;
        LOG_IF_FAIL(queryIntValue(SELECT_ALL_EXCLUSION_TEMPLATE_REQUEST_ID, 3, deleted));

        (void) exclusionTemplateList.emplace_back(template_, static_cast<bool>(warning), static_cast<bool>(def),
                                                  static_cast<bool>(deleted));
    }
    LOG_IF_FAIL(queryResetAndClearBindings(SELECT_ALL_EXCLUSION_TEMPLATE_REQUEST_ID));

    return true;
}

bool ParmsDb::selectAllExclusionTemplates(bool defaultTemplates, std::vector<ExclusionTemplate> &exclusionTemplateList) {
    const std::scoped_lock lock(_mutex);

    exclusionTemplateList.clear();

    LOG_IF_FAIL(queryResetAndClearBindings(SELECT_ALL_EXCLUSION_TEMPLATE_BY_DEF_REQUEST_ID));
    LOG_IF_FAIL(queryBindValue(SELECT_ALL_EXCLUSION_TEMPLATE_BY_DEF_REQUEST_ID, 1, defaultTemplates));
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
        LOG_IF_FAIL(queryStringValue(SELECT_ALL_EXCLUSION_TEMPLATE_BY_DEF_REQUEST_ID, 0, templ));
        int warning;
        LOG_IF_FAIL(queryIntValue(SELECT_ALL_EXCLUSION_TEMPLATE_BY_DEF_REQUEST_ID, 1, warning));
        int deleted;
        LOG_IF_FAIL(queryIntValue(SELECT_ALL_EXCLUSION_TEMPLATE_BY_DEF_REQUEST_ID, 2, deleted));

        (void) exclusionTemplateList.emplace_back(templ, static_cast<bool>(warning), defaultTemplates,
                                                  static_cast<bool>(deleted));
    }
    LOG_IF_FAIL(queryResetAndClearBindings(SELECT_ALL_EXCLUSION_TEMPLATE_BY_DEF_REQUEST_ID));

    return true;
}

bool ParmsDb::updateAllExclusionTemplates(bool defaultTemplates, const std::vector<ExclusionTemplate> &exclusionTemplateList) {
    const std::scoped_lock lock(_mutex);

    int errId;
    std::string error;

    startTransaction();

    // Delete existing ExclusionTemplates
    LOG_IF_FAIL(queryResetAndClearBindings(DELETE_ALL_EXCLUSION_TEMPLATE_BY_DEF_REQUEST_ID));
    LOG_IF_FAIL(queryBindValue(DELETE_ALL_EXCLUSION_TEMPLATE_BY_DEF_REQUEST_ID, 1, defaultTemplates));
    if (!queryExec(DELETE_ALL_EXCLUSION_TEMPLATE_BY_DEF_REQUEST_ID, errId, error)) {
        LOG_WARN(_logger, "Error running query: " << DELETE_ALL_EXCLUSION_TEMPLATE_BY_DEF_REQUEST_ID);
        rollbackTransaction();
        return false;
    }

    // Insert new ExclusionTemplates
    for (const ExclusionTemplate &exclusionTemplate: exclusionTemplateList) {
        LOG_IF_FAIL(queryResetAndClearBindings(INSERT_EXCLUSION_TEMPLATE_REQUEST_ID));
        LOG_IF_FAIL(queryBindValue(INSERT_EXCLUSION_TEMPLATE_REQUEST_ID, 1, exclusionTemplate.templ()));
        LOG_IF_FAIL(queryBindValue(INSERT_EXCLUSION_TEMPLATE_REQUEST_ID, 2, exclusionTemplate.warning()));
        LOG_IF_FAIL(queryBindValue(INSERT_EXCLUSION_TEMPLATE_REQUEST_ID, 3, exclusionTemplate.def()));
        LOG_IF_FAIL(queryBindValue(INSERT_EXCLUSION_TEMPLATE_REQUEST_ID, 4, exclusionTemplate.deleted()));
        if (!queryExec(INSERT_EXCLUSION_TEMPLATE_REQUEST_ID, errId, error)) {
            LOG_WARN(_logger, "Error running query: " << INSERT_EXCLUSION_TEMPLATE_REQUEST_ID);
            rollbackTransaction();
            return false;
        }
    }

    commitTransaction();

    return true;
}

#if defined(KD_MACOS)
bool ParmsDb::insertExclusionApp(const ExclusionApp &exclusionApp, bool &constraintError) {
    const std::scoped_lock lock(_mutex);

    int errId;
    std::string error;

    LOG_IF_FAIL(queryResetAndClearBindings(INSERT_EXCLUSION_APP_REQUEST_ID));
    LOG_IF_FAIL(queryBindValue(INSERT_EXCLUSION_APP_REQUEST_ID, 1, exclusionApp.appId()));
    LOG_IF_FAIL(queryBindValue(INSERT_EXCLUSION_APP_REQUEST_ID, 2, exclusionApp.description()));
    LOG_IF_FAIL(queryBindValue(INSERT_EXCLUSION_APP_REQUEST_ID, 3, exclusionApp.def()));
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

    LOG_IF_FAIL(queryResetAndClearBindings(UPDATE_EXCLUSION_APP_REQUEST_ID));
    LOG_IF_FAIL(queryBindValue(UPDATE_EXCLUSION_APP_REQUEST_ID, 1, exclusionApp.description()));
    LOG_IF_FAIL(queryBindValue(UPDATE_EXCLUSION_APP_REQUEST_ID, 2, exclusionApp.def()));
    LOG_IF_FAIL(queryBindValue(UPDATE_EXCLUSION_APP_REQUEST_ID, 3, exclusionApp.appId()));
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

    LOG_IF_FAIL(queryResetAndClearBindings(DELETE_EXCLUSION_APP_REQUEST_ID));
    LOG_IF_FAIL(queryBindValue(DELETE_EXCLUSION_APP_REQUEST_ID, 1, appId));
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

    LOG_IF_FAIL(queryResetAndClearBindings(SELECT_ALL_EXCLUSION_APP_REQUEST_ID));
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
        LOG_IF_FAIL(queryStringValue(SELECT_ALL_EXCLUSION_APP_REQUEST_ID, 0, appId));
        std::string description;
        LOG_IF_FAIL(queryStringValue(SELECT_ALL_EXCLUSION_APP_REQUEST_ID, 1, description));
        int def;
        LOG_IF_FAIL(queryIntValue(SELECT_ALL_EXCLUSION_APP_REQUEST_ID, 2, def));

        exclusionAppList.push_back(ExclusionApp(appId, description, static_cast<bool>(def)));
    }
    LOG_IF_FAIL(queryResetAndClearBindings(SELECT_ALL_EXCLUSION_APP_REQUEST_ID));

    return true;
}

bool ParmsDb::selectAllExclusionApps(bool def, std::vector<ExclusionApp> &exclusionAppList) {
    const std::scoped_lock lock(_mutex);

    exclusionAppList.clear();

    LOG_IF_FAIL(queryResetAndClearBindings(SELECT_ALL_EXCLUSION_APP_BY_DEF_REQUEST_ID));
    LOG_IF_FAIL(queryBindValue(SELECT_ALL_EXCLUSION_APP_BY_DEF_REQUEST_ID, 1, def));
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
        LOG_IF_FAIL(queryStringValue(SELECT_ALL_EXCLUSION_APP_BY_DEF_REQUEST_ID, 0, appId));
        std::string description;
        LOG_IF_FAIL(queryStringValue(SELECT_ALL_EXCLUSION_APP_BY_DEF_REQUEST_ID, 1, description));

        exclusionAppList.push_back(ExclusionApp(appId, description, def));
    }
    LOG_IF_FAIL(queryResetAndClearBindings(SELECT_ALL_EXCLUSION_APP_BY_DEF_REQUEST_ID));

    return true;
}

bool ParmsDb::updateAllExclusionApps(bool def, const std::vector<ExclusionApp> &exclusionAppList) {
    const std::scoped_lock lock(_mutex);

    int errId;
    std::string error;

    startTransaction();

    // Delete existing ExclusionApps
    LOG_IF_FAIL(queryResetAndClearBindings(DELETE_ALL_EXCLUSION_APP_BY_DEF_REQUEST_ID));
    LOG_IF_FAIL(queryBindValue(DELETE_ALL_EXCLUSION_APP_BY_DEF_REQUEST_ID, 1, def));
    if (!queryExec(DELETE_ALL_EXCLUSION_APP_BY_DEF_REQUEST_ID, errId, error)) {
        LOG_WARN(_logger, "Error running query: " << DELETE_ALL_EXCLUSION_APP_BY_DEF_REQUEST_ID);
        rollbackTransaction();
        return false;
    }

    // Insert new ExclusionApps
    for (const ExclusionApp &exclusionApp: exclusionAppList) {
        LOG_IF_FAIL(queryResetAndClearBindings(INSERT_EXCLUSION_APP_REQUEST_ID));
        LOG_IF_FAIL(queryBindValue(INSERT_EXCLUSION_APP_REQUEST_ID, 1, exclusionApp.appId()));
        LOG_IF_FAIL(queryBindValue(INSERT_EXCLUSION_APP_REQUEST_ID, 2, exclusionApp.description()));
        LOG_IF_FAIL(queryBindValue(INSERT_EXCLUSION_APP_REQUEST_ID, 3, exclusionApp.def()));
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

    LOG_IF_FAIL(queryResetAndClearBindings(INSERT_ERROR_REQUEST_ID));
    LOG_IF_FAIL(queryBindValue(INSERT_ERROR_REQUEST_ID, 1, err.time()));
    LOG_IF_FAIL(queryBindValue(INSERT_ERROR_REQUEST_ID, 2, toInt(err.level())));
    LOG_IF_FAIL(queryBindValue(INSERT_ERROR_REQUEST_ID, 3, err.functionName()));
    LOG_IF_FAIL(queryBindValue(INSERT_ERROR_REQUEST_ID, 4, err.syncDbId() ? dbtype(err.syncDbId()) : std::monostate()));
    LOG_IF_FAIL(queryBindValue(INSERT_ERROR_REQUEST_ID, 5, err.workerName()));
    LOG_IF_FAIL(queryBindValue(INSERT_ERROR_REQUEST_ID, 6, toInt(err.exitCode())));
    LOG_IF_FAIL(queryBindValue(INSERT_ERROR_REQUEST_ID, 7, toInt(err.exitCause())));
    LOG_IF_FAIL(queryBindValue(INSERT_ERROR_REQUEST_ID, 8, err.localNodeId()));
    LOG_IF_FAIL(queryBindValue(INSERT_ERROR_REQUEST_ID, 9, err.remoteNodeId()));
    LOG_IF_FAIL(queryBindValue(INSERT_ERROR_REQUEST_ID, 10, toInt(err.nodeType())));
    LOG_IF_FAIL(queryBindValue(INSERT_ERROR_REQUEST_ID, 11, err.path().native()));
    LOG_IF_FAIL(queryBindValue(INSERT_ERROR_REQUEST_ID, 12, 0)); // TODO : Not used anymore
    LOG_IF_FAIL(queryBindValue(INSERT_ERROR_REQUEST_ID, 13, toInt(err.conflictType())));
    LOG_IF_FAIL(queryBindValue(INSERT_ERROR_REQUEST_ID, 14, toInt(err.inconsistencyType())));
    LOG_IF_FAIL(queryBindValue(INSERT_ERROR_REQUEST_ID, 15, toInt(err.cancelType())));
    LOG_IF_FAIL(queryBindValue(INSERT_ERROR_REQUEST_ID, 16, err.destinationPath()));
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

    LOG_IF_FAIL(queryResetAndClearBindings(UPDATE_ERROR_REQUEST_ID));
    LOG_IF_FAIL(queryBindValue(UPDATE_ERROR_REQUEST_ID, 1, err.time()));
    LOG_IF_FAIL(queryBindValue(UPDATE_ERROR_REQUEST_ID, 2, err.path()));
    LOG_IF_FAIL(queryBindValue(UPDATE_ERROR_REQUEST_ID, 3, err.dbId()));
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

    LOG_IF_FAIL(queryResetAndClearBindings(DELETE_ALL_ERROR_BY_EXITCODE_REQUEST_ID));
    LOG_IF_FAIL(queryBindValue(DELETE_ALL_ERROR_BY_EXITCODE_REQUEST_ID, 1, static_cast<int>(exitCode)));
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

    LOG_IF_FAIL(queryResetAndClearBindings(DELETE_ALL_ERROR_BY_EXITCAUSEREQUEST_ID));
    LOG_IF_FAIL(queryBindValue(DELETE_ALL_ERROR_BY_EXITCAUSEREQUEST_ID, 1, static_cast<int>(exitCause)));
    if (!queryExec(DELETE_ALL_ERROR_BY_EXITCAUSEREQUEST_ID, errId, error)) {
        LOG_WARN(_logger, "Error running query: " << DELETE_ALL_ERROR_BY_EXITCAUSEREQUEST_ID);
        return false;
    }

    return true;
}

bool ParmsDb::selectAllErrors(ErrorLevel level, int syncDbId, int limit, std::vector<Error> &errs) {
    const std::scoped_lock lock(_mutex);

    LOG_IF_FAIL(queryResetAndClearBindings(SELECT_ALL_ERROR_BY_LEVEL_AND_SYNCDBID_REQUEST_ID));
    LOG_IF_FAIL(queryBindValue(SELECT_ALL_ERROR_BY_LEVEL_AND_SYNCDBID_REQUEST_ID, 1, toInt(level)));
    LOG_IF_FAIL(queryBindValue(SELECT_ALL_ERROR_BY_LEVEL_AND_SYNCDBID_REQUEST_ID, 2, syncDbId));
    LOG_IF_FAIL(queryBindValue(SELECT_ALL_ERROR_BY_LEVEL_AND_SYNCDBID_REQUEST_ID, 3, limit));
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
        LOG_IF_FAIL(queryInt64Value(SELECT_ALL_ERROR_BY_LEVEL_AND_SYNCDBID_REQUEST_ID, 0, dbId));
        int64_t time;
        LOG_IF_FAIL(queryInt64Value(SELECT_ALL_ERROR_BY_LEVEL_AND_SYNCDBID_REQUEST_ID, 1, time));
        std::string functionName;
        LOG_IF_FAIL(queryStringValue(SELECT_ALL_ERROR_BY_LEVEL_AND_SYNCDBID_REQUEST_ID, 2, functionName));
        std::string workerName;
        LOG_IF_FAIL(queryStringValue(SELECT_ALL_ERROR_BY_LEVEL_AND_SYNCDBID_REQUEST_ID, 3, workerName));
        int exitCode;
        LOG_IF_FAIL(queryIntValue(SELECT_ALL_ERROR_BY_LEVEL_AND_SYNCDBID_REQUEST_ID, 4, exitCode));
        int exitCause;
        LOG_IF_FAIL(queryIntValue(SELECT_ALL_ERROR_BY_LEVEL_AND_SYNCDBID_REQUEST_ID, 5, exitCause));
        std::string localNodeId;
        LOG_IF_FAIL(queryStringValue(SELECT_ALL_ERROR_BY_LEVEL_AND_SYNCDBID_REQUEST_ID, 6, localNodeId));
        std::string remoteNodeId;
        LOG_IF_FAIL(queryStringValue(SELECT_ALL_ERROR_BY_LEVEL_AND_SYNCDBID_REQUEST_ID, 7, remoteNodeId));
        int nodeType;
        LOG_IF_FAIL(queryIntValue(SELECT_ALL_ERROR_BY_LEVEL_AND_SYNCDBID_REQUEST_ID, 8, nodeType));
        SyncName path;
        LOG_IF_FAIL(querySyncNameValue(SELECT_ALL_ERROR_BY_LEVEL_AND_SYNCDBID_REQUEST_ID, 9, path));
        int status;
        LOG_IF_FAIL(queryIntValue(SELECT_ALL_ERROR_BY_LEVEL_AND_SYNCDBID_REQUEST_ID, 10, status));
        int conflictType;
        LOG_IF_FAIL(queryIntValue(SELECT_ALL_ERROR_BY_LEVEL_AND_SYNCDBID_REQUEST_ID, 11, conflictType));
        int inconsistencyType;
        LOG_IF_FAIL(queryIntValue(SELECT_ALL_ERROR_BY_LEVEL_AND_SYNCDBID_REQUEST_ID, 12, inconsistencyType));
        int cancelType;
        LOG_IF_FAIL(queryIntValue(SELECT_ALL_ERROR_BY_LEVEL_AND_SYNCDBID_REQUEST_ID, 13, cancelType));
        SyncName destinationPath;
        LOG_IF_FAIL(querySyncNameValue(SELECT_ALL_ERROR_BY_LEVEL_AND_SYNCDBID_REQUEST_ID, 14, destinationPath));

        errs.push_back(Error(dbId, time, static_cast<ErrorLevel>(level), functionName, syncDbId, workerName,
                             static_cast<ExitCode>(exitCode), static_cast<ExitCause>(exitCause), static_cast<NodeId>(localNodeId),
                             static_cast<NodeId>(remoteNodeId), static_cast<NodeType>(nodeType), static_cast<SyncPath>(path),
                             static_cast<ConflictType>(conflictType), static_cast<InconsistencyType>(inconsistencyType),
                             static_cast<CancelType>(cancelType), static_cast<SyncPath>(destinationPath)));
    }
    LOG_IF_FAIL(queryResetAndClearBindings(SELECT_ALL_ERROR_BY_LEVEL_AND_SYNCDBID_REQUEST_ID));

    return true;
}

bool ParmsDb::selectConflicts(int syncDbId, ConflictType filter, std::vector<Error> &errs) {
    const std::scoped_lock lock(_mutex);

    std::string requestId = (filter == ConflictType::None ? SELECT_ALL_CONFLICTS_BY_SYNCDBID_REQUEST_ID
                                                          : SELECT_FILTERED_CONFLICTS_BY_SYNCDBID_REQUEST_ID);

    LOG_IF_FAIL(queryResetAndClearBindings(requestId));
    LOG_IF_FAIL(queryBindValue(requestId, 1, syncDbId));
    LOG_IF_FAIL(queryBindValue(requestId, 2, std::to_string(toInt(filter))));

    bool found = false;
    for (;;) {
        if (!queryNext(requestId, found)) {
            LOG_WARN(_logger, "Error getting query result: " << requestId);
            return false;
        }
        if (!found) {
            break;
        }

        int64_t dbId = 0;
        LOG_IF_FAIL(queryInt64Value(requestId, 0, dbId));
        int64_t time = 0;
        LOG_IF_FAIL(queryInt64Value(requestId, 1, time));
        std::string functionName;
        LOG_IF_FAIL(queryStringValue(requestId, 2, functionName));
        std::string workerName;
        LOG_IF_FAIL(queryStringValue(requestId, 3, workerName));
        int exitCode = 0;
        LOG_IF_FAIL(queryIntValue(requestId, 4, exitCode));
        int exitCause = 0;
        LOG_IF_FAIL(queryIntValue(requestId, 5, exitCause));
        std::string localNodeId;
        LOG_IF_FAIL(queryStringValue(requestId, 6, localNodeId));
        std::string remoteNodeId;
        LOG_IF_FAIL(queryStringValue(requestId, 7, remoteNodeId));
        int nodeType = 0;
        LOG_IF_FAIL(queryIntValue(requestId, 8, nodeType));
        SyncName path;
        LOG_IF_FAIL(querySyncNameValue(requestId, 9, path));
        int status = 0;
        LOG_IF_FAIL(queryIntValue(requestId, 10, status));
        int conflictType = 0;
        LOG_IF_FAIL(queryIntValue(requestId, 11, conflictType));
        int inconsistencyType = 0;
        LOG_IF_FAIL(queryIntValue(requestId, 12, inconsistencyType));
        int cancelType = 0;
        LOG_IF_FAIL(queryIntValue(requestId, 13, cancelType));
        SyncName destinationPath;
        LOG_IF_FAIL(querySyncNameValue(requestId, 14, destinationPath));

        errs.push_back(Error(dbId, time, ErrorLevel::Node, functionName, syncDbId, workerName, static_cast<ExitCode>(exitCode),
                             static_cast<ExitCause>(exitCause), static_cast<NodeId>(localNodeId),
                             static_cast<NodeId>(remoteNodeId), static_cast<NodeType>(nodeType), static_cast<SyncPath>(path),
                             static_cast<ConflictType>(conflictType), static_cast<InconsistencyType>(inconsistencyType),
                             static_cast<CancelType>(cancelType), static_cast<SyncPath>(destinationPath)));
    }
    LOG_IF_FAIL(queryResetAndClearBindings(requestId));

    return true;
}

bool ParmsDb::deleteErrors(ErrorLevel level) {
    const std::scoped_lock lock(_mutex);

    int errId;
    std::string error;

    LOG_IF_FAIL(queryResetAndClearBindings(DELETE_ALL_ERROR_BY_LEVEL_REQUEST_ID));
    LOG_IF_FAIL(queryBindValue(DELETE_ALL_ERROR_BY_LEVEL_REQUEST_ID, 1, toInt(level)));
    if (!queryExec(DELETE_ALL_ERROR_BY_LEVEL_REQUEST_ID, errId, error)) {
        LOG_WARN(_logger, "Error running query: " << DELETE_ALL_ERROR_BY_LEVEL_REQUEST_ID);
        return false;
    }

    return true;
}

bool ParmsDb::deleteError(int64_t dbId, bool &found) {
    const std::scoped_lock lock(_mutex);

    int errId;
    std::string error;

    LOG_IF_FAIL(queryResetAndClearBindings(DELETE_ERROR_BY_DBID_REQUEST_ID));
    LOG_IF_FAIL(queryBindValue(DELETE_ERROR_BY_DBID_REQUEST_ID, 1, dbId));
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

    LOG_IF_FAIL(queryResetAndClearBindings(INSERT_MIGRATION_SELECTIVESYNC_REQUEST_ID));
    LOG_IF_FAIL(queryBindValue(INSERT_MIGRATION_SELECTIVESYNC_REQUEST_ID, 1, migrationSelectiveSync.syncDbId()));
    LOG_IF_FAIL(queryBindValue(INSERT_MIGRATION_SELECTIVESYNC_REQUEST_ID, 2, migrationSelectiveSync.path().native()));
    LOG_IF_FAIL(queryBindValue(INSERT_MIGRATION_SELECTIVESYNC_REQUEST_ID, 3, toInt(migrationSelectiveSync.type())));
    if (!queryExec(INSERT_MIGRATION_SELECTIVESYNC_REQUEST_ID, errId, error)) {
        LOG_WARN(_logger, "Error running query: " << INSERT_MIGRATION_SELECTIVESYNC_REQUEST_ID);
        return false;
    }

    return true;
}

bool ParmsDb::selectAllMigrationSelectiveSync(std::vector<MigrationSelectiveSync> &migrationSelectiveSyncList) {
    const std::scoped_lock lock(_mutex);

    migrationSelectiveSyncList.clear();

    LOG_IF_FAIL(queryResetAndClearBindings(SELECT_ALL_MIGRATION_SELECTIVESYNC_REQUEST_ID));
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
        LOG_IF_FAIL(queryIntValue(SELECT_ALL_MIGRATION_SELECTIVESYNC_REQUEST_ID, 0, syncDbId));

        SyncName path;
        LOG_IF_FAIL(querySyncNameValue(SELECT_ALL_MIGRATION_SELECTIVESYNC_REQUEST_ID, 1, path));

        int type;
        LOG_IF_FAIL(queryIntValue(SELECT_ALL_MIGRATION_SELECTIVESYNC_REQUEST_ID, 2, type));

        migrationSelectiveSyncList.push_back(MigrationSelectiveSync(syncDbId, SyncPath(path), fromInt<SyncNodeType>(type)));
    }
    LOG_IF_FAIL(queryResetAndClearBindings(SELECT_ALL_MIGRATION_SELECTIVESYNC_REQUEST_ID));

    return true;
}

#if defined(KD_WINDOWS)
bool ParmsDb::replaceShortDbPathsWithLongPaths() {
    LOG_INFO(_logger, "Replacing short DB path names with long ones in sync table.")

    if (!createAndPrepareRequest(SELECT_ALL_SYNCS_REQUEST_ID, SELECT_ALL_SYNCS_REQUEST)) return false;
    std::vector<Sync> syncList;
    selectAllSyncs(syncList);
    queryFree(SELECT_ALL_SYNCS_REQUEST_ID);

    if (!createAndPrepareRequest(UPDATE_SYNC_REQUEST_ID, UPDATE_SYNC_REQUEST)) return false;
    for (auto &sync: syncList) {
        SyncPath longPathName;
        auto ioError = IoError::Success;
        if (!IoHelper::getLongPathName(sync.dbPath(), longPathName, ioError)) {
            LOGW_WARN(_logger, L"Error in IoHelper::getLongPathName: " << Utility::formatIoError(sync.dbPath(), ioError));
            continue;
        }
        bool exists = false;
        if (!IoHelper::checkIfPathExists(longPathName, exists, ioError)) {
            LOGW_WARN(_logger, L"Error in IoHelper::checkIfPathExists: " << Utility::formatIoError(sync.dbPath(), ioError));
            continue;
        } else if (!exists) {
            LOGW_DEBUG(_logger, L"The sync DB item indicated by the computed long path does not exist: "
                                        << Utility::formatSyncPath(longPathName));
            continue;
        }
        sync.setDbPath(longPathName);
        bool found = false;
        if (!updateSync(sync, found)) return false;
    }
    queryFree(UPDATE_SYNC_REQUEST_ID);

    return true;
}
#endif

} // namespace KDC

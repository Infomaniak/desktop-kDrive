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

#include "syncenginelib.h"
#include "libcommon/utility/types.h"
#include "libcommon/info/userinfo.h"
#include "libcommon/info/accountinfo.h"
#include "libcommon/info/driveinfo.h"
#include "libcommon/info/driveavailableinfo.h"
#include "libcommon/info/syncinfo.h"
#include "libcommon/info/nodeinfo.h"
#include "libcommon/info/syncfileiteminfo.h"
#include "libcommon/info/errorinfo.h"
#include "libcommon/info/parametersinfo.h"
#include "libcommon/info/proxyconfiginfo.h"
#include "libcommon/info/exclusiontemplateinfo.h"
#include "libcommon/info/exclusionappinfo.h"
#include "libparms/db/drive.h"
#include "libparms/db/account.h"
#include "libparms/db/user.h"
#include "libparms/db/sync.h"
#include "libparms/db/error.h"
#include "libparms/db/parameters.h"
#include "libparms/db/exclusiontemplate.h"
#include "libparms/db/exclusionapp.h"
#include "libsyncengine/login/login.h"
#include "libsyncengine/progress/syncfileitem.h"

#include <QList>
#include <QString>
#include <QColor>

namespace KDC {

struct SYNCENGINE_EXPORT ServerRequests {
        // C/S requests (access to DB)
        // Use COMM_SHORT_TIMEOUT
        static ExitCode getUserDbIdList(QList<int> &list);
        static ExitCode getUserInfoList(QList<UserInfo> &list);
        static ExitCode getUserIdFromUserDbId(int userDbId, int &userId);
        static ExitCode getAccountInfoList(QList<AccountInfo> &list);
        static ExitCode getDriveInfoList(QList<DriveInfo> &list);
        static ExitCode getDriveInfo(int driveDbId, DriveInfo &driveInfo);
        static ExitCode getDriveIdFromDriveDbId(int driveDbId, int &driveId);
        static ExitCode getDriveIdFromSyncDbId(int syncDbId, int &driveId);
        static ExitCode updateDrive(const DriveInfo &driveInfo);
        static ExitCode getSyncInfoList(QList<SyncInfo> &list);
        static ExitCode getParameters(ParametersInfo &parametersInfo);
        static ExitCode updateParameters(const ParametersInfo &parametersInfo);
        static ExitCode findGoodPathForNewSync(int driveDbId, const QString &basePath, QString &path, QString &error);
        static ExitCode getPrivateLinkUrl(int driveDbId, const QString &fileId, QString &linkUrl);
        static ExitCode getExclusionTemplateList(bool def, QList<ExclusionTemplateInfo> &list);
        static ExitCode setExclusionTemplateList(bool def, const QList<ExclusionTemplateInfo> &list);
        static ExitCode getExclusionAppList(bool def, QList<ExclusionAppInfo> &list);
        static ExitCode setExclusionAppList(bool def, const QList<ExclusionAppInfo> &list);
        static ExitCode getErrorInfoList(ErrorLevel level, int syncDbId, int limit, QList<ErrorInfo> &list);
        static ExitCode getConflictList(int syncDbId, const std::unordered_set<ConflictType> &filter,
                                        std::vector<Error> &errorLis);
        static ExitCode getConflictErrorInfoList(int driveDbId, const std::unordered_set<ConflictType> &filter,
                                                 QList<ErrorInfo> &errorInfoList);
        static ExitCode deleteErrorsServer();
        static ExitCode deleteErrorsForSync(int syncDbId, bool autoResolved);
        static ExitCode deleteInvalidTokenErrors();
#ifdef Q_OS_MAC
        static ExitCode deleteLiteSyncNotAllowedErrors();
#endif

        // C/S requests (access to network)
        // !!! Use COMM_AVERAGE_TIMEOUT !!!
        static ExitCode requestToken(QString code, QString codeVerifier, UserInfo &userInfo, bool &userCreated,
                                     std::string &error, std::string &errorDescr);
        static ExitCode getUserAvailableDrives(int userDbId, QHash<int, DriveAvailableInfo> &list);
        static ExitCode addSync(int userDbId, int accountId, int driveId, const QString &localFolderPath,
                                const QString &serverFolderPath, const QString &serverFolderNodeId, bool liteSync,
                                bool showInNavigationPane, AccountInfo &accountInfo, DriveInfo &driveInfo, SyncInfo &syncInfo);
        static ExitCode addSync(int driveDbId, const QString &localFolderPath, const QString &serverFolderPath,
                                const QString &serverFolderNodeId, bool liteSync, bool showInNavigationPane, SyncInfo &syncInfo);
        static ExitInfo getNodeInfo(int userDbId, int driveId, const QString &nodeId, NodeInfo &nodeInfo, bool withPath = false);
        static ExitInfo getSubFolders(int userDbId, int driveId, const QString &nodeId, QList<NodeInfo> &list,
                                      bool withPath = false);
        static ExitInfo getSubFolders(int driveDbId, const QString &nodeId, QList<NodeInfo> &list, bool withPath = false);
        static ExitCode createDir(int driveDbId, const QString &parentNodeId, const QString &dirName, QString &newNodeId);
        static ExitCode getPublicLinkUrl(int driveDbId, const NodeId &nodeId, std::string &linkUrl);
        static ExitInfo getFolderSize(int userDbId, int driveId, const NodeId &nodeId,
                                      std::function<void(const QString &, qint64)> callback);
        static ExitCode getNodeIdByPath(int userDbId, int driveId, const SyncPath &path, QString &nodeId);
        static ExitInfo getPathByNodeId(int userDbId, int driveId, const QString &nodeId, QString &path);

        // C/S requests (others)
        static ExitCode deleteUser(int userDbId); // !!! Use COMM_LONG_TIMEOUT !!!
        static ExitCode deleteAccount(int accountDbId); // !!! Use COMM_LONG_TIMEOUT !!!
        static ExitCode deleteDrive(int driveDbId); // !!! Use COMM_LONG_TIMEOUT !!!
        static ExitCode deleteSync(int syncDbId); // !!! Use COMM_LONG_TIMEOUT !!!

        // Server requests
        static ExitInfo loadDriveInfo(Drive &drive, Account &account, bool &updated, bool &quotaUpdated, bool &accountUpdated);
        static ExitCode loadUserInfo(User &user, bool &updated);
        static ExitCode loadUserAvatar(User &user);
        static ExitCode getThumbnail(int driveDbId, NodeId nodeId, int width, std::string &thumbnail);

        // Utility
        static void userToUserInfo(const User &user, UserInfo &userInfo);
        static void accountToAccountInfo(const Account &account, AccountInfo &accountInfo);
        static void driveToDriveInfo(const Drive &drive, DriveInfo &driveInfo);
        static void syncToSyncInfo(const Sync &sync, SyncInfo &syncInfo);
        static void syncInfoToSync(const SyncInfo &syncInfo, Sync &sync);
        static void errorToErrorInfo(const Error &error, ErrorInfo &errorInfo);
        static void syncFileItemToSyncFileItemInfo(const SyncFileItem &item, SyncFileItemInfo &itemInfo);
        static void parametersToParametersInfo(const Parameters &parameters, ParametersInfo &parametersInfo);
        static void parametersInfoToParameters(const ParametersInfo &parametersInfo, Parameters &parameters);
        static void proxyConfigToProxyConfigInfo(const ProxyConfig &proxyConfig, ProxyConfigInfo &proxyConfigInfo);
        static void proxyConfigInfoToProxyConfig(const ProxyConfigInfo &proxyConfigInfo, ProxyConfig &proxyConfig);
        static void exclusionTemplateToExclusionTemplateInfo(const ExclusionTemplate &exclusionTemplate,
                                                             ExclusionTemplateInfo &exclusionTemplateInfo);
        static void exclusionTemplateInfoToExclusionTemplate(const ExclusionTemplateInfo &exclusionTemplateInfo,
                                                             ExclusionTemplate &exclusionTemplate);
        static void exclusionAppToExclusionAppInfo(const ExclusionApp &exclusionApp, ExclusionAppInfo &exclusionAppInfo);
        static void exclusionAppInfoToExclusionApp(const ExclusionAppInfo &exclusionAppInfo, ExclusionApp &exclusionApp);
        static ExitCode loadOldSelectiveSyncTable(const SyncPath &syncDbPath, QList<QPair<QString, SyncNodeType>> &list);
        static ExitCode migrateSelectiveSync(int syncDbId, std::pair<SyncPath, SyncName> &syncToMigrate);
        static bool isDisplayableError(const Error &error);
        static bool isAutoResolvedError(const Error &error);
        static ExitCode getUserFromSyncDbId(int syncDbId, User &user);
        static ExitCode fixProxyConfig();

    private:
        static ExitCode processRequestTokenFinished(const Login &login, UserInfo &userInfo, bool &userCreated);
        static QString canonicalPath(const QString &path);
        static ExitCode checkPathValidityRecursive(const QString &path, QString &error);
        static ExitCode checkPathValidityForNewFolder(const std::vector<Sync> &syncList, int driveDbId, const QString &path,
                                                      QString &error);
        static ExitCode syncForPath(const std::vector<Sync> &syncList, const QString &path, int &syncDbId);
        static QString excludeFile(bool liteSync);
        static ExitCode createUser(const User &user, UserInfo &userInfo);
        static ExitCode updateUser(const User &user, UserInfo &userInfo);
        static ExitCode createAccount(const Account &account, AccountInfo &accountInfo);
        static ExitCode createDrive(const Drive &drive, DriveInfo &driveInfo);
        static ExitCode createSync(const Sync &sync, SyncInfo &syncInfo);
};

} // namespace KDC

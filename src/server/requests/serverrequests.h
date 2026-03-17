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
        // TODO: Remove functions with QList parameter after switching to the new comm layer
        static ExitCode getUserDbIdList(QList<int> &list);
        static ExitCode getUserDbIdList(std::vector<int> &list);
        static ExitCode getUserInfoList(QList<UserInfo> &list);
        static ExitCode getUserInfoList(std::vector<UserInfo> &list);
        static ExitCode getAccountInfoList(QList<AccountInfo> &list);
        static ExitCode getAccountInfoList(std::vector<AccountInfo> &list);
        static ExitCode getDriveInfoList(QList<DriveInfo> &list);
        static ExitCode getDriveInfoList(std::vector<DriveInfo> &list);
        static ExitCode getDriveInfo(int driveDbId, DriveInfo &driveInfo);
        static ExitCode updateDrive(const DriveInfo &driveInfo);
        static ExitCode getSyncInfoList(QList<SyncInfo> &list);
        static ExitCode getSyncInfoList(std::vector<SyncInfo> &list);
        static ExitCode getParameters(ParametersInfo &parametersInfo);
        static ExitCode updateParameters(const ParametersInfo &parametersInfo);
        static ExitInfo isPathValidForNewSync(const SyncPath &path, bool &valid);
        static ExitInfo findGoodPathForNewSync(const SyncPath &basePath, SyncPath &path, std::string &error);
        static ExitInfo findGoodPathForNewSync(const QString &basePath, QString &path, QString &error);
        static ExitCode getPrivateLinkUrl(int driveDbId, const std::string &fileId, std::string &linkUrl);
        static ExitCode getPrivateLinkUrl(int driveDbId, const QString &fileId, QString &linkUrl);
        static ExitCode getExclusionTemplateList(bool def, std::vector<ExclusionTemplateInfo> &list);
        static ExitCode getExclusionTemplateList(bool def, QList<ExclusionTemplateInfo> &list);
        static ExitCode setUserExclusionTemplateList(const std::vector<ExclusionTemplateInfo> &list);
        static ExitCode setExclusionAppList(const bool def, const std::vector<ExclusionAppInfo> &list);
        static ExitCode setUserExclusionTemplateList(const QList<ExclusionTemplateInfo> &list);
        static ExitCode getExclusionAppList(bool def, QList<ExclusionAppInfo> &list);
        static ExitCode getExclusionAppList(bool def, std::vector<ExclusionAppInfo> &list);
        static ExitCode setExclusionAppList(bool def, const QList<ExclusionAppInfo> &list);
        static ExitCode getErrorInfoList(ErrorLevel level, int syncDbId, int limit, QList<ErrorInfo> &list);
        static ExitInfo getErrorInfoList(int limit, std::vector<ErrorInfo> &list);
        static ExitCode getConflictList(int syncDbId, const std::unordered_set<ConflictType> &filter,
                                        std::vector<Error> &errorLis);
        static ExitCode getConflictErrorInfoList(int driveDbId, const std::unordered_set<ConflictType> &filter,
                                                 QList<ErrorInfo> &errorInfoList);
        static ExitCode deleteErrorsServer();
        static ExitCode deleteErrorsForSync(int syncDbId, bool autoResolved);
        static ExitCode deleteInvalidTokenErrors();
#ifdef Q_OS_MAC
        static ExitCode deleteLiteSyncErrors();
#endif

        // C/S requests (access to network)
        // !!! Use COMM_AVERAGE_TIMEOUT !!!
        static ExitCode requestToken(const std::string &code, const std::string &codeVerifier, UserInfo &userInfo,
                                     bool &userCreated, std::string &error, std::string &errorDescr);
        static ExitCode requestToken(const QString &code, const QString &codeVerifier, UserInfo &userInfo, bool &userCreated,
                                     std::string &error, std::string &errorDescr);
        static ExitInfo getUserAvailableDrives(
                int userDbId, QList<DriveAvailableInfo> &list); // TODO: Delete after switching to the new comm layer
        static ExitInfo getUserAvailableDrives(int userDbId, std::vector<DriveAvailableInfo> &list);
        static ExitInfo addSync(int userDbId, int accountId, int driveId, const SyncPath &localFolderPath,
                                const SyncPath &serverFolderPath, const NodeId &serverFolderNodeId, bool liteSync,
                                AccountInfo &accountInfo, DriveInfo &driveInfo, SyncInfo &syncInfo);
        static ExitInfo addSync(int userDbId, int accountId, int driveId, const QString &localFolderPath,
                                const QString &serverFolderPath, const QString &serverFolderNodeId, bool liteSync,
                                AccountInfo &accountInfo, DriveInfo &driveInfo, SyncInfo &syncInfo);
        static ExitInfo addSync(int driveDbId, const SyncPath &localFolderPath, const SyncPath &serverFolderPath,
                                const NodeId &serverFolderNodeId, bool liteSync, SyncInfo &syncInfo);
        static ExitInfo addSync(int driveDbId, const QString &localFolderPath, const QString &serverFolderPath,
                                const QString &serverFolderNodeId, bool liteSync, SyncInfo &syncInfo);
        static ExitInfo getNodeInfo(int userDbId, int driveId, const std::string &nodeId, NodeInfo &nodeInfo,
                                    bool withPath = false);
        static ExitInfo getNodeInfo(int userDbId, int driveId, const QString &nodeId, NodeInfo &nodeInfo, bool withPath = false);

        static ExitInfo getSubFolders(const int userDbId, const int driveId, const NodeId &nodeId, std::vector<NodeInfo> &list,
                                      const bool withPath = false);

        static ExitInfo getSubFolders(int userDbId, int driveId, const QString &nodeId, QList<NodeInfo> &list,
                                      bool withPath = false); // TODO: Delete after switching to the new comm layer
        static ExitInfo getSubFolders(int driveDbId, const NodeId &nodeId, std::vector<NodeInfo> &list, bool withPath = false);
        static ExitInfo getSubFolders(int driveDbId, const QString &nodeId, QList<NodeInfo> &list, bool withPath = false);
        static ExitCode createDir(int driveDbId, const NodeId &parentNodeId, const CommString &dirName, NodeId &newNodeId);
        static ExitCode createDir(int driveDbId, const QString &parentNodeId, const QString &dirName, QString &newNodeId);
        static ExitCode getPublicLinkUrl(int driveDbId, const NodeId &nodeId, std::string &linkUrl);
        static ExitInfo getFolderSizeWithCallback(int userDbId, int driveId, const NodeId &nodeId,
                                                  std::function<void(const QString &, qint64)> callback);
        static ExitInfo getFolderSize(int userDbId, int driveId, const NodeId &nodeId, int64_t &result);
        static ExitCode getNodeIdByPath(int userDbId, int driveId, const SyncPath &path, QString &nodeId);
        static ExitInfo getPathByNodeId(int userDbId, int driveId, const QString &nodeId, QString &path);
        static ExitInfo getPathByNodeId(int userDbId, int driveId, const NodeId &nodeId, CommString &path);

        // C/S requests (others)
        static ExitInfo deleteUser(int userDbId); // !!! Use COMM_LONG_TIMEOUT !!!
        static ExitInfo deleteAccount(int accountDbId); // !!! Use COMM_LONG_TIMEOUT !!!
        static ExitInfo deleteDrive(int32_t driveDbId); // !!! Use COMM_LONG_TIMEOUT !!!
        static ExitCode deleteSync(int syncDbId); // !!! Use COMM_LONG_TIMEOUT !!!

        // Server requests
        static ExitInfo loadAccountInfo(Account &account, bool &updated);
        static ExitInfo loadDriveInfo(Drive &drive, const uint64_t previousAccountId, uint64_t &newAccountId, bool &updated,
                                      bool &quotaUpdated);
        static ExitInfo loadUserInfo(User &user, bool &updated);
        static ExitInfo loadUserAvatar(User &user);
        static ExitInfo getThumbnail(int driveDbId, const NodeId &nodeId, int width, std::string &thumbnail);

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
        static bool isDisplayableError(const Error &error);
        static bool isAutoResolvedError(const Error &error);
        static ExitCode getUserFromSyncDbId(int syncDbId, User &user);
        static ExitCode fixProxyConfig();

    private:
        static ExitCode processRequestTokenFinished(const Login &login, UserInfo &userInfo, bool &userCreated);
        static QString canonicalPath(const QString &path);
        static ExitCode checkPathValidityRecursive(const QString &path, QString &error);
        static ExitInfo checkPathValidityForNewFolder(const std::vector<Sync> &syncList, const QString &path, QString &error);
        static ExitCode syncForPath(const std::vector<Sync> &syncList, const QString &path, int &syncDbId);
        static QString excludeFile(bool liteSync);
        static ExitCode createUser(const User &user, UserInfo &userInfo);
        static ExitCode updateUser(const User &user, UserInfo &userInfo);
        static ExitCode createAccount(const Account &account, AccountInfo &accountInfo);
        static ExitCode createDrive(const Drive &drive, DriveInfo &driveInfo);
        static ExitCode createSync(const Sync &sync, SyncInfo &syncInfo);
};

} // namespace KDC

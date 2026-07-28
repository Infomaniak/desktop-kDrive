/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2026 Infomaniak Network SA
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
#include "libcommon/data/user.h"
#include "libcommon/data/account.h"
#include "libcommon/data/driveavailable.h"
#include "libcommon/data/drive.h"
#include "libcommon/data/sync.h"
#include "libcommon/info/nodeinfo.h"
#include "libcommon/info/syncfileiteminfo.h"
#include "libcommon/data/error.h"
#include "libcommon/info/parametersinfo.h"
#include "libcommon/info/proxyconfiginfo.h"
#include "libcommon/info/exclusiontemplateinfo.h"
#include "libcommon/data/exclusionapp.h"
#include "libparms/db/parameters.h"

#include "libparms/db/exclusiontemplate.h"
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
        static ExitCode getUserDbIdList(QList<UserDbId> &list);
        static ExitCode getUserDbIdList(std::vector<UserDbId> &list);
        static ExitCode getUserList(QList<User> &list);
        static ExitCode getUserList(std::vector<User> &list);
        static ExitCode getAccountList(QList<Account> &list);
        static ExitCode getAccountList(std::vector<Account> &list);
        static ExitInfo getDriveList(QList<Drive> &list);
        static ExitInfo getDriveList(std::vector<Drive> &list);
        static ExitInfo getDrive(DriveDbId driveDbId, Drive &drive);
        static ExitInfo updateDrive(const Drive &drive);
        static ExitCode getSyncList(QList<Sync> &list);
        static ExitCode getSyncList(std::vector<Sync> &list);
        static ExitCode getParameters(ParametersInfo &parametersInfo);
        static ExitCode updateParameters(const ParametersInfo &parametersInfo);
        static ExitInfo isPathValidForNewSync(const SyncPath &path, SyncConfiguration syncConfig, bool &valid);
        static ExitInfo folderContainsNonExcludedItem(const SyncPath &path, bool &containsNonExcludedFile);
        static ExitInfo findGoodPathForNewSync(const SyncPath &basePath, SyncPath &path, std::string &error);
        static ExitInfo findGoodPathForNewSync(const QString &basePath, QString &path, QString &error);
        static ExitCode getPrivateLinkUrl(DriveDbId driveDbId, const std::string &fileId, std::string &linkUrl);
        static ExitCode getPrivateLinkUrl(DriveDbId driveDbId, const QString &fileId, QString &linkUrl);
        static ExitCode getExclusionTemplateList(bool def, std::vector<ExclusionTemplateInfo> &list);
        static ExitCode getExclusionTemplateList(bool def, QList<ExclusionTemplateInfo> &list);
        static ExitInfo setUserExclusionTemplateList(const std::vector<ExclusionTemplateInfo> &list);
        static ExitCode setExclusionAppList(const bool def, const std::vector<ExclusionApp> &list);
        static ExitCode setUserExclusionTemplateList(const QList<ExclusionTemplateInfo> &list);
        static ExitCode getExclusionAppList(bool def, QList<ExclusionApp> &list);
        static ExitCode getExclusionAppList(bool def, std::vector<ExclusionApp> &list);
        static ExitCode setExclusionAppList(bool def, const QList<ExclusionApp> &list);
        static ExitCode getErrorList(ErrorLevel level, SyncDbId syncDbId, int limit, QList<Error> &list);
        static ExitInfo getErrorList(int limit, std::vector<Error> &list);
        static ExitCode getConflictList(SyncDbId syncDbId, const std::unordered_set<ConflictType> &filter,
                                        std::vector<Error> &errorList);
        static ExitCode getConflictErrorList(DriveDbId driveDbId, const std::unordered_set<ConflictType> &filter,
                                             QList<Error> &errorList);
        static ExitCode deleteErrorsServer();
        static ExitCode deleteErrorsForSync(SyncDbId syncDbId, bool autoResolved);
        static ExitCode deleteInvalidTokenErrors();
        static ExitInfo keepError(const Error &error, bool &keepErrorFlag);
#ifdef Q_OS_MAC
        static ExitCode deleteLiteSyncErrors();
#endif

        // C/S requests (access to network)
        // !!! Use COMM_AVERAGE_TIMEOUT !!!
        static ExitCode requestToken(const std::string &code, const std::string &codeVerifier, User &user, bool &userCreated,
                                     std::string &error, std::string &errorDescr);
        static ExitCode requestToken(const QString &code, const QString &codeVerifier, User &user, bool &userCreated,
                                     std::string &error, std::string &errorDescr);
        static ExitInfo getUserAvailableDrives(UserDbId userDbId,
                                               QList<DriveAvailable> &list); // TODO: Delete after switching to the new comm layer
        static ExitInfo getUserAvailableDrives(UserDbId userDbId, std::vector<DriveAvailable> &list);
        static ExitInfo addSync(UserDbId userDbId, AccountId accountId, DriveId driveId, const SyncPath &localFolderPath,
                                const SyncPath &serverFolderPath, const NodeId &serverFolderNodeId, bool liteSync,
                                Account &account, Drive &drive, Sync &sync, bool &accountCreated, bool &driveCreated);
        static ExitInfo addSync(UserDbId userDbId, AccountId accountId, DriveId driveId, const QString &localFolderPath,
                                const QString &serverFolderPath, const QString &serverFolderNodeId, bool liteSync,
                                Account &account, Drive &drive, Sync &sync, bool &accountCreated, bool &driveCreated);
        static ExitInfo addSync(DriveDbId driveDbId, const SyncPath &localFolderPath, const SyncPath &serverFolderPath,
                                const NodeId &serverFolderNodeId, bool liteSync, Sync &sync);
        static ExitInfo addSync(DriveDbId driveDbId, const QString &localFolderPath, const QString &serverFolderPath,
                                const QString &serverFolderNodeId, bool liteSync, Sync &sync);
        static ExitInfo getNodeInfo(UserDbId userDbId, DriveId driveId, const std::string &nodeId, NodeInfo &nodeInfo,
                                    bool withPath = false);
        static ExitInfo getNodeInfo(UserDbId userDbId, DriveId driveId, const QString &nodeId, NodeInfo &nodeInfo,
                                    bool withPath = false);

        static ExitInfo getSubFolders(const UserDbId userDbId, const DriveId driveId, const NodeId &nodeId,
                                      std::vector<NodeInfo> &list, const bool withPath = false);

        static ExitInfo getSubFolders(UserDbId userDbId, DriveId driveId, const QString &nodeId, QList<NodeInfo> &list,
                                      bool withPath = false); // TODO: Delete after switching to the new comm layer
        static ExitInfo getSubFolders(DriveDbId driveDbId, const NodeId &nodeId, std::vector<NodeInfo> &list,
                                      bool withPath = false);
        static ExitInfo getSubFolders(DriveDbId driveDbId, const QString &nodeId, QList<NodeInfo> &list, bool withPath = false);
        static ExitCode createDir(DriveDbId driveDbId, const NodeId &parentNodeId, const CommString &dirName, NodeId &newNodeId);
        static ExitCode createDir(UserDbId userDbId, DriveId driveId, const NodeId &parentNodeId, const SyncName &dirName,
                                  NodeId &newNodeId);
        static ExitCode createDir(DriveDbId driveDbId, const QString &parentNodeId, const QString &dirName, QString &newNodeId);
        static ExitCode getPublicLinkUrl(DriveDbId driveDbId, const NodeId &nodeId, std::string &linkUrl);
        static ExitInfo getFolderSizeWithCallback(UserDbId userDbId, DriveId driveId, const NodeId &nodeId,
                                                  std::function<void(const QString &, qint64)> callback);
        static ExitInfo getFolderSize(UserDbId userDbId, DriveId driveId, const NodeId &nodeId, int64_t &result);
        static ExitCode getNodeIdByPath(UserDbId userDbId, DriveId driveId, const SyncPath &path, QString &nodeId);
        static ExitInfo getPathByNodeId(UserDbId userDbId, DriveId driveId, const QString &nodeId, QString &path);
        static ExitInfo getPathByNodeId(UserDbId userDbId, DriveId driveId, const NodeId &nodeId, CommString &path);

        // C/S requests (others)
        static ExitInfo deleteUser(UserDbId userDbId); // !!! Use COMM_LONG_TIMEOUT !!!
        static ExitInfo deleteAccount(AccountDbId accountDbId); // !!! Use COMM_LONG_TIMEOUT !!!
        static ExitCode deleteDrive(DriveDbId driveDbId); // !!! Use COMM_LONG_TIMEOUT !!!
        static ExitCode deleteSync(SyncDbId syncDbId); // !!! Use COMM_LONG_TIMEOUT !!!

        // Server requests
        static ExitInfo loadAccountInfo(Account &account, bool &updated);
        static ExitInfo loadDriveInfo(Drive &drive, const AccountId previousAccountId, AccountId &newAccountId, bool &updated,
                                      bool &quotaUpdated);
        static ExitInfo loadUserInfo(User &user, bool &updated);
        static ExitInfo loadUserAvatar(User &user);
        static ExitInfo getThumbnail(DriveDbId driveDbId, const NodeId &nodeId, int width, std::string &thumbnail);

        // Utility
        static void syncFileItemToSyncFileItemInfo(const SyncFileItem &item, SyncFileItemInfo &itemInfo);
        static void parametersToParametersInfo(const Parameters &parameters, ParametersInfo &parametersInfo);
        static void parametersInfoToParameters(const ParametersInfo &parametersInfo, Parameters &parameters);
        static void proxyConfigToProxyConfigInfo(const ProxyConfig &proxyConfig, ProxyConfigInfo &proxyConfigInfo);
        static void proxyConfigInfoToProxyConfig(const ProxyConfigInfo &proxyConfigInfo, ProxyConfig &proxyConfig);
        static void exclusionTemplateToExclusionTemplateInfo(const ExclusionTemplate &exclusionTemplate,
                                                             ExclusionTemplateInfo &exclusionTemplateInfo);
        static void exclusionTemplateInfoToExclusionTemplate(const ExclusionTemplateInfo &exclusionTemplateInfo,
                                                             ExclusionTemplate &exclusionTemplate);
        static bool isDisplayableError(const Error &error);
        static ExitCode getDbStructsFromSyncDbId(SyncDbId syncDbId, User &user, Account &account, Drive &drive, Sync &sync);
        static ExitCode fixProxyConfig();

    private:
        friend class TestServerRequests;
        static ExitCode processRequestTokenFinished(const Login &login, User &user, bool &userCreated);
        static QString canonicalPath(const QString &path);
        static ExitCode checkPathValidityRecursive(const QString &path, QString &error);
        static ExitInfo checkSyncNesting(const std::vector<Sync> &syncList, const QString &path, QString &error);
        static ExitCode syncForPath(const std::vector<Sync> &syncList, const QString &path, SyncDbId &syncDbId);
        static QString excludeFile(bool liteSync);
        static ExitInfo createUser(User &user);
        static ExitInfo updateUser(User &user);
        static ExitCode createAccount(Account &account);
        static ExitCode createDrive(Drive &drive);
        static ExitCode createSync(const Sync &sync);
};

} // namespace KDC

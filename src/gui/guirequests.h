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

#include "libcommon/utility/types.h"
#include "libcommon/info/userinfo.h"
#include "libcommon/info/accountinfo.h"
#include "libcommon/info/nodeinfo.h"
#include "libcommon/info/driveinfo.h"
#include "libcommon/info/driveavailableinfo.h"
#include "libcommon/info/syncinfo.h"
#include "libcommon/info/errorinfo.h"
#include "libcommon/info/parametersinfo.h"
#include "libcommon/info/exclusiontemplateinfo.h"
#include "libcommon/info/exclusionappinfo.h"

#include <QDataStream>
#include <QIODevice>
#include <QList>
#include <QString>
#include <QColor>

namespace KDC {

struct GuiRequests {
        static bool isConnnected();

        // C/S requests (access to DB)
        // Use COMM_SHORT_TIMEOUT
        static ExitCode getUserDbIdList(QList<int> &list);
        static ExitCode getUserInfoList(QList<UserInfo> &list);
        static ExitCode getUserIdFromUserDbId(int userDbId, int &userId);
        static ExitCode getAccountInfoList(QList<AccountInfo> &list);
        static ExitCode getDriveInfoList(QList<DriveInfo> &list);
        static ExitCode getDriveIdFromDriveDbId(int driveDbId, int &driveId);
        static ExitCode getDriveIdFromSyncDbId(int syncDbId, int &driveId);
        static ExitCode getDriveDefaultColor(QColor &color);
        static ExitCode updateDrive(const DriveInfo &driveInfo);
        static ExitCode getSyncInfoList(QList<SyncInfo> &list);
        static ExitCode getSyncStatus(int syncDbId, SyncStatus &status);
        static ExitCode getSyncIsRunning(int syncDbId, bool &running);
        static ExitCode getSyncIdSet(int syncDbId, SyncNodeType type, QSet<QString> &syncIdSet);
        static ExitCode setSyncIdSet(int syncDbId, SyncNodeType type, const QSet<QString> &syncIdSet);
        static ExitCode getParameters(ParametersInfo &parametersInfo);
        static ExitCode updateParameters(const ParametersInfo &parametersInfo);
        static ExitCode getNodePath(int syncDbId, const QString &nodeId, QString &path);
        static ExitCode findGoodPathForNewSync(int driveDbId, const QString &basePath, QString &path, QString &error);
        static ExitCode getPrivateLinkUrl(int driveDbId, const QString &fileId, QString &linkUrl);
        static ExitCode getNameExcluded(const QString &name, bool excluded);
        static ExitCode getExclusionTemplateList(bool def, QList<ExclusionTemplateInfo> &templateList);
        static ExitCode setExclusionTemplateList(bool def, const QList<ExclusionTemplateInfo> &templateList);
#ifdef Q_OS_MAC
        static ExitCode getExclusionAppList(bool def, QList<ExclusionAppInfo> &appList);
        static ExitCode setExclusionAppList(bool def, const QList<ExclusionAppInfo> &appList);
        static ExitCode getFetchingAppList(QHash<QString, QString> &appTable);
#endif
        static ExitCode getErrorInfoList(ErrorLevel level, int syncDbId, int limit, QList<ErrorInfo> &list);
        static ExitCode getConflictList(int driveDbId, const QList<ConflictType> &filter, QList<ErrorInfo> &list);
        static ExitCode deleteErrorsServer();
        static ExitCode deleteErrorsForSync(int syncDbId, bool autoResolved);
        static ExitCode deleteInvalidTokenErrors();
        static ExitCode resolveConflictErrors(int driveDbId, bool keepLocalVersion);
        static ExitCode resolveUnsupportedCharErrors(int driveDbId);
        static ExitCode setSupportsVirtualFiles(int syncDbId, bool value);
        static ExitCode setRootPinState(int syncDbId, PinState pinState);

#ifdef Q_OS_WIN
        static ExitCode showInExplorerNavigationPane(bool &show);
        static ExitCode setShowInExplorerNavigationPane(const bool &show);
#endif

        // C/S requests (access to network)
        // !!! Use COMM_AVERAGE_TIMEOUT !!!
        static ExitCode requestToken(const QString &code, const QString &codeVerifier, int &userDbId, QString &error,
                                     QString &errorDescr);
        static ExitCode getUserAvailableDrives(int userDbId, QHash<int, DriveAvailableInfo> &list);
        static ExitCode addSync(int userDbId, int accountId, int driveId, const QString &localFolderPath,
                                const QString &serverFolderPath, const QString &serverFolderNodeId, bool liteSync,
                                const QSet<QString> &blackList, const QSet<QString> &whiteList, int &syncDbId);
        static ExitCode addSync(int driveDbId, const QString &localFolderPath, const QString &serverFolderPath,
                                const QString &serverFolderNodeId, bool liteSync, const QSet<QString> &blackList,
                                const QSet<QString> &whiteList, int &syncDbId);
        static ExitCode startSyncs(int userDbId);
        static ExitCode getNodeInfo(int userDbId, int driveId, const QString &nodeId, NodeInfo &nodeInfo, bool withPath = false);
        static ExitInfo getSubFolders(int userDbId, int driveId, const QString &nodeId, QList<NodeInfo> &list,
                                      bool withPath = false);
        static ExitInfo getSubFolders(int driveDbId, const QString &nodeId, QList<NodeInfo> &list, bool withPath = false);
        static ExitCode createMissingFolders(int driveDbId, const QList<QPair<QString, QString>> &serverFolderList,
                                             QString &nodeId);
        static ExitCode getPublicLinkUrl(int driveDbId, const QString &fileId, QString &linkUrl);
        static ExitCode getFolderSize(int userDbId, int driveId, const QString &nodeId);

        // C/S requests (others)
        static ExitCode syncStart(int syncDbId); // !!! Use COMM_AVERAGE_TIMEOUT !!!
        static ExitCode syncStop(int syncDbId); // !!! Use COMM_AVERAGE_TIMEOUT !!!
        static ExitCode activateLoadInfo(bool activate);
        static ExitCode askForStatus();
        static ExitCode checkCommStatus(); // !!! Use COMM_LONG_TIMEOUT !!!
        static ExitCode deleteUser(int userDbId); // !!! Use COMM_LONG_TIMEOUT !!!
        static ExitCode deleteDrive(int driveDbId); // !!! Use COMM_LONG_TIMEOUT !!!
        static ExitCode deleteSync(int syncDbId); // Asynchronous because it can be time consuming
        static ExitCode propagateSyncListChange(int syncDbId, bool restartSync);
        static ExitCode bestAvailableVfsMode(VirtualFileMode &mode);
        static ExitCode propagateExcludeListChange(); // !!! Use COMM_LONG_TIMEOUT !!!
        static ExitCode hasSystemLaunchOnStartup(bool &enabled);
        static ExitCode hasLaunchOnStartup(bool &enabled);
        static ExitCode setLaunchOnStartup(bool enabled);
        static ExitCode getAppState(AppStateKey key, AppStateValue &value);
        static ExitCode updateAppState(AppStateKey key, const AppStateValue &value);
        static ExitCode getLogDirEstimatedSize(uint64_t &size);
        static ExitCode sendLogToSupport(bool sendArchivedLogs);
        static ExitCode cancelLogUploadToSupport();
        static ExitCode crash();

        static ExitCode changeDistributionChannel(VersionChannel channel);
        static ExitCode versionInfo(VersionInfo &versionInfo, VersionChannel channel = VersionChannel::Unknown);
        static ExitCode updateState(UpdateState &state);
        static ExitCode startInstaller();
        static ExitCode skipUpdate(const std::string &version);
        static ExitCode reportClientDisplayed();
        static ExitCode updateSystray(SyncStatus syncStatus, const QString &tooltip, bool alert = false);
        static ExitCode getOfflineFilesTotalSize(int driveDbId, uint64_t &totalSize);
};
} // namespace KDC

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

#include "communicationlayer/ipcclient.h"
#include "communicationlayer/signaldispatcher.h"
#include "libcommon/utility/cstypes.h"
#include "libcommon/utility/types.h"
#include "libcommon/info/accountinfo.h"
#include "libcommon/info/driveinfo.h"
#include "libcommon/info/driveavailableinfo.h"
#include "libcommon/info/errorinfo.h"
#include "libcommon/info/exclusiontemplateinfo.h"
#include "libcommon/info/nodeconflictinfo.h"
#include "libcommon/info/nodeinfo.h"
#include "libcommon/info/searchinfo.h"
#include "libcommon/info/parametersinfo.h"
#include "libcommon/info/syncfileiteminfo.h"
#include "libcommon/info/syncinfo.h"
#include "libcommon/info/userinfo.h"

#include <QObject>
#include <QString>

#include <functional>
#include <vector>

namespace KDC {

// ---------------------------------------------------------------------------
// Result types for requests that return more than one semantic value
// ---------------------------------------------------------------------------

struct LoginTokenResult {
        UserDbId userDbId{0};
        QString error;
        QString errorDescription;
};

struct GoodPathResult {
        SyncPath goodPath;
        QString errorMessage;
};

struct SyncAddRequest {
        UserDbId userDbId{0};
        AccountId accountId{0};
        DriveId driveId{0};
        SyncPath localFolderPath;
        SyncPath serverFolderPath;
        NodeId serverFolderNodeId;
        bool liteSync{false};
        std::vector<NodeId> blackList;
};

struct SyncAdd2Request {
        DriveDbId driveDbId{0};
        SyncPath localFolderPath;
        SyncPath serverFolderPath;
        NodeId serverFolderNodeId;
        bool liteSync{false};
        std::vector<NodeId> blackList;
};

struct DriveSearchResult {
        std::vector<SearchInfo> searchInfoList;
        // NOTE: hasMore is always false. DriveSearchJob on the server side never populates _hasMore
        // in its process() implementation. Do not rely on this field until the server is fixed.
        bool hasMore{false};
};

// ---------------------------------------------------------------------------

/**
 * Low-level backend-facing service.
 *
 * Responsibilities:
 * - registers handlers on SignalDispatcher and re-emits server-push signals as typed Qt signals
 * - sends feature requests through IpcClient and parses JSON results into typed DTOs
 *
 * This is an internal C++ boundary. QML must not call this class directly;
 * it should interact with higher-level services instead.
 */
class ServerCommService : public QObject {
        Q_OBJECT

    public:
        // Callback types
        using VoidCallback = std::function<void(const ExitInfo &)>;
        using BoolCallback = std::function<void(const ExitInfo &, bool)>;
        using LoginTokenCallback = std::function<void(const ExitInfo &, const LoginTokenResult &)>;
        using UserDbIdListCallback = std::function<void(const ExitInfo &, const std::vector<UserDbId> &)>;
        using UserInfoListCallback = std::function<void(const ExitInfo &, const std::vector<UserInfo> &)>;
        using DriveAvailableInfoListCallback = std::function<void(const ExitInfo &, const std::vector<DriveAvailableInfo> &)>;
        using AccountInfoListCallback = std::function<void(const ExitInfo &, const std::vector<AccountInfo> &)>;
        using DriveInfoListCallback = std::function<void(const ExitInfo &, const std::vector<DriveInfo> &)>;
        using SyncInfoListCallback = std::function<void(const ExitInfo &, const std::vector<SyncInfo> &)>;
        using SyncInfoCallback = std::function<void(const ExitInfo &, const SyncInfo &)>;
        using SyncStatusCallback = std::function<void(const ExitInfo &, SyncStatus)>;
        using GoodPathCallback = std::function<void(const ExitInfo &, const GoodPathResult &)>;
        using AppStateCallback = std::function<void(const ExitInfo &, const QString &)>;
        using LogSizeCallback = std::function<void(const ExitInfo &, uint64_t)>;
        using StringCallback = std::function<void(const ExitInfo &, const QString &)>;
        using NodeIdListCallback = std::function<void(const ExitInfo &, const std::vector<NodeId> &)>;
        using NodeInfoCallback = std::function<void(const ExitInfo &, const NodeInfo &)>;
        using NodeInfoListCallback = std::function<void(const ExitInfo &, const std::vector<NodeInfo> &)>;
        using FolderSizeCallback = std::function<void(const ExitInfo &, int64_t)>;
        using ParametersInfoCallback = std::function<void(const ExitInfo &, const ParametersInfo &)>;
        using ErrorInfoListCallback = std::function<void(const ExitInfo &, const std::vector<ErrorInfo> &)>;
        using ExclusionTemplateListCallback = std::function<void(const ExitInfo &, const std::vector<ExclusionTemplateInfo> &)>;
        using UpdateStateCallback = std::function<void(const ExitInfo &, UpdateState)>;
        using VersionInfoCallback = std::function<void(const ExitInfo &, const VersionInfo &)>;
        using NodeIdCallback = std::function<void(const ExitInfo &, const NodeId &)>;
        using DriveSearchCallback = std::function<void(const ExitInfo &, const DriveSearchResult &)>;
        using NodeConflictInfoCallback = std::function<void(const ExitInfo &, const NodeConflictInfo &)>;

        explicit ServerCommService(IpcClient &client, SignalDispatcher &dispatcher, QObject *parent = nullptr);

        // --- Request methods ---
        //
        // All request methods are marked `const` because they do not modify any member of *this*.
        // They do, however, cause observable side effects by sending IPC messages through _ipcClient,
        // which is stored as a reference member (so the const qualifier of *this* does not propagate to it).
        // Callers must ensure that any objects captured in the callback outlive the server response.
        //

        // --- Login ---
        void requestLoginToken(const QString &code, const QString &codeVerifier, LoginTokenCallback callback) const;

        // --- User ---
        void requestUserDbIdList(UserDbIdListCallback callback) const;
        void requestUserInfoList(UserInfoListCallback callback) const;
        void requestUserAvailableDrives(UserDbId userDbId, DriveAvailableInfoListCallback callback) const;
        void requestDeleteUser(UserDbId userDbId, VoidCallback callback) const;

        // --- Account ---
        void requestAccountInfoList(AccountInfoListCallback callback) const;

        // --- Drive ---
        void requestDriveInfoList(DriveInfoListCallback callback) const;
        void requestDriveUpdate(const DriveInfo &driveInfo, VoidCallback callback) const;
        void requestDriveDelete(DriveDbId driveDbId, VoidCallback callback) const;
        void requestDriveSearch(SyncDbId syncDbId, const QString &searchString, DriveSearchCallback callback) const;

        // --- Sync ---
        void requestSyncInfoList(SyncInfoListCallback callback) const;
        void requestSyncAdd(const SyncAddRequest &request, SyncInfoCallback callback) const;
        void requestSyncAdd2(const SyncAdd2Request &request, SyncInfoCallback callback) const;
        void requestSyncStart(SyncDbId syncDbId, VoidCallback callback) const;
        void requestSyncStop(SyncDbId syncDbId, VoidCallback callback) const;
        void requestSyncStatus(SyncDbId syncDbId, SyncStatusCallback callback) const;
        void requestSyncDelete(SyncDbId syncDbId, VoidCallback callback) const;
        void requestStartSyncsAfterLogin(UserDbId userDbId, VoidCallback callback) const;
        void requestSyncTriggerProgressUpdate(SyncDbId syncDbId, VoidCallback callback) const;
        void requestSyncGetPrivateLinkUrl(DriveDbId driveDbId, const NodeId &nodeId, StringCallback callback) const;
        void requestSyncGetPublicLinkUrl(DriveDbId driveDbId, const NodeId &nodeId, StringCallback callback) const;

        // --- Error ---
        void requestErrorInfoList(ErrorInfoListCallback callback) const;
        void requestErrorDelete(ErrorDbId errorDbId, VoidCallback callback) const;
        void requestErrorResolveConflicts(const std::vector<ErrorDbId> &keepLocalList,
                                          const std::vector<ErrorDbId> &keepRemoteList, VoidCallback callback) const;
        void requestErrorResolveConflictsQuick(const std::vector<ErrorDbId> &errorDbIdList,
                                               ConflictResolutionStrategy strategy, VoidCallback callback) const;

        // --- Node ---
        void requestNodeInfo(UserDbId userDbId, DriveId driveId, const NodeId &nodeId, bool withPath,
                             NodeInfoCallback callback) const;
        void requestNodeConflictInfo(SyncDbId syncDbId, const SyncPath &relativePath, ReplicaSide replicaSide,
                                     NodeConflictInfoCallback callback) const;
        void requestNodePath(SyncDbId syncDbId, const NodeId &nodeId, StringCallback callback) const;
        void requestNodeSubfolders(DriveDbId driveDbId, const NodeId &nodeId, bool withPath, NodeInfoListCallback callback) const;
        void requestNodeSubfolders2(DriveDbId driveDbId, const NodeId &nodeId, bool withPath,
                                    NodeInfoListCallback callback) const;
        void requestNodeFolderSize(UserDbId userDbId, DriveId driveId, const NodeId &nodeId, FolderSizeCallback callback) const;
        void requestNodeCreateMissingFolders(DriveDbId driveDbId, const NodeId &parentNodeId, const SyncPath &relativePath,
                                             NodeIdCallback callback) const;

        // --- Parameters ---
        void requestParametersInfo(ParametersInfoCallback callback) const;
        void requestParametersUpdate(const ParametersInfo &parametersInfo, VoidCallback callback) const;

        // --- Blacklist ---
        void requestBlacklistedNodeList(SyncDbId syncDbId, NodeIdListCallback callback) const;
        void requestBlacklistedNodeSetList(SyncDbId syncDbId, const std::vector<NodeId> &nodeIdList, VoidCallback callback) const;

        // --- Exclusion templates ---
        void requestExclTemplGetList(bool defaultTemplates, ExclusionTemplateListCallback callback) const;
        void requestExclTemplSetList(const std::vector<ExclusionTemplateInfo> &templateList, VoidCallback callback) const;
        void requestExclTemplGetExcluded(const QString &name, BoolCallback callback) const;

        // --- Updater ---
        void requestUpdaterState(UpdateStateCallback callback) const;
        void requestUpdaterVersionInfo(VersionChannel channel, VersionInfoCallback callback) const;
        void requestUpdaterSkipVersion(const QString &skippedVersion, VoidCallback callback) const;
        void requestUpdaterStartInstaller(VoidCallback callback) const;

        // --- Utility ---
        void requestCheckCommStatus(VoidCallback callback) const;
        void requestFindGoodPathForNewSync(const SyncPath &basePath, GoodPathCallback callback) const;
        void requestIsPathValidForNewSync(const SyncPath &path, SyncConfiguration syncConfiguration, BoolCallback callback) const;
        void requestGetAppState(AppStateKey key, AppStateCallback callback) const;
        void requestSetAppState(AppStateKey key, const QString &value, VoidCallback callback) const;
        void requestHasSystemLaunchOnStartup(BoolCallback callback) const;
        void requestActivateLoadInfo(VoidCallback callback) const;
        void requestSendLogToSupport(bool includeArchivedLogs, VoidCallback callback) const;
        void requestCancelLogToSupport(VoidCallback callback) const;
        void requestGetLogEstimatedSize(LogSizeCallback callback) const;
        void requestSendAppStartTrace(VoidCallback callback) const;
        void requestQuit(VoidCallback callback) const;

    signals:
        // --- User ---
        void userAdded(const UserInfo &info);
        void userUpdated(const UserInfo &info);
        void userRemoved(UserDbId userDbId);

        // --- Account ---
        void accountAdded(const AccountInfo &info);
        void accountUpdated(const AccountInfo &info);
        void accountRemoved(AccountDbId accountDbId);

        // --- Drive ---
        void driveAdded(const DriveInfo &info);
        void driveUpdated(const DriveInfo &info);
        void driveRemoved(DriveDbId driveDbId);

        // --- Sync ---
        void syncAdded(const SyncInfo &info);
        void syncUpdated(const SyncInfo &info);
        void syncRemoved(SyncDbId syncDbId);
        void syncProgressInfo(SyncDbId syncDbId, SyncStatus status, SyncStep step, int64_t currentFile, int64_t totalFiles,
                              int64_t completedSize, int64_t totalSize, int64_t estimatedRemainingTime);
        void itemCompleted(SyncDbId syncDbId, const SyncFileItemInfo &info);

        // --- Error ---
        void errorAdded(const ErrorInfo &info);
        void errorRemoved(ErrorDbId errorDbId);

        // --- Updater ---
        void updateStateChanged(UpdateState state);
        void showUpdateDialog(const VersionInfo &info);

        // --- Utility ---
        void showNotification(const QString &title, const QString &message);
        void showSettings();
        void showSynthesis();
        void logUploadStatusUpdated(LogUploadState state, int32_t percentage);
        void quit();

    private:
        IpcClient &_ipcClient;

        void registerUserHandlers(SignalDispatcher &dispatcher);
        void registerAccountHandlers(SignalDispatcher &dispatcher);
        void registerDriveHandlers(SignalDispatcher &dispatcher);
        void registerSyncHandlers(SignalDispatcher &dispatcher);
        void registerErrorHandlers(SignalDispatcher &dispatcher);
        void registerUpdaterHandlers(SignalDispatcher &dispatcher);
        void registerUtilityHandlers(SignalDispatcher &dispatcher);
};

} // namespace KDC

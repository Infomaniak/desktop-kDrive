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

#include "servercommservice.h"

#include "libcommon/comm.h"
#include "libcommon/utility/utility.h"

namespace KDC {

ServerCommService::ServerCommService(IpcClient &client, SignalDispatcher &dispatcher, QObject *parent) :
    QObject(parent),
    _ipcClient(client) {
    registerUserHandlers(dispatcher);
    registerAccountHandlers(dispatcher);
    registerDriveHandlers(dispatcher);
    registerSyncHandlers(dispatcher);
    registerErrorHandlers(dispatcher);
    registerUpdaterHandlers(dispatcher);
    registerUtilityHandlers(dispatcher);
}

// -- User ---------------------------------------------------------------------

void ServerCommService::registerUserHandlers(SignalDispatcher &dispatcher) {
    dispatcher.registerHandler(SignalNum::USER_ADDED, [this](const Poco::DynamicStruct &params) {
        UserInfo info;
        info.fromDynamicStruct(params[msgParamUserInfo].extract<Poco::DynamicStruct>());
        emit userAdded(info);
    });

    dispatcher.registerHandler(SignalNum::USER_UPDATED, [this](const Poco::DynamicStruct &params) {
        UserInfo info;
        info.fromDynamicStruct(params[msgParamUserInfo].extract<Poco::DynamicStruct>());
        emit userUpdated(info);
    });

    dispatcher.registerHandler(SignalNum::USER_REMOVED, [this](const Poco::DynamicStruct &params) {
        UserDbId userDbId = 0;
        CommonUtility::readValueFromStruct(params, msgParamUserDbId, userDbId);
        emit userRemoved(userDbId);
    });
}

// -- Account ------------------------------------------------------------------

void ServerCommService::registerAccountHandlers(SignalDispatcher &dispatcher) {
    dispatcher.registerHandler(SignalNum::ACCOUNT_ADDED, [this](const Poco::DynamicStruct &params) {
        AccountInfo info;
        info.fromDynamicStruct(params[msgParamAccountInfo].extract<Poco::DynamicStruct>());
        emit accountAdded(info);
    });

    dispatcher.registerHandler(SignalNum::ACCOUNT_UPDATED, [this](const Poco::DynamicStruct &params) {
        AccountInfo info;
        info.fromDynamicStruct(params[msgParamAccountInfo].extract<Poco::DynamicStruct>());
        emit accountUpdated(info);
    });

    dispatcher.registerHandler(SignalNum::ACCOUNT_REMOVED, [this](const Poco::DynamicStruct &params) {
        AccountDbId accountDbId = 0;
        CommonUtility::readValueFromStruct(params, msgParamAccountDbId, accountDbId);
        emit accountRemoved(accountDbId);
    });
}

// -- Drive ---------------------------------------------------------------------

void ServerCommService::registerDriveHandlers(SignalDispatcher &dispatcher) {
    dispatcher.registerHandler(SignalNum::DRIVE_ADDED, [this](const Poco::DynamicStruct &params) {
        DriveInfo info;
        info.fromDynamicStruct(params[msgParamDriveInfo].extract<Poco::DynamicStruct>());
        emit driveAdded(info);
    });

    dispatcher.registerHandler(SignalNum::DRIVE_UPDATED, [this](const Poco::DynamicStruct &params) {
        DriveInfo info;
        info.fromDynamicStruct(params[msgParamDriveInfo].extract<Poco::DynamicStruct>());
        emit driveUpdated(info);
    });

    dispatcher.registerHandler(SignalNum::DRIVE_REMOVED, [this](const Poco::DynamicStruct &params) {
        DriveDbId driveDbId = 0;
        CommonUtility::readValueFromStruct(params, msgParamDriveDbId, driveDbId);
        emit driveRemoved(driveDbId);
    });
}

// -- Sync ----------------------------------------------------------------------

void ServerCommService::registerSyncHandlers(SignalDispatcher &dispatcher) {
    dispatcher.registerHandler(SignalNum::SYNC_ADDED, [this](const Poco::DynamicStruct &params) {
        SyncInfo info;
        info.fromDynamicStruct(params[msgParamSyncInfo].extract<Poco::DynamicStruct>());
        emit syncAdded(info);
    });

    dispatcher.registerHandler(SignalNum::SYNC_UPDATED, [this](const Poco::DynamicStruct &params) {
        SyncInfo info;
        info.fromDynamicStruct(params[msgParamSyncInfo].extract<Poco::DynamicStruct>());
        emit syncUpdated(info);
    });

    dispatcher.registerHandler(SignalNum::SYNC_REMOVED, [this](const Poco::DynamicStruct &params) {
        SyncDbId syncDbId = 0;
        CommonUtility::readValueFromStruct(params, msgParamSyncDbId, syncDbId);
        emit syncRemoved(syncDbId);
    });

    dispatcher.registerHandler(SignalNum::SYNC_PROGRESSINFO, [this](const Poco::DynamicStruct &params) {
        SyncDbId syncDbId = 0;
        auto status = SyncStatus::Undefined;
        auto step = SyncStep::Idle;
        CommonUtility::readValueFromStruct(params, msgParamSyncDbId, syncDbId);
        CommonUtility::readValueFromStruct(params, msgParamSyncStatus, status);
        CommonUtility::readValueFromStruct(params, msgParamSyncStep, step);

        const Poco::DynamicStruct progressStruct = params[msgParamSyncProgress].extract<Poco::DynamicStruct>();
        int64_t currentFile = 0;
        int64_t totalFiles = 0;
        int64_t completedSize = 0;
        int64_t totalSize = 0;
        int64_t estimatedRemainingTime = 0;
        CommonUtility::readValueFromStruct(progressStruct, msgParamCurrentFile, currentFile);
        CommonUtility::readValueFromStruct(progressStruct, msgParamTotalFiles, totalFiles);
        CommonUtility::readValueFromStruct(progressStruct, msgParamCompletedSize, completedSize);
        CommonUtility::readValueFromStruct(progressStruct, msgParamTotalSize, totalSize);
        CommonUtility::readValueFromStruct(progressStruct, msgParamEstimatedRemainingTime, estimatedRemainingTime);

        emit syncProgressInfo(syncDbId, status, step, currentFile, totalFiles, completedSize, totalSize, estimatedRemainingTime);
    });

    dispatcher.registerHandler(SignalNum::SYNC_COMPLETEDITEM, [this](const Poco::DynamicStruct &params) {
        SyncDbId syncDbId = 0;
        CommonUtility::readValueFromStruct(params, msgParamSyncDbId, syncDbId);
        SyncFileItemInfo info;
        info.fromDynamicStruct(params[msgParamItemInfo].extract<Poco::DynamicStruct>());
        emit itemCompleted(syncDbId, info);
    });
}

// -- Error ---------------------------------------------------------------------

void ServerCommService::registerErrorHandlers(SignalDispatcher &dispatcher) {
    dispatcher.registerHandler(SignalNum::UTILITY_ERROR_ADDED, [this](const Poco::DynamicStruct &params) {
        ErrorInfo info;
        info.fromDynamicStruct(params[msgParamErrorInfo].extract<Poco::DynamicStruct>());
        emit errorAdded(info);
    });

    dispatcher.registerHandler(SignalNum::UTILITY_ERROR_REMOVED, [this](const Poco::DynamicStruct &params) {
        ErrorDbId errorDbId = 0;
        CommonUtility::readValueFromStruct(params, msgParamErrorDbId, errorDbId);
        emit errorRemoved(errorDbId);
    });
}

// -- Updater -------------------------------------------------------------------

void ServerCommService::registerUpdaterHandlers(SignalDispatcher &dispatcher) {
    dispatcher.registerHandler(SignalNum::UPDATER_STATE_CHANGED, [this](const Poco::DynamicStruct &params) {
        UpdateState state = UpdateState::Unknown;
        CommonUtility::readValueFromStruct(params, msgParamUpdateState, state);
        emit updateStateChanged(state);
    });
}

// -- Utility -------------------------------------------------------------------

void ServerCommService::registerUtilityHandlers(SignalDispatcher &dispatcher) {
    dispatcher.registerHandler(SignalNum::UTILITY_SHOW_NOTIFICATION, [this](const Poco::DynamicStruct &params) {
        CommString title;
        CommString message;
        CommonUtility::readValueFromStruct(params, msgParamTitle, title);
        CommonUtility::readValueFromStruct(params, msgParamMessage, message);
        emit showNotification(CommonUtility::commString2QStr(title), CommonUtility::commString2QStr(message));
    });

    dispatcher.registerHandler(SignalNum::UTILITY_SHOW_SETTINGS, [this](const Poco::DynamicStruct &) { emit showSettings(); });

    dispatcher.registerHandler(SignalNum::UTILITY_SHOW_SYNTHESIS, [this](const Poco::DynamicStruct &) { emit showSynthesis(); });

    dispatcher.registerHandler(SignalNum::UTILITY_LOG_UPLOAD_STATUS_UPDATED, [this](const Poco::DynamicStruct &params) {
        LogUploadState state = LogUploadState::None;
        int32_t percentage = 0;
        CommonUtility::readValueFromStruct(params, msgParamLogUploadState, state);
        CommonUtility::readValueFromStruct(params, msgParamPercentage, percentage);
        emit logUploadStatusUpdated(state, percentage);
    });

    dispatcher.registerHandler(SignalNum::UTILITY_QUIT, [this](const Poco::DynamicStruct &) { emit quit(); });
}

// =============================================================================
// Request methods
// =============================================================================

// -- Login ---------------------------------------------------------------

void ServerCommService::requestLoginToken(const QString &code, const QString &codeVerifier,
                                          const LoginTokenCallback &callback) const {
    Poco::DynamicStruct params;
    params[msgParamAuthCode] = CommonUtility::qStr2CommString(code);
    params[msgParamCodeVerifier] = CommonUtility::qStr2CommString(codeVerifier);
    _ipcClient.sendRequest(RequestNum::LOGIN_REQUESTTOKEN, params,
                           [callback](const ExitInfo &exitInfo, const Poco::DynamicStruct &result) {
                               LoginTokenResult loginResult;
                               if (exitInfo.code() == ExitCode::Ok) {
                                   CommonUtility::readValueFromStruct(result, msgParamUserDbId, loginResult.userDbId);
                               } else {
                                   CommString error;
                                   CommString errorDescr;
                                   CommonUtility::readValueFromStruct(result, msgParamError, error);
                                   CommonUtility::readValueFromStruct(result, msgParamErrorDescr, errorDescr);
                                   loginResult.error = CommonUtility::commString2QStr(error);
                                   loginResult.errorDescription = CommonUtility::commString2QStr(errorDescr);
                               }
                               callback(exitInfo, loginResult);
                           });
}

// -- User ----------------------------------------------------------------

void ServerCommService::requestUserDbIdList(const UserDbIdListCallback &callback) const {
    _ipcClient.sendRequest(RequestNum::USER_DBIDLIST, {},
                           [callback](const ExitInfo &exitInfo, const Poco::DynamicStruct &result) {
                               std::vector<UserDbId> list;
                               if (exitInfo.code() == ExitCode::Ok) {
                                   CommonUtility::readValuesFromStruct(result, msgParamUserDbIdList, list);
                               }
                               callback(exitInfo, list);
                           });
}

void ServerCommService::requestUserInfoList(const UserInfoListCallback &callback) const {
    _ipcClient.sendRequest(
            RequestNum::USER_INFOLIST, {}, [callback](const ExitInfo &exitInfo, const Poco::DynamicStruct &result) {
                std::vector<UserInfo> list;
                if (exitInfo.code() == ExitCode::Ok) {
                    CommonUtility::readValuesFromStruct(result, msgParamUserInfoList, list, dynamicVar2Struct<UserInfo>);
                }
                callback(exitInfo, list);
            });
}

void ServerCommService::requestUserAvailableDrives(const UserDbId userDbId,
                                                   const DriveAvailableInfoListCallback &callback) const {
    Poco::DynamicStruct params;
    params[msgParamUserDbId] = userDbId;
    _ipcClient.sendRequest(RequestNum::USER_AVAILABLEDRIVES, params,
                           [callback](const ExitInfo &exitInfo, const Poco::DynamicStruct &result) {
                               std::vector<DriveAvailableInfo> list;
                               if (exitInfo.code() == ExitCode::Ok) {
                                   CommonUtility::readValuesFromStruct(result, msgParamDriveAvailableInfoList, list,
                                                                       dynamicVar2Struct<DriveAvailableInfo>);
                               }
                               callback(exitInfo, list);
                           });
}

void ServerCommService::requestDeleteUser(const UserDbId userDbId, const VoidCallback &callback) const {
    Poco::DynamicStruct params;
    params[msgParamUserDbId] = userDbId;
    _ipcClient.sendRequest(RequestNum::USER_DELETE, params,
                           [callback](const ExitInfo &exitInfo, const Poco::DynamicStruct &) { callback(exitInfo); });
}

// -- Account -------------------------------------------------------------

void ServerCommService::requestAccountInfoList(const AccountInfoListCallback &callback) const {
    _ipcClient.sendRequest(
            RequestNum::ACCOUNT_INFOLIST, {}, [callback](const ExitInfo &exitInfo, const Poco::DynamicStruct &result) {
                std::vector<AccountInfo> list;
                if (exitInfo.code() == ExitCode::Ok) {
                    CommonUtility::readValuesFromStruct(result, msgParamAccountInfoList, list, dynamicVar2Struct<AccountInfo>);
                }
                callback(exitInfo, list);
            });
}

// -- Drive ---------------------------------------------------------------

void ServerCommService::requestDriveInfoList(const DriveInfoListCallback &callback) const {
    _ipcClient.sendRequest(
            RequestNum::DRIVE_INFOLIST, {}, [callback](const ExitInfo &exitInfo, const Poco::DynamicStruct &result) {
                std::vector<DriveInfo> list;
                if (exitInfo.code() == ExitCode::Ok) {
                    CommonUtility::readValuesFromStruct(result, msgParamDriveInfoList, list, dynamicVar2Struct<DriveInfo>);
                }
                callback(exitInfo, list);
            });
}

void ServerCommService::requestDriveUpdate(const DriveInfo &driveInfo, const VoidCallback &callback) const {
    Poco::DynamicStruct params;
    Poco::DynamicStruct driveStruct;
    driveInfo.toDynamicStruct(driveStruct);
    params[msgParamDriveInfo] = driveStruct;
    _ipcClient.sendRequest(RequestNum::DRIVE_UPDATE, params,
                           [callback](const ExitInfo &exitInfo, const Poco::DynamicStruct &) { callback(exitInfo); });
}

void ServerCommService::requestDriveDelete(const DriveDbId driveDbId, const VoidCallback &callback) const {
    Poco::DynamicStruct params;
    params[msgParamDriveDbId] = driveDbId;
    _ipcClient.sendRequest(RequestNum::DRIVE_DELETE, params,
                           [callback](const ExitInfo &exitInfo, const Poco::DynamicStruct &) { callback(exitInfo); });
}

void ServerCommService::requestDriveSearch(const SyncDbId syncDbId, const QString &searchString,
                                           const DriveSearchCallback &callback) const {
    Poco::DynamicStruct params;
    params[msgParamSyncDbId] = syncDbId;
    params[msgParamSearchString] = CommonUtility::qStr2CommString(searchString);
    _ipcClient.sendRequest(
            RequestNum::DRIVE_SEARCH, params, [callback](const ExitInfo &exitInfo, const Poco::DynamicStruct &result) {
                DriveSearchResult searchResult;
                if (exitInfo.code() == ExitCode::Ok) {
                    CommonUtility::readValuesFromStruct(result, msgParamSearchInfoList, searchResult.searchInfoList,
                                                        dynamicVar2Struct<SearchInfo>);
                    CommonUtility::readValueFromStruct(result, msgParamHasMore, searchResult.hasMore);
                }
                callback(exitInfo, searchResult);
            });
}

// -- Sync core -----------------------------------------------------------

void ServerCommService::requestSyncInfoList(const SyncInfoListCallback &callback) const {
    _ipcClient.sendRequest(
            RequestNum::SYNC_INFOLIST, {}, [callback](const ExitInfo &exitInfo, const Poco::DynamicStruct &result) {
                std::vector<SyncInfo> list;
                if (exitInfo.code() == ExitCode::Ok) {
                    CommonUtility::readValuesFromStruct(result, msgParamSyncInfoList, list, dynamicVar2Struct<SyncInfo>);
                }
                callback(exitInfo, list);
            });
}

void ServerCommService::requestSyncAdd(const SyncAddRequest &request, const SyncInfoCallback &callback) const {
    Poco::DynamicStruct params;
    params[msgParamUserDbId] = request.userDbId;
    params[msgParamAccountId] = request.accountId;
    params[msgParamDriveId] = request.driveId;
    params[msgParamLocalFolderPath] = CommonUtility::syncPath2CommString(request.localFolderPath);
    params[msgParamServerFolderPath] = CommonUtility::syncPath2CommString(request.serverFolderPath);
    params[msgParamServerFolderNodeId] = request.serverFolderNodeId;
    params[msgParamLiteSync] = request.liteSync;
    params[msgParamBlackList] = request.blackList;
    _ipcClient.sendRequest(RequestNum::SYNC_ADD, params, [callback](const ExitInfo &exitInfo, const Poco::DynamicStruct &result) {
        SyncInfo info;
        if (exitInfo.code() == ExitCode::Ok) {
            info.fromDynamicStruct(result[msgParamSyncInfo].extract<Poco::DynamicStruct>());
        }
        callback(exitInfo, info);
    });
}

void ServerCommService::requestSyncAdd2(const SyncAdd2Request &request, const SyncInfoCallback &callback) const {
    Poco::DynamicStruct params;
    params[msgParamDriveDbId] = request.driveDbId;
    params[msgParamLocalFolderPath] = CommonUtility::syncPath2CommString(request.localFolderPath);
    params[msgParamServerFolderPath] = CommonUtility::syncPath2CommString(request.serverFolderPath);
    params[msgParamServerFolderNodeId] = request.serverFolderNodeId;
    params[msgParamLiteSync] = request.liteSync;
    params[msgParamBlackList] = request.blackList;
    _ipcClient.sendRequest(RequestNum::SYNC_ADD2, params,
                           [callback](const ExitInfo &exitInfo, const Poco::DynamicStruct &result) {
                               SyncInfo info;
                               if (exitInfo.code() == ExitCode::Ok) {
                                   info.fromDynamicStruct(result[msgParamSyncInfo].extract<Poco::DynamicStruct>());
                               }
                               callback(exitInfo, info);
                           });
}

void ServerCommService::requestSyncStart(const SyncDbId syncDbId, const VoidCallback &callback) const {
    Poco::DynamicStruct params;
    params[msgParamSyncDbId] = syncDbId;
    _ipcClient.sendRequest(RequestNum::SYNC_START, params,
                           [callback](const ExitInfo &exitInfo, const Poco::DynamicStruct &) { callback(exitInfo); });
}

void ServerCommService::requestSyncStop(const SyncDbId syncDbId, const VoidCallback &callback) const {
    Poco::DynamicStruct params;
    params[msgParamSyncDbId] = syncDbId;
    _ipcClient.sendRequest(RequestNum::SYNC_STOP, params,
                           [callback](const ExitInfo &exitInfo, const Poco::DynamicStruct &) { callback(exitInfo); });
}

void ServerCommService::requestSyncStatus(const SyncDbId syncDbId, const SyncStatusCallback &callback) const {
    Poco::DynamicStruct params;
    params[msgParamSyncDbId] = syncDbId;
    _ipcClient.sendRequest(RequestNum::SYNC_STATUS, params,
                           [callback](const ExitInfo &exitInfo, const Poco::DynamicStruct &result) {
                               auto status = SyncStatus::Undefined;
                               if (exitInfo.code() == ExitCode::Ok) {
                                   CommonUtility::readValueFromStruct(result, msgParamSyncStatus, status);
                               }
                               callback(exitInfo, status);
                           });
}

void ServerCommService::requestSyncDelete(const SyncDbId syncDbId, const VoidCallback &callback) const {
    Poco::DynamicStruct params;
    params[msgParamSyncDbId] = syncDbId;
    _ipcClient.sendRequest(RequestNum::SYNC_DELETE, params,
                           [callback](const ExitInfo &exitInfo, const Poco::DynamicStruct &) { callback(exitInfo); });
}

void ServerCommService::requestStartSyncsAfterLogin(const UserDbId userDbId, const VoidCallback &callback) const {
    Poco::DynamicStruct params;
    params[msgParamUserDbId] = userDbId;
    _ipcClient.sendRequest(RequestNum::SYNC_START_AFTER_LOGIN, params,
                           [callback](const ExitInfo &exitInfo, const Poco::DynamicStruct &) { callback(exitInfo); });
}

// -- Sync advanced -------------------------------------------------------

void ServerCommService::requestSyncTriggerProgressUpdate(const SyncDbId syncDbId, const VoidCallback &callback) const {
    Poco::DynamicStruct params;
    params[msgParamSyncDbId] = syncDbId;
    _ipcClient.sendRequest(RequestNum::SYNC_TRIGGER_PROGRESS_UPDATE, params,
                           [callback](const ExitInfo &exitInfo, const Poco::DynamicStruct &) { callback(exitInfo); });
}

void ServerCommService::requestSyncGetPrivateLinkUrl(const DriveDbId driveDbId, const NodeId &nodeId,
                                                     const StringCallback &callback) const {
    Poco::DynamicStruct params;
    params[msgParamDriveDbId] = driveDbId;
    params[msgParamNodeId] = nodeId;
    _ipcClient.sendRequest(RequestNum::SYNC_GETPRIVATELINKURL, params,
                           [callback](const ExitInfo &exitInfo, const Poco::DynamicStruct &result) {
                               QString url;
                               if (exitInfo.code() == ExitCode::Ok) {
                                   CommString linkUrl;
                                   CommonUtility::readValueFromStruct(result, msgParamLinkUrl, linkUrl);
                                   url = CommonUtility::commString2QStr(linkUrl);
                               }
                               callback(exitInfo, url);
                           });
}

void ServerCommService::requestSyncGetPublicLinkUrl(const DriveDbId driveDbId, const NodeId &nodeId,
                                                    const StringCallback &callback) const {
    Poco::DynamicStruct params;
    params[msgParamDriveDbId] = driveDbId;
    params[msgParamNodeId] = nodeId;
    _ipcClient.sendRequest(RequestNum::SYNC_GETPUBLICLINKURL, params,
                           [callback](const ExitInfo &exitInfo, const Poco::DynamicStruct &result) {
                               QString url;
                               if (exitInfo.code() == ExitCode::Ok) {
                                   CommString linkUrl;
                                   CommonUtility::readValueFromStruct(result, msgParamLinkUrl, linkUrl);
                                   url = CommonUtility::commString2QStr(linkUrl);
                               }
                               callback(exitInfo, url);
                           });
}

// -- Error ---------------------------------------------------------------

void ServerCommService::requestErrorInfoList(const ErrorInfoListCallback &callback) const {
    _ipcClient.sendRequest(
            RequestNum::ERROR_INFOLIST, {}, [callback](const ExitInfo &exitInfo, const Poco::DynamicStruct &result) {
                std::vector<ErrorInfo> list;
                if (exitInfo.code() == ExitCode::Ok) {
                    CommonUtility::readValuesFromStruct(result, msgParamErrorInfoList, list, dynamicVar2Struct<ErrorInfo>);
                }
                callback(exitInfo, list);
            });
}

void ServerCommService::requestErrorDelete(const ErrorDbId errorDbId, const VoidCallback &callback) const {
    Poco::DynamicStruct params;
    params[msgParamErrorDbId] = errorDbId;
    _ipcClient.sendRequest(RequestNum::ERROR_DELETE, params,
                           [callback](const ExitInfo &exitInfo, const Poco::DynamicStruct &) { callback(exitInfo); });
}

void ServerCommService::requestErrorResolveConflicts(const std::vector<ErrorDbId> &keepLocalList,
                                                     const std::vector<ErrorDbId> &keepRemoteList,
                                                     const VoidCallback &callback) const {
    Poco::DynamicStruct params;
    params[msgParamKeepLocalErrorDbIdList] = keepLocalList;
    params[msgParamKeepRemoteErrorDbIdList] = keepRemoteList;
    _ipcClient.sendRequest(RequestNum::ERROR_RESOLVE_CONFLICTS, params,
                           [callback](const ExitInfo &exitInfo, const Poco::DynamicStruct &) { callback(exitInfo); });
}

void ServerCommService::requestErrorResolveConflictsQuick(const std::vector<ErrorDbId> &errorDbIdList,
                                                          const ConflictResolutionStrategy strategy,
                                                          const VoidCallback &callback) const {
    Poco::DynamicStruct params;
    params[msgParamErrorDbIdList] = errorDbIdList;
    params[msgParamStrategy] = toInt(strategy);
    _ipcClient.sendRequest(RequestNum::ERROR_RESOLVE_CONFLICTS_QUICK, params,
                           [callback](const ExitInfo &exitInfo, const Poco::DynamicStruct &) { callback(exitInfo); });
}

// -- Node ----------------------------------------------------------------

void ServerCommService::requestNodeInfo(const UserDbId userDbId, const DriveId driveId, const NodeId &nodeId, const bool withPath,
                                        const NodeInfoCallback &callback) const {
    Poco::DynamicStruct params;
    params[msgParamUserDbId] = userDbId;
    params[msgParamDriveId] = driveId;
    params[msgParamNodeId] = nodeId;
    params[msgParamWithPath] = withPath;
    _ipcClient.sendRequest(RequestNum::NODE_INFO, params,
                           [callback](const ExitInfo &exitInfo, const Poco::DynamicStruct &result) {
                               NodeInfo info;
                               if (exitInfo.code() == ExitCode::Ok) {
                                   info.fromDynamicStruct(result[msgParamNodeInfo].extract<Poco::DynamicStruct>());
                               }
                               callback(exitInfo, info);
                           });
}

void ServerCommService::requestNodeConflictInfo(const SyncDbId syncDbId, const SyncPath &relativePath,
                                                const ReplicaSide replicaSide, const NodeConflictInfoCallback &callback) const {
    Poco::DynamicStruct params;
    params[msgParamSyncDbId] = syncDbId;
    params[msgParamRelativePath] = CommonUtility::syncPath2CommString(relativePath);
    params[msgParamReplicaSide] = toInt(replicaSide);
    _ipcClient.sendRequest(RequestNum::NODE_CONFLICT_INFO, params,
                           [callback](const ExitInfo &exitInfo, const Poco::DynamicStruct &result) {
                               NodeConflictInfo info;
                               if (exitInfo.code() == ExitCode::Ok) {
                                   info.fromDynamicStruct(result[msgParamNodeConflictInfo].extract<Poco::DynamicStruct>());
                               }
                               callback(exitInfo, info);
                           });
}

void ServerCommService::requestNodePath(const SyncDbId syncDbId, const NodeId &nodeId, const StringCallback &callback) const {
    Poco::DynamicStruct params;
    params[msgParamSyncDbId] = syncDbId;
    params[msgParamNodeId] = nodeId;
    _ipcClient.sendRequest(RequestNum::NODE_PATH, params,
                           [callback](const ExitInfo &exitInfo, const Poco::DynamicStruct &result) {
                               QString path;
                               if (exitInfo.code() == ExitCode::Ok) {
                                   CommString pathStr;
                                   CommonUtility::readValueFromStruct(result, msgParamPath, pathStr);
                                   path = CommonUtility::commString2QStr(pathStr);
                               }
                               callback(exitInfo, path);
                           });
}

void ServerCommService::requestNodeSubfolders(const DriveDbId driveDbId, const NodeId &nodeId, const bool withPath,
                                              const NodeInfoListCallback &callback) const {
    Poco::DynamicStruct params;
    params[msgParamDriveDbId] = driveDbId;
    params[msgParamNodeId] = nodeId;
    params[msgParamWithPath] = withPath;
    _ipcClient.sendRequest(
            RequestNum::NODE_SUBFOLDERS, params, [callback](const ExitInfo &exitInfo, const Poco::DynamicStruct &result) {
                std::vector<NodeInfo> list;
                if (exitInfo.code() == ExitCode::Ok) {
                    CommonUtility::readValuesFromStruct(result, msgParamNodeSubFolderInfoList, list, dynamicVar2Struct<NodeInfo>);
                }
                callback(exitInfo, list);
            });
}

void ServerCommService::requestNodeSubfolders2(const DriveDbId driveDbId, const NodeId &nodeId, const bool withPath,
                                               const NodeInfoListCallback &callback) const {
    Poco::DynamicStruct params;
    params[msgParamDriveDbId] = driveDbId;
    params[msgParamNodeId] = nodeId;
    params[msgParamWithPath] = withPath;
    _ipcClient.sendRequest(
            RequestNum::NODE_SUBFOLDERS2, params, [callback](const ExitInfo &exitInfo, const Poco::DynamicStruct &result) {
                std::vector<NodeInfo> list;
                if (exitInfo.code() == ExitCode::Ok) {
                    CommonUtility::readValuesFromStruct(result, msgParamNodeSubFolderInfoList, list, dynamicVar2Struct<NodeInfo>);
                }
                callback(exitInfo, list);
            });
}

void ServerCommService::requestNodeFolderSize(const UserDbId userDbId, const DriveId driveId, const NodeId &nodeId,
                                              const FolderSizeCallback &callback) const {
    Poco::DynamicStruct params;
    params[msgParamUserDbId] = userDbId;
    params[msgParamDriveId] = driveId;
    params[msgParamNodeId] = nodeId;
    _ipcClient.sendRequest(RequestNum::NODE_FOLDER_SIZE, params,
                           [callback](const ExitInfo &exitInfo, const Poco::DynamicStruct &result) {
                               int64_t folderSize = 0;
                               if (exitInfo.code() == ExitCode::Ok) {
                                   CommonUtility::readValueFromStruct(result, msgParamFolderSize, folderSize);
                               }
                               callback(exitInfo, folderSize);
                           });
}

void ServerCommService::requestNodeCreateMissingFolders(const DriveDbId driveDbId, const NodeId &parentNodeId,
                                                        const SyncPath &relativePath, const NodeIdCallback &callback) const {
    Poco::DynamicStruct params;
    params[msgParamDriveDbId] = driveDbId;
    params[msgParamParentNodeId] = parentNodeId;
    params[msgParamRelativePath] = CommonUtility::syncPath2CommString(relativePath);
    _ipcClient.sendRequest(RequestNum::NODE_CREATEMISSINGFOLDERS, params,
                           [callback](const ExitInfo &exitInfo, const Poco::DynamicStruct &result) {
                               NodeId nodeId;
                               if (exitInfo.code() == ExitCode::Ok) {
                                   CommonUtility::readValueFromStruct(result, msgParamNodeId, nodeId);
                               }
                               callback(exitInfo, nodeId);
                           });
}

// -- Parameters ----------------------------------------------------------

void ServerCommService::requestParametersInfo(const ParametersInfoCallback &callback) const {
    _ipcClient.sendRequest(RequestNum::PARAMETERS_INFO, {},
                           [callback](const ExitInfo &exitInfo, const Poco::DynamicStruct &result) {
                               ParametersInfo info;
                               if (exitInfo.code() == ExitCode::Ok) {
                                   info.fromDynamicStruct(result[msgParamParametersInfo].extract<Poco::DynamicStruct>());
                               }
                               callback(exitInfo, info);
                           });
}

void ServerCommService::requestParametersUpdate(const ParametersInfo &parametersInfo, const VoidCallback &callback) const {
    Poco::DynamicStruct params;
    Poco::DynamicStruct infoStruct;
    parametersInfo.toDynamicStruct(infoStruct);
    params[msgParamParametersInfo] = infoStruct;
    _ipcClient.sendRequest(RequestNum::PARAMETERS_UPDATE, params,
                           [callback](const ExitInfo &exitInfo, const Poco::DynamicStruct &) { callback(exitInfo); });
}

// -- Exclusions ---------------------------------------------------------

void ServerCommService::requestBlacklistedNodeList(const SyncDbId syncDbId, const NodeIdListCallback &callback) const {
    Poco::DynamicStruct params;
    params[msgParamSyncDbId] = syncDbId;
    _ipcClient.sendRequest(RequestNum::BLACKLISTED_NODE_LIST, params,
                           [callback](const ExitInfo &exitInfo, const Poco::DynamicStruct &result) {
                               std::vector<NodeId> list;
                               if (exitInfo.code() == ExitCode::Ok) {
                                   CommonUtility::readValuesFromStruct(result, msgParamNodeIdList, list);
                               }
                               callback(exitInfo, list);
                           });
}

void ServerCommService::requestBlacklistedNodeSetList(const SyncDbId syncDbId, const std::vector<NodeId> &nodeIdList,
                                                      const VoidCallback &callback) const {
    Poco::DynamicStruct params;
    params[msgParamSyncDbId] = syncDbId;
    params[msgParamNodeIdList] = nodeIdList;
    _ipcClient.sendRequest(RequestNum::BLACKLISTED_NODE_SETLIST, params,
                           [callback](const ExitInfo &exitInfo, const Poco::DynamicStruct &) { callback(exitInfo); });
}

void ServerCommService::requestExclTemplGetList(const bool defaultTemplates,
                                                const ExclusionTemplateListCallback &callback) const {
    Poco::DynamicStruct params;
    params[msgParamDefault] = defaultTemplates;
    _ipcClient.sendRequest(RequestNum::EXCLTEMPL_GETLIST, params,
                           [callback](const ExitInfo &exitInfo, const Poco::DynamicStruct &result) {
                               std::vector<ExclusionTemplateInfo> list;
                               if (exitInfo.code() == ExitCode::Ok) {
                                   CommonUtility::readValuesFromStruct(result, msgParamExclusionTemplateList, list,
                                                                       dynamicVar2Struct<ExclusionTemplateInfo>);
                               }
                               callback(exitInfo, list);
                           });
}

void ServerCommService::requestExclTemplSetList(const std::vector<ExclusionTemplateInfo> &templateList,
                                                const VoidCallback &callback) const {
    Poco::DynamicStruct params;
    Poco::Dynamic::Array arr;
    for (const auto &tmpl: templateList) {
        Poco::DynamicStruct s;
        tmpl.toDynamicStruct(s);
        (void) arr.emplace_back(s);
    }
    params[msgParamExclusionTemplateList] = arr;
    _ipcClient.sendRequest(RequestNum::EXCLTEMPL_SETUSERLIST, params,
                           [callback](const ExitInfo &exitInfo, const Poco::DynamicStruct &) { callback(exitInfo); });
}

void ServerCommService::requestExclTemplGetExcluded(const QString &name, const BoolCallback &callback) const {
    Poco::DynamicStruct params;
    params[msgParamName] = CommonUtility::qStr2CommString(name);
    _ipcClient.sendRequest(RequestNum::EXCLTEMPL_GETEXCLUDED, params,
                           [callback](const ExitInfo &exitInfo, const Poco::DynamicStruct &result) {
                               bool isExcluded = false;
                               if (exitInfo.code() == ExitCode::Ok) {
                                   CommonUtility::readValueFromStruct(result, msgParamIsExcluded, isExcluded);
                               }
                               callback(exitInfo, isExcluded);
                           });
}

// -- Updater ------------------------------------------------------------

void ServerCommService::requestUpdaterState(const UpdateStateCallback &callback) const {
    _ipcClient.sendRequest(RequestNum::UPDATER_STATE, {},
                           [callback](const ExitInfo &exitInfo, const Poco::DynamicStruct &result) {
                               auto state = UpdateState::Unknown;
                               if (exitInfo.code() == ExitCode::Ok) {
                                   CommonUtility::readValueFromStruct(result, msgParamUpdateState, state);
                               }
                               callback(exitInfo, state);
                           });
}

void ServerCommService::requestUpdaterVersionInfo(const VersionChannel channel, const VersionInfoCallback &callback) const {
    Poco::DynamicStruct params;
    params[msgParamChannel] = toInt(channel);
    _ipcClient.sendRequest(RequestNum::UPDATER_VERSION_INFO, params,
                           [callback](const ExitInfo &exitInfo, const Poco::DynamicStruct &result) {
                               VersionInfo info;
                               if (exitInfo.code() == ExitCode::Ok) {
                                   info.fromDynamicStruct(result[msgParamVersionInfo].extract<Poco::DynamicStruct>());
                               }
                               callback(exitInfo, info);
                           });
}

// -- Utility ------------------------------------------------------------

void ServerCommService::requestCheckCommStatus(const VoidCallback &callback) const {
    _ipcClient.sendRequest(RequestNum::UTILITY_CHECKCOMMSTATUS, {},
                           [callback](const ExitInfo &exitInfo, const Poco::DynamicStruct &) { callback(exitInfo); });
}

void ServerCommService::requestFindGoodPathForNewSync(const SyncPath &basePath, const GoodPathCallback &callback) const {
    Poco::DynamicStruct params;
    params[msgParamBasePath] = CommonUtility::syncPath2CommString(basePath);
    _ipcClient.sendRequest(RequestNum::UTILITY_FINDGOODPATHFORNEWSYNC, params,
                           [callback](const ExitInfo &exitInfo, const Poco::DynamicStruct &result) {
                               GoodPathResult pathResult;
                               if (exitInfo.code() == ExitCode::Ok) {
                                   CommString goodPath;
                                   CommString errorMessage;
                                   CommonUtility::readValueFromStruct(result, msgParamGoodPath, goodPath);
                                   // The server always serializes both goodPath and errorMessage (even on success).
                                   // errorMessage may be non-empty as supplementary info even when ExitCode::Ok.
                                   CommonUtility::readValueFromStruct(result, msgParamErrorMessage, errorMessage);
                                   pathResult.goodPath = SyncPath(goodPath);
                                   pathResult.errorMessage = CommonUtility::commString2QStr(errorMessage);
                               }
                               callback(exitInfo, pathResult);
                           });
}

void ServerCommService::requestIsPathValidForNewSync(const SyncPath &path, const SyncConfiguration syncConfiguration,
                                                     const BoolCallback &callback) const {
    Poco::DynamicStruct params;
    params[msgParamPath] = CommonUtility::syncPath2CommString(path);
    params[msgParamSyncConfiguration] = toInt(syncConfiguration);
    _ipcClient.sendRequest(RequestNum::UTILITY_ISPATHVALIDFORNEWSYNC, params,
                           [callback](const ExitInfo &exitInfo, const Poco::DynamicStruct &result) {
                               bool isValid = false;
                               if (exitInfo.code() == ExitCode::Ok) {
                                   CommonUtility::readValueFromStruct(result, msgParamIsValid, isValid);
                               }
                               callback(exitInfo, isValid);
                           });
}

void ServerCommService::requestGetAppState(const AppStateKey key, const AppStateCallback &callback) const {
    Poco::DynamicStruct params;
    params[msgParamKey] = toInt(key);
    _ipcClient.sendRequest(RequestNum::UTILITY_GET_APPSTATE, params,
                           [callback](const ExitInfo &exitInfo, const Poco::DynamicStruct &result) {
                               QString value;
                               if (exitInfo.code() == ExitCode::Ok) {
                                   // AppState values are stored as a std::variant on the server side, so the wire
                                   // type may not be a plain string. Use Poco's type-coercing convert<> instead
                                   // of readValueFromStruct (which uses strict extraction) to handle all variants.
                                   value = QString::fromStdString(result[msgParamValue].convert<std::string>());
                               }
                               callback(exitInfo, value);
                           });
}

void ServerCommService::requestSetAppState(const AppStateKey key, const QString &value, const VoidCallback &callback) const {
    Poco::DynamicStruct params;
    params[msgParamKey] = toInt(key);
    params[msgParamValue] = CommonUtility::qStr2CommString(value);
    _ipcClient.sendRequest(RequestNum::UTILITY_SET_APPSTATE, params,
                           [callback](const ExitInfo &exitInfo, const Poco::DynamicStruct &) { callback(exitInfo); });
}

void ServerCommService::requestHasSystemLaunchOnStartup(const BoolCallback &callback) const {
    _ipcClient.sendRequest(RequestNum::UTILITY_HASSYSTEMLAUNCHONSTARTUP, {},
                           [callback](const ExitInfo &exitInfo, const Poco::DynamicStruct &result) {
                               bool enabled = false;
                               if (exitInfo.code() == ExitCode::Ok) {
                                   CommonUtility::readValueFromStruct(result, msgParamEnabled, enabled);
                               }
                               callback(exitInfo, enabled);
                           });
}

void ServerCommService::requestActivateLoadInfo(const VoidCallback &callback) const {
    _ipcClient.sendRequest(RequestNum::UTILITY_ACTIVATELOADINFO, {},
                           [callback](const ExitInfo &exitInfo, const Poco::DynamicStruct &) { callback(exitInfo); });
}

void ServerCommService::requestSendLogToSupport(const bool includeArchivedLogs, const VoidCallback &callback) const {
    Poco::DynamicStruct params;
    params[msgParamIncludeArchivedLogs] = includeArchivedLogs;
    _ipcClient.sendRequest(RequestNum::UTILITY_SEND_LOG_TO_SUPPORT, params,
                           [callback](const ExitInfo &exitInfo, const Poco::DynamicStruct &) { callback(exitInfo); });
}

void ServerCommService::requestCancelLogToSupport(const VoidCallback &callback) const {
    _ipcClient.sendRequest(RequestNum::UTILITY_CANCEL_LOG_TO_SUPPORT, {},
                           [callback](const ExitInfo &exitInfo, const Poco::DynamicStruct &) { callback(exitInfo); });
}

void ServerCommService::requestGetLogEstimatedSize(const LogSizeCallback &callback) const {
    _ipcClient.sendRequest(RequestNum::UTILITY_GET_LOG_ESTIMATED_SIZE_LEGACY, {},
                           [callback](const ExitInfo &exitInfo, const Poco::DynamicStruct &result) {
                               uint64_t logSize = 0;
                               if (exitInfo.code() == ExitCode::Ok) {
                                   CommonUtility::readValueFromStruct(result, msgParamLogSize, logSize);
                               }
                               callback(exitInfo, logSize);
                           });
}

void ServerCommService::requestSendAppStartTrace(const VoidCallback &callback) const {
    _ipcClient.sendRequest(RequestNum::UTILITY_SEND_APP_START_TRACE, {},
                           [callback](const ExitInfo &exitInfo, const Poco::DynamicStruct &) { callback(exitInfo); });
}

void ServerCommService::requestQuit(const VoidCallback &callback) const {
    _ipcClient.sendRequest(RequestNum::UTILITY_QUIT, {},
                           [callback](const ExitInfo &exitInfo, const Poco::DynamicStruct &) { callback(exitInfo); });
}

} // namespace KDC

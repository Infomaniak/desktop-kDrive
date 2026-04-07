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

#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcServerCommService, "gui.v4.servercommservice", QtInfoMsg)

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
        info.fromDynamicStruct(params[MSG_PARAM_USER_INFO].extract<Poco::DynamicStruct>());
        emit userAdded(info);
    });

    dispatcher.registerHandler(SignalNum::USER_UPDATED, [this](const Poco::DynamicStruct &params) {
        UserInfo info;
        info.fromDynamicStruct(params[MSG_PARAM_USER_INFO].extract<Poco::DynamicStruct>());
        emit userUpdated(info);
    });

    dispatcher.registerHandler(SignalNum::USER_REMOVED, [this](const Poco::DynamicStruct &params) {
        UserDbId userDbId = 0;
        CommonUtility::readValueFromStruct(params, MSG_PARAM_USER_DB_ID, userDbId);
        emit userRemoved(userDbId);
    });
}

// -- Account ------------------------------------------------------------------

void ServerCommService::registerAccountHandlers(SignalDispatcher &dispatcher) {
    dispatcher.registerHandler(SignalNum::ACCOUNT_ADDED, [this](const Poco::DynamicStruct &params) {
        AccountInfo info;
        info.fromDynamicStruct(params[MSG_PARAM_ACCOUNT_INFO].extract<Poco::DynamicStruct>());
        emit accountAdded(info);
    });

    dispatcher.registerHandler(SignalNum::ACCOUNT_UPDATED, [this](const Poco::DynamicStruct &params) {
        AccountInfo info;
        info.fromDynamicStruct(params[MSG_PARAM_ACCOUNT_INFO].extract<Poco::DynamicStruct>());
        emit accountUpdated(info);
    });

    dispatcher.registerHandler(SignalNum::ACCOUNT_REMOVED, [this](const Poco::DynamicStruct &params) {
        AccountDbId accountDbId = 0;
        CommonUtility::readValueFromStruct(params, MSG_PARAM_ACCOUNT_DB_ID, accountDbId);
        emit accountRemoved(accountDbId);
    });
}

// -- Drive ---------------------------------------------------------------------

void ServerCommService::registerDriveHandlers(SignalDispatcher &dispatcher) {
    dispatcher.registerHandler(SignalNum::DRIVE_ADDED, [this](const Poco::DynamicStruct &params) {
        DriveInfo info;
        info.fromDynamicStruct(params[MSG_PARAM_DRIVE_INFO].extract<Poco::DynamicStruct>());
        emit driveAdded(info);
    });

    dispatcher.registerHandler(SignalNum::DRIVE_UPDATED, [this](const Poco::DynamicStruct &params) {
        DriveInfo info;
        info.fromDynamicStruct(params[MSG_PARAM_DRIVE_INFO].extract<Poco::DynamicStruct>());
        emit driveUpdated(info);
    });

    dispatcher.registerHandler(SignalNum::DRIVE_REMOVED, [this](const Poco::DynamicStruct &params) {
        DriveDbId driveDbId = 0;
        CommonUtility::readValueFromStruct(params, MSG_PARAM_DRIVE_DB_ID, driveDbId);
        emit driveRemoved(driveDbId);
    });
}

// -- Sync ----------------------------------------------------------------------

void ServerCommService::registerSyncHandlers(SignalDispatcher &dispatcher) {
    dispatcher.registerHandler(SignalNum::SYNC_ADDED, [this](const Poco::DynamicStruct &params) {
        SyncInfo info;
        info.fromDynamicStruct(params[MSG_PARAM_SYNC_INFO].extract<Poco::DynamicStruct>());
        emit syncAdded(info);
    });

    dispatcher.registerHandler(SignalNum::SYNC_UPDATED, [this](const Poco::DynamicStruct &params) {
        SyncInfo info;
        info.fromDynamicStruct(params[MSG_PARAM_SYNC_INFO].extract<Poco::DynamicStruct>());
        emit syncUpdated(info);
    });

    dispatcher.registerHandler(SignalNum::SYNC_REMOVED, [this](const Poco::DynamicStruct &params) {
        SyncDbId syncDbId = 0;
        CommonUtility::readValueFromStruct(params, MSG_PARAM_SYNC_DB_ID, syncDbId);
        emit syncRemoved(syncDbId);
    });

    dispatcher.registerHandler(SignalNum::SYNC_PROGRESSINFO, [this](const Poco::DynamicStruct &params) {
        SyncDbId syncDbId = 0;
        auto status = SyncStatus::Undefined;
        auto step = SyncStep::Idle;
        CommonUtility::readValueFromStruct(params, MSG_PARAM_SYNC_DB_ID, syncDbId);
        CommonUtility::readValueFromStruct(params, MSG_PARAM_SYNC_STATUS, status);
        CommonUtility::readValueFromStruct(params, MSG_PARAM_SYNC_STEP, step);

        const Poco::DynamicStruct progressStruct = params[MSG_PARAM_SYNC_PROGRESS].extract<Poco::DynamicStruct>();
        int64_t currentFile = 0;
        int64_t totalFiles = 0;
        int64_t completedSize = 0;
        int64_t totalSize = 0;
        int64_t estimatedRemainingTime = 0;
        CommonUtility::readValueFromStruct(progressStruct, MSG_PARAM_CURRENT_FILE, currentFile);
        CommonUtility::readValueFromStruct(progressStruct, MSG_PARAM_TOTAL_FILES, totalFiles);
        CommonUtility::readValueFromStruct(progressStruct, MSG_PARAM_COMPLETED_SIZE, completedSize);
        CommonUtility::readValueFromStruct(progressStruct, MSG_PARAM_TOTAL_SIZE, totalSize);
        CommonUtility::readValueFromStruct(progressStruct, MSG_PARAM_ESTIMATED_REMAINING_TIME, estimatedRemainingTime);

        emit syncProgressInfo(syncDbId, status, step, currentFile, totalFiles, completedSize, totalSize, estimatedRemainingTime);
    });

    dispatcher.registerHandler(SignalNum::SYNC_COMPLETEDITEM, [this](const Poco::DynamicStruct &params) {
        SyncDbId syncDbId = 0;
        CommonUtility::readValueFromStruct(params, MSG_PARAM_SYNC_DB_ID, syncDbId);
        SyncFileItemInfo info;
        info.fromDynamicStruct(params[MSG_PARAM_ITEM_INFO].extract<Poco::DynamicStruct>());
        emit itemCompleted(syncDbId, info);
    });
}

// -- Error ---------------------------------------------------------------------

void ServerCommService::registerErrorHandlers(SignalDispatcher &dispatcher) {
    dispatcher.registerHandler(SignalNum::UTILITY_ERROR_ADDED, [this](const Poco::DynamicStruct &params) {
        ErrorInfo info;
        info.fromDynamicStruct(params[MSG_PARAM_ERROR_INFO].extract<Poco::DynamicStruct>());
        emit errorAdded(info);
    });

    dispatcher.registerHandler(SignalNum::UTILITY_ERROR_REMOVED, [this](const Poco::DynamicStruct &params) {
        ErrorDbId errorDbId = 0;
        CommonUtility::readValueFromStruct(params, MSG_PARAM_ERROR_DB_ID, errorDbId);
        emit errorRemoved(errorDbId);
    });
}

// -- Updater -------------------------------------------------------------------

void ServerCommService::registerUpdaterHandlers(SignalDispatcher &dispatcher) {
    dispatcher.registerHandler(SignalNum::UPDATER_STATE_CHANGED, [this](const Poco::DynamicStruct &params) {
        UpdateState state = UpdateState::Unknown;
        CommonUtility::readValueFromStruct(params, MSG_PARAM_UPDATE_STATE, state);
        emit updateStateChanged(state);
    });

    dispatcher.registerHandler(SignalNum::UPDATER_SHOW_DIALOG, [this](const Poco::DynamicStruct &params) {
        VersionInfo info;
        info.fromDynamicStruct(params[MSG_PARAM_VERSION_INFO].extract<Poco::DynamicStruct>());
        emit showUpdateDialog(info);
    });
}

// -- Utility -------------------------------------------------------------------

void ServerCommService::registerUtilityHandlers(SignalDispatcher &dispatcher) {
    dispatcher.registerHandler(SignalNum::UTILITY_SHOW_NOTIFICATION, [this](const Poco::DynamicStruct &params) {
        CommString title;
        CommString message;
        CommonUtility::readValueFromStruct(params, MSG_PARAM_TITLE, title);
        CommonUtility::readValueFromStruct(params, MSG_PARAM_MESSAGE, message);
        emit showNotification(CommonUtility::commString2QStr(title), CommonUtility::commString2QStr(message));
    });

    dispatcher.registerHandler(SignalNum::UTILITY_SHOW_SETTINGS, [this](const Poco::DynamicStruct &) { emit showSettings(); });

    dispatcher.registerHandler(SignalNum::UTILITY_SHOW_SYNTHESIS, [this](const Poco::DynamicStruct &) { emit showSynthesis(); });

    dispatcher.registerHandler(SignalNum::UTILITY_LOG_UPLOAD_STATUS_UPDATED, [this](const Poco::DynamicStruct &params) {
        LogUploadState state = LogUploadState::None;
        int32_t percentage = 0;
        CommonUtility::readValueFromStruct(params, MSG_PARAM_LOG_UPLOAD_STATE, state);
        CommonUtility::readValueFromStruct(params, MSG_PARAM_PERCENTAGE, percentage);
        emit logUploadStatusUpdated(state, percentage);
    });

    dispatcher.registerHandler(SignalNum::UTILITY_QUIT, [this](const Poco::DynamicStruct &) { emit quit(); });
}
// =============================================================================
// Request methods
// =============================================================================

// -- Login ---------------------------------------------------------------

void ServerCommService::requestLoginToken(const QString &code, const QString &codeVerifier, LoginTokenCallback callback) const {
    Poco::DynamicStruct params;
    params[MSG_PARAM_AUTH_CODE] = CommonUtility::qStr2CommString(code);
    params[MSG_PARAM_CODE_VERIFIER] = CommonUtility::qStr2CommString(codeVerifier);
    _ipcClient.sendRequest(RequestNum::LOGIN_REQUESTTOKEN, params,
                           [callback](const ExitInfo &exitInfo, const Poco::DynamicStruct &result) {
                               LoginTokenResult loginResult;
                               if (exitInfo.code() == ExitCode::Ok) {
                                   CommonUtility::readValueFromStruct(result, MSG_PARAM_USER_DB_ID, loginResult.userDbId);
                               } else {
                                   CommString error;
                                   CommString errorDescr;
                                   CommonUtility::readValueFromStruct(result, MSG_PARAM_ERROR, error);
                                   CommonUtility::readValueFromStruct(result, MSG_PARAM_ERROR_DESCR, errorDescr);
                                   loginResult.error = CommonUtility::commString2QStr(error);
                                   loginResult.errorDescription = CommonUtility::commString2QStr(errorDescr);
                               }
                               callback(exitInfo, loginResult);
                           });
}

// -- User ----------------------------------------------------------------

void ServerCommService::requestUserDbIdList(UserDbIdListCallback callback) const {
    _ipcClient.sendRequest(RequestNum::USER_DBIDLIST, {},
                           [callback](const ExitInfo &exitInfo, const Poco::DynamicStruct &result) {
                               std::vector<UserDbId> list;
                               if (exitInfo.code() == ExitCode::Ok) {
                                   CommonUtility::readValuesFromStruct(result, MSG_PARAM_USER_DB_ID_LIST, list);
                               }
                               callback(exitInfo, list);
                           });
}

void ServerCommService::requestUserInfoList(UserInfoListCallback callback) const {
    _ipcClient.sendRequest(
            RequestNum::USER_INFOLIST, {}, [callback](const ExitInfo &exitInfo, const Poco::DynamicStruct &result) {
                std::vector<UserInfo> list;
                if (exitInfo.code() == ExitCode::Ok) {
                    CommonUtility::readValuesFromStruct(result, MSG_PARAM_USER_INFO_LIST, list, dynamicVar2Struct<UserInfo>);
                }
                callback(exitInfo, list);
            });
}

void ServerCommService::requestUserAvailableDrives(const UserDbId userDbId, DriveAvailableInfoListCallback callback) const {
    Poco::DynamicStruct params;
    params[MSG_PARAM_USER_DB_ID] = userDbId;
    _ipcClient.sendRequest(RequestNum::USER_AVAILABLEDRIVES, params,
                           [callback](const ExitInfo &exitInfo, const Poco::DynamicStruct &result) {
                               std::vector<DriveAvailableInfo> list;
                               if (exitInfo.code() == ExitCode::Ok) {
                                   CommonUtility::readValuesFromStruct(result, MSG_PARAM_DRIVE_AVAILABLE_INFO_LIST, list,
                                                                       dynamicVar2Struct<DriveAvailableInfo>);
                               }
                               callback(exitInfo, list);
                           });
}

void ServerCommService::requestDeleteUser(const UserDbId userDbId, VoidCallback callback) const {
    Poco::DynamicStruct params;
    params[MSG_PARAM_USER_DB_ID] = userDbId;
    _ipcClient.sendRequest(RequestNum::USER_DELETE, params,
                           [callback](const ExitInfo &exitInfo, const Poco::DynamicStruct &) { callback(exitInfo); });
}

// -- Account -------------------------------------------------------------

void ServerCommService::requestAccountInfoList(AccountInfoListCallback callback) const {
    _ipcClient.sendRequest(RequestNum::ACCOUNT_INFOLIST, {},
                           [callback](const ExitInfo &exitInfo, const Poco::DynamicStruct &result) {
                               std::vector<AccountInfo> list;
                               if (exitInfo.code() == ExitCode::Ok) {
                                   CommonUtility::readValuesFromStruct(result, MSG_PARAM_ACCOUNT_INFO_LIST, list,
                                                                       dynamicVar2Struct<AccountInfo>);
                               }
                               callback(exitInfo, list);
                           });
}

// -- Drive ---------------------------------------------------------------

void ServerCommService::requestDriveInfoList(DriveInfoListCallback callback) const {
    _ipcClient.sendRequest(
            RequestNum::DRIVE_INFOLIST, {}, [callback](const ExitInfo &exitInfo, const Poco::DynamicStruct &result) {
                std::vector<DriveInfo> list;
                if (exitInfo.code() == ExitCode::Ok) {
                    CommonUtility::readValuesFromStruct(result, MSG_PARAM_DRIVE_INFO_LIST, list, dynamicVar2Struct<DriveInfo>);
                }
                callback(exitInfo, list);
            });
}

void ServerCommService::requestDriveUpdate(const DriveInfo &driveInfo, VoidCallback callback) const {
    Poco::DynamicStruct params;
    Poco::DynamicStruct driveStruct;
    driveInfo.toDynamicStruct(driveStruct);
    params[MSG_PARAM_DRIVE_INFO] = driveStruct;
    _ipcClient.sendRequest(RequestNum::DRIVE_UPDATE, params,
                           [callback](const ExitInfo &exitInfo, const Poco::DynamicStruct &) { callback(exitInfo); });
}

void ServerCommService::requestDriveDelete(const DriveDbId driveDbId, VoidCallback callback) const {
    Poco::DynamicStruct params;
    params[MSG_PARAM_DRIVE_DB_ID] = driveDbId;
    _ipcClient.sendRequest(RequestNum::DRIVE_DELETE, params,
                           [callback](const ExitInfo &exitInfo, const Poco::DynamicStruct &) { callback(exitInfo); });
}

// -- Sync core -----------------------------------------------------------

void ServerCommService::requestSyncInfoList(SyncInfoListCallback callback) const {
    _ipcClient.sendRequest(
            RequestNum::SYNC_INFOLIST, {}, [callback](const ExitInfo &exitInfo, const Poco::DynamicStruct &result) {
                std::vector<SyncInfo> list;
                if (exitInfo.code() == ExitCode::Ok) {
                    CommonUtility::readValuesFromStruct(result, MSG_PARAM_SYNC_INFO_LIST, list, dynamicVar2Struct<SyncInfo>);
                }
                callback(exitInfo, list);
            });
}

void ServerCommService::requestSyncAdd(const SyncAddRequest &request, SyncInfoCallback callback) const {
    Poco::DynamicStruct params;
    params[MSG_PARAM_USER_DB_ID] = request.userDbId;
    params[MSG_PARAM_ACCOUNT_ID] = request.accountId;
    params[MSG_PARAM_DRIVE_ID] = request.driveId;
    params[MSG_PARAM_LOCAL_FOLDER_PATH] = CommonUtility::syncPath2CommString(request.localFolderPath);
    params[MSG_PARAM_SERVER_FOLDER_PATH] = CommonUtility::syncPath2CommString(request.serverFolderPath);
    params[MSG_PARAM_SERVER_FOLDER_NODE_ID] = request.serverFolderNodeId;
    params[MSG_PARAM_LITE_SYNC] = request.liteSync;
    params[MSG_PARAM_BLACK_LIST] = request.blackList;
    _ipcClient.sendRequest(RequestNum::SYNC_ADD, params, [callback](const ExitInfo &exitInfo, const Poco::DynamicStruct &result) {
        SyncInfo info;
        if (exitInfo.code() == ExitCode::Ok) {
            info.fromDynamicStruct(result[MSG_PARAM_SYNC_INFO].extract<Poco::DynamicStruct>());
        }
        callback(exitInfo, info);
    });
}

void ServerCommService::requestSyncStart(const SyncDbId syncDbId, VoidCallback callback) const {
    Poco::DynamicStruct params;
    params[MSG_PARAM_SYNC_DB_ID] = syncDbId;
    _ipcClient.sendRequest(RequestNum::SYNC_START, params,
                           [callback](const ExitInfo &exitInfo, const Poco::DynamicStruct &) { callback(exitInfo); });
}

void ServerCommService::requestSyncStop(const SyncDbId syncDbId, VoidCallback callback) const {
    Poco::DynamicStruct params;
    params[MSG_PARAM_SYNC_DB_ID] = syncDbId;
    _ipcClient.sendRequest(RequestNum::SYNC_STOP, params,
                           [callback](const ExitInfo &exitInfo, const Poco::DynamicStruct &) { callback(exitInfo); });
}

void ServerCommService::requestSyncStatus(const SyncDbId syncDbId, SyncStatusCallback callback) const {
    Poco::DynamicStruct params;
    params[MSG_PARAM_SYNC_DB_ID] = syncDbId;
    _ipcClient.sendRequest(RequestNum::SYNC_STATUS, params,
                           [callback](const ExitInfo &exitInfo, const Poco::DynamicStruct &result) {
                               auto status = SyncStatus::Undefined;
                               if (exitInfo.code() == ExitCode::Ok) {
                                   CommonUtility::readValueFromStruct(result, MSG_PARAM_SYNC_STATUS, status);
                               }
                               callback(exitInfo, status);
                           });
}

void ServerCommService::requestSyncDelete(const SyncDbId syncDbId, VoidCallback callback) const {
    Poco::DynamicStruct params;
    params[MSG_PARAM_SYNC_DB_ID] = syncDbId;
    _ipcClient.sendRequest(RequestNum::SYNC_DELETE, params,
                           [callback](const ExitInfo &exitInfo, const Poco::DynamicStruct &) { callback(exitInfo); });
}

void ServerCommService::requestStartSyncsAfterLogin(const UserDbId userDbId, VoidCallback callback) const {
    Poco::DynamicStruct params;
    params[MSG_PARAM_USER_DB_ID] = userDbId;
    _ipcClient.sendRequest(RequestNum::SYNC_START_AFTER_LOGIN, params,
                           [callback](const ExitInfo &exitInfo, const Poco::DynamicStruct &) { callback(exitInfo); });
}

// -- Sync advanced -------------------------------------------------------

void ServerCommService::requestSyncTriggerProgressUpdate(const SyncDbId syncDbId, VoidCallback callback) const {
    Poco::DynamicStruct params;
    params[MSG_PARAM_SYNC_DB_ID] = syncDbId;
    _ipcClient.sendRequest(RequestNum::SYNC_TRIGGER_PROGRESS_UPDATE, params,
                           [callback](const ExitInfo &exitInfo, const Poco::DynamicStruct &) { callback(exitInfo); });
}

void ServerCommService::requestSyncGetPrivateLinkUrl(const DriveDbId driveDbId, const NodeId &nodeId,
                                                     StringCallback callback) const {
    Poco::DynamicStruct params;
    params[MSG_PARAM_DRIVE_DB_ID] = driveDbId;
    params[MSG_PARAM_NODE_ID] = nodeId;
    _ipcClient.sendRequest(RequestNum::SYNC_GETPRIVATELINKURL, params,
                           [callback](const ExitInfo &exitInfo, const Poco::DynamicStruct &result) {
                               QString url;
                               if (exitInfo.code() == ExitCode::Ok) {
                                   CommString linkUrl;
                                   CommonUtility::readValueFromStruct(result, MSG_PARAM_LINK_URL, linkUrl);
                                   url = CommonUtility::commString2QStr(linkUrl);
                               }
                               callback(exitInfo, url);
                           });
}

void ServerCommService::requestSyncGetPublicLinkUrl(const DriveDbId driveDbId, const NodeId &nodeId,
                                                    StringCallback callback) const {
    Poco::DynamicStruct params;
    params[MSG_PARAM_DRIVE_DB_ID] = driveDbId;
    params[MSG_PARAM_NODE_ID] = nodeId;
    _ipcClient.sendRequest(RequestNum::SYNC_GETPUBLICLINKURL, params,
                           [callback](const ExitInfo &exitInfo, const Poco::DynamicStruct &result) {
                               QString url;
                               if (exitInfo.code() == ExitCode::Ok) {
                                   CommString linkUrl;
                                   CommonUtility::readValueFromStruct(result, MSG_PARAM_LINK_URL, linkUrl);
                                   url = CommonUtility::commString2QStr(linkUrl);
                               }
                               callback(exitInfo, url);
                           });
}

// -- Error ---------------------------------------------------------------

void ServerCommService::requestErrorInfoList(ErrorInfoListCallback callback) const {
    _ipcClient.sendRequest(
            RequestNum::ERROR_INFOLIST, {}, [callback](const ExitInfo &exitInfo, const Poco::DynamicStruct &result) {
                std::vector<ErrorInfo> list;
                if (exitInfo.code() == ExitCode::Ok) {
                    CommonUtility::readValuesFromStruct(result, MSG_PARAM_ERROR_INFO_LIST, list, dynamicVar2Struct<ErrorInfo>);
                }
                callback(exitInfo, list);
            });
}

// -- Node ----------------------------------------------------------------

void ServerCommService::requestNodeInfo(const UserDbId userDbId, const DriveId driveId, const NodeId &nodeId, const bool withPath,
                                        NodeInfoCallback callback) const {
    Poco::DynamicStruct params;
    params[MSG_PARAM_USER_DB_ID] = userDbId;
    params[MSG_PARAM_DRIVE_ID] = driveId;
    params[MSG_PARAM_NODE_ID] = nodeId;
    params[MSG_PARAM_WITH_PATH] = withPath;
    _ipcClient.sendRequest(RequestNum::NODE_INFO, params,
                           [callback](const ExitInfo &exitInfo, const Poco::DynamicStruct &result) {
                               NodeInfo info;
                               if (exitInfo.code() == ExitCode::Ok) {
                                   info.fromDynamicStruct(result[MSG_PARAM_NODE_INFO].extract<Poco::DynamicStruct>());
                               }
                               callback(exitInfo, info);
                           });
}

void ServerCommService::requestNodePath(const SyncDbId syncDbId, const NodeId &nodeId, StringCallback callback) const {
    Poco::DynamicStruct params;
    params[MSG_PARAM_SYNC_DB_ID] = syncDbId;
    params[MSG_PARAM_NODE_ID] = nodeId;
    _ipcClient.sendRequest(RequestNum::NODE_PATH, params,
                           [callback](const ExitInfo &exitInfo, const Poco::DynamicStruct &result) {
                               QString path;
                               if (exitInfo.code() == ExitCode::Ok) {
                                   CommString pathStr;
                                   CommonUtility::readValueFromStruct(result, MSG_PARAM_PATH, pathStr);
                                   path = CommonUtility::commString2QStr(pathStr);
                               }
                               callback(exitInfo, path);
                           });
}

void ServerCommService::requestNodeSubfolders(const DriveDbId driveDbId, const NodeId &nodeId, const bool withPath,
                                              NodeInfoListCallback callback) const {
    Poco::DynamicStruct params;
    params[MSG_PARAM_DRIVE_DB_ID] = driveDbId;
    params[MSG_PARAM_NODE_ID] = nodeId;
    params[MSG_PARAM_WITH_PATH] = withPath;
    _ipcClient.sendRequest(RequestNum::NODE_SUBFOLDERS, params,
                           [callback](const ExitInfo &exitInfo, const Poco::DynamicStruct &result) {
                               std::vector<NodeInfo> list;
                               if (exitInfo.code() == ExitCode::Ok) {
                                   CommonUtility::readValuesFromStruct(result, MSG_PARAM_NODE_SUB_FOLDER_INFO_LIST, list,
                                                                       dynamicVar2Struct<NodeInfo>);
                               }
                               callback(exitInfo, list);
                           });
}

void ServerCommService::requestNodeSubfolders2(const DriveDbId driveDbId, const NodeId &nodeId, const bool withPath,
                                               NodeInfoListCallback callback) const {
    Poco::DynamicStruct params;
    params[MSG_PARAM_DRIVE_DB_ID] = driveDbId;
    params[MSG_PARAM_NODE_ID] = nodeId;
    params[MSG_PARAM_WITH_PATH] = withPath;
    _ipcClient.sendRequest(RequestNum::NODE_SUBFOLDERS2, params,
                           [callback](const ExitInfo &exitInfo, const Poco::DynamicStruct &result) {
                               std::vector<NodeInfo> list;
                               if (exitInfo.code() == ExitCode::Ok) {
                                   CommonUtility::readValuesFromStruct(result, MSG_PARAM_NODE_SUB_FOLDER_INFO_LIST, list,
                                                                       dynamicVar2Struct<NodeInfo>);
                               }
                               callback(exitInfo, list);
                           });
}

void ServerCommService::requestNodeFolderSize(const UserDbId userDbId, const DriveId driveId, const NodeId &nodeId,
                                              FolderSizeCallback callback) const {
    Poco::DynamicStruct params;
    params[MSG_PARAM_USER_DB_ID] = userDbId;
    params[MSG_PARAM_DRIVE_ID] = driveId;
    params[MSG_PARAM_NODE_ID] = nodeId;
    _ipcClient.sendRequest(RequestNum::NODE_FOLDER_SIZE, params,
                           [callback](const ExitInfo &exitInfo, const Poco::DynamicStruct &result) {
                               int64_t folderSize = 0;
                               if (exitInfo.code() == ExitCode::Ok) {
                                   CommonUtility::readValueFromStruct(result, MSG_PARAM_FOLDER_SIZE, folderSize);
                               }
                               callback(exitInfo, folderSize);
                           });
}

void ServerCommService::requestNodeCreateMissingFolders(const DriveDbId driveDbId, const NodeId &parentNodeId,
                                                        const SyncPath &relativePath, StringCallback callback) const {
    Poco::DynamicStruct params;
    params[MSG_PARAM_DRIVE_DB_ID] = driveDbId;
    params[MSG_PARAM_PARENT_NODE_ID] = parentNodeId;
    params[MSG_PARAM_RELATIVE_PATH] = CommonUtility::syncPath2CommString(relativePath);
    _ipcClient.sendRequest(RequestNum::NODE_CREATEMISSINGFOLDERS, params,
                           [callback](const ExitInfo &exitInfo, const Poco::DynamicStruct &result) {
                               QString nodeId;
                               if (exitInfo.code() == ExitCode::Ok) {
                                   NodeId id;
                                   CommonUtility::readValueFromStruct(result, MSG_PARAM_NODE_ID, id);
                                   nodeId = QString::fromStdString(id);
                               }
                               callback(exitInfo, nodeId);
                           });
}

// -- Parameters ----------------------------------------------------------

void ServerCommService::requestParametersInfo(ParametersInfoCallback callback) const {
    _ipcClient.sendRequest(RequestNum::PARAMETERS_INFO, {},
                           [callback](const ExitInfo &exitInfo, const Poco::DynamicStruct &result) {
                               ParametersInfo info;
                               if (exitInfo.code() == ExitCode::Ok) {
                                   info.fromDynamicStruct(result[MSG_PARAM_PARAMETERS_INFO].extract<Poco::DynamicStruct>());
                               }
                               callback(exitInfo, info);
                           });
}

void ServerCommService::requestParametersUpdate(const ParametersInfo &parametersInfo, VoidCallback callback) const {
    Poco::DynamicStruct params;
    Poco::DynamicStruct infoStruct;
    parametersInfo.toDynamicStruct(infoStruct);
    params[MSG_PARAM_PARAMETERS_INFO] = infoStruct;
    _ipcClient.sendRequest(RequestNum::PARAMETERS_UPDATE, params,
                           [callback](const ExitInfo &exitInfo, const Poco::DynamicStruct &) { callback(exitInfo); });
}

// -- Exclusions ---------------------------------------------------------

void ServerCommService::requestBlacklistedNodeList(const SyncDbId syncDbId, NodeIdListCallback callback) const {
    Poco::DynamicStruct params;
    params[MSG_PARAM_SYNC_DB_ID] = syncDbId;
    _ipcClient.sendRequest(RequestNum::BLACKLISTED_NODE_LIST, params,
                           [callback](const ExitInfo &exitInfo, const Poco::DynamicStruct &result) {
                               std::vector<NodeId> list;
                               if (exitInfo.code() == ExitCode::Ok) {
                                   CommonUtility::readValuesFromStruct(result, MSG_PARAM_NODE_ID_LIST, list);
                               }
                               callback(exitInfo, list);
                           });
}

void ServerCommService::requestBlacklistedNodeSetList(const SyncDbId syncDbId, const std::vector<NodeId> &nodeIdList,
                                                      VoidCallback callback) const {
    Poco::DynamicStruct params;
    params[MSG_PARAM_SYNC_DB_ID] = syncDbId;
    params[MSG_PARAM_NODE_ID_LIST] = nodeIdList;
    _ipcClient.sendRequest(RequestNum::BLACKLISTED_NODE_SETLIST, params,
                           [callback](const ExitInfo &exitInfo, const Poco::DynamicStruct &) { callback(exitInfo); });
}

void ServerCommService::requestExclTemplGetList(const bool defaultTemplates, ExclusionTemplateListCallback callback) const {
    Poco::DynamicStruct params;
    params[MSG_PARAM_DEFAULT] = defaultTemplates;
    _ipcClient.sendRequest(RequestNum::EXCLTEMPL_GETLIST, params,
                           [callback](const ExitInfo &exitInfo, const Poco::DynamicStruct &result) {
                               std::vector<ExclusionTemplateInfo> list;
                               if (exitInfo.code() == ExitCode::Ok) {
                                   CommonUtility::readValuesFromStruct(result, MSG_PARAM_EXCLUSION_TEMPLATE_LIST, list,
                                                                       dynamicVar2Struct<ExclusionTemplateInfo>);
                               }
                               callback(exitInfo, list);
                           });
}

void ServerCommService::requestExclTemplSetList(const std::vector<ExclusionTemplateInfo> &templateList,
                                                VoidCallback callback) const {
    Poco::DynamicStruct params;
    Poco::Dynamic::Array arr;
    for (const auto &tmpl: templateList) {
        Poco::DynamicStruct s;
        tmpl.toDynamicStruct(s);
        arr.push_back(s);
    }
    params[MSG_PARAM_EXCLUSION_TEMPLATE_LIST] = arr;
    _ipcClient.sendRequest(RequestNum::EXCLTEMPL_SETUSERLIST, params,
                           [callback](const ExitInfo &exitInfo, const Poco::DynamicStruct &) { callback(exitInfo); });
}

void ServerCommService::requestExclTemplGetExcluded(const QString &name, BoolCallback callback) const {
    Poco::DynamicStruct params;
    params[MSG_PARAM_NAME] = CommonUtility::qStr2CommString(name);
    _ipcClient.sendRequest(RequestNum::EXCLTEMPL_GETEXCLUDED, params,
                           [callback](const ExitInfo &exitInfo, const Poco::DynamicStruct &result) {
                               bool isExcluded = false;
                               if (exitInfo.code() == ExitCode::Ok) {
                                   CommonUtility::readValueFromStruct(result, MSG_PARAM_IS_EXCLUDED, isExcluded);
                               }
                               callback(exitInfo, isExcluded);
                           });
}

// -- Updater ------------------------------------------------------------

void ServerCommService::requestUpdaterState(UpdateStateCallback callback) const {
    _ipcClient.sendRequest(RequestNum::UPDATER_STATE, {},
                           [callback](const ExitInfo &exitInfo, const Poco::DynamicStruct &result) {
                               auto state = UpdateState::Unknown;
                               if (exitInfo.code() == ExitCode::Ok) {
                                   CommonUtility::readValueFromStruct(result, MSG_PARAM_UPDATE_STATE, state);
                               }
                               callback(exitInfo, state);
                           });
}

void ServerCommService::requestUpdaterVersionInfo(const VersionChannel channel, VersionInfoCallback callback) const {
    Poco::DynamicStruct params;
    params[MSG_PARAM_CHANNEL] = static_cast<int>(channel);
    _ipcClient.sendRequest(RequestNum::UPDATER_VERSION_INFO, params,
                           [callback](const ExitInfo &exitInfo, const Poco::DynamicStruct &result) {
                               VersionInfo info;
                               if (exitInfo.code() == ExitCode::Ok) {
                                   info.fromDynamicStruct(result[MSG_PARAM_VERSION_INFO].extract<Poco::DynamicStruct>());
                               }
                               callback(exitInfo, info);
                           });
}

void ServerCommService::requestUpdaterSkipVersion(const QString &skippedVersion, VoidCallback callback) const {
    Poco::DynamicStruct params;
    params[MSG_PARAM_SKIPPED_VERSION] = CommonUtility::qStr2CommString(skippedVersion);
    _ipcClient.sendRequest(RequestNum::UPDATER_SKIP_VERSION, params,
                           [callback](const ExitInfo &exitInfo, const Poco::DynamicStruct &) { callback(exitInfo); });
}

void ServerCommService::requestUpdaterStartInstaller(VoidCallback callback) const {
    _ipcClient.sendRequest(RequestNum::UPDATER_START_INSTALLER, {},
                           [callback](const ExitInfo &exitInfo, const Poco::DynamicStruct &) { callback(exitInfo); });
}

// -- Utility ------------------------------------------------------------

void ServerCommService::requestCheckCommStatus(VoidCallback callback) const {
    _ipcClient.sendRequest(RequestNum::UTILITY_CHECKCOMMSTATUS, {},
                           [callback](const ExitInfo &exitInfo, const Poco::DynamicStruct &) { callback(exitInfo); });
}

void ServerCommService::requestFindGoodPathForNewSync(const SyncPath &basePath, GoodPathCallback callback) const {
    Poco::DynamicStruct params;
    params[MSG_PARAM_BASE_PATH] = CommonUtility::syncPath2CommString(basePath);
    _ipcClient.sendRequest(RequestNum::UTILITY_FINDGOODPATHFORNEWSYNC, params,
                           [callback](const ExitInfo &exitInfo, const Poco::DynamicStruct &result) {
                               GoodPathResult pathResult;
                               if (exitInfo.code() == ExitCode::Ok) {
                                   CommString goodPath;
                                   CommString errorMessage;
                                   CommonUtility::readValueFromStruct(result, MSG_PARAM_GOOD_PATH, goodPath);
                                   CommonUtility::readValueFromStruct(result, MSG_PARAM_ERROR_MESSAGE, errorMessage);
                                   pathResult.goodPath = SyncPath(goodPath);
                                   pathResult.errorMessage = CommonUtility::commString2QStr(errorMessage);
                               }
                               callback(exitInfo, pathResult);
                           });
}

void ServerCommService::requestIsPathValidForNewSync(const SyncPath &path, const SyncConfiguration syncConfiguration,
                                                     BoolCallback callback) const {
    Poco::DynamicStruct params;
    params[MSG_PARAM_PATH] = CommonUtility::syncPath2CommString(path);
    params[MSG_PARAM_SYNC_CONFIGURATION] = static_cast<int>(syncConfiguration);
    _ipcClient.sendRequest(RequestNum::UTILITY_ISPATHVALIDFORNEWSYNC, params,
                           [callback](const ExitInfo &exitInfo, const Poco::DynamicStruct &result) {
                               bool isValid = false;
                               if (exitInfo.code() == ExitCode::Ok) {
                                   CommonUtility::readValueFromStruct(result, MSG_PARAM_IS_VALID, isValid);
                               }
                               callback(exitInfo, isValid);
                           });
}

void ServerCommService::requestGetAppState(const AppStateKey key, AppStateCallback callback) const {
    Poco::DynamicStruct params;
    params[MSG_PARAM_KEY] = static_cast<int>(key);
    _ipcClient.sendRequest(RequestNum::UTILITY_GET_APPSTATE, params,
                           [callback](const ExitInfo &exitInfo, const Poco::DynamicStruct &result) {
                               QString value;
                               if (exitInfo.code() == ExitCode::Ok && result.contains(MSG_PARAM_VALUE)) {
                                   value = QString::fromStdString(result[MSG_PARAM_VALUE].convert<std::string>());
                               }
                               callback(exitInfo, value);
                           });
}

void ServerCommService::requestSetAppState(const AppStateKey key, const QString &value, VoidCallback callback) const {
    Poco::DynamicStruct params;
    params[MSG_PARAM_KEY] = static_cast<int>(key);
    params[MSG_PARAM_VALUE] = CommonUtility::qStr2CommString(value);
    _ipcClient.sendRequest(RequestNum::UTILITY_SET_APPSTATE, params,
                           [callback](const ExitInfo &exitInfo, const Poco::DynamicStruct &) { callback(exitInfo); });
}

void ServerCommService::requestHasSystemLaunchOnStartup(BoolCallback callback) const {
    _ipcClient.sendRequest(RequestNum::UTILITY_HASSYSTEMLAUNCHONSTARTUP, {},
                           [callback](const ExitInfo &exitInfo, const Poco::DynamicStruct &result) {
                               bool enabled = false;
                               if (exitInfo.code() == ExitCode::Ok) {
                                   CommonUtility::readValueFromStruct(result, MSG_PARAM_ENABLED, enabled);
                               }
                               callback(exitInfo, enabled);
                           });
}

void ServerCommService::requestActivateLoadInfo(VoidCallback callback) const {
    _ipcClient.sendRequest(RequestNum::UTILITY_ACTIVATELOADINFO, {},
                           [callback](const ExitInfo &exitInfo, const Poco::DynamicStruct &) { callback(exitInfo); });
}

void ServerCommService::requestSendLogToSupport(const bool includeArchivedLogs, VoidCallback callback) const {
    Poco::DynamicStruct params;
    params[MSG_PARAM_INCLUDE_ARCHIVED_LOGS] = includeArchivedLogs;
    _ipcClient.sendRequest(RequestNum::UTILITY_SEND_LOG_TO_SUPPORT, params,
                           [callback](const ExitInfo &exitInfo, const Poco::DynamicStruct &) { callback(exitInfo); });
}

void ServerCommService::requestCancelLogToSupport(VoidCallback callback) const {
    _ipcClient.sendRequest(RequestNum::UTILITY_CANCEL_LOG_TO_SUPPORT, {},
                           [callback](const ExitInfo &exitInfo, const Poco::DynamicStruct &) { callback(exitInfo); });
}

void ServerCommService::requestGetLogEstimatedSize(LogSizeCallback callback) const {
    _ipcClient.sendRequest(RequestNum::UTILITY_GET_LOG_ESTIMATED_SIZE_LEGACY, {},
                           [callback](const ExitInfo &exitInfo, const Poco::DynamicStruct &result) {
                               uint64_t logSize = 0;
                               if (exitInfo.code() == ExitCode::Ok) {
                                   CommonUtility::readValueFromStruct(result, MSG_PARAM_LOG_SIZE, logSize);
                               }
                               callback(exitInfo, logSize);
                           });
}

void ServerCommService::requestSendAppStartTrace(VoidCallback callback) const {
    _ipcClient.sendRequest(RequestNum::UTILITY_SEND_APP_START_TRACE, {},
                           [callback](const ExitInfo &exitInfo, const Poco::DynamicStruct &) { callback(exitInfo); });
}

void ServerCommService::requestQuit(VoidCallback callback) const {
    _ipcClient.sendRequest(RequestNum::UTILITY_QUIT, {},
                           [callback](const ExitInfo &exitInfo, const Poco::DynamicStruct &) { callback(exitInfo); });
}

} // namespace KDC

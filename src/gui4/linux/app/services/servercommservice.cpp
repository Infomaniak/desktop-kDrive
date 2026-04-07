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


} // namespace KDC

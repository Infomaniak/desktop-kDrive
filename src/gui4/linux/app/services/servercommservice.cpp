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
#include <QPointer>

Q_LOGGING_CATEGORY(lcServerCommService, "gui.v4.servercommservice", QtInfoMsg)

namespace {

bool isSelfInvalid(const QPointer<KDC::ServerCommService> &self, SignalNum num) {
    if (!self) {
        qCWarning(lcServerCommService) << "Signal" << toString(num).c_str()
                                       << "received after ServerCommService destruction - ignoring";
        return true;
    }
    return false;
}

} // namespace

namespace KDC {

ServerCommService::ServerCommService(SignalDispatcher &dispatcher, QObject *parent) :
    QObject(parent) {
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
    dispatcher.registerHandler(SignalNum::USER_ADDED,
                               [self = QPointer<ServerCommService>(this)](const Poco::DynamicStruct &params) {
                                   if (isSelfInvalid(self, SignalNum::USER_ADDED)) return;
                                   UserInfo info;
                                   info.fromDynamicStruct(params[MSG_PARAM_USER_INFO].extract<Poco::DynamicStruct>());
                                   emit self->userAdded(info);
                               });

    dispatcher.registerHandler(SignalNum::USER_UPDATED,
                               [self = QPointer<ServerCommService>(this)](const Poco::DynamicStruct &params) {
                                   if (isSelfInvalid(self, SignalNum::USER_UPDATED)) return;
                                   UserInfo info;
                                   info.fromDynamicStruct(params[MSG_PARAM_USER_INFO].extract<Poco::DynamicStruct>());
                                   emit self->userUpdated(info);
                               });

    dispatcher.registerHandler(SignalNum::USER_REMOVED,
                               [self = QPointer<ServerCommService>(this)](const Poco::DynamicStruct &params) {
                                   if (isSelfInvalid(self, SignalNum::USER_REMOVED)) return;
                                   UserDbId userDbId = 0;
                                   CommonUtility::readValueFromStruct(params, MSG_PARAM_USER_DB_ID, userDbId);
                                   emit self->userRemoved(userDbId);
                               });
}

// -- Account ------------------------------------------------------------------

void ServerCommService::registerAccountHandlers(SignalDispatcher &dispatcher) {
    dispatcher.registerHandler(SignalNum::ACCOUNT_ADDED,
                               [self = QPointer<ServerCommService>(this)](const Poco::DynamicStruct &params) {
                                   if (isSelfInvalid(self, SignalNum::ACCOUNT_ADDED)) return;
                                   AccountInfo info;
                                   info.fromDynamicStruct(params[MSG_PARAM_ACCOUNT_INFO].extract<Poco::DynamicStruct>());
                                   emit self->accountAdded(info);
                               });

    dispatcher.registerHandler(SignalNum::ACCOUNT_UPDATED,
                               [self = QPointer<ServerCommService>(this)](const Poco::DynamicStruct &params) {
                                   if (isSelfInvalid(self, SignalNum::ACCOUNT_UPDATED)) return;
                                   AccountInfo info;
                                   info.fromDynamicStruct(params[MSG_PARAM_ACCOUNT_INFO].extract<Poco::DynamicStruct>());
                                   emit self->accountUpdated(info);
                               });

    dispatcher.registerHandler(SignalNum::ACCOUNT_REMOVED,
                               [self = QPointer<ServerCommService>(this)](const Poco::DynamicStruct &params) {
                                   if (isSelfInvalid(self, SignalNum::ACCOUNT_REMOVED)) return;
                                   AccountDbId accountDbId = 0;
                                   CommonUtility::readValueFromStruct(params, MSG_PARAM_ACCOUNT_DB_ID, accountDbId);
                                   emit self->accountRemoved(accountDbId);
                               });
}

// -- Drive ---------------------------------------------------------------------

void ServerCommService::registerDriveHandlers(SignalDispatcher &dispatcher) {
    dispatcher.registerHandler(SignalNum::DRIVE_ADDED,
                               [self = QPointer<ServerCommService>(this)](const Poco::DynamicStruct &params) {
                                   if (isSelfInvalid(self, SignalNum::DRIVE_ADDED)) return;
                                   DriveInfo info;
                                   info.fromDynamicStruct(params[MSG_PARAM_DRIVE_INFO].extract<Poco::DynamicStruct>());
                                   emit self->driveAdded(info);
                               });

    dispatcher.registerHandler(SignalNum::DRIVE_UPDATED,
                               [self = QPointer<ServerCommService>(this)](const Poco::DynamicStruct &params) {
                                   if (isSelfInvalid(self, SignalNum::DRIVE_UPDATED)) return;
                                   DriveInfo info;
                                   info.fromDynamicStruct(params[MSG_PARAM_DRIVE_INFO].extract<Poco::DynamicStruct>());
                                   emit self->driveUpdated(info);
                               });

    dispatcher.registerHandler(SignalNum::DRIVE_REMOVED,
                               [self = QPointer<ServerCommService>(this)](const Poco::DynamicStruct &params) {
                                   if (isSelfInvalid(self, SignalNum::DRIVE_REMOVED)) return;
                                   DriveDbId driveDbId = 0;
                                   CommonUtility::readValueFromStruct(params, MSG_PARAM_DRIVE_DB_ID, driveDbId);
                                   emit self->driveRemoved(driveDbId);
                               });
}

// -- Sync ----------------------------------------------------------------------

void ServerCommService::registerSyncHandlers(SignalDispatcher &dispatcher) {
    dispatcher.registerHandler(SignalNum::SYNC_ADDED,
                               [self = QPointer<ServerCommService>(this)](const Poco::DynamicStruct &params) {
                                   if (isSelfInvalid(self, SignalNum::SYNC_ADDED)) return;
                                   SyncInfo info;
                                   info.fromDynamicStruct(params[MSG_PARAM_SYNC_INFO].extract<Poco::DynamicStruct>());
                                   emit self->syncAdded(info);
                               });

    dispatcher.registerHandler(SignalNum::SYNC_UPDATED,
                               [self = QPointer<ServerCommService>(this)](const Poco::DynamicStruct &params) {
                                   if (isSelfInvalid(self, SignalNum::SYNC_UPDATED)) return;
                                   SyncInfo info;
                                   info.fromDynamicStruct(params[MSG_PARAM_SYNC_INFO].extract<Poco::DynamicStruct>());
                                   emit self->syncUpdated(info);
                               });

    dispatcher.registerHandler(SignalNum::SYNC_REMOVED,
                               [self = QPointer<ServerCommService>(this)](const Poco::DynamicStruct &params) {
                                   if (isSelfInvalid(self, SignalNum::SYNC_REMOVED)) return;
                                   SyncDbId syncDbId = 0;
                                   CommonUtility::readValueFromStruct(params, MSG_PARAM_SYNC_DB_ID, syncDbId);
                                   emit self->syncRemoved(syncDbId);
                               });

    dispatcher.registerHandler(
            SignalNum::SYNC_PROGRESSINFO, [self = QPointer<ServerCommService>(this)](const Poco::DynamicStruct &params) {
                if (isSelfInvalid(self, SignalNum::SYNC_PROGRESSINFO)) return;
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

                emit self->syncProgressInfo(syncDbId, status, step, currentFile, totalFiles, completedSize, totalSize,
                                            estimatedRemainingTime);
            });

    dispatcher.registerHandler(SignalNum::SYNC_COMPLETEDITEM,
                               [self = QPointer<ServerCommService>(this)](const Poco::DynamicStruct &params) {
                                   if (isSelfInvalid(self, SignalNum::SYNC_COMPLETEDITEM)) return;
                                   SyncDbId syncDbId = 0;
                                   CommonUtility::readValueFromStruct(params, MSG_PARAM_SYNC_DB_ID, syncDbId);
                                   SyncFileItemInfo info;
                                   info.fromDynamicStruct(params[MSG_PARAM_ITEM_INFO].extract<Poco::DynamicStruct>());
                                   emit self->itemCompleted(syncDbId, info);
                               });
}

// -- Error ---------------------------------------------------------------------

void ServerCommService::registerErrorHandlers(SignalDispatcher &dispatcher) {
    dispatcher.registerHandler(SignalNum::UTILITY_ERROR_ADDED,
                               [self = QPointer<ServerCommService>(this)](const Poco::DynamicStruct &params) {
                                   if (isSelfInvalid(self, SignalNum::UTILITY_ERROR_ADDED)) return;
                                   ErrorInfo info;
                                   info.fromDynamicStruct(params[MSG_PARAM_ERROR_INFO].extract<Poco::DynamicStruct>());
                                   emit self->errorAdded(info);
                               });

    dispatcher.registerHandler(SignalNum::UTILITY_ERROR_REMOVED,
                               [self = QPointer<ServerCommService>(this)](const Poco::DynamicStruct &params) {
                                   if (isSelfInvalid(self, SignalNum::UTILITY_ERROR_REMOVED)) return;
                                   ErrorDbId errorDbId = 0;
                                   CommonUtility::readValueFromStruct(params, MSG_PARAM_ERROR_DB_ID, errorDbId);
                                   emit self->errorRemoved(errorDbId);
                               });
}

// -- Updater -------------------------------------------------------------------

void ServerCommService::registerUpdaterHandlers(SignalDispatcher &dispatcher) {
    dispatcher.registerHandler(SignalNum::UPDATER_STATE_CHANGED,
                               [self = QPointer<ServerCommService>(this)](const Poco::DynamicStruct &params) {
                                   if (isSelfInvalid(self, SignalNum::UPDATER_STATE_CHANGED)) return;
                                   UpdateState state = UpdateState::Unknown;
                                   CommonUtility::readValueFromStruct(params, MSG_PARAM_UPDATE_STATE, state);
                                   emit self->updateStateChanged(state);
                               });

    dispatcher.registerHandler(SignalNum::UPDATER_SHOW_DIALOG,
                               [self = QPointer<ServerCommService>(this)](const Poco::DynamicStruct &params) {
                                   if (isSelfInvalid(self, SignalNum::UPDATER_SHOW_DIALOG)) return;
                                   VersionInfo info;
                                   info.fromDynamicStruct(params[MSG_PARAM_VERSION_INFO].extract<Poco::DynamicStruct>());
                                   emit self->showUpdateDialog(info);
                               });
}

// -- Utility -------------------------------------------------------------------

void ServerCommService::registerUtilityHandlers(SignalDispatcher &dispatcher) {
    dispatcher.registerHandler(
            SignalNum::UTILITY_SHOW_NOTIFICATION, [self = QPointer<ServerCommService>(this)](const Poco::DynamicStruct &params) {
                if (isSelfInvalid(self, SignalNum::UTILITY_SHOW_NOTIFICATION)) return;
                CommString title;
                CommString message;
                CommonUtility::readValueFromStruct(params, MSG_PARAM_TITLE, title);
                CommonUtility::readValueFromStruct(params, MSG_PARAM_MESSAGE, message);
                emit self->showNotification(CommonUtility::commString2QStr(title), CommonUtility::commString2QStr(message));
            });

    dispatcher.registerHandler(SignalNum::UTILITY_SHOW_SETTINGS,
                               [self = QPointer<ServerCommService>(this)](const Poco::DynamicStruct &) {
                                   if (isSelfInvalid(self, SignalNum::UTILITY_SHOW_SETTINGS)) return;
                                   emit self->showSettings();
                               });

    dispatcher.registerHandler(SignalNum::UTILITY_SHOW_SYNTHESIS,
                               [self = QPointer<ServerCommService>(this)](const Poco::DynamicStruct &) {
                                   if (isSelfInvalid(self, SignalNum::UTILITY_SHOW_SYNTHESIS)) return;
                                   emit self->showSynthesis();
                               });

    dispatcher.registerHandler(SignalNum::UTILITY_LOG_UPLOAD_STATUS_UPDATED,
                               [self = QPointer<ServerCommService>(this)](const Poco::DynamicStruct &params) {
                                   if (isSelfInvalid(self, SignalNum::UTILITY_LOG_UPLOAD_STATUS_UPDATED)) return;
                                   LogUploadState state = LogUploadState::None;
                                   int32_t percentage = 0;
                                   CommonUtility::readValueFromStruct(params, MSG_PARAM_LOG_UPLOAD_STATE, state);
                                   CommonUtility::readValueFromStruct(params, MSG_PARAM_PERCENTAGE, percentage);
                                   emit self->logUploadStatusUpdated(state, percentage);
                               });

    dispatcher.registerHandler(SignalNum::UTILITY_QUIT, [self = QPointer<ServerCommService>(this)](const Poco::DynamicStruct &) {
        if (isSelfInvalid(self, SignalNum::UTILITY_QUIT)) return;
        emit self->quit();
    });
}

} // namespace KDC

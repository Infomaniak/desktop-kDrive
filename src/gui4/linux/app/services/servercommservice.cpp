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

#include "libcommon/utility/utility.h"

#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcServerCommService, "gui.v4.servercommservice", QtInfoMsg)

namespace KDC {

// Param keys ─ mirrors the server-side signal job static constants.
namespace {
// User
constexpr auto kUserInfo = "userInfo";
constexpr auto kUserDbId = "userDbId";
// Account
constexpr auto kAccountInfo = "accountInfo";
constexpr auto kAccountDbId = "accountDbId";
// Drive
constexpr auto kDriveInfo = "driveInfo";
constexpr auto kDriveDbId = "driveDbId";
// Sync
constexpr auto kSyncInfo = "syncInfo";
constexpr auto kSyncDbId = "syncDbId";
constexpr auto kSyncStatus = "syncStatus";
constexpr auto kSyncStep = "syncStep";
constexpr auto kSyncProgress = "SyncProgress";
constexpr auto kCurrentFile = "currentFile";
constexpr auto kTotalFiles = "totalFiles";
constexpr auto kCompletedSize = "completedSize";
constexpr auto kTotalSize = "totalSize";
constexpr auto kEstimatedRemainingTime = "estimatedRemainingTime";
constexpr auto kItemInfo = "itemInfo";
// Error
constexpr auto kErrorInfo = "errorInfo";
constexpr auto kErrorDbId = "errorDbId";
// Updater
constexpr auto kUpdateState = "updateState";
constexpr auto kVersionInfo = "versionInfo";
// Utility
constexpr auto kTitle = "title";
constexpr auto kMessage = "message";
constexpr auto kLogUploadState = "state";
constexpr auto kPercentage = "percentage";
} // namespace

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
    dispatcher.registerHandler(SignalNum::USER_ADDED, [this](const Poco::DynamicStruct &params) {
        UserInfo info;
        info.fromDynamicStruct(params[kUserInfo].extract<Poco::DynamicStruct>());
        emit userAdded(info);
    });

    dispatcher.registerHandler(SignalNum::USER_UPDATED, [this](const Poco::DynamicStruct &params) {
        UserInfo info;
        info.fromDynamicStruct(params[kUserInfo].extract<Poco::DynamicStruct>());
        emit userUpdated(info);
    });

    dispatcher.registerHandler(SignalNum::USER_REMOVED, [this](const Poco::DynamicStruct &params) {
        UserDbId userDbId = 0;
        CommonUtility::readValueFromStruct(params, kUserDbId, userDbId);
        emit userRemoved(userDbId);
    });
}

// -- Account ------------------------------------------------------------------

void ServerCommService::registerAccountHandlers(SignalDispatcher &dispatcher) {
    dispatcher.registerHandler(SignalNum::ACCOUNT_ADDED, [this](const Poco::DynamicStruct &params) {
        AccountInfo info;
        info.fromDynamicStruct(params[kAccountInfo].extract<Poco::DynamicStruct>());
        emit accountAdded(info);
    });

    dispatcher.registerHandler(SignalNum::ACCOUNT_UPDATED, [this](const Poco::DynamicStruct &params) {
        AccountInfo info;
        info.fromDynamicStruct(params[kAccountInfo].extract<Poco::DynamicStruct>());
        emit accountUpdated(info);
    });

    dispatcher.registerHandler(SignalNum::ACCOUNT_REMOVED, [this](const Poco::DynamicStruct &params) {
        AccountDbId accountDbId = 0;
        CommonUtility::readValueFromStruct(params, kAccountDbId, accountDbId);
        emit accountRemoved(accountDbId);
    });
}

// -- Drive ---------------------------------------------------------------------

void ServerCommService::registerDriveHandlers(SignalDispatcher &dispatcher) {
    dispatcher.registerHandler(SignalNum::DRIVE_ADDED, [this](const Poco::DynamicStruct &params) {
        DriveInfo info;
        info.fromDynamicStruct(params[kDriveInfo].extract<Poco::DynamicStruct>());
        emit driveAdded(info);
    });

    dispatcher.registerHandler(SignalNum::DRIVE_UPDATED, [this](const Poco::DynamicStruct &params) {
        DriveInfo info;
        info.fromDynamicStruct(params[kDriveInfo].extract<Poco::DynamicStruct>());
        emit driveUpdated(info);
    });

    dispatcher.registerHandler(SignalNum::DRIVE_REMOVED, [this](const Poco::DynamicStruct &params) {
        DriveDbId driveDbId = 0;
        CommonUtility::readValueFromStruct(params, kDriveDbId, driveDbId);
        emit driveRemoved(driveDbId);
    });
}


} // namespace KDC

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

#include "syncaddjob.h"
#include "appserver.h"
#include "requests/serverrequests.h"
#include "signalaccountaddedjob.h"
#include "signaldriveaddedjob.h"
#include "signalsyncaddedjob.h"
#include "signalsyncremovedjob.h"
#include "server/comm/guijobmanager.h"
#include "libcommon/utility/utility.h"
#include "libcommon/comm.h"
#include "libcommonserver/log/log.h"

// Input parameters keys
static const auto inParamsUserDbId = "userDbId";
static const auto inParamsAccountId = "accountId";
static const auto inParamsDriveId = "driveId";

namespace KDC {

SyncAddJob::SyncAddJob(std::shared_ptr<CommManager> commManager, int requestId, const Poco::DynamicStruct &inParams,
                       std::shared_ptr<AbstractCommChannel> channel) :
    SyncAddAbstractJob(commManager, requestId, inParams, channel) {
    _requestNum = RequestNum::SYNC_ADD;
}

ExitInfo SyncAddJob::deserializeInputParms() {
    try {
        readParamValue(inParamsUserDbId, _userDbId);
        readParamValue(inParamsAccountId, _accountId);
        readParamValue(inParamsDriveId, _driveId);
    } catch (std::exception &e) {
        LOG_WARN(_logger, "Exception in AbstractGuiJob::readParamValue: error=" << e.what());
        return ExitCode::LogicError;
    }

    return SyncAddAbstractJob::deserializeInputParms();
}

ExitInfo SyncAddJob::process() {
    // Add sync in DB
#if defined(KD_WINDOWS)
    bool showInNavigationPane = _commManager->appServer().navigationPaneHelper()->showInExplorerNavigationPane();
#else
    bool showInNavigationPane = false;
#endif

    SyncInfo syncInfo;
    AccountInfo accountInfo;
    DriveInfo driveInfo;
    if (const auto exitCode =
                ServerRequests::addSync(_userDbId, _accountId, _driveId, _localFolderPath, _serverFolderPath, _serverFolderNodeId,
                                        _liteSync, showInNavigationPane, accountInfo, driveInfo, syncInfo);
        exitCode != ExitCode::Ok) {
        LOGW_WARN(_logger, L"Error in Requests::addSync - userDbId="
                                   << _userDbId << L" accountId=" << _accountId << L" driveId=" << _driveId
                                   << L" localFolderPath=" << Utility::formatSyncPath(_localFolderPath) << L" serverFolderPath="
                                   << Utility::formatSyncPath(_serverFolderPath) << L" serverFolderNodeId="
                                   << Utility::v2ws(_serverFolderNodeId) << L" liteSync=" << _liteSync
                                   << L" showInNavigationPane=" << showInNavigationPane);
        AppServer::addError(Error(ERR_ID, exitCode));
        return exitCode;
    }

    if (accountInfo.dbId() != 0) {
        auto signalAccountAddedJob = std::make_shared<SignalAccountAddedJob>(_commManager, accountInfo);
        _commManager->sendGuiSignal(signalAccountAddedJob);
    }

    if (driveInfo.dbId() != 0) {
        auto signalDriveAddedJob = std::make_shared<SignalDriveAddedJob>(_commManager, driveInfo);
        _commManager->sendGuiSignal(signalDriveAddedJob);
    }

    return SyncAddAbstractJob::process(syncInfo);
}

} // namespace KDC

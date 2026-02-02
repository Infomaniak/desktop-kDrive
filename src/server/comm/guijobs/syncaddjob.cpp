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
#include "libcommon/log/log.h"

// Input parameters keys
static const auto inParamsUserDbId = "userDbId";
static const auto inParamsAccountId = "accountId";
static const auto inParamsDriveId = "driveId";

namespace KDC {

SyncAddJob::SyncAddJob(std::shared_ptr<CommManager> commManager, int requestId, const Poco::DynamicStruct &inParams,
                       std::shared_ptr<AbstractCommChannel> channel) :
    AbstractSyncAddJob(commManager, requestId, inParams, channel) {
    _requestNum = RequestNum::SYNC_ADD;
}

ExitInfo SyncAddJob::deserializeInputParms() {
    try {
        readParamValue(inParamsUserDbId, _userDbId);
        readParamValue(inParamsAccountId, _accountId);
        readParamValue(inParamsDriveId, _driveId);
    } catch (const std::exception &e) {
        LOG_WARN(_logger, "Exception in SyncAddJob::readParamValue: error=" << e.what());
        return ExitCode::LogicError;
    }

    return AbstractSyncAddJob::deserializeInputParms();
}

ExitInfo SyncAddJob::process() {
    // Add sync in DB
    SyncInfo syncInfo;
    AccountInfo accountInfo;
    DriveInfo driveInfo;
    if (const auto exitInfo =
                ServerRequests::addSync(_userDbId, _accountId, _accountName, _driveId, localFolderPath(), serverFolderPath(),
                                        serverFolderNodeId(), liteSync(), accountInfo, driveInfo, syncInfo);
        !exitInfo) {
        LOGW_WARN(_logger, L"Error in Requests::addSync - userDbId="
                                   << _userDbId << L" accountId=" << _accountId << L" accountName=" << QStr2WStr(_accountName)
                                   << L" driveId=" << _driveId << L" local " << CommonUtility::formatSyncPath(localFolderPath())
                                   << L" server " << CommonUtility::formatSyncPath(serverFolderPath()) << L" serverFolderNodeId="
                                   << Utility::v2ws(serverFolderNodeId()) << L" liteSync=" << liteSync());
        addError(Error(ERR_ID, exitInfo));
        return exitInfo;
    }

    if (accountInfo.dbId() != 0) {
        auto signalAccountAddedJob = std::make_shared<SignalAccountAddedJob>(accountInfo);
        _commManager->sendGuiSignal(signalAccountAddedJob);
    }

    if (driveInfo.dbId() != 0) {
        auto signalDriveAddedJob = std::make_shared<SignalDriveAddedJob>(driveInfo);
        _commManager->sendGuiSignal(signalDriveAddedJob);
    }

    auto signalSyncAddedJob = std::make_shared<SignalSyncAddedJob>(syncInfo);
    _commManager->sendGuiSignal(signalSyncAddedJob);

    return AbstractSyncAddJob::process(syncInfo);
}

} // namespace KDC

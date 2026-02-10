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

#include "syncadd2job.h"
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
static const auto inParamsDriveDbId = "driveDbId";

namespace KDC {

SyncAdd2Job::SyncAdd2Job(std::shared_ptr<CommManager> commManager, int requestId, const Poco::DynamicStruct &inParams,
                         std::shared_ptr<AbstractCommChannel> channel) :
    AbstractSyncAddJob(commManager, requestId, inParams, channel) {
    _requestNum = RequestNum::SYNC_ADD2;
}

ExitInfo SyncAdd2Job::deserializeInputParms() {
    try {
        readParamValue(inParamsDriveDbId, _driveDbId);
    } catch (const std::exception &e) {
        LOG_WARN(_logger, "Exception in SyncAdd2Job::readParamValue: error=" << e.what());
        return ExitCode::LogicError;
    }

    return AbstractSyncAddJob::deserializeInputParms();
}

ExitInfo SyncAdd2Job::process() {
    // Add sync in DB
    SyncInfo syncInfo;
    if (const auto exitInfo = ServerRequests::addSync(_driveDbId, localFolderPath(), serverFolderPath(), serverFolderNodeId(),
                                                      liteSync(), syncInfo);
        !exitInfo) {
        LOGW_WARN(_logger, L"Error in Requests::addSync - driveDbId="
                                   << _driveDbId << L" local " << Utility::formatSyncPath(localFolderPath()) << L" server "
                                   << Utility::formatSyncPath(serverFolderPath()) << L" serverFolderNodeId="
                                   << Utility::v2ws(serverFolderNodeId()) << L" liteSync=" << liteSync());
        addError(Error(ERR_ID, exitInfo));
        return exitInfo;
    }

    auto signalSyncAddedJob = std::make_shared<SignalSyncAddedJob>(syncInfo);
    _commManager->sendGuiSignal(signalSyncAddedJob);

    return AbstractSyncAddJob::process(syncInfo);
}

} // namespace KDC

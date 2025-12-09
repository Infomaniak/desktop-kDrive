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

#include "abstractsyncaddjob.h"
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
static const auto inParamsLocalFolderPath = "localFolderPath";
static const auto inParamsServerFolderPath = "serverFolderPath";
static const auto inParamsServerFolderNodeId = "serverFolderNodeId";
static const auto inParamsLiteSync = "liteSync";
static const auto inParamsBlackList = "blackList";

// Output parameters keys
static const auto outParamsSyncInfo = "syncInfo";

namespace KDC {

AbstractSyncAddJob::AbstractSyncAddJob(std::shared_ptr<CommManager> commManager, int requestId,
                                       const Poco::DynamicStruct &inParams, std::shared_ptr<AbstractCommChannel> channel) :
    AbstractGuiJob(commManager, requestId, inParams, channel) {}

ExitInfo AbstractSyncAddJob::deserializeInputParms() {
    try {
        CommString localFolderPath;
        readParamValue(inParamsLocalFolderPath, localFolderPath);
        _localFolderPath = SyncPath(localFolderPath);

        CommString serverFolderPath;
        readParamValue(inParamsServerFolderPath, serverFolderPath);
        _serverFolderPath = SyncPath(serverFolderPath);

        readParamValue(inParamsServerFolderNodeId, _serverFolderNodeId);
        readParamValue(inParamsLiteSync, _liteSync);
        readParamValues(inParamsBlackList, _blackList);
    } catch (const std::exception &e) {
        LOG_WARN(_logger, "Exception in AbstractSyncAddJob::readParamValue: error=" << e.what());
        return ExitCode::LogicError;
    }

    return ExitCode::Ok;
}

ExitInfo AbstractSyncAddJob::serializeOutputParms() {
    writeParamValue(outParamsSyncInfo, _syncInfo, info2DynamicVar<SyncInfo>);

    return ExitCode::Ok;
}

ExitInfo AbstractSyncAddJob::process(SyncInfo &syncInfo) {
    // Check if sync is valid
    Sync sync;
    ServerRequests::syncInfoToSync(syncInfo, sync);
    if (const auto exitInfo = _commManager->appServer().checkIfSyncIsValid(sync); !exitInfo) {
        LOG_WARN(_logger, "Error in checkIfSyncIsValid for syncDbId=" << sync.dbId() << " : " << exitInfo);
        AppServer::addError(Error(sync.dbId(), ERR_ID, exitInfo));
        return exitInfo;
    }

    // Create and start Vfs
    bool startPostponed = false;
    if (const auto exitInfo = _commManager->appServer().tryCreateAndStartVfs(sync, startPostponed); !exitInfo) {
        LOG_WARN(_logger, "Error in tryCreateAndStartVfs for syncDbId=" << sync.dbId() << " : " << exitInfo);
        if (!Utility::isLiteSyncExtError(exitInfo)) {
            return exitInfo;
        }
    }

    // Create and start SyncPal
    NodeSet blackList(std::make_move_iterator(_blackList.begin()), std::make_move_iterator(_blackList.end()));
    if (const auto exitInfo = _commManager->appServer().initSyncPal(sync, blackList, !startPostponed,
                                                                    std::chrono::seconds(0), false, true);
        !exitInfo) {
        _commManager->appServer().stopSyncTask(syncInfo.dbId());

        // Delete sync from DB
        if (const ExitInfo exitInfo2 = ServerRequests::deleteSync(syncInfo.dbId()); !exitInfo2) {
            LOG_WARN(_logger, "Error in Requests::deleteSync for syncDbId=" << syncInfo.dbId() << " : " << exitInfo2);
            AppServer::addError(Error(ERR_ID, exitInfo));
        }

        return exitInfo;
    }

#if defined(KD_MACOS)
    Utility::restartFinderExtension();
#endif

    return ExitCode::Ok;
}

} // namespace KDC

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

#include "syncstartjob.h"
#include "appserver.h"
#include "requests/serverrequests.h"
#include "server/comm/guijobmanager.h"
#include "libcommon/utility/utility.h"
#include "libcommon/comm.h"
#include "libcommonserver/log/log.h"

// Input parameters keys
static const auto inParamsSyncDbId = "syncDbId";

namespace KDC {

SyncStartJob::SyncStartJob(std::shared_ptr<CommManager> commManager, int requestId, const Poco::DynamicStruct &inParams,
                           std::shared_ptr<AbstractCommChannel> channel) :
    AbstractGuiJob(commManager, requestId, inParams, channel) {
    _requestNum = RequestNum::SYNC_START;
}

ExitInfo SyncStartJob::deserializeInputParms() {
    try {
        readParamValue(inParamsSyncDbId, _syncDbId);
    } catch (const std::exception &e) {
        LOG_WARN(_logger, "Exception in SyncStartJob::readParamValue: error=" << e.what());
        return ExitCode::LogicError;
    }

    return ExitCode::Ok;
}

ExitInfo SyncStartJob::serializeOutputParms() {
    return ExitCode::Ok;
}

ExitInfo SyncStartJob::process() {
    _commManager->appServer().clearSyncCacheMap();


    Sync sync;
    bool found = false;
    if (!ParmsDb::instance()->selectSync(_syncDbId, sync, found)) {
        LOG_WARN(_logger, "Error in ParmsDb::selectSync");
        return ExitCode::DbError;
    }
    if (!found) {
        LOG_WARN(_logger, "Sync not found in sync table for syncDbId=" << _syncDbId);
        return ExitCode::DataError;
    }

    // Clear old errors for this sync
    (void) _commManager->appServer().clearErrors(sync.dbId(), false);
    (void) _commManager->appServer().clearErrors(sync.dbId(), true);

    if (const auto exitInfo = _commManager->appServer().checkIfSyncIsValid(sync); !exitInfo) {
        LOG_WARN(_logger, "Error in checkIfSyncIsValid for syncDbId=" << sync.dbId() << " : " << exitInfo);
        addError(Error(sync.dbId(), ERR_ID, exitInfo));
        return exitInfo;
    }

    ExitInfo mainExitInfo = ExitCode::Ok;
    bool startPostponed = false;
    if (const auto exitInfo = _commManager->appServer().tryCreateAndStartVfs(sync, startPostponed); !exitInfo) {
        LOG_WARN(_logger, "Error in tryCreateAndStartVfs for syncDbId=" << sync.dbId() << " : " << exitInfo);
        if (!Utility::isLiteSyncExtError(exitInfo)) {
            return exitInfo;
        }
        mainExitInfo = exitInfo;
    }

    if (const auto exitInfo =
                _commManager->appServer().initSyncPal(sync, NodeSet(), !startPostponed, std::chrono::seconds(0), true, false);
        !exitInfo) {
        LOG_WARN(_logger, "Error in initSyncPal for syncDbId=" << sync.dbId() << " : " << exitInfo);
        addError(Error(ERR_ID, exitInfo));
        mainExitInfo.merge(exitInfo, {ExitCode::SystemError});
    }
#if defined(KD_MACOS)
    Utility::restartFinderExtension();
#endif

    return mainExitInfo;
}

} // namespace KDC

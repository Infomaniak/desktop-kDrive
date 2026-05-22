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

#include "blacklistednodesetlistjob.h"
#include "useractionscopedlock.h"
#include "appserver.h"
#include "requests/serverrequests.h"
#include "server/comm/guijobmanager.h"
#include "libcommon/utility/utility.h"
#include "libcommon/comm.h"
#include "libcommonserver/log/log.h"

// Input parameters keys
static const auto inParamsSyncDbId = "syncDbId";
static const auto inParamsNodeIdList = "nodeIdList";

// User action lock timeout duration
static const int32_t userActionLockTimeoutMs = 5000;

namespace KDC {

BlacklistedNodeSetListJob::BlacklistedNodeSetListJob(std::shared_ptr<CommManager> commManager, int requestId,
                                                     const Poco::DynamicStruct &inParams,
                                                     std::shared_ptr<AbstractCommChannel> channel) :
    AbstractGuiJob(commManager, requestId, inParams, channel) {
    _requestNum = RequestNum::BLACKLISTED_NODE_SETLIST;
}

ExitInfo BlacklistedNodeSetListJob::deserializeInputParms() {
    try {
        readParamValue(inParamsSyncDbId, _syncDbId);
        readParamValues(inParamsNodeIdList, _nodeIdList);
    } catch (const std::exception &e) {
        LOG_WARN(_logger, "Exception in AbstractGuiJob::readParamValue: error=" << e.what());
        return ExitCode::LogicError;
    }

    return ExitCode::Ok;
}

ExitInfo BlacklistedNodeSetListJob::process() {
    std::shared_ptr<SyncPal> syncPal;
    if (ExitInfo exitInfo = getSyncPal(_syncDbId, syncPal); !exitInfo) {
        return exitInfo;
    }

    UserActionScopedLock lock;
    if (syncPal != nullptr && !lock.tryLock(syncPal, std::chrono::milliseconds(userActionLockTimeoutMs))) {
        LOG_WARN(_logger, "Could not acquire user action lock for syncDbId="
                                  << _syncDbId << ". Another user action is running. Aborting BlacklistedNodeSetListJob.");
        return ExitCode::OperationCanceled;
    }

    NodeSet nodeIdSet;
    nodeIdSet.insert(_nodeIdList.begin(), _nodeIdList.end());
    if (ExitInfo exitInfo = syncPal->setSyncIdSet(SyncNodeType::BlackList, nodeIdSet); !exitInfo) {
        LOG_WARN(_logger, "BlacklistedNodeSetListJob::process: setSyncIdSet failed for syncDbId=" << _syncDbId);
        addError(Error(ERR_ID, exitInfo.code(), exitInfo.cause()));
        return exitInfo;
    }

    if (ExitInfo exitInfo = syncPal->propagateSyncIdSetChange(syncPal->isRunning()); !exitInfo) {
        LOG_WARN(_logger, "BlacklistedNodeSetListJob::process: propagateSyncIdSetChangeAsync failed for syncDbId=" << _syncDbId);
        addError(Error(ERR_ID, exitInfo.code(), exitInfo.cause()));
        return exitInfo;
    }
    return ExitCode::Ok;
}
} // namespace KDC

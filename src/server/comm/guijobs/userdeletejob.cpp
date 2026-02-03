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

#include "userdeletejob.h"
#include "appserver.h"
#include "requests/serverrequests.h"
#include "signaluserremovedjob.h"
#include "server/comm/guijobmanager.h"
#include "libcommon/utility/utility.h"
#include "libcommon/comm.h"
#include "libcommon/log/log.h"

// Input parameters keys
static const auto inParamsUserDbId = "userDbId";

namespace KDC {

UserDeleteJob::UserDeleteJob(std::shared_ptr<CommManager> commManager, int requestId, const Poco::DynamicStruct &inParams,
                             std::shared_ptr<AbstractCommChannel> channel) :
    AbstractGuiJob(commManager, requestId, inParams, channel) {
    _requestNum = RequestNum::USER_DELETE;
}

ExitInfo UserDeleteJob::deserializeInputParms() {
    try {
        readParamValue(inParamsUserDbId, _userDbId);
    } catch (const std::exception &e) {
        LOG_WARN(_logger, "Exception in UserDeleteJob::readParamValue: error=" << e.what());
        return ExitCode::LogicError;
    }

    return ExitCode::Ok;
}

ExitInfo UserDeleteJob::serializeOutputParms() {
    return ExitCode::Ok;
}

ExitInfo UserDeleteJob::process() {
    // Get syncs do delete
    std::vector<int> syncDbIdList;
    const std::scoped_lock lock(_commManager->appServer().syncPalMapMutex);
    for (const auto &[syncDbId, syncPal]: _commManager->appServer().syncPalMap) {
        if (!syncPal) continue;
        if (syncPal->userDbId() == _userDbId) {
            syncDbIdList.push_back(syncDbId);
        }
    }

    // Stop syncs for this user and remove them from syncPalMap.
    _commManager->appServer().stopAllSyncsTask(syncDbIdList);

    // Delete user from DB
    const ExitInfo exitInfo = ServerRequests::deleteUser(_userDbId);
    if (exitInfo) {
        auto signalUserRemovedJob = std::make_shared<SignalUserRemovedJob>(_userDbId);
        _commManager->sendGuiSignal(signalUserRemovedJob);
    } else {
        LOG_WARN(_logger, "Error in ServerRequests::deleteUser:" << exitInfo);
        addError(Error(ERR_ID, exitInfo));
        return exitInfo;
    }

    return ExitCode::Ok;
}

} // namespace KDC

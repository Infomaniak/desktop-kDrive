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

#include "utilityactivateloadinfojob.h"
#include "appserver.h"
#include "libcommon/comm.h"
#include "libparms/db/parmsdb.h"

namespace KDC {

UtilityActivateLoadInfoJob::UtilityActivateLoadInfoJob(std::shared_ptr<CommManager> commManager, int requestId,
                                                       const Poco::DynamicStruct &inParams,
                                                       std::shared_ptr<AbstractCommChannel> channel) :
    AbstractGuiJob(commManager, requestId, inParams, channel) {
    _requestNum = RequestNum::UTILITY_ACTIVATELOADINFO;
}

ExitInfo UtilityActivateLoadInfoJob::deserializeInputParms() {
    return ExitCode::Ok;
}

ExitInfo UtilityActivateLoadInfoJob::serializeOutputParms() {
    return ExitCode::Ok;
}

ExitInfo UtilityActivateLoadInfoJob::process() {
    if (ExitInfo exitInfo = checkUpdateIfNeeded(); !exitInfo) {
        return exitInfo;
    }

    _commManager->appServer().triggerSyncProgressUpdate();
    _commManager->appServer().loadUsersInfo();
    return ExitCode::Ok;
}

ExitInfo UtilityActivateLoadInfoJob::checkUpdateIfNeeded() {
    std::vector<Error> errs;
    if (!ParmsDb::instance()->selectAllErrors(INT_MAX, errs)) {
        LOG_WARN(_logger, "Failed to get errors from database to check if the app is locked");
        return ExitCode::Ok; // Not critical, we can skip the updater state refresh in this case
    }

    bool isUpdateRequired = false;
    for (const auto &err: errs) {
        if (err.exitCode() == ExitCode::UpdateRequired) {
            isUpdateRequired = true;
            break;
        }
    }

    if (!isUpdateRequired) {
        return ExitCode::Ok;
    }

    auto updaterState = _commManager->appServer().getUpdateState();

    if (updaterState != UpdateState::Checking && updaterState != UpdateState::Available &&
        updaterState != UpdateState::Downloading && updaterState != UpdateState::Ready &&
        updaterState != UpdateState::ManualUpdateAvailable) {
        LOG_INFO(_logger,
                 "An update is required, but the app is not currently checking for updates or downloading an update. Forcing "
                 "update state refresh.");
        _commManager->appServer().refreshUpdateState();
    }

    return ExitCode::UpdateRequired;
}

} // namespace KDC

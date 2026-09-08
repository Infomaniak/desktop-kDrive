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

#include "utilityispathvalidfornewsyncjob.h"
#include "libcommon/comm.h"
#include "libcommonserver/log/log.h"
#include "requests/serverrequests.h"

// Input parameters keys
static const auto inParamsPath = "path";
static const auto inParamsSyncConfiguration = "syncConfiguration";

// Output parameters keys
static const auto outParamsIsValid = "isValid";

namespace KDC {

UtilityIsPathValidForNewSyncJob::UtilityIsPathValidForNewSyncJob(std::shared_ptr<CommManager> commManager, int requestId,
                                                                 const Poco::DynamicStruct &inParams,
                                                                 std::shared_ptr<AbstractCommChannel> channel) :
    AbstractGuiJob(commManager, requestId, inParams, channel) {
    _requestNum = RequestNum::UTILITY_ISPATHVALIDFORNEWSYNC;
}

ExitInfo UtilityIsPathValidForNewSyncJob::deserializeInputParms() {
    constexpr auto logMessage = "Exception in UtilityIsPathValidForNewSyncJob::readParamValue: error=";
    CommString pathStr;
    try {
        readParamValue(inParamsPath, pathStr);
        readParamValue(inParamsSyncConfiguration, _syncConfig);

    } catch (const Poco::Exception &pocoException) {
        LOG_WARN(_logger, logMessage << pocoException.message());
        return ExitCode::LogicError;
    }
    _path = SyncPath(pathStr);

    return ExitCode::Ok;
}

ExitInfo UtilityIsPathValidForNewSyncJob::serializeOutputParms() {
    writeParamValue(outParamsIsValid, _isValid);
    return ExitCode::Ok;
}

ExitInfo UtilityIsPathValidForNewSyncJob::process() {
    if (const auto exitInfo = ServerRequests::isPathValidForNewSync(_path, _syncConfig, _isValid); !exitInfo) {
        LOGW_WARN(_logger, L"isPathValidForNewSync failed: " << Utility::formatSyncPath(_path));
        return exitInfo;
    }
    return ExitCode::Ok;
}

} // namespace KDC

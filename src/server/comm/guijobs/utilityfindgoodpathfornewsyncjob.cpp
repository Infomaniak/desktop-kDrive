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

#include "utilityfindgoodpathfornewsyncjob.h"
#include "libcommon/comm.h"
#include "libcommonserver/log/log.h"
#include "requests/serverrequests.h"

// Input parameters keys
static const auto inParamsDriveDbId = "driveDbId";
static const auto inParamsBasePath = "basePath";

// Output parameters keys
static const auto outParamsGoodPath = "goodPath";
static const auto outParamsErrorMessage = "errorMessage";

namespace KDC {

UtilityFindGoodPathForNewSyncJob::UtilityFindGoodPathForNewSyncJob(std::shared_ptr<CommManager> commManager, int requestId,
                                                                   const Poco::DynamicStruct &inParams,
                                                                   std::shared_ptr<AbstractCommChannel> channel) :
    AbstractGuiJob(commManager, requestId, inParams, channel) {
    _requestNum = RequestNum::UTILITY_FINDGOODPATHFORNEWSYNC;
}

ExitInfo UtilityFindGoodPathForNewSyncJob::deserializeInputParms() {
    constexpr auto logMessage = "Exception in UtilityFindGoodPathForNewSyncJob::readParamValue: error=";
    std::string pathStr;
    try {
        readParamValue(inParamsDriveDbId, _driveDbId);
        readParamValue(inParamsBasePath, pathStr);
    } catch (const Poco::Exception &pocoException) {
        LOG_WARN(_logger, logMessage << pocoException.message());
        return ExitCode::LogicError;
    }
    _basePath = SyncPath(pathStr);

    return ExitCode::Ok;
}

ExitInfo UtilityFindGoodPathForNewSyncJob::serializeOutputParms() {
    writeParamValue(outParamsGoodPath, _goodPath.native());
    writeParamValue(outParamsErrorMessage, _errorMessage);
    return ExitCode::Ok;
}

ExitInfo UtilityFindGoodPathForNewSyncJob::process() {
    if (const auto exitInfo = ServerRequests::findGoodPathForNewSync(_driveDbId, _basePath, _goodPath, _errorMessage);
        !exitInfo) {
        LOGW_WARN(_logger, L"findGoodPathForNewSync failed: " << L"driveDbId=" << _driveDbId << L", basePath="
                                                              << Utility::formatSyncPath(_basePath) << L", errorMessage="
                                                              << CommonUtility::s2ws(_errorMessage));
        return exitInfo;
    }
    return ExitCode::Ok;
}

} // namespace KDC

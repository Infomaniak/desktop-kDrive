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

#include "updaterversioninfojob.h"
#include "appserver.h"

#include "libcommon/comm.h"


// Input parameters keys
static const auto inParamsChannel = "channel";
// Output parameters keys
static const auto outParamsVersionInfo = "versionInfo";

namespace KDC {

UpdaterVersionInfoJob::UpdaterVersionInfoJob(std::shared_ptr<CommManager> commManager, int requestId,
                                             const Poco::DynamicStruct &inParams, std::shared_ptr<AbstractCommChannel> channel) :
    AbstractGuiJob(commManager, requestId, inParams, channel) {
    _requestNum = RequestNum::UPDATER_VERSION_INFO;
}

ExitInfo UpdaterVersionInfoJob::deserializeInputParms() {
    constexpr auto logMessage = "Exception in UpdaterVersionInfoJob::readParamValue: error=";
    try {
        readParamValue(inParamsChannel, _channel);
    } catch (const Poco::Exception &pocoException) {
        LOG_WARN(_logger, logMessage << pocoException.message());

        return ExitCode::LogicError;
    } catch (const CommonUtility::InvalidEnumerationValue &cuException) {
        LOG_WARN(_logger, logMessage << cuException.what());

        return ExitCode::LogicError;
    }

    return ExitCode::Ok;
}

ExitInfo UpdaterVersionInfoJob::serializeOutputParms() {
    writeParamValue(outParamsVersionInfo, _versionInfo, info2DynamicVar<VersionInfo>);

    return ExitCode::Ok;
}

ExitInfo UpdaterVersionInfoJob::process() {
    _versionInfo = _commManager->appServer().getVersionInfo(_channel);

    return ExitCode::Ok;
}

} // namespace KDC

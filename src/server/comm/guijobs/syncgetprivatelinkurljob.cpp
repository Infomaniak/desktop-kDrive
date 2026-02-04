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

#include "syncgetprivatelinkurljob.h"
#include "appserver.h"
#include "requests/serverrequests.h"
#include "server/comm/guijobmanager.h"
#include "libcommon/utility/utility.h"
#include "libcommon/comm.h"
#include "libcommon/log/log.h"

// Input parameters keys
static const auto inParamsDriveDbId = "driveDbId";
static const auto inParamsFileId = "fileId";

// Output parameters keys
static const auto outParamsLinkUrl = "linkUrl";


namespace KDC {

SyncGetPrivateLinkUrlJob::SyncGetPrivateLinkUrlJob(std::shared_ptr<CommManager> commManager, int requestId,
                                                   const Poco::DynamicStruct &inParams,
                                                   std::shared_ptr<AbstractCommChannel> channel) :
    AbstractGuiJob(commManager, requestId, inParams, channel) {
    _requestNum = RequestNum::SYNC_GETPRIVATELINKURL;
}

ExitInfo SyncGetPrivateLinkUrlJob::deserializeInputParms() {
    constexpr auto logMessage = "Exception in SyncGetPrivateLinkUrlJob::readParamValue: error=";
    try {
        readParamValue(inParamsDriveDbId, _driveDbId);
        readParamValue(inParamsFileId, _fileId);
    } catch (const Poco::Exception &pocoException) {
        LOG_WARN(_logger, logMessage << pocoException.message());

        return ExitCode::LogicError;
    } catch (const CommonUtility::InvalidEnumerationValue &cuException) {
        LOG_WARN(_logger, logMessage << cuException.what());

        return ExitCode::LogicError;
    }

    return ExitCode::Ok;
}

ExitInfo SyncGetPrivateLinkUrlJob::serializeOutputParms() {
    writeParamValue(outParamsLinkUrl, _linkUrl);

    return ExitCode::Ok;
}

ExitInfo SyncGetPrivateLinkUrlJob::process() {
    if (const auto exitCode = ServerRequests::getPrivateLinkUrl(_driveDbId, CommonUtility::commString2Str(_fileId), _linkUrl);
        exitCode != ExitCode::Ok) {
        LOG_WARN(_logger, "Error in ServerRequests::getPrivateLinkUrl");
        addError(Error(ERR_ID, exitCode, ExitCause::Unknown));

        return exitCode;
    }

    return ExitCode::Ok;
}

} // namespace KDC

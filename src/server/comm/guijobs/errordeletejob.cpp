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

#include "errordeletejob.h"
#include "appserver.h"
#include "requests/serverrequests.h"
#include "libcommon/utility/utility.h"
#include "libcommon/comm.h"
#include "libcommonserver/log/log.h"
#include "signalerrorremovedjob.h"

// Input parameters keys
static const auto inParamsErrorDbId = "errorDbId";

namespace KDC {

ErrorDeleteJob::ErrorDeleteJob(std::shared_ptr<CommManager> commManager, int requestId, const Poco::DynamicStruct &inParams,
                               std::shared_ptr<AbstractCommChannel> channel) :
    AbstractGuiJob(commManager, requestId, inParams, channel) {
    _requestNum = RequestNum::ERROR_DELETE;
}

ExitInfo ErrorDeleteJob::deserializeInputParms() {
    try {
        readParamValue(inParamsErrorDbId, _errorDbId);
    } catch (const std::exception &e) {
        LOG_WARN(_logger, "Exception in DriveDeleteJob::readParamValue: error=" << e.what());
        return ExitCode::LogicError;
    }

    return ExitCode::Ok;
}

ExitInfo ErrorDeleteJob::serializeOutputParms() {
    return ExitCode::Ok;
}

ExitInfo ErrorDeleteJob::process() {
    bool found = false;

    Error error;
    if (!ParmsDb::instance()->selectError(_errorDbId, error, found)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectError");
        return ExitCode::DbError;
    }

    bool keepErrorFlag = false;
    if (ExitInfo exitInfo = ServerRequests::keepError(error, keepErrorFlag); !exitInfo) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ServerRequests::keepError: " << exitInfo);
        return exitInfo;
    }

    if (keepErrorFlag) {
        LOG_INFO(Log::instance()->getLogger(), "Keeping error with errorDbId=" << _errorDbId << " as keepError flag is set");
        return ExitCode::InvalidOperation;
    }


    if (!ParmsDb::instance()->deleteError(_errorDbId, found)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::deleteError");
        return ExitCode::DbError;
    }

    if (!found) {
        // Not really an issue as we want to make sure the error is removed, but log it just in case as it can indicate a problem
        LOG_WARN(Log::instance()->getLogger(), "Error with errorDbId=" << _errorDbId << ": not found in database");
    }

    _commManager->appServer().commManager()->sendGuiSignal(std::make_shared<SignalErrorRemovedJob>(_errorDbId));
    return ExitCode::Ok;
}

} // namespace KDC

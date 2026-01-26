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

#include "utilitysendlogtosupportjob.h"
#include "appserver.h"
#include "server/comm/guijobmanager.h"
#include "libcommon/comm.h"
#include "libcommon/log/log.h"

// Input parameters keys
static const auto inParamsIncludeArchivedLogs = "includeArchivedLogs";

namespace KDC {

UtilitySendLogToSupportJob::UtilitySendLogToSupportJob(std::shared_ptr<CommManager> commManager, int requestId,
                                                       const Poco::DynamicStruct &inParams,
                                                       std::shared_ptr<AbstractCommChannel> channel) :
    AbstractGuiJob(commManager, requestId, inParams, channel) {
    _requestNum = RequestNum::UTILITY_SEND_LOG_TO_SUPPORT;
}

ExitInfo UtilitySendLogToSupportJob::deserializeInputParms() {
    constexpr auto logMessage = "Exception in UtilitySendLogToSupportJob::readParamValue: error=";
    try {
        readParamValue(inParamsIncludeArchivedLogs, _includeArchivedLogs);
    } catch (const Poco::Exception &pocoException) {
        LOG_WARN(_logger, logMessage << pocoException.message());

        return ExitCode::LogicError;
    } catch (const CommonUtility::InvalidEnumerationValue &cuException) {
        LOG_WARN(_logger, logMessage << cuException.what());

        return ExitCode::LogicError;
    }

    return ExitCode::Ok;
}

ExitInfo UtilitySendLogToSupportJob::serializeOutputParms() {
    return ExitCode::Ok;
}

ExitInfo UtilitySendLogToSupportJob::process() {
    AppServer::uploadLog(_includeArchivedLogs);

    return ExitCode::Ok;
}

} // namespace KDC

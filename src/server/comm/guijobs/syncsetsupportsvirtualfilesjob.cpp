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

#include "syncsetsupportsvirtualfilesjob.h"
#include "appserver.h"

#include "libcommon/utility/utility.h"
#include "libcommon/comm.h"
#include "libcommonserver/log/log.h"

// Input parameters keys
static const auto inParamsSyncDbId = "syncDbId";
static const auto inParamsValue = "value";

namespace KDC {

SyncSetSupportsVirtualFilesJob::SyncSetSupportsVirtualFilesJob(std::shared_ptr<CommManager> commManager, int requestId,
                                                               const Poco::DynamicStruct &inParams,
                                                               std::shared_ptr<AbstractCommChannel> channel) :
    AbstractGuiJob(commManager, requestId, inParams, channel) {
    _requestNum = RequestNum::SYNC_SETSUPPORTSVIRTUALFILES;
}

ExitInfo SyncSetSupportsVirtualFilesJob::deserializeInputParms() {
    constexpr auto logMessage = "Exception in SyncSetSupportsVirtualFilesJob::readParamValue: error=";
    try {
        readParamValue(inParamsSyncDbId, _syncDbId);
        readParamValue(inParamsValue, _value);
    } catch (const Poco::Exception &pocoException) {
        LOG_WARN(_logger, logMessage << pocoException.message());
    } catch (const CommonUtility::InvalidEnumerationValue &cuException) {
        LOG_WARN(_logger, logMessage << cuException.what());
        return ExitCode::LogicError;
    }

    return ExitCode::Ok;
}

ExitInfo SyncSetSupportsVirtualFilesJob::process() {
    if (const auto exitInfo = _commManager->appServer().setSupportsVirtualFiles(_syncDbId, _value); !exitInfo) {
        LOG_WARN(_logger, "Error in setSupportsVirtualFiles for syncDbId=" << _syncDbId);

        return exitInfo;
    }

    return ExitCode::Ok;
}

} // namespace KDC

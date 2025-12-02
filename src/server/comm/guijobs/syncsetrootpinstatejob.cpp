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

#include "syncsetrootpinstatejob.h"
#include "appserver.h"
#include "requests/serverrequests.h"
#include "server/comm/guijobmanager.h"
#include "libcommon/utility/utility.h"
#include "libcommon/comm.h"
#include "libcommonserver/log/log.h"

// Input parameters keys
static const auto inParamsSyncDbId = "syncDbId";
static const auto inParamsPinState = "state";

namespace KDC {

SyncSetRootPinStateJob::SyncSetRootPinStateJob(std::shared_ptr<CommManager> commManager, int requestId,
                                               const Poco::DynamicStruct &inParams,
                                               std::shared_ptr<AbstractCommChannel> channel) :
    AbstractGuiJob(commManager, requestId, inParams, channel) {
    _requestNum = RequestNum::SYNC_SETROOTPINSTATE;
}

ExitInfo SyncSetRootPinStateJob::deserializeInputParms() noexcept {
    constexpr auto logMessage = "Exception in SyncSetRootPinStateJob::readParamValue: error=";
    try {
        readParamValue(inParamsSyncDbId, _syncDbId);
        readParamValue(inParamsPinState, _state);
    } catch (const Poco::Exception &pocoException) {
        LOG_WARN(_logger, logMessage << pocoException.message());

        return ExitCode::LogicError;
    } catch (const CommonUtility::InvalidEnumerationValue &cuException) {
        LOG_WARN(_logger, logMessage << cuException.what());

        return ExitCode::LogicError;
    }

    return ExitCode::Ok;
}

ExitInfo SyncSetRootPinStateJob::process() {
    std::shared_ptr<Vfs> vfs;
    const std::scoped_lock lock(AppServer::vfsMapMutex);
    if (const ExitInfo exitInfo = AppServer::getVfs(_syncDbId, vfs); !exitInfo) {
        LOG_WARN(_logger, "Error in getVfs for syncDbId=" << _syncDbId << " : " << exitInfo);
        return exitInfo;
    }

    if (const ExitInfo exitInfo = vfs->setPinState("", _state); !exitInfo) {
        LOG_WARN(_logger, "Error in vfsSetPinState for syncDbId=" << _syncDbId << " : " << exitInfo);
        return exitInfo;
    }

    return ExitCode::Ok;
}

} // namespace KDC

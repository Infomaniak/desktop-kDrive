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

#include "exclappsetlistjob.h"
#include "appserver.h"
#include "requests/serverrequests.h"
#include "libcommon/info/userinfo.h"
#include "libcommon/utility/utility.h"
#include "libcommon/comm.h"
#include "libcommonserver/log/log.h"

// Input parameters keys
static const auto inParamsDefault = "default";
static const auto inParamsApplicationList = "applicationList";


namespace KDC {

ExclAppSetListJob::ExclAppSetListJob(std::shared_ptr<CommManager> commManager, int requestId, const Poco::DynamicStruct &inParams,
                                     std::shared_ptr<AbstractCommChannel> channel) :
    AbstractGuiJob(commManager, requestId, inParams, channel) {
    _requestNum = RequestNum::EXCLAPP_SETLIST;
}

ExitInfo ExclAppSetListJob::deserializeInputParms() {
    try {
        readParamValue(inParamsDefault, _default);
        readParamValues(inParamsApplicationList, _applicationList, dynamicVar2Struct<ExclusionAppInfo>);
    } catch (const std::exception &e) {
        LOG_WARN(_logger, "Exception in ExclAppSetListJob::readParamValue: error=" << e.what());
        return ExitCode::LogicError;
    }

    return ExitCode::Ok;
}

ExitInfo ExclAppSetListJob::process() {
    if (const auto exitCode = ServerRequests::setExclusionAppList(_default, _applicationList); exitCode != ExitCode::Ok) {
        LOG_WARN(_logger, "Error in Requests::setExclusionAppList: code=" << exitCode);
        AppServer::addError(Error(ERR_ID, exitCode, ExitCause::Unknown));

        return exitCode;
    }

    const std::scoped_lock lock(AppServer::vfsMapMutex);
    const auto it = std::find_if(AppServer::vfsMap.cbegin(), AppServer::vfsMap.cend(),
                                 [](const auto &pair) { return pair.second->mode() == VirtualFileMode::Mac; });
    if (it != AppServer::vfsMap.cend() && !it->second->setAppExcludeList()) {
        LOG_WARN(_logger, "Error in Vfs::setAppExcludeList!");
        AppServer::addError(Error(ERR_ID, ExitCode::SystemError));

        return ExitCode::SystemError;
    }

    return ExitCode::Ok;
}

} // namespace KDC

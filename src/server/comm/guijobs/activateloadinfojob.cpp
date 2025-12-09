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

#include "activateloadinfojob.h"
#include "appserver.h"
#include "requests/serverrequests.h"
#include "libcommon/utility/utility.h"
#include "libcommon/comm.h"
#include "libcommonserver/log/log.h"

namespace KDC {

ActivateLoadInfoJob::ActivateLoadInfoJob(std::shared_ptr<CommManager> commManager, int requestId,
                                         const Poco::DynamicStruct &inParams, std::shared_ptr<AbstractCommChannel> channel) :
    AbstractGuiJob(commManager, requestId, inParams, channel) {
    _requestNum = RequestNum::UTILITY_ACTIVATELOADINFO;
}

ExitInfo ActivateLoadInfoJob::deserializeInputParms() {
    return ExitCode::Ok;
}

ExitInfo ActivateLoadInfoJob::serializeOutputParms() {
    return ExitCode::Ok;
}

ExitInfo ActivateLoadInfoJob::process() {
    // Clear sync cache and reload user information
    _commManager->appServer().clearSyncCacheMap();
    _commManager->appServer().loadUsersInfo();
    return ExitCode::Ok;
}

} // namespace KDC

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

#include "unknownrequestjob.h"
#include "requests/serverrequests.h"
#include "signaluseraddedjob.h"
#include "signaluserupdatedjob.h"
#include "libcommon/info/userinfo.h"
#include "libcommon/utility/utility.h"
#include "libcommon/comm.h"
#include "libcommonserver/log/log.h"

namespace KDC {

UnknownRequestJob::UnknownRequestJob(std::shared_ptr<CommManager> commManager, int requestId, const Poco::DynamicStruct &inParams,
                                     const std::shared_ptr<AbstractCommChannel> channel) :
    AbstractGuiJob(commManager, requestId, inParams, channel) {
    _requestNum = RequestNum::Unknown;
}

ExitInfo UnknownRequestJob::deserializeInputParms() {
    return ExitCode::Ok;
}

ExitInfo UnknownRequestJob::serializeOutputParms() {
    return ExitCode::Ok;
}

ExitInfo UnknownRequestJob::process() {
    return ExitCode::LogicError;
}

} // namespace KDC

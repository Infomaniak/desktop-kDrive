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

#include "utilitycancellogtosupportjob.h"
#include "appserver.h"
#include "requests/serverrequests.h"
#include "signaluseraddedjob.h"
#include "signaluserupdatedjob.h"
#include "server/comm/guijobmanager.h"
#include "jobs/network/kDrive_API/upload/loguploadjob.h"
#include "libcommon/info/userinfo.h"
#include "libcommon/utility/utility.h"
#include "libcommon/comm.h"
#include "libcommonserver/log/log.h"

namespace KDC {

UtilityCancelLogToSupportJob::UtilityCancelLogToSupportJob(std::shared_ptr<CommManager> commManager, int requestId,
                                                           const Poco::DynamicStruct &inParams,
                                                           std::shared_ptr<AbstractCommChannel> channel) :
    AbstractGuiJob(commManager, requestId, inParams, channel) {
    _requestNum = RequestNum::UTILITY_CANCEL_LOG_TO_SUPPORT;
}

ExitInfo UtilityCancelLogToSupportJob::deserializeInputParms() {
    return ExitCode::Ok;
}

ExitInfo UtilityCancelLogToSupportJob::serializeOutputParms() {
    return ExitCode::Ok;
}

ExitInfo UtilityCancelLogToSupportJob::process() {
    LogUploadJob::cancelUpload();

    return ExitCode::Ok;
}

} // namespace KDC

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

#include "accountinfolistjob.h"
#include "requests/serverrequests.h"
#include "libcommon/utility/utility.h"
#include "libcommon/comm.h"
#include "libcommonserver/log/log.h"

// Output parameters keys
static const auto outParamsAccountInfoList = "accountInfoList";

namespace KDC {

AccountInfoListJob::AccountInfoListJob(std::shared_ptr<CommManager> commManager, int requestId,
                                       const Poco::DynamicStruct &inParams, std::shared_ptr<AbstractCommChannel> channel) :
    AbstractGuiJob(commManager, requestId, inParams, channel) {
    _requestNum = RequestNum::ACCOUNT_INFOLIST;
}

ExitInfo AccountInfoListJob::deserializeInputParms() {
    return ExitCode::Ok;
}

ExitInfo AccountInfoListJob::serializeOutputParms() {
    writeParamValues(outParamsAccountInfoList, _accountInfoList, info2DynamicVar<AccountInfo>);

    return ExitCode::Ok;
}

ExitInfo AccountInfoListJob::process() {
    ExitCode exitCode = ServerRequests::getAccountInfoList(_accountInfoList);
    if (exitCode != ExitCode::Ok) {
        LOG_WARN(_logger, "Error in ServerRequests::getAccountInfoList: code=" << exitCode);
    }

    return exitCode;
}

} // namespace KDC

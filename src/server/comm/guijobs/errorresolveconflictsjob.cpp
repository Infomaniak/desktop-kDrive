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

#include "errorresolveconflictsjob.h"
#include "libcommon/comm.h"
#include "libcommonserver/log/log.h"

#include <unordered_set>

// Input parameters keys
static const auto inParamsKeepLocalErrorDbIdList = "keepLocalErrorDbIdList";
static const auto inParamsKeepRemoteErrorDbIdList = "keepRemoteErrorDbIdList";

namespace KDC {

ErrorResolveConflictsJob::ErrorResolveConflictsJob(std::shared_ptr<CommManager> commManager, int32_t requestId,
                                                   const Poco::DynamicStruct &inParams,
                                                   std::shared_ptr<AbstractCommChannel> channel) :
    AbstractErrorResolveConflictsJob(commManager, requestId, inParams, channel) {
    _requestNum = RequestNum::ERROR_RESOLVE_CONFLICTS;
}

ExitInfo ErrorResolveConflictsJob::deserializeInputParms() {
    try {
        readParamValues(inParamsKeepLocalErrorDbIdList, _keepLocalErrorDbIdList);
        readParamValues(inParamsKeepRemoteErrorDbIdList, _keepRemoteErrorDbIdList);
    } catch (const std::exception &e) {
        LOG_WARN(_logger, "Exception in ErrorResolveConflictsJob::readParamValue: error=" << e.what());
        return ExitCode::LogicError;
    }

    return ExitCode::Ok;
}

ExitInfo ErrorResolveConflictsJob::process() {
    std::vector<Error> errorList;
    if (ExitInfo exitInfo = fetchAllErrors(errorList); !exitInfo) {
        return exitInfo;
    }

    // Dispatch errors into keepLocal / keepRemote lists based on the provided dbId lists
    const std::unordered_set<int64_t> keepLocalDbIdSet(_keepLocalErrorDbIdList.begin(), _keepLocalErrorDbIdList.end());
    const std::unordered_set<int64_t> keepRemoteDbIdSet(_keepRemoteErrorDbIdList.begin(), _keepRemoteErrorDbIdList.end());
    std::vector<Error> keepLocalErrors;
    std::vector<Error> keepRemoteErrors;
    for (const auto &error: errorList) {
        if (keepLocalDbIdSet.contains(error.dbId())) {
            keepLocalErrors.push_back(error);
            continue;
        }

        if (keepRemoteDbIdSet.contains(error.dbId())) {
            keepRemoteErrors.push_back(error);
            continue;
        }
    }

    if (keepLocalErrors.empty() && keepRemoteErrors.empty()) {
        LOG_INFO(Log::instance()->getLogger(), "ErrorResolveConflictsJob::process: No errors found for the provided dbId lists");
        return ExitCode::Ok;
    }

    int32_t syncDbId = 0;
    if (ExitInfo exitInfo = getSyncDbIdFromErrors(keepLocalErrors, keepRemoteErrors, syncDbId); !exitInfo) {
        return exitInfo;
    }

    std::shared_ptr<SyncPal> syncPal;
    if (ExitInfo exitInfo = getSyncPal(syncDbId, syncPal); !exitInfo) {
        return exitInfo;
    }

    return fixConflictsAndNotify(syncPal, keepLocalErrors, keepRemoteErrors);
}

} // namespace KDC

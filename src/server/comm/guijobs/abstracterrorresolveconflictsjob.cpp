#include "abstracterrorresolveconflictsjob.h"
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

#include "abstracterrorresolveconflictsjob.h"
#include "signalerrorremovedjob.h"
#include "appserver.h"
#include "libcommon/comm.h"
#include "libcommonserver/log/log.h"
#include "libparms/db/parmsdb.h"

namespace KDC {

AbstractErrorResolveConflictsJob::AbstractErrorResolveConflictsJob(std::shared_ptr<CommManager> commManager, int requestId,
                                                                   const Poco::DynamicStruct &inParams,
                                                                   std::shared_ptr<AbstractCommChannel> channel) :
    AbstractGuiJob(commManager, requestId, inParams, channel) {}

ExitInfo AbstractErrorResolveConflictsJob::fetchAllErrors(std::vector<Error> &errorList) {
    if (!ParmsDb::instance()->selectAllErrors(ErrorLevel::Server, 0, INT_MAX, errorList)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectAllErrors");
        return ExitCode::DbError;
    }
    return ExitCode::Ok;
}

ExitInfo AbstractErrorResolveConflictsJob::getSyncDbIdFromErrors(const std::vector<Error> &list1, int64_t &syncDbId) {
    std::vector<Error> empty;
    return getSyncDbIdFromErrors(list1, empty, syncDbId);
}

ExitInfo AbstractErrorResolveConflictsJob::getSyncDbIdFromErrors(const std::vector<Error> &list1, const std::vector<Error> &list2,
                                                                 int64_t &syncDbId) {

    std::set<int64_t> syncDbIdSet;
    for (const auto &error: list1) {
        syncDbIdSet.insert(error.syncDbId());
    }
    for (const auto &error: list2) {
        syncDbIdSet.insert(error.syncDbId());
    }

    if (syncDbIdSet.empty() || syncDbIdSet.size() > 1) {
        LOG_WARN(Log::instance()->getLogger(),
                 "AbstractErrorResolveConflictsJob::getSyncDbIdFromErrors: Expected exactly one syncDbId, found "
                         << syncDbIdSet.size());
        return ExitCode::LogicError;
    }

    syncDbId = *syncDbIdSet.begin();
    return ExitCode::Ok;
}

ExitInfo AbstractErrorResolveConflictsJob::getSyncPal(const int64_t syncDbId, std::shared_ptr<SyncPal> &syncPal) {
    std::scoped_lock lock(_commManager->appServer().syncPalMapMutex);
    auto syncPalMapIt = _commManager->appServer().syncPalMap.find(syncDbId);
    if (syncPalMapIt == _commManager->appServer().syncPalMap.end()) {
        LOG_WARN(_logger, "SyncPal not found in syncPalMap for syncDbId=" << syncDbId);
        return ExitCode::DataError;
    }
    if (!syncPalMapIt->second) {
        LOG_WARN(_logger, "SyncPal not set in syncPalMap for syncDbId=" << syncDbId);
        return ExitCode::DataError;
    }
    syncPal = syncPalMapIt->second;
    return ExitCode::Ok;
}

ExitInfo AbstractErrorResolveConflictsJob::fixConflictsAndNotify(const std::shared_ptr<SyncPal> &syncPal,
                                                                 const std::vector<Error> &keepLocalErrors,
                                                                 const std::vector<Error> &keepRemoteErrors) {
    std::vector<int64_t> removedErrorsDbIds;
    if (ExitInfo exitInfo = syncPal->fixConflictingFiles(keepLocalErrors, keepRemoteErrors, removedErrorsDbIds); !exitInfo) {
        LOG_WARN(_logger, "Error in SyncPal::fixConflictingFiles: " << exitInfo);
        return exitInfo;
    }
    const size_t totalExpected = keepLocalErrors.size() + keepRemoteErrors.size();
    if (removedErrorsDbIds.size() != totalExpected) {
        LOG_INFO(_logger,
                 "The number of removed error dbIds returned by SyncPal::fixConflictingFiles "
                 "differs from the number of provided errors. This is normal if some conflicts have been manually resolved "
                 "(removedErrorsDbIds="
                         << removedErrorsDbIds.size() << ", expected=" << totalExpected << ")");
    }

    for (const auto &dbId: removedErrorsDbIds) {
        _commManager->sendGuiSignal(std::make_shared<SignalErrorRemovedJob>(dbId));
    }

    return ExitCode::Ok;
}

} // namespace KDC

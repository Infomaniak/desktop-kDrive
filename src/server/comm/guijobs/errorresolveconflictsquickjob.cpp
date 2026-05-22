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

#include "errorresolveconflictsquickjob.h"
#include "libcommon/comm.h"
#include "libcommonserver/log/log.h"
#include "libcommonserver/io/iohelper.h"
#include "libcommonserver/io/filestat.h"

#include <unordered_set>

// Input parameters keys
static const auto inParamsErrorDbIdList = "errorDbIdList";
static const auto inParamsStrategy = "strategy";

// User action lock timeout duration
static const int userActionLockTimeoutMs = 5000;

namespace KDC {

ErrorResolveConflictsQuickJob::ErrorResolveConflictsQuickJob(std::shared_ptr<CommManager> commManager, int32_t requestId,
                                                             const Poco::DynamicStruct &inParams,
                                                             std::shared_ptr<AbstractCommChannel> channel) :
    AbstractErrorResolveConflictsJob(commManager, requestId, inParams, channel) {
    _requestNum = RequestNum::ERROR_RESOLVE_CONFLICTS_QUICK;
}

ExitInfo ErrorResolveConflictsQuickJob::deserializeInputParms() {
    try {
        readParamValues(inParamsErrorDbIdList, _errorDbIdList);
        readParamValue(inParamsStrategy, _strategy);
    } catch (const std::exception &e) {
        LOG_WARN(_logger, "Exception in ErrorResolveConflictsQuickJob::readParamValue: error=" << e.what());
        return ExitCode::LogicError;
    }

    return ExitCode::Ok;
}

ExitInfo ErrorResolveConflictsQuickJob::process() {
    std::vector<Error> errorList;
    if (ExitInfo exitInfo = fetchAllErrors(errorList); !exitInfo) {
        return exitInfo;
    }

    // Filter errors matching the provided dbIds
    const std::unordered_set<int64_t> errorDbIdSet(_errorDbIdList.begin(), _errorDbIdList.end());
    std::vector<Error> matchedErrors;
    for (const auto &error: errorList) {
        if (errorDbIdSet.contains(error.dbId())) {
            matchedErrors.push_back(error);
        }
    }

    if (matchedErrors.empty()) {
        LOG_INFO(Log::instance()->getLogger(),
                 "ErrorResolveConflictsQuickJob::process: No errors found for the provided dbId list");
        return ExitCode::Ok;
    }

    // Validate that all errors share the same syncDbId
    SyncDbId syncDbId = 0;
    if (ExitInfo exitInfo = getSyncDbIdFromErrors(matchedErrors, syncDbId); !exitInfo) {
        return exitInfo;
    }

    // Dispatch errors into keepLocal / keepRemote lists based on strategy
    std::vector<Error> keepLocalErrors;
    std::vector<Error> keepRemoteErrors;

    // Pre-fetch localPath for KeepMostRecent strategy
    SyncPath localPath;
    if (_strategy == ConflictResolutionStrategy::KeepMostRecent) {
        std::shared_ptr<SyncPal> syncPal;
        if (ExitInfo exitInfo = getSyncPal(syncDbId, syncPal); !exitInfo) {
            return exitInfo;
        }
        localPath = syncPal->localPath();
    }

    for (const auto &error: matchedErrors) {
        switch (_strategy) {
            case ConflictResolutionStrategy::KeepLocal:
                keepLocalErrors.push_back(error);
                break;
            case ConflictResolutionStrategy::KeepRemote:
                keepRemoteErrors.push_back(error);
                break;
            case ConflictResolutionStrategy::KeepMostRecent: {
                // path = original filename (remote version on disk)
                // destinationPath = conflict-renamed copy (local version on disk)
                const SyncPath originalAbsPath = localPath / error.destinationPath().parent_path() / error.path().filename();
                const SyncPath conflictAbsPath = localPath / error.destinationPath();

                FileStat originalStat;
                FileStat conflictStat;
                IoError ioError = IoError::Success;

                if (!IoHelper::getFileStat(originalAbsPath, &originalStat, ioError, IoHelper::PathCheckOption::Insensitive) ||
                    ioError != IoError::Success) {
                    LOG_WARN(_logger,
                             "Error getting file stat for original path, defaulting to keepLocal for errorDbId=" << error.dbId());
                    // If we cannot retrieve the file information, we fall back to keeping the local version.
                    // This is the safest choice to avoid accidental data loss. If the file no longer exists,
                    // the conflict will resolve itself later.
                    keepLocalErrors.push_back(error);
                    break;
                }

                if (!IoHelper::getFileStat(conflictAbsPath, &conflictStat, ioError, IoHelper::PathCheckOption::Insensitive) ||
                    ioError != IoError::Success) {
                    LOG_WARN(_logger,
                             "Error getting file stat for conflict path, defaulting to keepLocal for errorDbId=" << error.dbId());
                    // If we cannot retrieve the file information, we fall back to keeping the local version.
                    // This is the safest choice to avoid accidental data loss. If the file no longer exists,
                    // the conflict will resolve itself later.
                    keepLocalErrors.push_back(error);
                    break;
                }

                if (conflictStat.modificationTime >= originalStat.modificationTime) {
                    // Conflict copy (local version) is more recent or equal -> keep local
                    keepLocalErrors.push_back(error);
                } else {
                    // Original (remote version) is more recent -> keep remote
                    keepRemoteErrors.push_back(error);
                }
                break;
            }
            default:
                LOG_WARN(_logger, "ErrorResolveConflictsQuickJob::process: Unknown strategy");
                return ExitCode::LogicError;
        }
    }

    std::shared_ptr<SyncPal> syncPal;
    if (ExitInfo exitInfo = getSyncPal(syncDbId, syncPal); !exitInfo) {
        return exitInfo;
    }

    UserActionScopedLock lock;
    if (syncPal != nullptr && !lock.tryLock(syncPal, std::chrono::milliseconds(userActionLockTimeoutMs))) {
        LOG_WARN(_logger, "Could not acquire user action lock for syncDbId="
                                  << syncDbId << ". Another user action is running. Aborting ErrorResolveConflictsQuickJob.");
        return ExitCode::OperationCanceled;
    }

    return fixConflictsAndNotify(syncPal, keepLocalErrors, keepRemoteErrors);
}

} // namespace KDC

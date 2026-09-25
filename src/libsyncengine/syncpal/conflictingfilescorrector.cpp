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

#include "conflictingfilescorrector.h"

#include "jobs/local/synclocaldeletejob.h"
#include "jobs/local/localmovejob.h"
#include "libcommonserver/io/iohelper.h"
#include "libcommonserver/utility/utility.h" // Path2WStr

#include <log4cplus/loggingmacros.h>

#include <Poco/Timestamp.h>
#include <Poco/File.h>
#include <Poco/Timestamp.h>

namespace KDC {

ConflictingFilesCorrector::ConflictingFilesCorrector(std::shared_ptr<SyncPal> syncPal,
                                                     const std::vector<Error> &keepLocalErrorList,
                                                     const std::vector<Error> &keepRemoteErrorList) :
    _syncPal(syncPal),
    _keepLocalErrors(keepLocalErrorList),
    _keepRemoteErrors(keepRemoteErrorList) {}

ExitInfo ConflictingFilesCorrector::runJob() {
    if (ExitInfo exitInfo = resolveConflicts(_keepLocalErrors, ConflictResolutionStrategy::KeepLocal); !exitInfo) {
        return exitInfo;
    }
    if (ExitInfo exitInfo = resolveConflicts(_keepRemoteErrors, ConflictResolutionStrategy::KeepRemote); !exitInfo) {
        return exitInfo;
    }
    return ExitCode::Ok;
}

ExitInfo ConflictingFilesCorrector::resolveConflicts(const std::vector<Error> &errorList, ConflictResolutionStrategy strategy) {
    if (strategy != ConflictResolutionStrategy::KeepLocal && strategy != ConflictResolutionStrategy::KeepRemote) {
        LOG_WARN(Log::instance()->getLogger(), "Invalid conflict resolution strategy: " << strategy);
        return ExitCode::LogicError;
    }

    for (auto &error: errorList) {
        bool exists = false;
        IoError ioError = IoError::Success;
        if (!IoHelper::checkIfPathExists(_syncPal->localPath() / error.destinationPath(), exists, ioError,
                                         IoHelper::PathCheckOption::Insensitive)) {
            LOGW_WARN(Log::instance()->getLogger(),
                      L"Error in IoHelper::checkIfPathExists: " << Utility::formatIoError(error.destinationPath(), ioError));
            _nbErrors++;
            continue;
        }

        if (!exists) {
            // Ok, conflict already solved
            deleteError(error.dbId());
            continue;
        }

        if (strategy == ConflictResolutionStrategy::KeepLocal) {
            if (keepLocalVersion(error)) {
                deleteError(error.dbId());
            } else {
                _nbErrors++;
            }
            continue;
        }

        if (strategy == ConflictResolutionStrategy::KeepRemote) {
            if (keepRemoteVersion(error)) {
                deleteError(error.dbId());
            } else {
                _nbErrors++;
            }
            continue;
        }
    }

    return ExitCode::Ok;
}

ConflictingFilesCorrector::CanonicalPaths ConflictingFilesCorrector::getCanonicalSourceAndDestinationPaths(
        const SyncPath &sourcePath, const SyncPath &destinationPath) const {
    CanonicalPaths result;

    std::error_code ec;
    result.destinationPath = std::filesystem::canonical(destinationPath.parent_path(), ec) / destinationPath.filename();

    if (ec) {
        LOGW_WARN(Log::instance()->getLogger(), L"Error in std::filesystem::canonical for destinationPath: "
                                                        << Utility::formatSyncPath(destinationPath) << L" - "
                                                        << CommonUtility::s2ws(ec.message()));
        return {};
    }

    result.sourcePath = std::filesystem::canonical(sourcePath.parent_path(), ec) / sourcePath.filename();
    if (ec) {
        LOGW_WARN(Log::instance()->getLogger(), L"Error in std::filesystem::canonical for sourcePath: "
                                                        << Utility::formatSyncPath(sourcePath) << L" - "
                                                        << CommonUtility::s2ws(ec.message()));
        return {};
    }

    if (result.destinationPath.parent_path() != result.sourcePath.parent_path()) {
        LOGW_WARN(Log::instance()->getLogger(), L"Source and destination paths do not have the same parent path: "
                                                        << Utility::formatSyncPath(sourcePath) << L", "
                                                        << Utility::formatSyncPath(destinationPath));
        return {};
    }

    if (!CommonUtility::isSubDir(_syncPal->localPath(), result.destinationPath) ||
        result.destinationPath == _syncPal->localPath()) {
        LOGW_WARN(Log::instance()->getLogger(),
                  L"Invalid canonical destinationPath: " << Utility::formatSyncPath(result.destinationPath));
        return {};
    }

    if (!CommonUtility::isSubDir(_syncPal->localPath(), result.sourcePath) || result.sourcePath == _syncPal->localPath()) {
        LOGW_WARN(Log::instance()->getLogger(), L"Invalid canonical sourcePath: " << Utility::formatSyncPath(result.sourcePath));
    }

    result.valid = true;

    return result;
}

namespace {
// A first sanity check for destination path validity. The destination path must be a relative path with a non-empty
// filename that is not "." or "..".
bool errorPathIsValid(const SyncPath &errorPath) {
    return !errorPath.is_absolute() && !errorPath.filename().empty() && errorPath.filename() != SyncPath{"."} &&
           errorPath.filename() != SyncPath{".."};
}
} // namespace

bool ConflictingFilesCorrector::keepLocalVersion(const Error &error) {
    // A corruption of `ParmsDb` can lead to unwanted deletion of files if the error paths are empty, absolute or
    // indicate items located outside the sync directory.
    if (!errorPathIsValid(error.path()) || !errorPathIsValid(error.destinationPath())) {
        LOGW_WARN(Log::instance()->getLogger(), L"Invalid error paths in ConflictingFilesCorrector::keepLocalVersion: "
                                                        << Utility::formatSyncPath(error.path()) << L" / destination "
                                                        << Utility::formatSyncPath(error.destinationPath()));
        return false;
    }

    // Source and destination paths refer to the final local rename operation below.
    const auto canonicalPaths = getCanonicalSourceAndDestinationPaths(
            _syncPal->localPath() / error.destinationPath(),
            _syncPal->localPath() / error.destinationPath().parent_path() / error.path().filename());

    if (!canonicalPaths.valid) return false;

    // Delete remote version locally
    SyncLocalDeleteJob deleteJob(_syncPal, canonicalPaths.destinationPath);
    if (const auto exitInfo = deleteJob.runSynchronously(); !exitInfo) return false;

    // Rename the local version
    LocalMoveJob renameJob(canonicalPaths.sourcePath, canonicalPaths.destinationPath);
    if (const auto exitInfo = renameJob.runSynchronously(); !exitInfo) return false;

    // Set the local modification time to now
    const Poco::Timestamp lastModifiedTimestamp;
    (void) Poco::File(Path2Str(canonicalPaths.destinationPath)).setLastModified(lastModifiedTimestamp);

    return true;
}

bool ConflictingFilesCorrector::keepRemoteVersion(const Error &error) {
    // A corruption of `ParmsDb` can lead to unwanted deletion of files if the error destination path is empty, absolute or
    // indicates an item located outside the sync directory.
    if (!errorPathIsValid(error.destinationPath())) {
        LOGW_WARN(Log::instance()->getLogger(),
                  L"Invalid error destination path in ConflictingFilesCorrector::keepRemoteVersion: "
                          << Utility::formatSyncPath(error.destinationPath()));
        return false;
    }

    std::error_code ec;
    const SyncPath absoluteLocalPathToDelete =
            std::filesystem::canonical(_syncPal->localPath() / error.destinationPath().parent_path(), ec) /
            error.destinationPath().filename();
    if (ec) {
        LOGW_WARN(Log::instance()->getLogger(), L"Error in std::filesystem::canonical for absolute local path to delete: "
                                                        << Utility::formatSyncPath(absoluteLocalPathToDelete) << L" - "
                                                        << CommonUtility::s2ws(ec.message()));
        return false;
    }

    if (!CommonUtility::isSubDir(_syncPal->localPath(), absoluteLocalPathToDelete) ||
        absoluteLocalPathToDelete == _syncPal->localPath()) {
        LOGW_WARN(Log::instance()->getLogger(),
                  L"Invalid absolute local path to delete in ConflictingFilesCorrector::keepRemoteVersion: "
                          << Utility::formatSyncPath(absoluteLocalPathToDelete));
        return false;
    }

    // Delete local version
    SyncLocalDeleteJob deleteJob(_syncPal, absoluteLocalPathToDelete);
    if (const auto exitInfo = deleteJob.runSynchronously(); !exitInfo) return false;

    return true;
}

void ConflictingFilesCorrector::deleteError(const ErrorDbId errorDbId) {
    if (bool found = false; !ParmsDb::instance()->deleteError(errorDbId, found)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::deleteError");
        return;
    }
    _removedErrorsDbIds.push_back(errorDbId);
}

} // namespace KDC

/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2024 Infomaniak Network SA
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

#include "localdeletejob.h"
#include "../network/API_v2/getfileinfojob.h"
#include "libcommonserver/io/iohelper.h"
#include "libcommonserver/utility/utility.h"
#include "requests/parameterscache.h"

#include <log4cplus/loggingmacros.h>

#include <utility/utility.h>

#ifdef _WIN32
#include <objbase.h>
#endif

namespace KDC {

LocalDeleteJob::Path::Path(const SyncPath &path) : _path(path){};

bool LocalDeleteJob::Path::endsWith(SyncPath &&ending) const {
    if (!_path.empty() && ending.empty()) return false;

    SyncPath path = _path;

    while (!ending.empty() && !path.empty()) {
        if (ending.filename() != path.filename()) return false;
        ending = ending.parent_path();
        path = path.parent_path();
    }

    return !path.empty() || ending.empty();
};

bool LocalDeleteJob::matchRelativePaths(const SyncPath &targetPath, const SyncPath &localRelativePath,
                                        const SyncPath &remoteRelativePath) {
    if (targetPath.empty()) return localRelativePath == remoteRelativePath;

    // Case of an advanced synchronisation
    return Path(remoteRelativePath).endsWith(SyncPath(targetPath.filename()) / localRelativePath);
}

LocalDeleteJob::LocalDeleteJob(const SyncPalInfo &syncPalInfo, const SyncPath &relativePath, bool isDehydratedPlaceholder,
                               NodeId remoteId, bool forceToTrash /* = false */)
    : _syncInfo(syncPalInfo),
      _relativePath(relativePath),
      _absolutePath(syncPalInfo.localPath / relativePath),
      _isDehydratedPlaceholder(isDehydratedPlaceholder),
      _remoteNodeId(remoteId),
      _forceToTrash(forceToTrash) {}

LocalDeleteJob::LocalDeleteJob(const SyncPath &absolutePath) : _absolutePath(absolutePath) {
    setBypassCheck(true);
}

LocalDeleteJob::~LocalDeleteJob() {}


bool LocalDeleteJob::findRemoteItem(SyncPath &remoteItemPath) const {
    bool found = true;
    remoteItemPath.clear();

    // The item must be absent of remote replica for the job to run
    GetFileInfoJob job(syncInfo().driveDbId, _remoteNodeId);
    job.setWithPath(true);
    job.runSynchronously();

    if (job.hasHttpError()) {
        using namespace Poco::Net;
        if (job.getStatusCode() == HTTPResponse::HTTP_FORBIDDEN || job.getStatusCode() == HTTPResponse::HTTP_NOT_FOUND) {
            found = false;
            LOGW_DEBUG(_logger, L"Item: " << Utility::formatSyncPath(_absolutePath).c_str()
                                          << L" not found on remote replica. This is normal and expected.");
        }
        remoteItemPath = job.path();
    }

    return found;
}

bool LocalDeleteJob::canRun() {
    if (bypassCheck()) {
        return true;
    }

    // The item must exist locally for the job to run
    bool exists = false;
    IoError ioError = IoError::Success;
    if (!IoHelper::checkIfPathExists(_absolutePath, exists, ioError)) {
        LOGW_WARN(_logger, L"Error in IoHelper::checkIfPathExists: " << Utility::formatIoError(_absolutePath, ioError).c_str());
        _exitCode = ExitCode::SystemError;
        _exitCause = ExitCause::FileAccessError;
        return false;
    }

    if (!exists) {
        LOGW_DEBUG(_logger, L"Item does not exist anymore. Aborting current sync and restart: "
                                << Utility::formatSyncPath(_absolutePath).c_str());
        _exitCode = ExitCode::NeedRestart;
        _exitCause = ExitCause::UnexpectedFileSystemEvent;
        return false;
    }

    if (_remoteNodeId.empty()) {
        LOG_WARN(_logger, "Remote node ID is empty");
        _exitCode = ExitCode::SystemError;
        _exitCause = ExitCause::FileAccessError;
        return false;
    }

    // Check if the item we want to delete locally has a remote counterpart.

    SyncPath remoteRelativePath;
    const bool remotItemIsFound = findRemoteItem(remoteRelativePath);

    if (!remotItemIsFound) return true;  // Safe deletion.

    // Check whether the remote item has been moved.
    // If the remote item has been moved into a blacklisted folder, then this Delete job is created and
    // the local item should be deleted.
    // Note: the other remote move operations are not relevant: they generate Move jobs.

    if (matchRelativePaths(syncInfo().targetPath, Utility::normalizedSyncPath(_relativePath), remoteRelativePath)) {
        // Item is found at the same path on remote
        LOGW_DEBUG(_logger, L"Item with " << Utility::formatSyncPath(_absolutePath).c_str()
                                          << L" still exists on remote replica. Aborting current sync and restarting.");
        _exitCode = ExitCode::DataError;  // We need to rebuild the remote snapshot from scratch
        _exitCause = ExitCause::InvalidSnapshot;
        return false;
    }

    return true;
}

void LocalDeleteJob::runJob() {
    if (!canRun()) {
        return;
    }

    if ((ParametersCache::instance()->parameters().moveToTrash() && !_isDehydratedPlaceholder) || _forceToTrash) {
        const bool success = Utility::moveItemToTrash(_absolutePath);
        _exitCode = ExitCode::Ok;
        if (!success) {
            LOGW_WARN(_logger, L"Failed to move item: " << Utility::formatSyncPath(_absolutePath).c_str() << L" to trash");
            _exitCode = ExitCode::SystemError;
            _exitCause = ExitCause::MoveToTrashFailed;
            return;
        }
        if (ParametersCache::isExtendedLogEnabled()) {
            LOGW_DEBUG(_logger, L"Item: " << Utility::formatSyncPath(_absolutePath).c_str() << L" was moved to trash");
        }
    } else {
        LOGW_DEBUG(_logger, L"Delete item: " << Utility::formatSyncPath(_absolutePath).c_str());
        std::error_code ec;
        std::filesystem::remove_all(_absolutePath, ec);
        if (ec.value() != 0) {
            LOGW_WARN(_logger, L"Failed to delete: " << Utility::formatStdError(_absolutePath, ec).c_str());
            _exitCode = ExitCode::SystemError;
            _exitCause = ExitCause::FileAccessError;
            return;
        }

        LOGW_INFO(_logger, L"Item: " << Utility::formatSyncPath(_absolutePath).c_str() << L" deleted");
        _exitCode = ExitCode::Ok;
    }
}

}  // namespace KDC

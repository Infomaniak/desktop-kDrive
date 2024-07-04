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

#include "deletejob.h"

#include "requests/parameterscache.h"
#include "libcommonserver/io/filestat.h"
#include "libcommonserver/io/iohelper.h"
#include "libcommonserver/utility/utility.h"  // Path2WStr

namespace KDC {

DeleteJob::DeleteJob(int driveDbId, const NodeId &remoteItemId, const NodeId &localItemId, const SyncPath &absoluteLocalFilepath)
    : AbstractTokenNetworkJob(ApiDrive, 0, 0, driveDbId, 0)
      , _remoteItemId(remoteItemId)
      , _localItemId(localItemId)
      , _absoluteLocalFilepath(absoluteLocalFilepath) {
    _httpMethod = Poco::Net::HTTPRequest::HTTP_DELETE;
}

bool DeleteJob::canRun() {
    if (bypassCheck()) {
        return true;
    }

    if (_remoteItemId.empty()
        || _localItemId.empty()
        || _absoluteLocalFilepath.empty()) {
        LOGW_WARN(_logger,
                  L"Error in DeleteJob::canRun: missing required input, remote ID:" << Utility::s2ws(_remoteItemId).c_str()
                  << L", local ID: " << Utility::s2ws(_localItemId).c_str()
                  << L", " << Utility::formatSyncPath(_absoluteLocalFilepath)
                  );
        _exitCode = ExitCodeDataError;
        _exitCause = ExitCauseUnknown;
        return false;
    }

    // The item must be absent on local replica for the job to run
    bool existsWithSameId = false;
    NodeId otherNodeId;
    IoError ioError = IoErrorSuccess;
    if (!IoHelper::checkIfPathExistsWithSameNodeId(_absoluteLocalFilepath, _localItemId, existsWithSameId, otherNodeId,
                                                   ioError)) {
        LOGW_WARN(_logger,
                  L"Error in IoHelper::checkIfPathExists: " << Utility::formatIoError(_absoluteLocalFilepath, ioError).c_str());
        _exitCode = ExitCodeSystemError;
        _exitCause = ExitCauseFileAccessError;
        return false;
    }

    if (existsWithSameId) {
        FileStat filestat;
        ioError = IoErrorSuccess;
        if (!IoHelper::getFileStat(_absoluteLocalFilepath, &filestat, ioError)) {
            LOGW_WARN(_logger,
                      L"Error in IoHelper::getFileStat: " << Utility::formatIoError(_absoluteLocalFilepath, ioError).c_str());
            _exitCode = ExitCodeSystemError;
            _exitCause = ExitCauseFileAccessError;
            return false;
        }

        if (ioError == IoErrorNoSuchFileOrDirectory) {
            LOGW_WARN(_logger, L"Item does not exist anymore: " << Utility::formatSyncPath(_absoluteLocalFilepath).c_str());
            _exitCode = ExitCodeDataError;
            _exitCause = ExitCauseInvalidSnapshot;
            return false;
        } else if (ioError == IoErrorAccessDenied) {
            LOGW_WARN(_logger, L"Item misses search permission: " << Utility::formatSyncPath(_absoluteLocalFilepath).c_str());
            _exitCode = ExitCodeSystemError;
            _exitCause = ExitCauseNoSearchPermission;
            return false;
        }

        if (!ParametersCache::instance()->parameters().syncHiddenFiles() && filestat.isHidden) {
            // The item is hidden, remove it from sync
            return true;
        }

        LOGW_DEBUG(_logger, L"Item: " << Utility::formatSyncPath(_absoluteLocalFilepath).c_str()
                                      << L" still exist on local replica. Aborting current sync and restart.");
        _exitCode = ExitCodeDataError;  // Data error so the snapshots will be re-created
        _exitCause = ExitCauseUnexpectedFileSystemEvent;
        return false;
    } else if (_localItemId != otherNodeId) {
        LOGW_DEBUG(_logger, L"Item: " << Utility::formatSyncPath(_absoluteLocalFilepath).c_str()
                                      << L" exist on local replica with another ID (" << Utility::s2ws(_localItemId).c_str()
                                      << L"/" << Utility::s2ws(otherNodeId).c_str() << L")");

#ifdef NDEBUG
        std::stringstream ss;
        ss << "File exists with another ID (" << _localItemId << "/" << otherNodeId << ")";
        sentry_capture_event(
            sentry_value_new_message_event(SENTRY_LEVEL_WARNING, "IoHelper::checkIfPathExistsWithSameNodeId", ss.str().c_str()));
#endif
    }

    return true;
}

std::string DeleteJob::getSpecificUrl() {
    std::string str = AbstractTokenNetworkJob::getSpecificUrl();
    str += "/files/";
    str += _remoteItemId;
    return str;
}

}  // namespace KDC

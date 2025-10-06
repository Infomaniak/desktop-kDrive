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

#include "deletejob.h"

#include "requests/parameterscache.h"
#include "libcommonserver/io/filestat.h"
#include "libcommonserver/io/iohelper.h"
#include "libcommonserver/utility/utility.h"

#include <Poco/Net/HTTPRequest.h>

namespace KDC {

DeleteJob::DeleteJob(const int driveDbId, const NodeId &remoteItemId, const NodeId &localItemId,
                     const SyncPath &absoluteLocalFilepath, const NodeType nodeType) :
    AbstractTokenNetworkJob(ApiType::Drive, 0, 0, driveDbId, 0),
    _remoteItemId(remoteItemId),
    _localItemId(localItemId),
    _absoluteLocalFilepath(absoluteLocalFilepath),
    _nodeType(nodeType) {
    _httpMethod = Poco::Net::HTTPRequest::HTTP_DELETE;
}

DeleteJob::DeleteJob(const int driveDbId, const NodeId &remoteItemId) :
    DeleteJob(driveDbId, remoteItemId, {}, {}, NodeType::Unknown) {}

bool DeleteJob::canRun() {
    if (bypassCheck()) {
        return true;
    }

    if (_remoteItemId.empty() || _localItemId.empty() || _absoluteLocalFilepath.empty()) {
        LOGW_WARN(_logger, L"Error in DeleteJob::canRun: missing required input, remote ID:"
                                   << CommonUtility::s2ws(_remoteItemId) << L", local ID: " << CommonUtility::s2ws(_localItemId)
                                   << L", " << Utility::formatSyncPath(_absoluteLocalFilepath));
        _exitInfo = ExitCode::DataError;
        return false;
    }

    // The item must be absent on local replica for the job to run
    bool existsWithSameId = false;
    NodeId otherNodeId;
    IoError ioError = IoError::Success;
    if (!IoHelper::checkIfPathExistsWithSameNodeId(_absoluteLocalFilepath, _localItemId, existsWithSameId, otherNodeId,
                                                   ioError)) {
        LOGW_WARN(_logger, L"Error in IoHelper::checkIfPathExists: " << Utility::formatIoError(_absoluteLocalFilepath, ioError));
        _exitInfo = ExitCode::SystemError;
        return false;
    }
    if (ioError == IoError::AccessDenied) {
        _exitInfo = {ExitCode::SystemError, ExitCause::FileAccessError};
        return false;
    }

    if (existsWithSameId) {
        FileStat filestat;
        ioError = IoError::Success;
        if (!IoHelper::getFileStat(_absoluteLocalFilepath, &filestat, ioError)) {
            LOGW_WARN(_logger, L"Error in IoHelper::getFileStat: " << Utility::formatIoError(_absoluteLocalFilepath, ioError));
            _exitInfo = ExitCode::SystemError;
            return false;
        }

        if (ioError == IoError::NoSuchFileOrDirectory) {
            LOGW_WARN(_logger, L"Item does not exist anymore: " << Utility::formatSyncPath(_absoluteLocalFilepath));
            _exitInfo = {ExitCode::DataError, ExitCause::InvalidSnapshot};
            return false;
        } else if (ioError == IoError::AccessDenied) {
            LOGW_WARN(_logger, L"Item misses search permission: " << Utility::formatSyncPath(_absoluteLocalFilepath));
            _exitInfo = {ExitCode::SystemError, ExitCause::FileAccessError};
            return false;
        }

        if (filestat.nodeType != _nodeType && filestat.nodeType != NodeType::Unknown && _nodeType != NodeType::Unknown) {
            // The nodeId has been reused by a new item.
            LOGW_DEBUG(_logger,
                       L"Item: " << Utility::formatSyncPath(_absoluteLocalFilepath) << L" has been reused by a new item.");
            return true;
        }

        LOGW_DEBUG(_logger, L"Item: " << Utility::formatSyncPath(_absoluteLocalFilepath) << L" still exists on local replica.");
        _exitInfo = {ExitCode::DataError, ExitCause::FileExists};
        return false;
    } else if (!otherNodeId.empty() && _localItemId != otherNodeId) {
        LOGW_DEBUG(_logger, L"Item: " << Utility::formatSyncPath(_absoluteLocalFilepath)
                                      << L" exists on local replica with another ID (" << CommonUtility::s2ws(_localItemId)
                                      << L"/" << CommonUtility::s2ws(otherNodeId) << L")");

        std::stringstream ss;
        ss << "File exists with another ID (" << _localItemId << "/" << otherNodeId << ")";
        sentry::Handler::captureMessage(sentry::Level::Warning, "IoHelper::checkIfPathExistsWithSameNodeId", ss.str());
    }

    return true;
}

std::string DeleteJob::getSpecificUrl() {
    std::string str = AbstractTokenNetworkJob::getSpecificUrl();
    str += "/files/";
    str += _remoteItemId;
    return str;
}

} // namespace KDC

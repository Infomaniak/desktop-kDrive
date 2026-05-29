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

#include "checkhashmatchjob.h"
#include "libcommonserver/io/iohelper.h"
#include "libcommonserver/utility/jsonparserutility.h"
#include "libcommonserver/utility/utility.h"

#include <fstream>
#include <Poco/Net/HTTPRequest.h>
#include <log4cplus/loggingmacros.h>

namespace KDC {

CheckHashMatchJob::CheckHashMatchJob(const DriveDbId driveDbId, const SyncPath &filepath, const NodeId &nodeId,
                                     const int64_t remoteSize) :
    AbstractTokenNetworkJob(ApiType::Drive, 0, 0, driveDbId, 0),
    _filePath(filepath),
    _nodeId(nodeId),
    _remoteSize(remoteSize) {
    _httpMethod = Poco::Net::HTTPRequest::HTTP_GET;
}

CheckHashMatchJob::CheckHashMatchJob(const DriveDbId driveDbId, const SyncPath &filepath, const NodeId &nodeId,
                                     const int64_t localSize, const int64_t remoteSize) :
    AbstractTokenNetworkJob(ApiType::Drive, 0, 0, driveDbId, 0),
    _filePath(filepath),
    _nodeId(nodeId),
    _localSize(localSize),
    _remoteSize(remoteSize) {
    _httpMethod = Poco::Net::HTTPRequest::HTTP_GET;
}

ExitInfo CheckHashMatchJob::getFileSize(const SyncPath &path, int64_t &size) {
    IoError ioError = IoError::Unknown;
    uint64_t tmpSize = 0;
    if (!IoHelper::getFileSize(path, tmpSize, ioError)) {
        LOGW_WARN(_logger, L"Error in IoHelper::getFileSize for " << Utility::formatIoError(path, ioError));
        return ExitCode::SystemError;
    }
    size = static_cast<int64_t>(tmpSize);

    if (ioError == IoError::NoSuchFileOrDirectory) { // The synchronization will be re-started.
        LOGW_WARN(_logger, L"File doesn't exist: " << Utility::formatSyncPath(path));
        return {ExitCode::SystemError, ExitCause::NotFound};
    }

    if (ioError == IoError::AccessDenied) { // An action from the user is requested.
        LOGW_WARN(_logger, L"File search permission missing: " << Utility::formatSyncPath(path));
        return {ExitCode::SystemError, ExitCause::FileAccessError};
    }

    assert(ioError == IoError::Success);
    if (ioError != IoError::Success) {
        LOGW_WARN(_logger, L"Unable to read file size for " << Utility::formatSyncPath(path));
        return ExitCode::SystemError;
    }

    return ExitCode::Ok;
}

ExitInfo CheckHashMatchJob::runJob() noexcept {
    if (!_localSize)
        if (const ExitInfo exitInfo = getFileSize(_filePath, _localSize); !exitInfo) return exitInfo;

    if (_localSize != _remoteSize) return ExitCode::Ok;
    if (const ExitInfo exitInfo = AbstractTokenNetworkJob::runJob(); !exitInfo) {
        return exitInfo;
    }

    const IoError checksumIoError = IoHelper::getFileChecksum(_filePath, _localHash);
    if (checksumIoError == IoError::NoSuchFileOrDirectory) {
        LOGW_WARN(_logger, L"File doesn't exist while computing checksum: " << Utility::formatSyncPath(_filePath));
        return ExitCode::DataError;
    }

    if (checksumIoError == IoError::AccessDenied) {
        LOGW_WARN(_logger, L"File read permission missing while computing checksum: " << Utility::formatSyncPath(_filePath));
        return {ExitCode::SystemError, ExitCause::FileAccessError};
    }

    if (checksumIoError == IoError::InvalidArgument) {
        LOGW_WARN(_logger, L"File is a symlink: " << Utility::formatSyncPath(_filePath));
        return {ExitCode::SystemError, ExitCause::FileAccessError};
    }

    assert(checksumIoError == IoError::Success);
    if (checksumIoError != IoError::Success) {
        LOGW_WARN(_logger, L"Unable to compute checksum for " << Utility::formatIoError(_filePath, checksumIoError));
        return ExitCode::SystemError;
    }

    if (_localHash != _remoteHash) return ExitCode::Ok;
    _shouldDownload = false;
    return ExitCode::Ok;
}

std::string CheckHashMatchJob::getSpecificUrl() {
    std::string str = AbstractTokenNetworkJob::getSpecificUrl();
    str += "/files/";
    str += _nodeId;
    str += "/hash";
    return str;
}

ExitInfo CheckHashMatchJob::handleResponse(std::istream &is) {
    if (const auto exitInfo = AbstractTokenNetworkJob::handleResponse(is); !exitInfo) {
        return exitInfo;
    }

    if (jsonRes()) {
        if (const auto dataObj = jsonRes()->getObject(dataKey); dataObj) {
            if (!JsonParserUtility::extractValue(dataObj, hashKey, _remoteHash)) {
                return {ExitCode::BackError, ExitCause::MissingReplyData};
            }
        }
    }

    return ExitCode::Ok;
}

} // namespace KDC

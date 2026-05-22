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
#include "libcommonserver/utility/jsonparserutility.h"

#include <Poco/Net/HTTPRequest.h>

namespace KDC {

CheckHashMatchJob::CheckHashMatchJob(const DriveDbId driveDbId, const SyncPath &filepath, const NodeId &nodeId, int64_t localsize,
                                     int64_t remotesize) :
    AbstractTokenNetworkJob(ApiType::Drive, 0, 0, driveDbId, 0),
    _filePath(filepath),
    _nodeId(nodeId),
    _localSize(localsize),
    _remoteSize(remotesize) {
    _httpMethod = Poco::Net::HTTPRequest::HTTP_GET;
}

ExitInfo CheckHashMatchJob::runJob() noexcept {
    if (_localSize != _remoteSize) return ExitCode::Ok;

    if (const ExitInfo exitInfo = AbstractTokenNetworkJob::runJob(); !exitInfo) {
        return exitInfo;
    }

    std::ifstream ifs;
    IoError ioError = IoError::Unknown;
    _localHash = "xxh3:" + IoHelper::getFileChecksum(_filePath, ifs, ioError);

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

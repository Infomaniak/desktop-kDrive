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

CheckHashMatchJob::CheckHashMatchJob(const DriveDbId driveDbId, const SyncPath &filepath, const NodeId &nodeId, int localsize,
                                     int remotesize) :
    AbstractTokenNetworkJob(ApiType::Drive, 0, 0, driveDbId, 0),
    _filePath(filepath),
    _nodeId(nodeId) {
    if (localsize != remotesize) abort();

    _httpMethod = Poco::Net::HTTPRequest::HTTP_GET;
}

CheckHashMatchJob::~CheckHashMatchJob() {}

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
            if (!JsonParserUtility::extractValue(dataObj, hashKey, _distantHash)) {
                return {ExitCode::BackError, ExitCause::MissingReplyData};
            }
        }
    }

    std::ifstream ifs;
    IoError ioError = IoError::Unknown;
    _localHash = "xxh3:" + IoHelper::getFileChecksum(_filePath, ifs, ioError);

    if (_localHash != _distantHash) return ExitCode::Ok;
    _shouldDownload = false;

    return ExitCode::Ok;
}

} // namespace KDC

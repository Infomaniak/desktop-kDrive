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

#include "postfilemodificationdatejob.h"

#include "jobs/network/networkjobsparams.h"
#include "libcommonserver/io/iohelper.h"
#include "libcommonserver/utility/jsonparserutility.h"
#include "libcommonserver/utility/utility.h"

#include <Poco/Net/HTTPRequest.h>

namespace KDC {

PostFileModificationDateJob::PostFileModificationDateJob(const DriveDbId driveDbId, const NodeId &nodeId, SyncTime lastModifiedAt,
                                                         const SyncPath &filePath) :
    AbstractTokenNetworkJob(ApiType::Drive, 0, 0, driveDbId, 0),
    _nodeId(nodeId),
    _lastModifiedAt(lastModifiedAt),
    _filePath(filePath) {
    _httpMethod = Poco::Net::HTTPRequest::HTTP_POST;
    _apiVersion = 3;
}
std::string PostFileModificationDateJob::getSpecificUrl() {
    std::string str = AbstractTokenNetworkJob::getSpecificUrl();
    str += "/files/";
    str += _nodeId;
    str += "/last-modified";
    return str;
}

ExitInfo PostFileModificationDateJob::setData() {
    Poco::JSON::Object json;
    (void) json.set("last_modified_at", _lastModifiedAt);

    std::stringstream ss;
    json.stringify(ss);
    _data = ss.str();
    return ExitCode::Ok;
}

ExitInfo PostFileModificationDateJob::runJob() noexcept {
    if (const ExitInfo exitInfo = AbstractTokenNetworkJob::runJob(); !exitInfo) return exitInfo;

    if (_filePath.empty()) return ExitCode::Ok;

    if (const IoError ioError = IoHelper::setFileDates(_filePath, 0, _lastModifiedAtOut, false); ioError != IoError::Success) {
        LOGW_WARN(_logger, L"Error in IoHelper::setFileDates: " << Utility::formatIoError(_filePath, ioError));
    }
    return ExitCode::Ok;
}

ExitInfo PostFileModificationDateJob::handleResponse(std::istream &is) {
    if (const auto exitInfo = AbstractTokenNetworkJob::handleResponse(is); !exitInfo) {
        return exitInfo;
    }

    if (!jsonRes()) return {ExitCode::BackError, ExitCause::MissingReplyData};

    const auto dataObj = jsonRes()->getObject(dataKey);
    if (!dataObj) return {ExitCode::BackError, ExitCause::MissingReplyData};

    if (!JsonParserUtility::extractValue(dataObj, lastModifiedAtKey, _lastModifiedAtOut))
        return {ExitCode::BackError, ExitCause::MissingReplyData};

    return ExitCode::Ok;
}

} // namespace KDC

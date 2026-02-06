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

#include "jobs/network/kDrive_API/getfilesindirectoryjob.h"

#include "utility/jsonparserutility.h"

#include <Poco/Net/HTTPRequest.h>

namespace KDC {

GetFilesInDirectoryJob::GetFilesInDirectoryJob(const int userDbId, const int driveId, const NodeId &fileId,
                                               const bool dirOnly /*= false*/) :
    AbstractTokenNetworkJob(ApiType::Drive, userDbId, 0, 0, driveId),
    _dirOnly(dirOnly) {
    _httpMethod = Poco::Net::HTTPRequest::HTTP_GET;
}

GetFilesInDirectoryJob::GetFilesInDirectoryJob(const int driveDbId, const NodeId &fileId, const bool dirOnly /*= false*/) :
    AbstractTokenNetworkJob(ApiType::Drive, 0, 0, driveDbId, 0),
    _dirOnly(dirOnly) {
    _httpMethod = Poco::Net::HTTPRequest::HTTP_GET;
}

std::string GetFilesInDirectoryJob::getSpecificUrl() {
    std::string str = AbstractTokenNetworkJob::getSpecificUrl();
    str += "/files";
    return str;
}

void GetFilesInDirectoryJob::setQueryParameters(Poco::URI &uri) {
    if (_dirOnly) {
        uri.addQueryParameter("type[]", "dir");
    }
    if (_withPath) {
        uri.addQueryParameter("with", "path,capabilities");
    } else {
        uri.addQueryParameter("with", "capabilities");
    }
}

ExitInfo GetFilesInDirectoryJob::handleResponse(std::istream &is) {
    if (const auto exitInfo = AbstractTokenNetworkJob::handleResponse(is); !exitInfo) return exitInfo;
    if (!jsonRes()) return {};
    return ExitCode::Ok;
}

} // namespace KDC

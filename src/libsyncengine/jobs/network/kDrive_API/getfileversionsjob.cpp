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

#include "getfileversionsjob.h"
#include "libcommonserver/utility/jsonparserutility.h"

#include <Poco/Net/HTTPRequest.h>

namespace KDC {

GetFileVersionsJob::GetFileVersionsJob(const int userDbId, const int driveId, const NodeId &nodeId) :
    AbstractTokenNetworkJob(ApiType::Drive, userDbId, 0, 0, driveId),
    _nodeId(nodeId) {
    _httpMethod = Poco::Net::HTTPRequest::HTTP_GET;
    _apiVersion = 3;
}

ExitInfo GetFileVersionsJob::handleResponse(std::istream &is) {
    if (const auto exitInfo = AbstractTokenNetworkJob::handleResponse(is); !exitInfo) return exitInfo;

    if (!jsonRes()) return ExitCode::Ok;

    const auto dataArray = jsonRes()->getArray(dataKey);
    if (!dataArray || dataArray->size() == 0) return ExitCode::Ok;

    const auto latestVersion = dataArray->getObject(0);
    if (!latestVersion) return ExitCode::Ok;

    (void) JsonParserUtility::extractValue(latestVersion, sizeKey, _size, false);
    (void) JsonParserUtility::extractValue(latestVersion, updatedAtKey, _updatedAt, false);

    if (const auto userObj = latestVersion->getObject(updatedByKey); userObj) {
        (void) JsonParserUtility::extractValue(userObj, displayNameKey, _updatedByName, false);
    }

    return ExitCode::Ok;
}

std::string GetFileVersionsJob::getSpecificUrl() {
    std::string str = AbstractTokenNetworkJob::getSpecificUrl();
    str += "/files/";
    str += _nodeId;
    str += "/versions";
    return str;
}

void GetFileVersionsJob::setQueryParameters(Poco::URI &uri) {
    uri.addQueryParameter("per_page", "1");
    uri.addQueryParameter("order_by", "last_modified_at");
    uri.addQueryParameter("order", "desc");
}

} // namespace KDC

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

#include "getthumbnailjob.h"

namespace KDC {

GetThumbnailJob::GetThumbnailJob(int driveDbId, NodeId nodeId, unsigned width)
    : AbstractTokenNetworkJob(ApiDrive, 0, 0, driveDbId, 0, false), _nodeId(nodeId), _width(width) {
    _httpMethod = Poco::Net::HTTPRequest::HTTP_GET;
}

std::string GetThumbnailJob::getSpecificUrl() {
    std::string str = AbstractTokenNetworkJob::getSpecificUrl();
    str += "/files/";
    str += _nodeId;
    str += "/thumbnail";
    return str;
}

void GetThumbnailJob::setQueryParameters(Poco::URI &uri, bool &canceled) {
    uri.addQueryParameter("width", std::to_string(_width));
    uri.addQueryParameter("height", std::to_string(_width));
    canceled = false;
}

}  // namespace KDC

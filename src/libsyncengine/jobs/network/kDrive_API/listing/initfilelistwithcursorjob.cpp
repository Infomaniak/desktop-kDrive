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

#include "initfilelistwithcursorjob.h"

#include <Poco/Net/HTTPRequest.h>

namespace KDC {

InitFileListWithCursorJob::InitFileListWithCursorJob(const int driveDbId, const NodeId &dirId) :
    AbstractListingJob(ApiType::Drive, driveDbId),
    _dirId(dirId) {
    _httpMethod = Poco::Net::HTTPRequest::HTTP_GET;
}

std::string InitFileListWithCursorJob::getSpecificUrl() {
    std::string str = AbstractTokenNetworkJob::getSpecificUrl();
    str += "/files/listing";
    return str;
}

void InitFileListWithCursorJob::setQueryParameters(Poco::URI &uri) {
    uri.addQueryParameter("directory_id", _dirId);
    uri.addQueryParameter("recursive", "true");
    uri.addQueryParameter("with", "files.capabilities");
    uri.addQueryParameter("limit", nbItemPerPage);
}

} // namespace KDC

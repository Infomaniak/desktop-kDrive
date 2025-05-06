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

#include "getdriveslistjob.h"

#include <Poco/Net/HTTPRequest.h>

namespace KDC {

GetDrivesListJob::GetDrivesListJob(int userDbId) : AbstractTokenNetworkJob(ApiType::DriveByUser, userDbId, 0, 0, 0) {
    _httpMethod = Poco::Net::HTTPRequest::HTTP_GET;
}

void GetDrivesListJob::setQueryParameters(Poco::URI &uri, bool &canceled) {
    uri.addQueryParameter("roles[]", "admin");
    uri.addQueryParameter("roles[]", "user");
    canceled = false;
}

std::string GetDrivesListJob::getSpecificUrl() {
    std::string str = AbstractTokenNetworkJob::getSpecificUrl();
    str += "/users/current/drives";
    return str;
}

} // namespace KDC

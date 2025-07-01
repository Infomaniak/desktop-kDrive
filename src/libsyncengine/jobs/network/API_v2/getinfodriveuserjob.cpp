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

#include "getinfodriveuserjob.h"

#include <Poco/Net/HTTPRequest.h>

namespace KDC {

GetInfoDriveUserJob::GetInfoDriveUserJob(int userDbId, int driveId) :
    AbstractTokenNetworkJob(ApiType::Drive, userDbId, 0, 0, driveId) {
    _httpMethod = Poco::Net::HTTPRequest::HTTP_GET;
}

GetInfoDriveUserJob::GetInfoDriveUserJob(int driveDbId) :
    AbstractTokenNetworkJob(ApiType::Drive, 0, 0, driveDbId, 0) {
    _httpMethod = Poco::Net::HTTPRequest::HTTP_GET;
}

std::string GetInfoDriveUserJob::getSpecificUrl() {
    std::string str = AbstractTokenNetworkJob::getSpecificUrl();
    str += "/users/";
    str += std::to_string(userId());
    return str;
}

} // namespace KDC

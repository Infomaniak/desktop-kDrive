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

#include "getrootfilelistjob.h"

namespace KDC {

GetRootFileListJob::GetRootFileListJob(int userDbId, int driveId, uint64_t page /*= 1*/, bool dirOnly /*= false*/)
    : AbstractTokenNetworkJob(ApiDrive, userDbId, 0, 0, driveId), _page(page), _dirOnly(dirOnly) {
    _httpMethod = Poco::Net::HTTPRequest::HTTP_GET;
}

GetRootFileListJob::GetRootFileListJob(int driveDbId, uint64_t page /*= 1*/, bool dirOnly /*= false*/)
    : AbstractTokenNetworkJob(ApiDrive, 0, 0, driveDbId, 0), _page(page), _dirOnly(dirOnly) {
    _httpMethod = Poco::Net::HTTPRequest::HTTP_GET;
}

GetRootFileListJob::~GetRootFileListJob() {}

std::string GetRootFileListJob::getSpecificUrl() {
    std::string str = AbstractTokenNetworkJob::getSpecificUrl();
    str += "/files";
    return str;
}

void GetRootFileListJob::setQueryParameters(Poco::URI &uri, bool &canceled) {
    uri.addQueryParameter("per_page", nbItemPerPage);
    if (_page > 1) {
        uri.addQueryParameter("page", std::to_string(_page));
    }
    if (_dirOnly) {
        uri.addQueryParameter("type[]", "dir");
    }
    if (_withPath) {
        uri.addQueryParameter("with", "path");
    }
    canceled = false;
}

}  // namespace KDC
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

#include "getinfouserjob.h"

#include "utility/jsonparserutility.h"

#include <Poco/Net/HTTPRequest.h>

namespace KDC {

static const std::string displayNameKey = "display_name";
static const std::string emailKey = "email";
static const std::string avatarKey = "avatar";
static const std::string isStaffKey = "is_staff";

GetInfoUserJob::GetInfoUserJob(const int userDbId) :
    AbstractTokenNetworkJob(ApiType::Profile, userDbId, 0, 0, 0) {
    _httpMethod = Poco::Net::HTTPRequest::HTTP_GET;
}

bool GetInfoUserJob::handleJsonResponse(std::istream &is) {
    if (!AbstractTokenNetworkJob::handleJsonResponse(is)) return false;

    Poco::JSON::Object::Ptr dataObj = jsonRes()->getObject(dataKey);
    if (!dataObj || dataObj->size() == 0) return false;


    if (!JsonParserUtility::extractValue(dataObj, displayNameKey, _name)) {
        _exitInfo = ExitCode::BackError;
        return false;
    }

    if (!JsonParserUtility::extractValue(dataObj, emailKey, _email)) {
        _exitInfo = ExitCode::BackError;
        return false;
    }

    if (!JsonParserUtility::extractValue(dataObj, avatarKey, _avatarUrl)) {
        _exitInfo = ExitCode::BackError;
        return false;
    }

    JsonParserUtility::extractValue(dataObj, isStaffKey, _isStaff, false);
    return true;
}

std::string GetInfoUserJob::getSpecificUrl() {
    std::string str = AbstractTokenNetworkJob::getSpecificUrl();
    return str;
}

} // namespace KDC

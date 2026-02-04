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

#include "getaccountinfojob.h"

#include "utility/jsonparserutility.h"

#include <Poco/Net/HTTPRequest.h>

namespace KDC {

GetAccountInfoJob::GetAccountInfoJob(const int userDbId, const uint64_t accountId) :
    AbstractTokenNetworkJob(ApiType::Profile, userDbId, 0, 0, 0),
    _accountId(accountId) {
    _httpMethod = Poco::Net::HTTPRequest::HTTP_GET;
    _apiVersion = 3;
}

ExitInfo GetAccountInfoJob::handleJsonResponse(const std::string &replyBody) {
    if (const auto exitInfo = AbstractTokenNetworkJob::handleJsonResponse(replyBody); !exitInfo) return exitInfo;

    Poco::JSON::Object::Ptr dataObj = jsonRes()->getObject(dataKey);
    if (!dataObj || dataObj->size() == 0) return {ExitCode::BackError, ExitCause::MissingReplyData};

    if (!JsonParserUtility::extractValue(dataObj, nameKey, _name)) {
        return {ExitCode::BackError, ExitCause::MissingReplyData};
    }
    return ExitCode::Ok;
}

std::string GetAccountInfoJob::getSpecificUrl() {
    return "/" + std::to_string(_accountId);
}

} // namespace KDC

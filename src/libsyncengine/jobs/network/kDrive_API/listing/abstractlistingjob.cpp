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

#include "abstractlistingjob.h"

#include <Poco/Net/HTTPRequest.h>

namespace KDC {

AbstractListingJob::AbstractListingJob(const int driveDbId, const NodeSet &blacklist /*= {}*/) :
    AbstractListingJob(ApiType::Drive, driveDbId, blacklist) {}

AbstractListingJob::AbstractListingJob(const ApiType apiType, const int driveDbId, const NodeSet &blacklist /*= {}*/) :
    AbstractTokenNetworkJob(apiType, 0, 0, driveDbId, 0),
    _blacklist(blacklist) {
    _httpMethod = Poco::Net::HTTPRequest::HTTP_GET;
}

void AbstractListingJob::setQueryParameters(Poco::URI &uri, bool &canceled) {
    setSpecificQueryParameters(uri);
    if (!_blacklist.empty()) {
        if (const std::string str = Utility::nodeSet2str(_blacklist); !str.empty()) {
            uri.addQueryParameter("without_ids", str);
        }
    }
    canceled = false;
}

bool AbstractListingJob::handleError(std::istream &is, const Poco::URI &uri) {
    if (_resHttp.getStatus() == Poco::Net::HTTPResponse::HTTP_FORBIDDEN) {
        // Access to the directory is forbidden or it doesn't exist
        _exitInfo = {ExitCode::InvalidSync, ExitCause::SyncDirAccessError};
        return true;
    } else {
        return AbstractTokenNetworkJob::handleError(is, uri);
    }
}

} // namespace KDC

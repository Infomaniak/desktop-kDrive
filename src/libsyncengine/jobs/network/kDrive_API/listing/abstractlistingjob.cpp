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

#include "abstractlistingjob.h"

#include <Poco/Net/HTTPRequest.h>

namespace KDC {


AbstractListingJob::AbstractListingJob(const DriveDbId driveDbId, RemoteNodeIdSet blacklist /*= {}*/) :
    AbstractListingJob(ApiType::Drive, driveDbId, std::move(blacklist)) {}

AbstractListingJob::AbstractListingJob(const ApiType apiType, const DriveDbId driveDbId, RemoteNodeIdSet blacklist /*= {}*/) :
    AbstractTokenNetworkJob(apiType, 0, driveDbId, 0),
    _blacklist(std::move(blacklist)) {
    _httpMethod = Poco::Net::HTTPRequest::HTTP_POST;
}

ExitInfo AbstractListingJob::setData() {
    if (_blacklist.empty()) return ExitCode::Ok;

    Poco::JSON::Object json;
    Poco::JSON::Array withoutIdsArray;
    for (const auto &id: _blacklist) {
        if (id.empty()) {
            assert(false);
            sentry::Handler::captureMessage(sentry::Level::Warning, "ID should not be NULL",
                                            "The IDs in `without_ids` should never be NULL.");
            LOG_WARN(_logger, "ID should not be NULL");
            continue;
        }
        (void) withoutIdsArray.add(id);
    }
    (void) json.set("without_ids", withoutIdsArray);

    std::stringstream ss;
    json.stringify(ss);
    _data = ss.str();

    return ExitCode::Ok;
}

ExitInfo AbstractListingJob::handleError(const std::string &replyBody, const Poco::URI &uri) {
    if (httpResponse().getStatus() == Poco::Net::HTTPResponse::HTTP_FORBIDDEN) {
        // Access to the directory is forbidden or it doesn't exist
        return {ExitCode::InvalidSync, ExitCause::SyncDirAccessError};
    }
    return AbstractTokenNetworkJob::handleError(replyBody, uri);
}

} // namespace KDC

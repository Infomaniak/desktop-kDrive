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

#include "jsonfullfilelistwithcursorjob.h"
#include "../../../../libcommonserver/utility/utility.h"

#include <Poco/JSON/Parser.h>

namespace KDC {

JsonFullFileListWithCursorJob::JsonFullFileListWithCursorJob(int driveDbId, const NodeId &dirId,
                                                             std::list<NodeId> blacklist /*= {}*/, bool zip /*= true*/)
    : AbstractTokenNetworkJob(ApiType::Drive, 0, 0, driveDbId, 0), _dirId(dirId), _blacklist(blacklist), _zip(zip) {
    _httpMethod = Poco::Net::HTTPRequest::HTTP_GET;

    if (_zip) {
        addRawHeader("Accept-Encoding", "gzip");
    }
}

std::string JsonFullFileListWithCursorJob::getSpecificUrl() {
    std::string str = AbstractTokenNetworkJob::getSpecificUrl();
    str += "/files/listing/full";
    return str;
}

void JsonFullFileListWithCursorJob::setQueryParameters(Poco::URI &uri, bool &canceled) {
    uri.addQueryParameter("directory_id", _dirId);
    uri.addQueryParameter("recursive", "true");
    uri.addQueryParameter("format", "json");
    if (!_blacklist.empty()) {
        uri.addQueryParameter("without_ids", Utility::list2str(_blacklist));
    }
    canceled = false;
}

bool JsonFullFileListWithCursorJob::handleResponse(std::istream &is) {
    if (_zip) {
        std::stringstream ss;
        unzip(is, ss);
        return AbstractTokenNetworkJob::handleJsonResponse(ss);
    } else {
        return AbstractTokenNetworkJob::handleJsonResponse(is);
    }
}

}  // namespace KDC

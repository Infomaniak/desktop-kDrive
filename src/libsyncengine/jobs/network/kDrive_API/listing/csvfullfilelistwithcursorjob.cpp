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

#include "csvfullfilelistwithcursorjob.h"

#if defined(KD_WINDOWS)
#include "reconciliation/platform_inconsistency_checker/platforminconsistencycheckerutility.h"
#endif

namespace KDC {

static const uint32_t apiTimout = 900;

CsvFullFileListWithCursorJob::CsvFullFileListWithCursorJob(const int driveDbId, const NodeId &dirId,
                                                           const NodeSet &blacklist /*= {}*/, const bool zip /*= true*/) :
    AbstractListingJob(driveDbId, blacklist),
    _dirId(dirId),
    _zip(zip),
    _snapshotItemHandler(_logger) {
    _customTimeout = apiTimout + 15;

    if (_zip) {
        addRawHeader("Accept-Encoding", "gzip");
    }
}

bool CsvFullFileListWithCursorJob::getItem(SnapshotItem &item, bool &error, bool &ignore, bool &eof) {
    error = false;
    ignore = false;

    return _snapshotItemHandler.getItem(item, _ss, error, ignore, eof);
}

std::string CsvFullFileListWithCursorJob::getCursor() {
    return httpResponse().get("X-kDrive-Cursor", "");
}

std::string CsvFullFileListWithCursorJob::getSpecificUrl() {
    std::string str = AbstractTokenNetworkJob::getSpecificUrl();
    str += "/files/listing/full";
    return str;
}

void CsvFullFileListWithCursorJob::setQueryParameters(Poco::URI &uri) {
    uri.addQueryParameter("directory_id", _dirId);
    uri.addQueryParameter("recursive", "true");
    uri.addQueryParameter("format", "safe_csv");
    uri.addQueryParameter("with", "files.is_link");
}

ExitInfo CsvFullFileListWithCursorJob::handleResponse(std::istream &is) {
    if (_zip) {
        unzip(is, _ss);
    } else {
        _ss << is.rdbuf();
    }

    // Check that the stringstream is not empty (network issues)
    _ss.seekg(0, std::ios_base::end);
    const auto length = _ss.tellg();
    if (length == 0) {
        LOG_ERROR(_logger, "Reply " << jobId() << " received with empty content.");
        return {};
    }

    _ss.seekg(0, std::ios_base::beg);
    if (isExtendedLog()) {
        LOGW_DEBUG(_logger,
                   L"Reply " << jobId() << L" received - length=" << length << L" value=" << CommonUtility::s2ws(_ss.str()));
    }
    return ExitCode::Ok;
}

} // namespace KDC

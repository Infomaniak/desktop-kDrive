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
#include "jobs/network/kDrive_API/apitranslator.h"

#include "jobs/network/jobexceptions.h"

#if defined(KD_WINDOWS)
#include "reconciliation/platform_inconsistency_checker/platforminconsistencycheckerutility.h"
#endif

namespace KDC {

static const uint32_t apiTimout = 900;


CsvFullFileListWithCursorJob::CsvFullFileListWithCursorJob(const DriveDbId driveDbId, RemoteNodeId remoteDirId,
                                                           const RemoteNodeIdSet &blacklist /*= {}*/, const bool zip /*= true*/) :
    AbstractListingJob(driveDbId, blacklist),
    _remoteDirId(std::move(remoteDirId)),
    _zip(zip),
    _snapshotItemHandler(driveDbId, _logger) {
    _customTimeout = apiTimout + 15;

    if (const auto exitInfo = ApiTranslator::translateV2ToV3(driveDbId, _remoteDirId); !exitInfo) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ApiTranslator::translateV2ToV3: " << exitInfo);
        throw JobException("Translation error in CsvFullFileListWithCursorJob::CsvFullFileListWithCursorJob.");
    }

    if (_zip) addRawHeader("Accept-Encoding", "gzip");
}

bool CsvFullFileListWithCursorJob::getItem(RemoteSnapshotItem &item, bool &error, bool &ignore, bool &eof) {
    error = false;
    ignore = false;

    return _snapshotItemHandler.getItem(item, _ss, error, ignore, eof);
}

std::string CsvFullFileListWithCursorJob::getCursor() {
    return httpResponse().get("X-kDrive-Cursor", "");
}

std::string CsvFullFileListWithCursorJob::getSpecificUrl() {
    std::string str = AbstractTokenNetworkJob::getSpecificUrl();
    str += "/files/";
    str += _remoteDirId;
    str += "/listing/full";

    return str;
}

std::string CsvFullFileListWithCursorJob::contentType() {
    return mimeTypeJson;
}

std::string CsvFullFileListWithCursorJob::acceptHeader() {
    return mimeTypeTextCsv + "," + mimeTypeJson;
}

void CsvFullFileListWithCursorJob::setQueryParameters(Poco::URI &uri) {
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
        return {ExitCode::BackError, ExitCause::FullListParsingError};
    }

    _ss.seekg(0, std::ios_base::beg);
    if (isExtendedLog()) {
        LOGW_DEBUG(_logger,
                   L"Reply " << jobId() << L" received - length=" << length << L" value=" << CommonUtility::s2ws(_ss.str()));
    }

    return ExitCode::Ok;
}

} // namespace KDC

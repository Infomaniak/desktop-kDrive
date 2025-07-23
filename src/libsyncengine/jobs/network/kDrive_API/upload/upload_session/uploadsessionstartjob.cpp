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

#include "uploadsessionstartjob.h"
#include "libcommonserver/utility/utility.h"

#include <Poco/Net/HTTPRequest.h>

namespace KDC {

UploadSessionStartJob::UploadSessionStartJob(const UploadSessionType uploadType, const int driveDbId, const SyncName &filename,
                                             const uint64_t size, const NodeId &remoteParentDirId, const uint64_t totalChunks) :
    AbstractUploadSessionJob(uploadType, driveDbId),
    _filename(filename),
    _totalSize(size),
    _remoteParentDirId(remoteParentDirId),
    _totalChunks(totalChunks),
    _uploadType(uploadType) {
    _httpMethod = Poco::Net::HTTPRequest::HTTP_POST;
}

UploadSessionStartJob::UploadSessionStartJob(const UploadSessionType uploadType, const int driveDbId, const NodeId &fileId,
                                             const uint64_t size, const uint64_t totalChunks) :
    UploadSessionStartJob(uploadType, driveDbId, SyncName(), size, "", totalChunks) {
    _fileId = fileId;
}

UploadSessionStartJob::UploadSessionStartJob(const UploadSessionType uploadType, const SyncName &filename, const uint64_t size,
                                             const uint64_t totalChunks) :
    UploadSessionStartJob(uploadType, 0, filename, size, "", totalChunks) {}

std::string UploadSessionStartJob::getSpecificUrl() {
    std::string str = AbstractTokenNetworkJob::getSpecificUrl();
    str += "/upload/session/start";
    return str;
}

ExitInfo UploadSessionStartJob::setData() {
    Poco::JSON::Object json;

    switch (_uploadType) {
        case UploadSessionType::Drive: {
            if (_fileId.empty()) {
                (void) json.set("file_name", _filename);
                (void) json.set("directory_id", _remoteParentDirId);
                // If an item already exists on the remote side with the same name, we want the backend to return an error.
                // However, in case of conflict with a directory, the backend will change the error resolution to `rename` and
                // automatically rename the uploaded file with a suffix counter (e.g.: test (1).txt)
                (void) json.set(conflictKey, conflictErrorValue);
            } else {
                (void) json.set("file_id", _fileId);
            }
            break;
        }
        case UploadSessionType::Log: {
            using namespace std::chrono;
            const auto timestamp = duration_cast<seconds>(time_point_cast<seconds>(system_clock::now()).time_since_epoch());
            (void) json.set("last_modified_at", timestamp.count());
            (void) json.set("file_name", _filename);
            break;
        }
        default: {
            LOGW_FATAL(_logger, L"Unknown upload type")
            break;
        }
    }

    (void) json.set("total_size", std::to_string(_totalSize));
    (void) json.set("total_chunks", std::to_string(_totalChunks));

    std::stringstream ss;
    json.stringify(ss);
    _data = ss.str();
    return ExitCode::Ok;
}

} // namespace KDC

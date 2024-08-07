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

#include "uploadsessionstartjob.h"
#include "libcommonserver/utility/utility.h"

namespace KDC {

UploadSessionStartJob::UploadSessionStartJob(UploadSessionType uploadType, int driveDbId, const SyncName &filename, uint64_t size,
                                             const NodeId &remoteParentDirId, uint64_t totalChunks)
    : AbstractUploadSessionJob(uploadType, driveDbId),
      _filename(filename),
      _totalSize(size),
      _remoteParentDirId(remoteParentDirId),
      _totalChunks(totalChunks),
      _uploadType(uploadType) {
    _httpMethod = Poco::Net::HTTPRequest::HTTP_POST;
}

UploadSessionStartJob::UploadSessionStartJob(UploadSessionType uploadType, int driveDbId, const NodeId &fileId, uint64_t size,
                                             uint64_t totalChunks)
    : UploadSessionStartJob(uploadType, driveDbId, SyncName(), size, "", totalChunks) {
    _fileId = fileId;
}

UploadSessionStartJob::UploadSessionStartJob(UploadSessionType uploadType, const SyncName &filename, uint64_t size,
                                             uint64_t totalChunks)
    : UploadSessionStartJob(uploadType, 0, filename, size, "", totalChunks) {}

std::string UploadSessionStartJob::getSpecificUrl() {
    std::string str = AbstractTokenNetworkJob::getSpecificUrl();
    str += "/upload/session/start";
    return str;
}

void UploadSessionStartJob::setData(bool &canceled) {
    Poco::JSON::Object json;
    using namespace std::chrono;
    auto timestamp = duration_cast<seconds>(time_point_cast<seconds>(system_clock::now()).time_since_epoch());

    switch (_uploadType) {
        case UploadSessionType::Standard:
            if (_fileId.empty()) {
                json.set("file_name", _filename);
                json.set("directory_id", _remoteParentDirId);
                json.set("conflict", "version");
            } else {
                json.set("file_id", _fileId);
            }
            break;
        case UploadSessionType::LogUpload:
            json.set("last_modified_at", timestamp.count());
            json.set("file_name", _filename);
            break;
        default:
            LOGW_FATAL(_logger, L"Unknown upload type");
            break;
    }

    json.set("total_size", std::to_string(_totalSize));
    json.set("total_chunks", std::to_string(_totalChunks));

    std::stringstream ss;
    json.stringify(ss);
    _data = ss.str();
    canceled = false;
}

}  // namespace KDC

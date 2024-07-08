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

UploadSessionStartJob::UploadSessionStartJob(int driveDbId, const SyncName &filename, uint64_t size,
                                             const NodeId &remoteParentDirId, uint64_t totalChunks)
    : AbstractUploadSessionJob(driveDbId),
      _filename(filename),
      _totalSize(size),
      _remoteParentDirId(remoteParentDirId),
      _totalChunks(totalChunks) {
    _httpMethod = Poco::Net::HTTPRequest::HTTP_POST;
}

UploadSessionStartJob::UploadSessionStartJob(int driveDbId, const NodeId &fileId, uint64_t size, uint64_t totalChunks)
    : UploadSessionStartJob(driveDbId, SyncName(), size, "", totalChunks) {
    _fileId = fileId;
}

UploadSessionStartJob::~UploadSessionStartJob() {
    // if (_vfsForceStatus) {
    //     if (!_vfsForceStatus(_filePath, true, 0, true)) {
    //         LOGW_WARN(_logger, L"Error in vfsForceStatus for path=" << Path2WStr(_filePath).c_str());
    //     }
    // }
}

std::string UploadSessionStartJob::getSpecificUrl() {
    std::string str = AbstractTokenNetworkJob::getSpecificUrl();
    str += "/upload/session/start";
    return str;
}

void UploadSessionStartJob::setData(bool &canceled) {
    Poco::JSON::Object json;
    if (_fileId.empty()) {
        json.set("file_name", _filename);
        json.set("directory_id", _remoteParentDirId);
        json.set("conflict", "version");
    } else {
        json.set("file_id", _fileId);
    }
    json.set("total_size", std::to_string(_totalSize));
    json.set("total_chunks", std::to_string(_totalChunks));

    std::stringstream ss;
    json.stringify(ss);
    _data = ss.str();
    canceled = false;
}

}  // namespace KDC

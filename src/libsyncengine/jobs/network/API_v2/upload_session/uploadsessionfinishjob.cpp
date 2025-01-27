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

#include "uploadsessionfinishjob.h"
#include "libcommonserver/utility/utility.h"
#include "utility/jsonparserutility.h"

namespace KDC {

UploadSessionFinishJob::UploadSessionFinishJob(const std::shared_ptr<Vfs> &vfs, UploadSessionType uploadType, int driveDbId,
                                               const SyncPath &filepath, const std::string &sessionToken,
                                               const std::string &totalChunkHash, uint64_t totalChunks, SyncTime modtime) :
    AbstractUploadSessionJob(uploadType, driveDbId, filepath, sessionToken), _totalChunkHash(totalChunkHash),
    _totalChunks(totalChunks), _modtimeIn(modtime), _vfs(vfs) {
    _httpMethod = Poco::Net::HTTPRequest::HTTP_POST;
}

UploadSessionFinishJob::UploadSessionFinishJob(UploadSessionType uploadType, const SyncPath &filepath,
                                               const std::string &sessionToken, const std::string &totalChunkHash,
                                               uint64_t totalChunks, SyncTime modtime) :
    UploadSessionFinishJob(nullptr, uploadType, 0, filepath, sessionToken, totalChunkHash, totalChunks, modtime) {}

UploadSessionFinishJob::~UploadSessionFinishJob() {
    if (!_vfs) return;
    if (const ExitInfo exitInfo = _vfs->forceStatus(_filePath, false, 0, true); !exitInfo) {
        LOGW_WARN(_logger, L"Error in vfsForceStatus for path=" << Path2WStr(_filePath) << L" : " << exitInfo);
    }
}

bool UploadSessionFinishJob::handleResponse(std::istream &is) {
    if (!AbstractTokenNetworkJob::handleResponse(is)) {
        return false;
    }

    if (jsonRes()) {
        Poco::JSON::Object::Ptr dataObj = jsonRes()->getObject(dataKey);
        if (dataObj) {
            Poco::JSON::Object::Ptr fileObj = dataObj->getObject(fileKey);
            if (fileObj) {
                if (!JsonParserUtility::extractValue(fileObj, idKey, _nodeId)) {
                    return false;
                }
                if (!JsonParserUtility::extractValue(fileObj, lastModifiedAtKey, _modtimeOut)) {
                    return false;
                }
            }
        }
    }

    return true;
}

std::string UploadSessionFinishJob::getSpecificUrl() {
    std::string str = AbstractTokenNetworkJob::getSpecificUrl();
    str += "/upload/session/";
    str += _sessionToken;
    str += "/finish";
    return str;
}

ExitInfo UploadSessionFinishJob::setData() {
    Poco::JSON::Object json;
    json.set("total_chunk_hash", "xxh3:" + _totalChunkHash);
    json.set("total_chunks", _totalChunks);
    json.set(lastModifiedAtKey, _modtimeIn);

    std::stringstream ss;
    json.stringify(ss);
    _data = ss.str();
    return ExitCode::Ok;
}

} // namespace KDC

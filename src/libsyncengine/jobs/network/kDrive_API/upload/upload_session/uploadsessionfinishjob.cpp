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

#include "uploadsessionfinishjob.h"

#include "jobs/network/kDrive_API/upload/uploadjobreplyhandler.h"
#include "libcommonserver/utility/utility.h"
#include "libcommonserver/utility/jsonparserutility.h"

#include <Poco/Net/HTTPRequest.h>

namespace KDC {

UploadSessionFinishJob::UploadSessionFinishJob(const UploadSessionType uploadType,
                                               const DriveDbId driveDbId, const SyncPath &absoluteFilePath,
                                               const std::string &sessionToken, const std::string &totalChunkHash,
                                               const uint64_t totalChunks, const SyncTime creationTime,
                                               SyncTime modificationTime) :
    AbstractUploadSessionJob(uploadType, driveDbId, absoluteFilePath, sessionToken),
    _totalChunkHash(totalChunkHash),
    _totalChunks(totalChunks),
    _creationTimeIn(creationTime),
    _modificationTimeIn(modificationTime) {
    _httpMethod = Poco::Net::HTTPRequest::HTTP_POST;
}

UploadSessionFinishJob::UploadSessionFinishJob(const UploadSessionType uploadType, const SyncPath &absoluteFilePath,
                                               const std::string &sessionToken, const std::string &totalChunkHash,
                                               const uint64_t totalChunks, const SyncTime creationTime,
                                               SyncTime modificationTime) :
    UploadSessionFinishJob(uploadType, 0, absoluteFilePath, sessionToken, totalChunkHash, totalChunks, creationTime,
                           modificationTime) {}


ExitInfo UploadSessionFinishJob::handleResponse(std::istream &is) {
    if (const auto exitInfo = AbstractTokenNetworkJob::handleResponse(is); !exitInfo) {
        return exitInfo;
    }

    if (getApiType() == ApiType::Drive) {
        UploadJobReplyHandler replyHandler(_absoluteFilePath, false, _creationTimeIn, _modificationTimeIn);
        if (!replyHandler.extractData(jsonRes())) return {};
        _nodeId = replyHandler.nodeId();
        _creationTimeOut = replyHandler.creationTime();
        _modificationTimeOut = replyHandler.modificationTime();
        _sizeOut = replyHandler.size();
    }

    return ExitCode::Ok;
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
    (void) json.set("total_chunk_hash", "xxh3:" + _totalChunkHash);
    (void) json.set("total_chunks", _totalChunks);
    (void) json.set(createdAtKey, _creationTimeIn);
    (void) json.set(lastModifiedAtKey, _modificationTimeIn);

    std::stringstream ss;
    json.stringify(ss);
    _data = ss.str();
    return ExitCode::Ok;
}

} // namespace KDC

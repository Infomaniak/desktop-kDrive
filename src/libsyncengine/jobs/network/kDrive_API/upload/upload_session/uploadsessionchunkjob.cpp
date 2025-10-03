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

#include "uploadsessionchunkjob.h"
#include "libcommonserver/utility/utility.h"

#include <Poco/Net/HTTPRequest.h>

#define TRIALS 5

namespace KDC {

UploadSessionChunkJob::UploadSessionChunkJob(UploadSessionType uploadType, int driveDbId, const SyncPath &filepath,
                                             const std::string &sessionToken, const std::string &chunkContent, uint64_t chunkNb,
                                             uint64_t chunkSize, UniqueId sessionJobId) :
    AbstractUploadSessionJob(uploadType, driveDbId, filepath, sessionToken),
    _chunkNb(chunkNb),
    _chunkSize(chunkSize),
    _sessionJobId(sessionJobId) {
    _httpMethod = Poco::Net::HTTPRequest::HTTP_POST;
    _customTimeout = 60;
    _trials = TRIALS;

    _data = chunkContent;
    _chunkHash = Utility::computeXxHash(_data);
}

UploadSessionChunkJob::UploadSessionChunkJob(UploadSessionType uploadType, const SyncPath &filepath,
                                             const std::string &sessionToken, const std::string &chunkContent, uint64_t chunkNb,
                                             uint64_t chunkSize, UniqueId sessionJobId) :
    UploadSessionChunkJob(uploadType, 0, filepath, sessionToken, chunkContent, chunkNb, chunkSize, sessionJobId) {}

UploadSessionChunkJob::~UploadSessionChunkJob() {}

std::string UploadSessionChunkJob::getSpecificUrl() {
    std::string str = AbstractTokenNetworkJob::getSpecificUrl();
    str += "/upload/session/";
    str += _sessionToken;
    str += "/chunk";
    return str;
}

std::string UploadSessionChunkJob::getContentType() {
    return mimeTypeOctetStream;
}

void UploadSessionChunkJob::setQueryParameters(Poco::URI &uri) {
    uri.addQueryParameter("chunk_hash", "xxh3:" + _chunkHash);
    uri.addQueryParameter("chunk_number", std::to_string(_chunkNb));
    uri.addQueryParameter("chunk_size", std::to_string(_chunkSize));
}

} // namespace KDC

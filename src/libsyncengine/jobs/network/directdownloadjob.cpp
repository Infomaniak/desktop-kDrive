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

#include "directdownloadjob.h"
#include "log/log.h"
#include "io/iohelper.h"
#include "utility/utility.h"

#include <Poco/Net/HTTPRequest.h>
#include <fstream>

namespace KDC {

constexpr uint64_t bufferSize = 4096 * 1000; // 4MB     // TODO : this should be defined in a common parent class

DirectDownloadJob::DirectDownloadJob(const SyncPath &destinationFile, const std::string &url, const bool headerOnly /*= false*/) :
    _destinationFile(destinationFile),
    _url(url) {
    _httpMethod = headerOnly ? Poco::Net::HTTPRequest::HTTP_HEAD : Poco::Net::HTTPRequest::HTTP_GET;
}

ExitInfo DirectDownloadJob::handleResponse(std::istream &is) {
    if (_httpMethod == Poco::Net::HTTPRequest::HTTP_HEAD) return ExitCode::Ok;
    std::ofstream output(_destinationFile.native().c_str(), std::ios::binary);
    if (!output) {
        LOGW_WARN(_logger, L"Failed to create file: " << Utility::formatSyncPath(_destinationFile));
        return {ExitCode::SystemError,
                Utility::enoughSpace(_destinationFile) ? ExitCause::FileAccessError : ExitCause::NotEnoughDiskSpace};
    }

    ExitInfo exitInfo = readFromStream(is, output);

    output.close();
    if (output.bad()) {
        // Read/writing error or logical error
        LOG_WARN(_logger, "Request " << jobId() << ": error after closing tmp file");
    }

    return exitInfo;
}

ExitInfo DirectDownloadJob::handleError(const std::string &replyBody, const Poco::URI &uri) {
    (void) replyBody;
    const auto errorCode = std::to_string(httpResponse().getStatus());
    LOG_WARN(_logger,
             "Download " << uri.toString() << " failed with error: " << errorCode << " - " << httpResponse().getReason());
    return {};
}

ExitInfo DirectDownloadJob::readFromStream(std::istream &is, std::ofstream &output) {
    const auto expectedSize = httpResponse().getContentLength();
    if (expectedSize != Poco::Net::HTTPMessage::UNKNOWN_CONTENT_LENGTH && expectedSize == 0) {
        LOG_WARN(_logger, "Reply " << jobId() << " is empty");
        return ExitCode::Ok;
    }

    setProgress(0);

    const std::unique_ptr<char[]> buffer(new char[bufferSize]);
    while (true) {
        if (isAborted()) {
            LOG_DEBUG(_logger, "Request " << jobId() << ": aborted");
            return ExitCode::Ok;
        }

        (void) is.read(buffer.get(), bufferSize);
        if (is.bad() && !is.fail()) {
            // Read/writing error and not logical error
            LOG_WARN(_logger, "Request " << jobId() << ": error after reading " << getProgress() << " bytes from input stream");
            return ExitCode::SystemError;
        }

        const auto readSize = is.gcount();
        addProgress(readSize);
        LOG_DEBUG(_logger, "Request " << jobId() << ": " << getProgress() << " bytes read");
        if (readSize > 0) {
            (void) output.write(buffer.get(), readSize);
            if (output.bad()) {
                // Read/writing error or logical error
                LOG_WARN(_logger, "Request " << jobId() << ": error after writing " << getProgress() << " bytes to tmp file");
                return ExitCode::SystemError;
            }
            (void) output.flush();
            if (output.bad()) {
                // Read/writing error or logical error
                LOG_WARN(_logger, "Request " << jobId() << ": error after flushing " << getProgress() << " bytes to tmp file");
                return ExitCode::SystemError;
            }
        }

        if (is.eof()) {
            // End of stream
            if (expectedSize == Poco::Net::HTTPMessage::UNKNOWN_CONTENT_LENGTH || getProgress() == expectedSize) {
                return ExitCode::Ok;
            }

            // Expected size hasn't been read
            LOG_WARN(_logger, "Request " << jobId() << ": eof after reading " << getProgress() << " bytes from input stream");
            return ExitCode::SystemError;
        }
    }

    return ExitCode::Ok;
}

} // namespace KDC

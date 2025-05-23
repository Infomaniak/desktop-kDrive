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

#define BUF_SIZE 4096 * 1000 // 4MB     // TODO : this should be defined in a common parent class

DirectDownloadJob::DirectDownloadJob(const SyncPath& destinationFile, const std::string& url) :
    _destinationFile(destinationFile),
    _url(url) {
    _httpMethod = Poco::Net::HTTPRequest::HTTP_GET;
}

bool DirectDownloadJob::handleResponse(std::istream& is) {
    std::ofstream output(_destinationFile.native().c_str(), std::ios::binary);
    if (!output) {
        LOGW_WARN(_logger, L"Failed to create file: " << Utility::formatSyncPath(_destinationFile));
        _exitInfo = {ExitCode::SystemError,
                     Utility::enoughSpace(_destinationFile) ? ExitCause::FileAccessError : ExitCause::NotEnoughDiskSpace};
        return false;
    }

    setProgress(0);
    if (std::streamsize expectedSize = _resHttp.getContentLength();
        expectedSize == Poco::Net::HTTPMessage::UNKNOWN_CONTENT_LENGTH || expectedSize > 0) {
        std::unique_ptr<char[]> buffer(new char[BUF_SIZE]);
        bool done = false;
        while (!done) {
            if (isAborted()) {
                LOG_DEBUG(_logger, "Request " << jobId() << ": aborted");
                break;
            }

            is.read(buffer.get(), BUF_SIZE);
            if (is.bad() && !is.fail()) {
                // Read/writing error and not logical error
                LOG_WARN(_logger,
                         "Request " << jobId() << ": error after reading " << getProgress() << " bytes from input stream");
                break;
            }

            std::streamsize readSize = is.gcount();
            addProgress(readSize);
            LOG_DEBUG(_logger, "Request " << jobId() << ": " << getProgress() << " bytes read");
            if (readSize > 0) {
                output.write(buffer.get(), readSize);
                if (output.bad()) {
                    // Read/writing error or logical error
                    LOG_WARN(_logger, "Request " << jobId() << ": error after writing " << getProgress() << " bytes to tmp file");
                    break;
                }
                output.flush();
                if (output.bad()) {
                    // Read/writing error or logical error
                    LOG_WARN(_logger,
                             "Request " << jobId() << ": error after flushing " << getProgress() << " bytes to tmp file");
                    break;
                }
            } else {
                if (is.eof()) {
                    // End of stream
                    if (expectedSize == Poco::Net::HTTPMessage::UNKNOWN_CONTENT_LENGTH || getProgress() == expectedSize) {
                        done = true;
                    } else {
                        // Expected size hasn't been read
                        LOG_WARN(_logger,
                                 "Request " << jobId() << ": eof after reading " << getProgress() << " bytes from input stream");
                        break;
                    }
                } else {
                    // Expected size hasn't been read
                    LOG_WARN(_logger, "Request " << jobId() << ": nothing more to read but eof not reached");
                    break;
                }
            }
        }
    } else {
        LOG_WARN(_logger, "Reply " << jobId() << " is empty");
    }

    output.close();
    if (output.bad()) {
        // Read/writing error or logical error
        LOG_WARN(_logger, "Request " << jobId() << ": error after closing tmp file");
    }

    return true;
}

bool DirectDownloadJob::handleError(std::istream& inputStream, const Poco::URI& uri) {
    (void) inputStream;
    (void) uri;
    const auto errorCode = std::to_string(_resHttp.getStatus());
    LOG_WARN(_logger, "Download failed with error: " << errorCode << " - " << _resHttp.getReason());
    return false;
}

} // namespace KDC

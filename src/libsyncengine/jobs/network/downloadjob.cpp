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

#include "downloadjob.h"

#include "libcommonserver/io/filestat.h"
#include "libcommonserver/io/iohelper.h"
#include "libcommonserver/utility/utility.h"

#include "libcommon/utility/utility.h"
#include "common/utility.h"

#if defined(__APPLE__) || defined(__unix__)
#include <unistd.h>
#endif

#include <fstream>

#include <Poco/File.h>

namespace KDC {

#define BUF_SIZE 4096 * 1000  // 4MB
#define NOTIFICATION_DELAY 1  // 1 sec
#define TRIALS 5
#define READ_PAUSE_SLEEP_PERIOD 100  // 0.1 s
#define READ_RETRIES 10

DownloadJob::DownloadJob(int driveDbId, const NodeId &remoteFileId, const SyncPath &localpath, int64_t expectedSize,
                         SyncTime creationTime, SyncTime modtime, bool isCreate)
    : AbstractTokenNetworkJob(ApiDrive, 0, 0, driveDbId, 0, false),
      _remoteFileId(remoteFileId),
      _localpath(localpath),
      _expectedSize(expectedSize),
      _creationTime(creationTime),
      _modtimeIn(modtime),
      _isCreate(isCreate) {
    _httpMethod = Poco::Net::HTTPRequest::HTTP_GET;
    _customTimeout = 60;
    _trials = TRIALS;
}

DownloadJob::DownloadJob(int driveDbId, const NodeId &remoteFileId, const SyncPath &localpath, int64_t expectedSize)
    : AbstractTokenNetworkJob(ApiDrive, 0, 0, driveDbId, 0, false),
      _remoteFileId(remoteFileId),
      _localpath(localpath),
      _expectedSize(expectedSize),
      _ignoreDateTime(true) {
    _httpMethod = Poco::Net::HTTPRequest::HTTP_GET;
    _customTimeout = 60;
    _trials = TRIALS;
}

DownloadJob::~DownloadJob() {
    if (_responseHandlingCanceled) {
        if (_vfsSetPinState) {
            if (!_vfsSetPinState(_localpath, PinStateOnlineOnly)) {
                LOGW_WARN(_logger, L"Error in vfsSetPinState: " << Utility::formatSyncPath(_localpath).c_str());
            }
        }

        // TODO: usefull ?
        if (_vfsForceStatus) {
            if (!_vfsForceStatus(_localpath, false, 0, false)) {
                LOGW_WARN(_logger, L"Error in vfsForceStatus: " << Utility::formatSyncPath(_localpath).c_str());
            }
        }

        if (_vfsCancelHydrate) {
            if (!_vfsCancelHydrate(_localpath)) {
                LOGW_WARN(_logger, L"Error in vfsCancelHydrate: " << Utility::formatSyncPath(_localpath).c_str());
            }
        }
    } else {
        if (_vfsSetPinState) {
            if (!_vfsSetPinState(_localpath, _exitCode == ExitCodeOk ? PinStateAlwaysLocal : PinStateOnlineOnly)) {
                LOGW_WARN(_logger, L"Error in vfsSetPinState: " << Utility::formatSyncPath(_localpath).c_str());
            }
        }

        if (_vfsForceStatus) {
            if (!_vfsForceStatus(_localpath, false, 0, _exitCode == ExitCodeOk)) {
                LOGW_WARN(_logger, L"Error in vfsForceStatus: " << Utility::formatSyncPath(_localpath).c_str());
            }
        }
    }
}

std::string DownloadJob::getSpecificUrl() {
    std::string str = AbstractTokenNetworkJob::getSpecificUrl();
    str += "/files/";
    str += _remoteFileId;
    str += "/download";
    return str;
}

bool DownloadJob::canRun() {
    if (bypassCheck()) {
        return true;
    }

    // Check that we can create the item here
    bool exists;
    IoError ioError = IoErrorSuccess;
    if (!IoHelper::checkIfPathExists(_localpath, exists, ioError)) {
        LOGW_WARN(_logger, L"Error in IoHelper::checkIfPathExists: " << Utility::formatIoError(_localpath, ioError).c_str());
        _exitCode = ExitCodeSystemError;
        _exitCause = ExitCauseFileAccessError;
        return false;
    }

    if (_isCreate && exists) {
        LOGW_DEBUG(_logger, L"Item: " << Utility::formatSyncPath(_localpath).c_str()
                                      << L" already exist. Aborting current sync and restart.");
        _exitCode = ExitCodeNeedRestart;
        _exitCause = ExitCauseUnexpectedFileSystemEvent;
        return false;
    }

    return true;
}

void DownloadJob::runJob() noexcept {
    if (!_isCreate) {
        // Update size on file system
        if (_vfsUpdateMetadata) {
            FileStat filestat;
            IoError ioError = IoErrorSuccess;
            if (!IoHelper::getFileStat(_localpath, &filestat, ioError)) {
                LOGW_WARN(_logger, L"Error in IoHelper::getFileStat: " << Utility::formatIoError(_localpath, ioError).c_str());
                return;
            }

            if (ioError == IoErrorNoSuchFileOrDirectory) {
                LOGW_WARN(_logger, L"Item does not exist anymore: " << Utility::formatSyncPath(_localpath).c_str());
                return;
            } else if (ioError == IoErrorAccessDenied) {
                LOGW_WARN(_logger, L"Item misses search permission: " << Utility::formatSyncPath(_localpath).c_str());
                return;
            }

            std::string error;
            if (!_vfsUpdateMetadata(_localpath, filestat.creationTime, filestat.modtime, _expectedSize,
                                    std::to_string(filestat.inode), error)) {
                LOGW_WARN(_logger, L"Update metadata failed: " << Utility::formatSyncPath(_localpath).c_str());
                return;
            }
        }

        if (_vfsForceStatus) {
            if (!_vfsForceStatus(_localpath, true, 0, false)) {
                LOGW_WARN(_logger, L"Error in vfsForceStatus: " << Utility::formatSyncPath(_localpath).c_str());
                return;
            }
        }
    }

    AbstractTokenNetworkJob::runJob();
}

bool DownloadJob::handleResponse(std::istream &is) {
    // Get Mime type
    std::string contentType;
    try {
        contentType = _resHttp.get("Content-Type");
    } catch (...) {
        // No Content-Type
    }

    std::string mimeType;
    if (!contentType.empty()) {
        std::vector<std::string> contentTypeElts = Utility::splitStr(contentType, ';');
        mimeType = contentTypeElts[0];
    }

    bool isLink = false;
    std::string linkData;
    if (mimeType == mimeTypeSymlink || mimeType == mimeTypeSymlinkFolder || mimeType == mimeTypeHardlink ||
        (mimeType == mimeTypeFinderAlias && OldUtility::isMac()) || (mimeType == mimeTypeJunction && OldUtility::isWindows())) {
        // Read link data
        getStringFromStream(is, linkData);
        isLink = true;
    }

    // Process download
    if (isLink) {
        // Create link
        LOG_DEBUG(_logger, "Create link: mimeType=" << mimeType.c_str());
        if (!createLink(mimeType, linkData)) {
            _exitCode = ExitCodeSystemError;
            _exitCause = ExitCauseFileAccessError;
            return false;
        }
    } else {
        // Create/fetch normal file
#ifdef _WIN32
        const std::string tmpFileName = tmpnam(nullptr);
#else
        const std::string tmpFileName = "kdrive_" + CommonUtility::generateRandomStringAlphaNum();
#endif

        SyncPath tmpPath;
        IoError ioError = IoErrorSuccess;
        if (!IoHelper::tempDirectoryPath(tmpPath, ioError)) {
            LOGW_WARN(_logger, L"Failed to get temporary directory path: " << Utility::formatIoError(tmpPath, ioError).c_str());
            _exitCode = ExitCodeSystemError;
            _exitCause = ExitCauseFileAccessError;
            return false;
        }

        tmpPath /= tmpFileName;

        std::ofstream output(tmpPath.native().c_str(), std::ios::binary);
        if (!output) {
            LOGW_WARN(_logger, L"Failed to create file: " << Utility::formatSyncPath(tmpPath).c_str());
            _exitCode = ExitCodeSystemError;
            _exitCause = Utility::enoughSpace(tmpPath) ? ExitCauseFileAccessError : ExitCauseNotEnoughDiskSpace;
            return false;
        }

        std::chrono::steady_clock::time_point fileProgressTimer = std::chrono::steady_clock::now();

        std::streamsize expectedSize = _resHttp.getContentLength();
        bool readError = false;
        bool writeError = false;
        bool fetchCanceled = false;
        bool fetchFinished = false;
        bool fetchError = false;
        _progress = 0;
        if (expectedSize == Poco::Net::HTTPMessage::UNKNOWN_CONTENT_LENGTH || expectedSize > 0) {
            std::unique_ptr<char[]> buffer(new char[BUF_SIZE]);
            bool done = false;
            int retryCount = 0;
            while (!done) {
                if (isAborted()) {
                    LOG_DEBUG(_logger, "Request " << jobId() << ": aborted");
                    break;
                }

                is.read(buffer.get(), BUF_SIZE);
                if (is.bad() && !is.fail()) {
                    // Read/writing error and not logical error
                    LOG_WARN(_logger,
                             "Request " << jobId() << ": error after reading " << _progress << " bytes from input stream");
                    readError = true;
                    break;
                } else {
                    std::streamsize readSize = is.gcount();
                    _progress += readSize;

                    if (readSize > 0) {
                        output.write(buffer.get(), readSize);
                        if (output.bad()) {
                            // Read/writing error or logical error
                            LOG_WARN(_logger,
                                     "Request " << jobId() << ": error after writing " << _progress << " bytes to tmp file");
                            writeError = true;
                            break;
                        }
                        output.flush();
                        if (output.bad()) {
                            // Read/writing error or logical error
                            LOG_WARN(_logger,
                                     "Request " << jobId() << ": error after flushing " << _progress << " bytes to tmp file");
                            writeError = true;
                            break;
                        }
                        retryCount = 0;
                    }

                    if (is.eof()) {
                        // End of stream
                        if (expectedSize == Poco::Net::HTTPMessage::UNKNOWN_CONTENT_LENGTH || _progress == expectedSize) {
                            done = true;
                        } else {
                            // Expected size hasn't be read
                            if (retryCount < READ_RETRIES) {
                                // Try to read again later
                                LOG_WARN(_logger, "Request " << jobId() << ": eof after reading " << _progress
                                                             << " bytes from input stream, retrying");
                                retryCount++;
                                Utility::msleep(READ_PAUSE_SLEEP_PERIOD);
                                continue;
                            } else {
                                LOG_WARN(_logger, "Request " << jobId() << ": eof after reading " << _progress
                                                             << " bytes from input stream");
                                readError = true;
                                break;
                            }
                        }
                    }
                }

                if (_vfsUpdateFetchStatus) {
                    std::chrono::duration<double> elapsed_seconds = std::chrono::steady_clock::now() - fileProgressTimer;
                    if (elapsed_seconds.count() > NOTIFICATION_DELAY || done) {
                        // Update fetch status
                        if (!_vfsUpdateFetchStatus(tmpPath, _localpath, _progress, fetchCanceled, fetchFinished)) {
                            LOGW_WARN(_logger, L"Error in vfsUpdateFetchStatus: " << Utility::formatSyncPath(_localpath).c_str());
                            fetchError = true;
                            break;
                        } else if (fetchCanceled) {
                            LOGW_WARN(_logger, L"Update fetch status canceled: " << Utility::formatSyncPath(_localpath).c_str());
                            break;
                        }
                        fileProgressTimer = std::chrono::steady_clock::now();
                    }
                }
            }
        }

        output.close();
        if (output.bad()) {
            // Read/writing error or logical error
            LOG_WARN(_logger, "Request " << jobId() << ": error after closing tmp file");
            writeError = true;
        }

        _responseHandlingCanceled = isAborted() || readError || writeError || fetchCanceled || fetchError;

        bool restartSync = false;
        if (!_responseHandlingCanceled) {
            if (_vfsUpdateFetchStatus && !fetchFinished) {
                // Update fetch status
                if (!_vfsUpdateFetchStatus(tmpPath, _localpath, _progress, fetchCanceled, fetchFinished)) {
                    LOGW_WARN(_logger, L"Error in vfsUpdateFetchStatus: " << Utility::formatSyncPath(_localpath).c_str());
                    fetchError = true;
                } else if (fetchCanceled) {
                    LOGW_WARN(_logger, L"Update fetch status canceled: " << Utility::formatSyncPath(_localpath).c_str());
                } else if (!fetchFinished) {
                    LOGW_WARN(_logger, L"Update fetch status not terminated: " << Utility::formatSyncPath(_localpath).c_str());
                }

                _responseHandlingCanceled = fetchCanceled || fetchError || (!fetchFinished);
            } else if (!_vfsUpdateFetchStatus) {
                // Replace file by tmp one
                bool replaceError = false;
                if (!moveTmpFile(tmpPath, restartSync)) {
                    LOGW_WARN(_logger, L"Failed to replace file by tmp one: " << Utility::formatSyncPath(tmpPath).c_str());
                    replaceError = true;
                }

                _responseHandlingCanceled = replaceError || restartSync;
            }
        }

        if (_responseHandlingCanceled) {
            // NB: VFS reset is done in the destructor

            // Remove tmp file
            if (!removeTmpFile(tmpPath)) {
                LOGW_WARN(_logger, L"Failed to remove tmp file: " << Utility::formatSyncPath(tmpPath).c_str());
            }

            if (isAborted() || fetchCanceled) {
                // Download aborted or canceled by the user
                _exitCode = ExitCodeOk;
                return true;
            } else if (readError) {
                // Download issue
                _exitCode = ExitCodeBackError;
                _exitCause = ExitCauseInvalidSize;
                return false;
            } else {
                // Fetch issue
                _exitCode = ExitCodeSystemError;
                _exitCause = ExitCauseFileAccessError;
                return false;
            }
        }
    }

    if (!_ignoreDateTime) {
        bool exists = false;
        if (!Utility::setFileDates(_localpath, std::make_optional<KDC::SyncTime>(_creationTime),
                                   std::make_optional<KDC::SyncTime>(_modtimeIn), isLink, exists)) {
            LOGW_WARN(_logger, L"Error in Utility::setFileDates: " << Utility::formatSyncPath(_localpath).c_str());
            // Do nothing (remote file will be updated during the next sync)
#ifdef NDEBUG
            sentry_capture_event(
                sentry_value_new_message_event(SENTRY_LEVEL_WARNING, "DownloadJob::handleResponse", "Unable to set file dates"));
#endif
        } else if (!exists) {
            LOGW_INFO(_logger, L"Item does not exist anymore. Restarting sync: " << Utility::formatSyncPath(_localpath).c_str());
            _exitCode = ExitCodeDataError;
            _exitCause = ExitCauseInvalidSnapshot;
            return false;
        }
    }

    // Retrieve inode
    FileStat filestat;
    IoError ioError = IoErrorSuccess;
    if (!IoHelper::getFileStat(_localpath, &filestat, ioError)) {
        LOGW_WARN(_logger, L"Error in IoHelper::getFileStat: " << Utility::formatIoError(_localpath, ioError).c_str());
        _exitCode = ExitCodeSystemError;
        _exitCause = ExitCauseFileAccessError;
        return false;
    }

    if (ioError == IoErrorNoSuchFileOrDirectory) {
        LOGW_WARN(_logger, L"Item does not exist anymore: " << Utility::formatSyncPath(_localpath).c_str());
        _exitCode = ExitCodeDataError;
        _exitCause = ExitCauseInvalidSnapshot;
        return false;
    } else if (ioError == IoErrorAccessDenied) {
        LOGW_WARN(_logger, L"Item misses search permission: " << Utility::formatSyncPath(_localpath).c_str());
        _exitCode = ExitCodeSystemError;
        _exitCause = ExitCauseNoSearchPermission;
        return false;
    }

    _localNodeId = std::to_string(filestat.inode);
    _exitCode = ExitCodeOk;

    return true;
}

bool DownloadJob::createLink(const std::string &mimeType, const std::string &data) {
    // Delete in case it already exists (EDIT operation)
    std::error_code ec;
    std::filesystem::remove_all(_localpath, ec);

    if (mimeType == mimeTypeSymlink || mimeType == mimeTypeSymlinkFolder) {
        // Create symlink
        const auto targetPath = Str2Path(data);
        if (targetPath == _localpath) {
            LOGW_DEBUG(_logger, L"Cannot create symlink on itself: " << Utility::formatSyncPath(_localpath).c_str());
            return false;
        }

        LOGW_DEBUG(_logger, L"Create symlink: " << Utility::formatSyncPath(targetPath).c_str() << L", "
                                                << Utility::formatSyncPath(_localpath).c_str());

        bool isFolder = mimeType == mimeTypeSymlinkFolder;
        IoError ioError = IoErrorSuccess;
        if (!IoHelper::createSymlink(targetPath, _localpath, isFolder, ioError)) {
            LOGW_WARN(_logger, L"Failed to create symlink: " << Utility::formatIoError(targetPath, ioError).c_str());
            return false;
        }
    } else if (mimeType == mimeTypeHardlink) {
        // Unreachable code
        const auto targetPath = Str2Path(data);
        if (targetPath == _localpath) {
            LOGW_DEBUG(_logger, L"Cannot create hardlink on itself: " << Utility::formatSyncPath(_localpath).c_str());
            return false;
        }

        LOGW_DEBUG(_logger, L"Create hardlink: target " << Utility::formatSyncPath(targetPath).c_str() << L", "
                                                        << Utility::formatSyncPath(_localpath).c_str());

        std::error_code ec;
        std::filesystem::create_hard_link(targetPath, _localpath, ec);
        if (ec) {
            LOGW_WARN(_logger, L"Failed to create hardlink: target " << Utility::formatSyncPath(targetPath).c_str() << L", "
                                                                     << Utility::formatSyncPath(_localpath).c_str() << L", "
                                                                     << Utility::formatStdError(ec).c_str());
            return false;
        }
    } else if (mimeType == mimeTypeJunction) {
#if defined(_WIN32)
        LOGW_DEBUG(_logger, L"Create junction: " << Utility::formatSyncPath(_localpath).c_str());

        IoError ioError = IoErrorSuccess;
        if (!IoHelper::createJunction(data, _localpath, ioError)) {
            LOGW_WARN(_logger, L"Failed to create junction: " << Utility::formatIoError(_localpath, ioError).c_str());
            return false;
        }
#endif
    } else if (mimeType == mimeTypeFinderAlias) {
#if defined(__APPLE__)
        LOGW_DEBUG(_logger, L"Create alias: " << Utility::formatSyncPath(_localpath).c_str());

        IoError ioError = IoErrorSuccess;
        if (!IoHelper::createAlias(data, _localpath, ioError)) {
            const std::wstring message = Utility::s2ws(IoHelper::ioError2StdString(ioError));
            LOGW_WARN(_logger, L"Failed to create alias: " << Utility::formatIoError(_localpath, ioError).c_str());

            return false;
        }
#endif
    } else {
        LOG_WARN(_logger, "Link type not managed: MIME type=" << mimeType.c_str());
        return false;
    }

    return true;
}

bool DownloadJob::removeTmpFile(const SyncPath &path) {
    std::error_code ec;
    if (!std::filesystem::remove_all(path, ec)) {
        if (ec) {
            LOGW_WARN(_logger, L"Failed to remove all: " << Utility::formatStdError(path, ec).c_str());
            return false;
        }

        LOGW_WARN(_logger, L"Failed to remove all: " << Utility::formatSyncPath(path).c_str());
        return false;
    }

    return true;
}

bool DownloadJob::moveTmpFile(const SyncPath &path, bool &restartSync) {
    restartSync = false;

    // Move downloaded file from tmp directory to sync directory
    std::error_code ec;
#ifdef _WIN32
    bool retry = true;
    int counter = 50;
    while (retry) {
        retry = false;
#endif
        std::filesystem::rename(path, _localpath, ec);
#ifdef _WIN32
        const bool crossDeviceLink = ec.value() == ERROR_NOT_SAME_DEVICE;
#else
    const bool crossDeviceLink = ec.value() == (int)std::errc::cross_device_link;
#endif
        if (crossDeviceLink) {
            // The sync might be on a different file system than tmp folder.
            // In that case, try to copy the file instead.
            ec.clear();
            std::filesystem::copy(path, _localpath, std::filesystem::copy_options::overwrite_existing, ec);
            if (ec) {
                LOGW_WARN(_logger, L"Failed to copy: " << Utility::formatSyncPath(_localpath).c_str() << L", "
                                                       << Utility::formatStdError(ec).c_str());
                return false;
            }
        }
#ifdef _WIN32
        else if (ec.value() == ERROR_SHARING_VIOLATION) {
            if (counter) {
                // Retry
                retry = true;
                Utility::msleep(10);
                LOGW_DEBUG(_logger, L"Retrying to move downloaded file: " << Utility::formatSyncPath(_localpath).c_str());
                counter--;
            } else {
                LOGW_WARN(_logger, L"Failed to rename: " << Utility::formatStdError(_localpath, ec).c_str());
                return false;
            }
        }
#endif
        else if (ec) {
            bool exists = false;
            IoError ioError = IoErrorSuccess;
            if (!IoHelper::checkIfPathExists(_localpath.parent_path(), exists, ioError)) {
                LOGW_WARN(_logger, L"Error in IoHelper::checkIfPathExists: "
                                       << Utility::formatIoError(_localpath.parent_path(), ioError).c_str());
                _exitCode = ExitCodeSystemError;
                _exitCause = ExitCauseFileAccessError;
                return false;
            }

            if (!exists) {
                LOGW_INFO(_logger, L"Parent of item does not exist anymore: " << Utility::formatStdError(_localpath, ec).c_str());
                restartSync = true;
                return true;
            }

            LOGW_WARN(_logger, L"Failed to rename: " << Utility::formatStdError(_localpath, ec).c_str());
            return false;
        }
#ifdef _WIN32
    }
#endif

    return true;
}

}  // namespace KDC

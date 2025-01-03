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

#define BUF_SIZE 4096 * 1000 // 4MB
#define NOTIFICATION_DELAY 1 // 1 sec
#define TRIALS 5
#define READ_PAUSE_SLEEP_PERIOD 100 // 0.1 s
#define READ_RETRIES 10

DownloadJob::DownloadJob(int driveDbId, const NodeId &remoteFileId, const SyncPath &localpath, int64_t expectedSize,
                         SyncTime creationTime, SyncTime modtime, bool isCreate) :
    AbstractTokenNetworkJob(ApiType::Drive, 0, 0, driveDbId, 0, false),
    _remoteFileId(remoteFileId), _localpath(localpath), _expectedSize(expectedSize), _creationTime(creationTime),
    _modtimeIn(modtime), _isCreate(isCreate) {
    _httpMethod = Poco::Net::HTTPRequest::HTTP_GET;
    _customTimeout = 60;
    _trials = TRIALS;
}

DownloadJob::DownloadJob(int driveDbId, const NodeId &remoteFileId, const SyncPath &localpath, int64_t expectedSize) :
    AbstractTokenNetworkJob(ApiType::Drive, 0, 0, driveDbId, 0, false), _remoteFileId(remoteFileId), _localpath(localpath),
    _expectedSize(expectedSize), _ignoreDateTime(true) {
    _httpMethod = Poco::Net::HTTPRequest::HTTP_GET;
    _customTimeout = 60;
    _trials = TRIALS;
}

DownloadJob::~DownloadJob() {
    if (_responseHandlingCanceled) {
        if (_vfsSetPinState) {
            if (!_vfsSetPinState(_localpath, PinState::OnlineOnly)) {
                LOGW_WARN(_logger, L"Error in vfsSetPinState: " << Utility::formatSyncPath(_localpath));
            }
        }

        // TODO: usefull ?
        if (_vfsForceStatus) {
            if (!_vfsForceStatus(_localpath, false, 0, false)) {
                LOGW_WARN(_logger, L"Error in vfsForceStatus: " << Utility::formatSyncPath(_localpath));
            }
        }

        if (_vfsCancelHydrate) {
            if (!_vfsCancelHydrate(_localpath)) {
                LOGW_WARN(_logger, L"Error in vfsCancelHydrate: " << Utility::formatSyncPath(_localpath));
            }
        }
    } else {
        if (_vfsSetPinState) {
            if (!_vfsSetPinState(_localpath, _exitCode == ExitCode::Ok ? PinState::AlwaysLocal : PinState::OnlineOnly)) {
                LOGW_WARN(_logger, L"Error in vfsSetPinState: " << Utility::formatSyncPath(_localpath));
            }
        }

        if (_vfsForceStatus) {
            if (!_vfsForceStatus(_localpath, false, 0, _exitCode == ExitCode::Ok)) {
                LOGW_WARN(_logger, L"Error in vfsForceStatus: " << Utility::formatSyncPath(_localpath));
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
    IoError ioError = IoError::Success;
    if (!IoHelper::checkIfPathExists(_localpath, exists, ioError)) {
        LOGW_WARN(_logger, L"Error in IoHelper::checkIfPathExists: " << Utility::formatIoError(_localpath, ioError));
        _exitCode = ExitCode::SystemError;
        _exitCause = ExitCause::FileAccessError;
        return false;
    }

    if (_isCreate && exists) {
        LOGW_DEBUG(_logger,
                   L"Item with " << Utility::formatSyncPath(_localpath) << L" already exists. Aborting current sync and restarting.");
        _exitCode = ExitCode::NeedRestart;
        _exitCause = ExitCause::UnexpectedFileSystemEvent;
        return false;
    }

    return true;
}

void DownloadJob::runJob() noexcept {
    if (!_isCreate) {
        // Update size on file system
        if (_vfsUpdateMetadata) {
            FileStat filestat;
            IoError ioError = IoError::Success;
            if (!IoHelper::getFileStat(_localpath, &filestat, ioError)) {
                LOGW_WARN(_logger, L"Error in IoHelper::getFileStat: " << Utility::formatIoError(_localpath, ioError));
                return;
            }

            if (ioError == IoError::NoSuchFileOrDirectory) {
                LOGW_WARN(_logger, L"Item does not exist anymore: " << Utility::formatSyncPath(_localpath));
                return;
            } else if (ioError == IoError::AccessDenied) {
                LOGW_WARN(_logger, L"Item misses search permission: " << Utility::formatSyncPath(_localpath));
                return;
            }

            std::string error;
            if (!_vfsUpdateMetadata(_localpath, filestat.creationTime, filestat.modtime, _expectedSize,
                                    std::to_string(filestat.inode), error)) {
                LOGW_WARN(_logger, L"Update metadata failed: " << Utility::formatSyncPath(_localpath));
                return;
            }
        }

        if (_vfsForceStatus) {
            if (!_vfsForceStatus(_localpath, true, 0, false)) {
                LOGW_WARN(_logger, L"Error in vfsForceStatus: " << Utility::formatSyncPath(_localpath));
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
        if (!createLink(mimeType, linkData)) { // We consider this as a permission denied error
            _exitCode = ExitCode::SystemError;
            _exitCause = ExitCause::FileAccessError;
            return false;
        }
    } else {
        SyncPath tmpDirectoryPath;
        IoError ioError = IoError::Success;
        if (!IoHelper::tempDirectoryPath(tmpDirectoryPath, ioError)) {
            LOGW_WARN(_logger, L"Failed to get temporary directory path: " << Utility::formatIoError(tmpDirectoryPath, ioError));
            _exitCode = ExitCode::SystemError;
            _exitCause = ExitCause::Unknown;
            return false;
        }

        SyncPath tmpPath;
        std::ofstream output;
        do {
            // Create/fetch normal file
#ifdef _WIN32
            const std::string tmpFileName = tmpnam(nullptr);
#else
            const std::string tmpFileName = "kdrive_" + CommonUtility::generateRandomStringAlphaNum();
#endif

            tmpPath = tmpDirectoryPath / tmpFileName;

            output.open(tmpPath.native().c_str(), std::ofstream::out | std::ofstream::binary);
            if (!output.is_open()) {
                LOGW_WARN(_logger, L"Failed to open tmp file: " << Utility::formatSyncPath(tmpPath));
                _exitCode = ExitCode::SystemError;
                _exitCause = Utility::enoughSpace(tmpPath) ? ExitCause::FileAccessError : ExitCause::NotEnoughDiskSpace;
                return false;
            }

            output.seekp(0, std::ios_base::end);
        } while (output.tellp() > 0); // If the file is not empty, generate a new file name

        std::chrono::steady_clock::time_point fileProgressTimer = std::chrono::steady_clock::now();

        std::streamsize expectedSize = _resHttp.getContentLength();
        bool readError = false;
        bool writeError = false;
        bool fetchCanceled = false;
        bool fetchFinished = false;
        bool fetchError = false;
        setProgress(0);
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
                             "Request " << jobId() << ": error after reading " << getProgress() << " bytes from input stream");
                    readError = true;
                    break;
                } else {
                    std::streamsize readSize = is.gcount();
                    addProgress(readSize);

                    if (readSize > 0) {
                        output.write(buffer.get(), readSize);
                        if (output.bad()) {
                            // Read/writing error or logical error
                            LOG_WARN(_logger,
                                     "Request " << jobId() << ": error after writing " << getProgress() << " bytes to tmp file");
                            writeError = true;
                            break;
                        }
                        output.flush();
                        if (output.bad()) {
                            // Read/writing error or logical error
                            LOG_WARN(_logger,
                                     "Request " << jobId() << ": error after flushing " << getProgress() << " bytes to tmp file");
                            writeError = true;
                            break;
                        }
                        retryCount = 0;
                    }

                    if (is.eof()) {
                        // End of stream
                        if (expectedSize == Poco::Net::HTTPMessage::UNKNOWN_CONTENT_LENGTH || getProgress() == expectedSize) {
                            done = true;
                        } else {
                            // Expected size hasn't be read
                            if (retryCount < READ_RETRIES) {
                                // Try to read again later
                                LOG_WARN(_logger, "Request " << jobId() << ": eof after reading " << getProgress()
                                                             << " bytes from input stream, retrying");
                                retryCount++;
                                Utility::msleep(READ_PAUSE_SLEEP_PERIOD);
                                continue;
                            } else {
                                LOG_WARN(_logger, "Request " << jobId() << ": eof after reading " << getProgress()
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
                        if (!_vfsUpdateFetchStatus(tmpPath, _localpath, getProgress(), fetchCanceled, fetchFinished)) {
                            LOGW_WARN(_logger, L"Error in vfsUpdateFetchStatus: " << Utility::formatSyncPath(_localpath));
                            fetchError = true;
                            break;
                        } else if (fetchCanceled) {
                            LOGW_WARN(_logger, L"Update fetch status canceled: " << Utility::formatSyncPath(_localpath));
                            break;
                        }
                        fileProgressTimer = std::chrono::steady_clock::now();
                    }
                }
            }
        }

        // Checks that the file has not been corrupted by another process
        // Unfortunately, the file hash is not available, so we check only its size
        output.flush();
        output.seekp(0, std::ios_base::end);
        if (expectedSize != output.tellp()) {
            LOG_WARN(_logger, "Request " << jobId() << ": tmp file has been corrupted by another process");
            writeError = true;
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
                if (!_vfsUpdateFetchStatus(tmpPath, _localpath, getProgress(), fetchCanceled, fetchFinished)) {
                    LOGW_WARN(_logger, L"Error in vfsUpdateFetchStatus: " << Utility::formatSyncPath(_localpath));
                    fetchError = true;
                } else if (fetchCanceled) {
                    LOGW_WARN(_logger, L"Update fetch status canceled: " << Utility::formatSyncPath(_localpath));
                } else if (!fetchFinished) {
                    LOGW_WARN(_logger, L"Update fetch status not terminated: " << Utility::formatSyncPath(_localpath));
                }

                _responseHandlingCanceled = fetchCanceled || fetchError || (!fetchFinished);
            } else if (!_vfsUpdateFetchStatus) {
                // Replace file by tmp one
                bool replaceError = false;
                if (!moveTmpFile(tmpPath, restartSync)) {
                    LOGW_WARN(_logger, L"Failed to replace file by tmp one: " << Utility::formatSyncPath(tmpPath));
                    replaceError = true;
                }

                _responseHandlingCanceled = replaceError || restartSync;
            }
        }

        if (_responseHandlingCanceled) {
            // NB: VFS reset is done in the destructor

            // Remove tmp file
            if (!removeTmpFile(tmpPath)) {
                LOGW_WARN(_logger, L"Failed to remove tmp file: " << Utility::formatSyncPath(tmpPath));
            }

            if (isAborted() || fetchCanceled) {
                // Download aborted or canceled by the user
                _exitCode = ExitCode::Ok;
                return true;
            } else if (readError) {
                // Download issue
                _exitCode = ExitCode::BackError;
                _exitCause = ExitCause::InvalidSize;
                return false;
            } else {
                // Fetch issue
                _exitCode = ExitCode::SystemError;
                _exitCause = ExitCause::FileAccessError;
                return false;
            }
        }
    }

    if (!_ignoreDateTime) {
        bool exists = false;
        if (!Utility::setFileDates(_localpath, std::make_optional<KDC::SyncTime>(_creationTime),
                                   std::make_optional<KDC::SyncTime>(_modtimeIn), isLink, exists)) {
            LOGW_WARN(_logger, L"Error in Utility::setFileDates: " << Utility::formatSyncPath(_localpath));
            // Do nothing (remote file will be updated during the next sync)
            sentry::Handler::captureMessage(sentry::Level::Warning, "DownloadJob::handleResponse",
                                                      "Unable to set file dates");
        } else if (!exists) {
            LOGW_INFO(_logger, L"Item does not exist anymore. Restarting sync: " << Utility::formatSyncPath(_localpath));
            _exitCode = ExitCode::DataError;
            _exitCause = ExitCause::InvalidSnapshot;
            return false;
        }
    }

    // Retrieve inode
    FileStat filestat;
    IoError ioError = IoError::Success;
    if (!IoHelper::getFileStat(_localpath, &filestat, ioError)) {
        LOGW_WARN(_logger, L"Error in IoHelper::getFileStat: " << Utility::formatIoError(_localpath, ioError));
        _exitCode = ExitCode::SystemError;
        _exitCause = ExitCause::Unknown;
        return false;
    }

    if (ioError == IoError::NoSuchFileOrDirectory) {
        LOGW_WARN(_logger, L"Item does not exist anymore: " << Utility::formatSyncPath(_localpath));
        _exitCode = ExitCode::DataError;
        _exitCause = ExitCause::InvalidSnapshot;
        return false;
    } else if (ioError == IoError::AccessDenied) {
        LOGW_WARN(_logger, L"Item misses search permission: " << Utility::formatSyncPath(_localpath));
        _exitCode = ExitCode::SystemError;
        _exitCause = ExitCause::FileAccessError;
        return false;
    }

    _localNodeId = std::to_string(filestat.inode);
    _exitCode = ExitCode::Ok;

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
            LOGW_DEBUG(_logger, L"Cannot create symlink on itself: " << Utility::formatSyncPath(_localpath));
            return false;
        }

        LOGW_DEBUG(_logger,
                   L"Create symlink with target " << Utility::formatSyncPath(targetPath) << L", " << Utility::formatSyncPath(_localpath));
      
        bool isFolder = mimeType == mimeTypeSymlinkFolder;
        IoError ioError = IoError::Success;
        if (!IoHelper::createSymlink(targetPath, _localpath, isFolder, ioError)) {
            LOGW_WARN(_logger, L"Failed to create symlink: " << Utility::formatIoError(targetPath, ioError));
            return false;
        }
    } else if (mimeType == mimeTypeHardlink) {
        // Unreachable code
        const auto targetPath = Str2Path(data);
        if (targetPath == _localpath) {
            LOGW_DEBUG(_logger, L"Cannot create hardlink on itself: " << Utility::formatSyncPath(_localpath));
            return false;
        }

        LOGW_DEBUG(_logger, L"Create hardlink: target " << Utility::formatSyncPath(targetPath) << L", "
                                                        << Utility::formatSyncPath(_localpath));

        std::error_code ec;
        std::filesystem::create_hard_link(targetPath, _localpath, ec);
        if (ec) {
            LOGW_WARN(_logger, L"Failed to create hardlink: target " << Utility::formatSyncPath(targetPath) << L", "
                                                                     << Utility::formatSyncPath(_localpath) << L", "
                                                                     << Utility::formatStdError(ec));
            return false;
        }
    } else if (mimeType == mimeTypeJunction) {
#if defined(_WIN32)
        LOGW_DEBUG(_logger, L"Create junction: " << Utility::formatSyncPath(_localpath));

        IoError ioError = IoError::Success;
        if (!IoHelper::createJunction(data, _localpath, ioError)) {
            LOGW_WARN(_logger, L"Failed to create junction: " << Utility::formatIoError(_localpath, ioError));
            return false;
        }
#endif
    } else if (mimeType == mimeTypeFinderAlias) {
#if defined(__APPLE__)
        LOGW_DEBUG(_logger, L"Create alias: " << Utility::formatSyncPath(_localpath));

        IoError ioError = IoError::Success;
        if (!IoHelper::createAlias(data, _localpath, ioError)) {
            LOGW_WARN(_logger, L"Failed to create alias: " << Utility::formatIoError(_localpath, ioError));

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
    if (std::error_code ec; !std::filesystem::remove_all(path, ec)) {
        LOGW_WARN(_logger, L"Failed to remove all: " << Utility::formatStdError(path, ec));
        return false;
    }

    return true;
}

bool DownloadJob::moveTmpFile(const SyncPath &tmpPath, bool &restartSync) {
    restartSync = false;

    // Move downloaded file from tmp directory to sync directory
    std::error_code ec;
#ifdef _WIN32
    bool retry = true;
    int counter = 50;
    while (retry) {
        retry = false;
#endif

        IoError ioError = IoError::Success;
        IoHelper::moveItem(tmpPath, _localpath, ioError);
        const bool crossDeviceLink = ioError == IoError::CrossDeviceLink;
        if (ioError != IoError::Success && !crossDeviceLink) {
            LOGW_WARN(_logger, L"Failed to move: " << Utility::formatIoError(tmpPath, ioError) << L" to "
                                                   << Utility::formatSyncPath(_localpath));
            return false;
        }

        if (crossDeviceLink) {
            // The sync might be on a different file system than tmp folder.
            // In that case, try to copy the file instead.
            ec.clear();
            std::filesystem::copy(tmpPath, _localpath, std::filesystem::copy_options::overwrite_existing, ec);
            bool removed = removeTmpFile(tmpPath);
            if (ec) {
                LOGW_WARN(_logger,
                          L"Failed to copy to " << Utility::formatSyncPath(_localpath) << L", " << Utility::formatStdError(ec));
                return false;
            }
            if (!removed) {
                LOGW_WARN(_logger, L"Failed to remove " << Utility::formatSyncPath(tmpPath));
                return false;
            }
        }
#ifdef _WIN32
        else if (ec.value() == ERROR_SHARING_VIOLATION) {
            if (counter) {
                // Retry
                retry = true;
                Utility::msleep(10);
                LOGW_DEBUG(_logger, L"Retrying to move downloaded file: " << Utility::formatSyncPath(_localpath));
                counter--;
            } else {
                LOGW_WARN(_logger, L"Failed to rename: " << Utility::formatStdError(_localpath, ec));
                return false;
            }
        }
#endif
        else if (IoHelper::stdError2ioError(ec.value()) == IoError::AccessDenied) {
            _exitCode = ExitCode::SystemError;
            _exitCause = ExitCause::FileAccessError;
            return false;
        } else if (ec) {
            bool exists = false;
            IoError ioError = IoError::Success;
            if (!IoHelper::checkIfPathExists(_localpath.parent_path(), exists, ioError)) {
                LOGW_WARN(_logger,
                          L"Error in IoHelper::checkIfPathExists: " << Utility::formatIoError(_localpath.parent_path(), ioError));
                _exitCode = ExitCode::SystemError;
                _exitCause = ExitCause::Unknown;
                return false;
            }
            if (ioError == IoError::AccessDenied) {
                LOGW_WARN(_logger, L"Access denied to " << Utility::formatSyncPath(_localpath.parent_path()));
                _exitCode = ExitCode::SystemError;
                _exitCause = ExitCause::FileAccessError;
                return false;
            }

            if (!exists) {
                LOGW_INFO(_logger, L"Parent of item does not exist anymore: " << Utility::formatStdError(_localpath, ec));
                restartSync = true;
                return true;
            }

            LOGW_WARN(_logger, L"Failed to rename: " << Utility::formatStdError(_localpath, ec));
            return false;
        }
#ifdef _WIN32
    }
#endif

    return true;
}

} // namespace KDC

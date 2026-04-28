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

#include "downloadjob.h"

#include "libcommonserver/io/filestat.h"
#include "libcommonserver/io/iohelper.h"
#include "libcommonserver/utility/utility.h"
#include "libcommonserver/io/permissionsgiver.h"

#include "libcommon/utility/utility.h"

#if defined(KD_MACOS) || defined(KD_LINUX)
#include <unistd.h>
#endif

#include "utility/timerutility.h"

#include <fstream>

#include <Poco/File.h>
#include <Poco/Net/HTTPRequest.h>

namespace KDC {

#define BUF_SIZE 4096 * 1000 // 4MB
#define NOTIFICATION_DELAY 1000 // 1'000ms => 1 sec
#define TRIALS 5
#define READ_PAUSE_SLEEP_PERIOD 100 // 0.1 s
#define READ_RETRIES 10
#define READ_RETRIES_NETWORK_LOST 100

DownloadJob::DownloadJob(const std::shared_ptr<Vfs> vfs, std::shared_ptr<CacheDirectory> cacheDirectory,
                         const FileDownloadInfo &fileDownloadInfo, DateTimePolicy dateTimePolicy) :
    AbstractTokenNetworkJob(ApiType::Drive, 0, 0, fileDownloadInfo.driveDbId, 0, false),
    _vfs(vfs),
    _cacheDirectory(cacheDirectory),
    _fileDownloadInfo(fileDownloadInfo),
    _dateTimePolicy(dateTimePolicy) {
    _httpMethod = Poco::Net::HTTPRequest::HTTP_GET;
    _customTimeout = 60;
    _trials = TRIALS;

    if (!_cacheDirectory) {
        // If no cache directory have been provided, fallback to creating a temporary cache directory into parent folder.
        _cacheDirectory = std::make_shared<CacheDirectory>(_fileDownloadInfo.localpath.parent_path());
    }
}

DownloadJob::~DownloadJob() {
    // Remove tmp file
    // For a remote CREATE operation, the tmp file should no longer exist, but if an error occurred in handleResponse, it must
    // be deleted.
    if (!removeTmpFile() && !_fileDownloadInfo.isCreate) {
        LOGW_WARN(_logger, L"Failed to remove tmp file: " << Utility::formatSyncPath(_tmpPath));
    }

    if (!_vfs) return;

    // If the download job intent is to create a new local file, then there is no downloaded file after cancellation.
    if (_responseHandlingCanceled && _fileDownloadInfo.isCreate) return;

    if (_responseHandlingCanceled) {
        if (const ExitInfo exitInfo = _vfs->setPinState(_fileDownloadInfo.localpath, PinState::OnlineOnly); !exitInfo) {
            LOGW_WARN(_logger,
                      L"Error in vfsSetPinState: " << Utility::formatSyncPath(_fileDownloadInfo.localpath) << L": " << exitInfo);
        }

        if (const ExitInfo exitInfo = _vfs->forceStatus(_fileDownloadInfo.localpath, VfsStatus()); !exitInfo) {
            LOGW_WARN(_logger,
                      L"Error in vfsForceStatus: " << Utility::formatSyncPath(_fileDownloadInfo.localpath) << L": " << exitInfo);
        }

        _vfs->cancelHydrate(_fileDownloadInfo.localpath);
    } else {
        if (const ExitInfo res =
                    _vfs->setPinState(_fileDownloadInfo.localpath,
                                      exitInfo().code() == ExitCode::Ok ? PinState::AlwaysLocal : PinState::OnlineOnly);
            !res) {
            LOGW_WARN(_logger,
                      L"Error in vfsSetPinState: " << Utility::formatSyncPath(_fileDownloadInfo.localpath) << L": " << res);
        }

        if (const ExitInfo res =
                    _vfs->forceStatus(_fileDownloadInfo.localpath, VfsStatus({.isHydrated = exitInfo().code() == ExitCode::Ok}));
            !res) {
            LOGW_WARN(_logger,
                      L"Error in vfsForceStatus: " << Utility::formatSyncPath(_fileDownloadInfo.localpath) << L": " << res);
        }
    }
}

std::string DownloadJob::getSpecificUrl() {
    std::string str = AbstractTokenNetworkJob::getSpecificUrl();
    str += "/files/";
    str += _fileDownloadInfo.remoteFileId;
    str += "/download";
    return str;
}

ExitInfo DownloadJob::canRun() {
    if (_fileDownloadInfo.remoteFileId.empty() || _fileDownloadInfo.localpath.empty()) {
        LOG_WARN(_logger, "Missing mandatory inputs! ");
        return ExitCode::LogicError;
    }

    if (bypassCheck()) {
        return ExitCode::Ok;
    }

    // Check that we can create the item here
    bool exists = false;
    IoError ioError = IoError::Success;
    if (!IoHelper::checkIfPathExists(_fileDownloadInfo.localpath, exists, ioError, IoHelper::PathCheckOption::Insensitive)) {
        LOGW_WARN(_logger,
                  L"Error in IoHelper::checkIfPathExists: " << Utility::formatIoError(_fileDownloadInfo.localpath, ioError));
        return {ExitCode::SystemError, ExitCause::FileAccessError};
    }

    if (_fileDownloadInfo.isCreate && exists) {
        LOGW_DEBUG(_logger, L"Item with " << Utility::formatSyncPath(_fileDownloadInfo.localpath)
                                          << L" already exists. Aborting current sync and restarting.");
        return {ExitCode::DataError, ExitCause::FileExists};
    }

    return ExitCode::Ok;
}

ExitInfo DownloadJob::runJob() noexcept {
    if (!_fileDownloadInfo.isCreate && _vfs) {
        // Get hydration status
        VfsStatus vfsStatus;
        (void) _vfs->status(_fileDownloadInfo.localpath, vfsStatus);
        _isHydrated = vfsStatus.isHydrated;

        // Update size on file system
        FileStat filestat;
        IoError ioError = IoError::Success;
        if (!IoHelper::getFileStat(_fileDownloadInfo.localpath, &filestat, ioError, IoHelper::PathCheckOption::Insensitive)) {
            LOGW_WARN(_logger,
                      L"Error in IoHelper::getFileStat: " << Utility::formatIoError(_fileDownloadInfo.localpath, ioError));
            return ExitCode::SystemError;
        }
        if (ioError == IoError::NoSuchFileOrDirectory) {
            LOGW_WARN(_logger, L"Item does not exist anymore: " << Utility::formatSyncPath(_fileDownloadInfo.localpath));
            return {ExitCode::SystemError, ExitCause::NotFound};
        } else if (ioError == IoError::AccessDenied) {
            LOGW_WARN(_logger, L"Item misses search permission: " << Utility::formatSyncPath(_fileDownloadInfo.localpath));
            return {ExitCode::SystemError, ExitCause::FileAccessError};
        }

        if (const ExitInfo exitInfo =
                    _vfs->updateMetadata(_fileDownloadInfo.localpath, filestat.creationTime, filestat.modificationTime,
                                         _fileDownloadInfo.expectedSize, std::to_string(filestat.inode));
            !exitInfo) {
            LOGW_WARN(_logger,
                      L"Update metadata failed " << exitInfo << L" " << Utility::formatSyncPath(_fileDownloadInfo.localpath));
            return exitInfo;
        }

        if (const ExitInfo exitInfo = _vfs->forceStatus(_fileDownloadInfo.localpath, VfsStatus({.isSyncing = true})); !exitInfo) {
            LOGW_WARN(_logger,
                      L"Error in vfsForceStatus: " << Utility::formatSyncPath(_fileDownloadInfo.localpath) << L": " << exitInfo);
            return exitInfo;
        }
    }

    return AbstractTokenNetworkJob::runJob();
}

ExitInfo DownloadJob::handleResponse(std::istream &is) {
    // Get Mime type
    std::string contentType;
    contentType = httpResponse().get("Content-Type", "");

    std::string mimeType;
    if (!contentType.empty()) {
        std::vector<std::string> contentTypeElts = Utility::splitStr(contentType, ';');
        mimeType = contentTypeElts[0];
    }

    bool isLink = false;
    std::string linkData;
    if (mimeType == mimeTypeSymlink || mimeType == mimeTypeSymlinkFolder || mimeType == mimeTypeHardlink ||
        (mimeType == mimeTypeFinderAlias && CommonUtility::isMac()) ||
        (mimeType == mimeTypeJunction && CommonUtility::isWindows())) {
        // Read link data
        getStringFromStream(is, linkData);
        isLink = true;
    }

    // Process download
    if (isLink) {
        // Create link
        LOG_DEBUG(_logger, "Create link: mimeType=" << mimeType);
        if (!createLink(mimeType, linkData)) { // We consider this as a permission denied error
            return {ExitCode::SystemError, ExitCause::FileAccessError};
        }
    } else {
        // Create file
        bool readError = false;
        bool writeError = false;
        bool fetchCanceled = false;
        bool fetchFinished = false;
        bool fetchError = false;
        if (const auto exitInfo = createTmpFile(is, readError, writeError, fetchCanceled, fetchFinished, fetchError); !exitInfo) {
            LOGW_WARN(_logger, L"Error in createTmpFile");
            return exitInfo;
        }

        _responseHandlingCanceled = isAborted() || readError || writeError || fetchCanceled || fetchError;

        if (!_responseHandlingCanceled) {
            if (_vfs && !_isHydrated && !fetchFinished) { // updateFetchStatus is used only for hydration.
                // Update fetch status
                if (ExitInfo exitInfo = _vfs->updateFetchStatus(_tmpPath, _fileDownloadInfo.localpath, getProgress(),
                                                                fetchCanceled, fetchFinished);
                    !exitInfo) {
                    LOGW_WARN(_logger, L"Error in vfsUpdateFetchStatus: " << Utility::formatSyncPath(_fileDownloadInfo.localpath)
                                                                          << L" : " << exitInfo);
                    fetchError = true;
                } else if (fetchCanceled) {
                    LOGW_WARN(_logger, L"Update fetch status canceled: " << Utility::formatSyncPath(_fileDownloadInfo.localpath));
                } else if (!fetchFinished) {
                    LOGW_WARN(_logger,
                              L"Update fetch status not terminated: " << Utility::formatSyncPath(_fileDownloadInfo.localpath));
                }

                _responseHandlingCanceled = fetchCanceled || fetchError || (!fetchFinished);
            } else if (_isHydrated) {
                // Replace file by tmp one
                if (!moveTmpFile()) {
                    LOGW_WARN(_logger, L"Failed to replace file by tmp one: " << Utility::formatSyncPath(_tmpPath));
                    writeError = true;
                }

                _responseHandlingCanceled = writeError;
            }
        }

        if (_responseHandlingCanceled) {
            // NB: VFS reset is done in the destructor
            if (isAborted() || fetchCanceled) {
                // Download aborted or canceled by the user
                return ExitCode::Ok;
            } else if (readError) {
                // Download issue
                return {ExitCode::BackError, ExitCause::InvalidSize};
            } else if (const std::streamsize neededPlace =
                               httpResponse().getContentLength() == Poco::Net::HTTPMessage::UNKNOWN_CONTENT_LENGTH
                                       ? BUF_SIZE
                                       : (httpResponse().getContentLength() - getProgress());
                       !hasEnoughPlace(_tmpPath, _fileDownloadInfo.localpath, neededPlace, _logger)) {
                return {ExitCode::SystemError, ExitCause::NotEnoughDiskSpace};
            } else {
                return {ExitCode::SystemError, ExitCause::FileAccessError};
            }
        }
    }
    if (_dateTimePolicy == DateTimePolicy::ApplyDateTime) {
        if (const IoError ioError = IoHelper::setFileDates(_fileDownloadInfo.localpath, _fileDownloadInfo.creationTime,
                                                           _fileDownloadInfo.modificationTime, isLink);
            ioError == IoError::Unknown) {
            LOGW_WARN(_logger, L"Error in IoHelper::setFileDates: " << Utility::formatSyncPath(_fileDownloadInfo.localpath));
            // Do nothing (remote file will be updated during the next sync)
            sentry::Handler::captureMessage(sentry::Level::Warning, "DownloadJob::handleResponse", "Unable to set file dates");
        } else if (ioError == IoError::NoSuchFileOrDirectory || ioError == IoError::AccessDenied) {
            LOGW_INFO(_logger, L"Item does not exist anymore or access is denied. Restarting sync: "
                                       << Utility::formatSyncPath(_fileDownloadInfo.localpath));
            return {ExitCode::DataError, ExitCause::InvalidSnapshot};
        }
    }

    // Retrieve inode
    FileStat filestat;
    IoError ioError = IoError::Success;
    if (!IoHelper::getFileStat(_fileDownloadInfo.localpath, &filestat, ioError, IoHelper::PathCheckOption::Insensitive)) {
        LOGW_WARN(_logger, L"Error in IoHelper::getFileStat: " << Utility::formatIoError(_fileDownloadInfo.localpath, ioError));
        return ExitCode::SystemError;
    }

    if (ioError == IoError::NoSuchFileOrDirectory) {
        LOGW_WARN(_logger, L"Item does not exist anymore: " << Utility::formatSyncPath(_fileDownloadInfo.localpath));
        return {ExitCode::DataError, ExitCause::InvalidSnapshot};
    } else if (ioError == IoError::AccessDenied) {
        LOGW_WARN(_logger, L"Item misses search permission: " << Utility::formatSyncPath(_fileDownloadInfo.localpath));
        return {ExitCode::SystemError, ExitCause::FileAccessError};
    }

    _localNodeId = std::to_string(filestat.inode);
    _creationTimeOut = filestat.creationTime;
    _modificationTimeOut = filestat.modificationTime;
    _sizeOut = filestat.size;
#if defined(KD_MACOS) || defined(KD_WINDOWS)
    if (_fileDownloadInfo.creationTime != _creationTimeOut || _fileDownloadInfo.modificationTime != _modificationTimeOut) {
        // In the following cases, it is not an issue:
        // - Windows: if creation/modification date = 0, it is set to current date
        // - macOS: if creation date > modification date, creation date is set to modification date
        LOGW_INFO(_logger, L"Impossible to set creation and/or modification time(s) on local file."
                                   << L" Desired values: " << _fileDownloadInfo.creationTime << L"/"
                                   << _fileDownloadInfo.modificationTime << L" Set values: " << _creationTimeOut << L"/"
                                   << _modificationTimeOut << L" for " << Utility::formatSyncPath(_fileDownloadInfo.localpath));
    }
#else
    // On Linux, the creation date cannot be set
    if (_fileDownloadInfo.modificationTime != _modificationTimeOut) {
        LOGW_INFO(_logger, L"Impossible to set modification time on local file."
                                   << L" Desired value: " << _fileDownloadInfo.modificationTime << L" Set value: "
                                   << _modificationTimeOut << L" for " << Utility::formatSyncPath(_fileDownloadInfo.localpath));
    }
#endif

    return ExitCode::Ok;
}

ExitInfo DownloadJob::createLink(const std::string &mimeType, const std::string &data) {
    // Delete in case it already exists (EDIT operation)
    (void) IoHelper::deleteItem(_fileDownloadInfo.localpath);

    if (mimeType == mimeTypeSymlink || mimeType == mimeTypeSymlinkFolder) {
        // Create symlink
        const auto targetPath = Str2Path(data);
        if (targetPath == _fileDownloadInfo.localpath) {
            LOGW_DEBUG(_logger, L"Cannot create symlink on itself: " << Utility::formatSyncPath(_fileDownloadInfo.localpath));
            return {};
        }

        LOGW_DEBUG(_logger, L"Create symlink with target " << Utility::formatSyncPath(targetPath) << L", "
                                                           << Utility::formatSyncPath(_fileDownloadInfo.localpath));

        bool isFolder = mimeType == mimeTypeSymlinkFolder;
        IoError ioError = IoError::Success;
        if (!IoHelper::createSymlink(targetPath, _fileDownloadInfo.localpath, isFolder, ioError)) {
            LOGW_WARN(_logger, L"Failed to create symlink: " << Utility::formatIoError(targetPath, ioError));
            return {};
        }
    } else if (mimeType == mimeTypeHardlink) {
        // Unreachable code
        const auto targetPath = Str2Path(data);
        if (targetPath == _fileDownloadInfo.localpath) {
            LOGW_DEBUG(_logger, L"Cannot create hardlink on itself: " << Utility::formatSyncPath(_fileDownloadInfo.localpath));
            return {};
        }

        LOGW_DEBUG(_logger, L"Create hardlink: target " << Utility::formatSyncPath(targetPath) << L", "
                                                        << Utility::formatSyncPath(_fileDownloadInfo.localpath));

        std::error_code ec;
        std::filesystem::create_hard_link(targetPath, _fileDownloadInfo.localpath, ec);
        if (ec) {
            LOGW_WARN(_logger, L"Failed to create hardlink: target " << Utility::formatSyncPath(targetPath) << L", "
                                                                     << Utility::formatSyncPath(_fileDownloadInfo.localpath)
                                                                     << L", " << Utility::formatStdError(ec));
            return {};
        }
    } else if (mimeType == mimeTypeJunction) {
#if defined(KD_WINDOWS)
        LOGW_DEBUG(_logger, L"Create junction: " << Utility::formatSyncPath(_fileDownloadInfo.localpath));

        IoError ioError = IoError::Success;
        if (!IoHelper::createJunction(data, _fileDownloadInfo.localpath, ioError)) {
            LOGW_WARN(_logger, L"Failed to create junction: " << Utility::formatIoError(_fileDownloadInfo.localpath, ioError));
            return {};
        }
#endif
    } else if (mimeType == mimeTypeFinderAlias) {
#if defined(KD_MACOS)
        LOGW_DEBUG(_logger, L"Create alias: " << Utility::formatSyncPath(_fileDownloadInfo.localpath));

        IoError ioError = IoError::Success;
        if (!IoHelper::createAlias(data, _fileDownloadInfo.localpath, ioError)) {
            LOGW_WARN(_logger, L"Failed to create alias: " << Utility::formatIoError(_fileDownloadInfo.localpath, ioError));

            if (ioError == IoError::Unknown) {
                // Could be an alias imported into the drive by drag&drop in the webapp
                bool writeError = false;
                if (const auto exitInfo = createTmpFile(data, writeError); !exitInfo) {
                    LOGW_WARN(_logger, L"Error in createTmpFile");
                    return exitInfo;
                }

                _responseHandlingCanceled = isAborted() || writeError;

                if (!_responseHandlingCanceled) {
                    std::string data2;
                    SyncPath targetPath;
                    if (!IoHelper::readAlias(_tmpPath, data2, targetPath, ioError)) {
                        LOGW_WARN(_logger, L"Error in IoHelper::readAlias: " << Utility::formatIoError(_tmpPath, ioError));
                        return {};
                    }

                    if (!IoHelper::createAlias(data2, _fileDownloadInfo.localpath, ioError)) {
                        LOGW_WARN(_logger,
                                  L"Failed to create alias: " << Utility::formatIoError(_fileDownloadInfo.localpath, ioError));
                        return {};
                    }

                    return ExitCode::Ok;
                }

                if (_responseHandlingCanceled) {
                    if (isAborted()) {
                        // Download aborted or canceled by the user
                        return ExitCode::Ok;
                    } else {
                        return {ExitCode::SystemError, ExitCause::FileAccessError};
                    }
                }
            }

            return {};
        }
#endif
    } else {
        LOG_WARN(_logger, "Link type not managed: MIME type=" << mimeType);
        return {};
    }

    return ExitCode::Ok;
}

bool DownloadJob::removeTmpFile() {
    if (_tmpPath.empty()) return true;

    if (auto ioError = IoError::Unknown; !IoHelper::deleteItem(_tmpPath, ioError)) {
        LOGW_WARN(_logger, L"Failed to remove a downloaded temporary file: " << Utility::formatIoError(_tmpPath, ioError));
        return false;
    }
    return true;
}

ExitInfo DownloadJob::moveTmpFile() {
    // Move downloaded file from tmp directory to sync directory
#if defined(KD_WINDOWS)
    bool retry = true;
    int counter = 50;
    while (retry) {
        retry = false;
#endif

        bool error = false;
        bool accessDeniedError = false;
        bool crossDeviceLinkError = false;
#if defined(KD_WINDOWS)
        bool sharingViolationError = false;
#endif
        static const bool forceCopy = CommonUtility::envVarValue("KDRIVE_PRESERVE_PERMISSIONS_ON_CREATE") == "1";
        if (_fileDownloadInfo.isCreate && !forceCopy) {
            // Make sure we are allowed to propagate the change
            PermissionsGiver _(_fileDownloadInfo.localpath.parent_path(), _logger);

            // Move file
            IoError ioError = IoError::Success;
            (void) IoHelper::moveItem(_tmpPath, _fileDownloadInfo.localpath, ioError);
            crossDeviceLinkError = ioError == IoError::CrossDeviceLink; // Unable to move between 2 distinct file systems
            if (ioError != IoError::Success && !crossDeviceLinkError) {
                LOGW_WARN(_logger, L"Failed to move downloaded file " << Utility::formatSyncPath(_tmpPath) << L" to "
                                                                      << Utility::formatSyncPath(_fileDownloadInfo.localpath)
                                                                      << L", err='" << Utility::formatIoError(ioError) << L"'");
                error = true;
                accessDeniedError = ioError == IoError::AccessDenied;
                // NB: On Windows, ec.value() == ERROR_SHARING_VIOLATION is translated as IoError::AccessDenied
            }
        }

        if (!_fileDownloadInfo.isCreate || crossDeviceLinkError || forceCopy) {
            // Make sure we are allowed to propagate the change
            PermissionsGiver _(_fileDownloadInfo.localpath.parent_path(), _logger);

            // Copy file content (i.e. when the target exists, do not change its node id).
            std::error_code ec;
            std::filesystem::copy(_tmpPath, _fileDownloadInfo.localpath, std::filesystem::copy_options::overwrite_existing, ec);
            if (ec) {
                LOGW_WARN(_logger, L"Failed to copy downloaded file " << Utility::formatSyncPath(_tmpPath) << L" to "
                                                                      << Utility::formatSyncPath(_fileDownloadInfo.localpath)
                                                                      << L", err='" << Utility::formatStdError(ec) << L"'");
                error = true;
                accessDeniedError = IoHelper::stdError2ioError(ec.value()) == IoError::AccessDenied;
#if defined(KD_WINDOWS)
                sharingViolationError = ec.value() == ERROR_SHARING_VIOLATION; // In this case, we will try again
#endif
            }
        }

        if (error) {
#if defined(KD_WINDOWS)
            if (sharingViolationError) {
                if (counter) {
                    // Retry
                    retry = true;
                    Utility::msleep(10);
                    LOGW_DEBUG(_logger,
                               L"Retrying to copy downloaded file: " << Utility::formatSyncPath(_fileDownloadInfo.localpath));
                    counter--;
                    continue;
                } else {
                    return {};
                }
            }
#endif

            if (accessDeniedError) {
                return {ExitCode::SystemError, ExitCause::FileAccessError};
            } else {
                bool exists = false;
                IoError ioError = IoError::Success;
                if (!IoHelper::checkIfPathExists(_fileDownloadInfo.localpath.parent_path(), exists, ioError,
                                                 IoHelper::PathCheckOption::Insensitive)) {
                    LOGW_WARN(_logger, L"Error in IoHelper::checkIfPathExists: "
                                               << Utility::formatIoError(_fileDownloadInfo.localpath.parent_path(), ioError));
                    return ExitCode::SystemError;
                }
                if (ioError == IoError::AccessDenied) {
                    LOGW_WARN(_logger,
                              L"Access denied to item " << Utility::formatSyncPath(_fileDownloadInfo.localpath.parent_path()));
                    return {ExitCode::SystemError, ExitCause::FileAccessError};
                }

                if (!exists) {
                    LOGW_INFO(_logger,
                              L"Parent of item does not exist anymore " << Utility::formatSyncPath(_fileDownloadInfo.localpath));
                    disableRetry();
                }

                return {};
            }
        }
#if defined(KD_WINDOWS)
    }
#endif

    return ExitCode::Ok;
}

bool DownloadJob::hasEnoughPlace(const SyncPath &tmpDirPath, const SyncPath &destDirPath, int64_t neededPlace,
                                 log4cplus::Logger logger) {
    auto tmpDirSize = Utility::Utility::getFreeDiskSpace(tmpDirPath);
    auto destDirSize = Utility::Utility::getFreeDiskSpace(destDirPath);

    if (const auto &freeBytes = std::min(tmpDirSize, destDirSize); freeBytes >= 0) {
        const auto totalNeededSpace = neededPlace + Utility::freeDiskSpaceLimit();
        if (freeBytes < totalNeededSpace) {
            const auto lowDiskSpacePath = tmpDirSize < destDirSize ? tmpDirPath : destDirPath;
            LOGW_WARN(logger, L"Not enough place at " << Utility::formatSyncPath(lowDiskSpacePath) << L". Required free space: "
                                                      << totalNeededSpace << L" bytes. Available free space: " << freeBytes
                                                      << L" bytes. Download cancelled.")
            return false;
        }
    } else {
        const SyncPath &smallerDir = tmpDirSize < destDirSize ? tmpDirPath : destDirPath;
        LOGW_WARN(logger, L"Could not determine free space available at " << Utility::formatSyncPath(smallerDir));
    }
    return true;
}

ExitInfo DownloadJob::createTmpFile(std::optional<std::reference_wrapper<std::istream>> istr,
                                    std::optional<std::reference_wrapper<const std::string>> data, bool &readError,
                                    bool &writeError, bool &fetchCanceled, bool &fetchFinished, bool &fetchError) {
    assert(istr || data);

    readError = false;
    writeError = false;
    fetchCanceled = false;
    fetchFinished = false;
    fetchError = false;

    SyncPath cacheDirectoryPath;
    if (const auto exitInfo = _cacheDirectory->path(cacheDirectoryPath); !exitInfo) {
        return exitInfo;
    }

    std::ofstream output;
    do {
        const std::string tmpFileName = CacheDirectory::createTmpFileName();
        _tmpPath = cacheDirectoryPath / tmpFileName;

        output.open(_tmpPath.native().c_str(), std::ofstream::out | std::ofstream::binary);
        if (!output.is_open()) {
            const bool enoughSpace = Utility::enoughSpace(_tmpPath);
            LOGW_WARN(_logger, L"Failed to open tmp file: " << Utility::formatSyncPath(_tmpPath) << L". Reason: "
                                                            << (enoughSpace ? L"file access error." : L"not enough space."));
            return {ExitCode::SystemError, enoughSpace ? ExitCause::TmpDirAccessError : ExitCause::NotEnoughDiskSpace};
        }

        output.seekp(0, std::ios_base::end);
    } while (output.tellp() > 0); // If the file is not empty, generate a new file name

    std::streamsize expectedSize = 0;
    if (istr) {
        expectedSize = httpResponse().getContentLength();

        setProgressExpectedFinalValue(expectedSize);
        setProgress(0);

        if (expectedSize != Poco::Net::HTTPMessage::UNKNOWN_CONTENT_LENGTH) {
            if (!hasEnoughPlace(_tmpPath, _fileDownloadInfo.localpath, expectedSize, _logger)) {
                writeError = true;
            }
            if (expectedSize < 0) {
                LOG_WARN(_logger, "Request " << jobId() << ": invalid content length: " << expectedSize);
                readError = true;
            }
        }

        if (!writeError && !readError) {
            TimerUtility timer;
            std::unique_ptr<char[]> buffer(new char[BUF_SIZE]);
            bool done = false;
            int retryCount = 0;
            while (!done) {
                if (isAborted()) {
                    LOG_DEBUG(_logger, "Request " << jobId() << ": aborted");
                    break;
                }

                istr->get().read(buffer.get(), BUF_SIZE);
                if (istr->get().bad() && !istr->get().fail()) {
                    // Read/writing error and not logical error
                    LOG_WARN(_logger,
                             "Request " << jobId() << ": error after reading " << getProgress() << " bytes from input stream");
                    readError = true;
                    break;
                } else {
                    std::streamsize readSize = istr->get().gcount();

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

                    if (readSize == 0 && !istr->get().eof()) {
                        if (retryCount < READ_RETRIES_NETWORK_LOST) {
                            // Try to read again later
                            LOG_WARN(_logger, "Request " << jobId() << ": empty buffer read after reading " << getProgress()
                                                         << " bytes from input stream, retrying");
                            retryCount++;
                            Utility::msleep(READ_PAUSE_SLEEP_PERIOD);
                            continue;
                        }

                        LOG_WARN(_logger, "Request " << jobId() << ": empty buffer read after reading " << getProgress()
                                                     << " bytes from input stream");
                        readError = true;
                        break;
                    }

                    if (istr->get().eof()) {
                        // End of stream
                        if (expectedSize == Poco::Net::HTTPMessage::UNKNOWN_CONTENT_LENGTH || getProgress() == expectedSize) {
                            done = true;
                        } else {
                            // Expected size hasn't been read
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

                if (_vfs && !_isHydrated) { // updateFetchStatus is used only for hydration.
                    if (timer.elapsed<std::chrono::milliseconds>().count() > NOTIFICATION_DELAY || done) {
                        // Update fetch status
                        if (!_vfs->updateFetchStatus(_tmpPath, _fileDownloadInfo.localpath, getProgress(), fetchCanceled,
                                                     fetchFinished)) {
                            LOGW_WARN(_logger,
                                      L"Error in vfsUpdateFetchStatus: " << Utility::formatSyncPath(_fileDownloadInfo.localpath));
                            fetchError = true;
                            break;
                        } else if (fetchCanceled) {
                            LOGW_WARN(_logger,
                                      L"Update fetch status canceled: " << Utility::formatSyncPath(_fileDownloadInfo.localpath));
                            break;
                        }
                        timer.restart();
                    }
                }
            }
        }
    } else if (data) {
        expectedSize = static_cast<std::streamsize>(data->get().length());
        output.write(data->get().c_str(), expectedSize);
    }

    // Checks that the file has not been corrupted by another process
    // Unfortunately, the file hash is not available, so we check only its size
    output.flush();
    output.seekp(0, std::ios_base::end);
    if (expectedSize != Poco::Net::HTTPMessage::UNKNOWN_CONTENT_LENGTH && output.tellp() != expectedSize && !readError &&
        !writeError && !fetchError && !isAborted()) {
        LOG_WARN(_logger, "Request " << jobId() << ": tmp file has been corrupted by another process");
        sentry::Handler::captureMessage(sentry::Level::Error, "DownloadJob::handleResponse", "Tmp file is corrupted");
        writeError = true;
    }

    output.close();
    if (output.bad()) {
        // Read/writing error or logical error
        LOG_WARN(_logger, "Request " << jobId() << ": error after closing tmp file");
        writeError = true;
    }

    return ExitCode::Ok;
}

ExitInfo DownloadJob::createTmpFile(std::istream &is, bool &readError, bool &writeError, bool &fetchCanceled, bool &fetchFinished,
                                    bool &fetchError) {
    return createTmpFile(std::make_optional<std::reference_wrapper<std::istream>>(is), std::nullopt, readError, writeError,
                         fetchCanceled, fetchFinished, fetchError);
}

ExitInfo DownloadJob::createTmpFile(const std::string &data, bool &writeError) {
    bool readError = false;
    bool fetchCanceled = false;
    bool fetchFinished = false;
    bool fetchError = false;
    return createTmpFile(std::nullopt, std::make_optional<std::reference_wrapper<const std::string>>(data), readError, writeError,
                         fetchCanceled, fetchFinished, fetchError);
}

} // namespace KDC

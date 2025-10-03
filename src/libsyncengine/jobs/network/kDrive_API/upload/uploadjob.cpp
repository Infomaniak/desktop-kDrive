// Infomaniak kDrive - Desktop
// Copyright (C) 2023-2025 Infomaniak Network SA
/// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.


#include "uploadjob.h"

#include "uploadjobreplyhandler.h"
#include "io/filestat.h"
#include "libcommonserver/io/iohelper.h"
#include "libcommonserver/utility/utility.h"
#include "libcommon/utility/jsonparserutility.h"

#include <fstream>
#include <Poco/Net/HTTPRequest.h>

#define TRIALS 5

namespace KDC {

UploadJob::UploadJob(const std::shared_ptr<Vfs> &vfs, const int driveDbId, const SyncPath &absoluteFilePath,
                     const SyncName &filename, const NodeId &remoteParentDirId, const SyncTime creationTime,
                     const SyncTime modificationTime) :
    AbstractTokenNetworkJob(ApiType::Drive, 0, 0, driveDbId, 0),
    _absoluteFilePath(absoluteFilePath),
    _filename(filename),
    _remoteParentDirId(remoteParentDirId),
    _creationTimeIn(creationTime),
    _modificationTimeIn(modificationTime),
    _vfs(vfs) {
    _httpMethod = Poco::Net::HTTPRequest::HTTP_POST;
    _customTimeout = 60;
    _trials = TRIALS;
    setProgress(0);
}

UploadJob::UploadJob(const std::shared_ptr<Vfs> &vfs, const int driveDbId, const SyncPath &absoluteFilePath, const NodeId &fileId,
                     const SyncTime modificationTime) :
    UploadJob(vfs, driveDbId, absoluteFilePath, SyncName(), "", 0, modificationTime) {
    _fileId = fileId;

    // Retrieve creation date from the local file
    FileStat fileStat;
    auto ioError = IoError::Unknown;
    if (!IoHelper::getFileStat(_absoluteFilePath, &fileStat, ioError) || ioError != IoError::Success) {
        LOGW_WARN(_logger, L"Failed to get FileStat for " << Utility::formatSyncPath(_absoluteFilePath) << L": " << ioError);
    }
    _creationTimeIn = fileStat.creationTime;
}

UploadJob::~UploadJob() {
    if (!_vfs || isAborted()) return;
    constexpr VfsStatus vfsStatus({.isHydrated = true, .isSyncing = false, .progress = 100});
    if (const auto exitInfo = _vfs->forceStatus(_absoluteFilePath, vfsStatus); !exitInfo) {
        LOGW_WARN(_logger, L"Error in vfsForceStatus - " << Utility::formatSyncPath(_absoluteFilePath) << L": " << exitInfo);
    }

    if (const auto exitInfo = _vfs->setPinState(_absoluteFilePath, PinState::AlwaysLocal); !exitInfo) {
        LOGW_WARN(_logger, L"Error in vfsSetPinState - " << Utility::formatSyncPath(_absoluteFilePath) << L": " << exitInfo);
    }
}

ExitInfo UploadJob::canRun() {
    if (bypassCheck()) {
        return ExitCode::Ok;
    }

    // Check that the item still exist
    bool exists = false;
    IoError ioError = IoError::Success;
    if (!IoHelper::checkIfPathExists(_absoluteFilePath, exists, ioError)) {
        LOGW_WARN(_logger, L"Error in IoHelper::checkIfPathExists: " << Utility::formatIoError(_absoluteFilePath, ioError));
        return ExitCode::SystemError;
    }
    if (ioError == IoError::AccessDenied) {
        LOGW_WARN(_logger, L"Access denied to " << Utility::formatSyncPath(_absoluteFilePath));
        return {ExitCode::SystemError, ExitCause::FileAccessError};
    }

    if (!exists) {
        LOGW_DEBUG(_logger, L"Item does not exist anymore. Aborting current sync and restart "
                                    << Utility::formatSyncPath(_absoluteFilePath));
        return {ExitCode::DataError, ExitCause::NotFound};
    }

    return ExitCode::Ok;
}

ExitInfo UploadJob::handleResponse(std::istream &is) {
    if (const auto exitInfo = AbstractTokenNetworkJob::handleResponse(is); !exitInfo) {
        return exitInfo;
    }

    UploadJobReplyHandler replyHandler(_absoluteFilePath, IoHelper::isLink(_linkType), _creationTimeIn, _modificationTimeIn);
    if (!replyHandler.extractData(jsonRes())) return ExitInfo();
    _nodeIdOut = replyHandler.nodeId();
    _creationTimeOut = replyHandler.creationTime();
    _modificationTimeOut = replyHandler.modificationTime();
    _sizeOut = replyHandler.size();

    return ExitCode::Ok;
}

std::string UploadJob::getSpecificUrl() {
    std::string str = AbstractTokenNetworkJob::getSpecificUrl();
    str += "/upload";
    return str;
}

void UploadJob::setQueryParameters(Poco::URI &uri) {
    if (_fileId.empty()) {
        uri.addQueryParameter("file_name", SyncName2Str(_filename));
        uri.addQueryParameter("directory_id", _remoteParentDirId);
        uri.addQueryParameter(createdAtKey, std::to_string(_creationTimeIn));
        // If an item already exists on the remote side with the same name, we want the backend to return an error.
        // However, in case of conflict with a directory, the backend will change the error resolution to `rename` and
        // automatically rename the uploaded file with a suffix counter (e.g.: test (1).txt)
        uri.addQueryParameter(conflictKey, conflictErrorValue);
    } else {
        uri.addQueryParameter("file_id", _fileId);
    }

    uri.addQueryParameter("total_size", std::to_string(_data.size()));

    uri.addQueryParameter("total_chunk_hash", "xxh3:" + _contentHash);
    uri.addQueryParameter(lastModifiedAtKey, std::to_string(_modificationTimeIn));

    if (IoHelper::isLink(_linkType)) {
        auto str2HtmlStr = [](const std::string &str) { return str.empty() ? "%02%03" : str; };
        uri.addQueryParameter(symbolicLinkKey, str2HtmlStr(Path2Str(_linkTarget)));
    }
}

ExitInfo UploadJob::setData() {
    ItemType itemType;
    if (!IoHelper::getItemType(_absoluteFilePath, itemType)) {
        LOGW_WARN(_logger, L"Error in IoHelper::getItemType - " << Utility::formatSyncPath(_absoluteFilePath));
        return ExitCode::SystemError;
    }

    if (itemType.ioError == IoError::NoSuchFileOrDirectory) {
        LOGW_DEBUG(_logger, L"Item does not exist anymore - " << Utility::formatSyncPath(_absoluteFilePath));
        return {ExitCode::SystemError, ExitCause::NotFound};
    }

    if (itemType.ioError == IoError::AccessDenied) {
        LOGW_DEBUG(_logger, L"Item misses search permission - " << Utility::formatSyncPath(_absoluteFilePath));
        return {ExitCode::SystemError, ExitCause::FileAccessError};
    }

    _linkType = itemType.linkType;
    _targetType = itemType.targetType;

    if (IoHelper::isLink(_linkType)) {
        LOG_DEBUG(_logger, "Read link data - type=" << _linkType);
        if (ExitInfo exitInfo = readLink(); !exitInfo) return exitInfo;
    } else {
        LOG_DEBUG(_logger, "Read file data");
        if (ExitInfo exitInfo = readFile(); !exitInfo) return exitInfo;
    }

    _contentHash = Utility::computeXxHash(_data);
    return ExitCode::Ok;
}

std::string UploadJob::getContentType() {
    switch (_linkType) {
        case LinkType::Symlink:
            return _targetType == NodeType::File ? mimeTypeSymlink : mimeTypeSymlinkFolder;
        case LinkType::Hardlink:
            return mimeTypeHardlink;
        case LinkType::FinderAlias:
            return mimeTypeFinderAlias;
        case LinkType::Junction:
            return mimeTypeJunction;
        default:
            return mimeTypeOctetStream;
    }
}

ExitInfo UploadJob::readFile() {
    // Some applications generate locked temporary files during save operations. To avoid spurious "access denied" errors,
    // we retry for 10 seconds, which is usually sufficient for the application to delete the tmp file. If the file is still
    // locked after 10 seconds, a file access error is displayed to the user. Proper handling is also implemented for
    // "file not found" errors.
    std::ifstream file;
    if (ExitInfo exitInfo = IoHelper::openFile(_absoluteFilePath, file, 10); !exitInfo) {
        LOGW_WARN(_logger, L"Failed to open file " << Utility::formatSyncPath(_absoluteFilePath));
        return exitInfo;
    }

    std::ostringstream ostrm;
    ostrm << file.rdbuf();
    if (ostrm.bad()) {
        // Read/writing error or logical error
        LOGW_WARN(_logger, L"Failed to insert file content into string stream - path=" << Path2WStr(_absoluteFilePath));
        return {ExitCode::SystemError, ExitCause::FileAccessError};
    }

    ostrm.flush();
    if (ostrm.bad()) {
        // Read/writing error or logical error
        LOGW_WARN(_logger, L"Failed to flush string stream - path=" << Path2WStr(_absoluteFilePath));
        return {ExitCode::SystemError, ExitCause::FileAccessError};
    }

    file.close();
    if (file.bad()) {
        // Read/writing error or logical error
        LOGW_WARN(_logger, L"Failed to close file - path=" << Path2WStr(_absoluteFilePath));
        return {ExitCode::SystemError, ExitCause::FileAccessError};
    }

    try {
        _data = ostrm.str();
    } catch (const std::bad_alloc &) {
        LOGW_WARN(_logger, L"Memory allocation error when setting data content - path=" << Path2WStr(_absoluteFilePath));
        return {ExitCode::SystemError, ExitCause::NotEnoughMemory};
    }

    return ExitCode::Ok;
}

ExitInfo UploadJob::readLink() {
    if (_linkType == LinkType::Symlink) {
        std::error_code ec;
        _linkTarget = std::filesystem::read_symlink(_absoluteFilePath, ec);
        if (ec.value() != 0) {
#if defined(KD_WINDOWS)
            bool exists = (ec.value() != ERROR_FILE_NOT_FOUND && ec.value() != ERROR_PATH_NOT_FOUND &&
                           ec.value() != ERROR_INVALID_DRIVE);
#else
            bool exists = (ec.value() != static_cast<int>(std::errc::no_such_file_or_directory));
#endif
            if (!exists) {
                LOGW_DEBUG(_logger, L"File doesn't exist - path=" << Path2WStr(_absoluteFilePath));
                return {ExitCode::SystemError, ExitCause::NotFound};
            }

            LOGW_WARN(_logger, L"Failed to read symlink - path=" << Path2WStr(_absoluteFilePath) << L": "
                                                                 << CommonUtility::s2ws(ec.message()) << L" (" << ec.value()
                                                                 << L")");
            return ExitCode::SystemError;
        }

        _data = Path2Str(_linkTarget);
    } else if (_linkType == LinkType::Hardlink) {
        if (ExitInfo exitInfo = readFile(); !exitInfo) {
            LOGW_WARN(_logger, L"Failed to read file - path=" << Path2WStr(_absoluteFilePath));
            return exitInfo;
        }

        _linkTarget = _absoluteFilePath;
    } else if (_linkType == LinkType::Junction) {
#if defined(KD_WINDOWS)
        IoError ioError = IoError::Success;
        if (!IoHelper::readJunction(_absoluteFilePath, _data, _linkTarget, ioError)) {
            LOGW_WARN(_logger, L"Failed to read junction - " << Utility::formatIoError(_absoluteFilePath, ioError));
            return ExitCode::SystemError;
        }

        if (ioError == IoError::NoSuchFileOrDirectory) {
            LOGW_DEBUG(_logger, L"File doesn't exist - " << Utility::formatSyncPath(_absoluteFilePath));
            return {ExitCode::SystemError, ExitCause::NotFound};
        }

        if (ioError == IoError::AccessDenied) {
            LOGW_DEBUG(_logger, L"File misses search permissions - " << Utility::formatSyncPath(_absoluteFilePath));
            return {ExitCode::SystemError, ExitCause::FileAccessError};
        }
#endif
    } else if (_linkType == LinkType::FinderAlias) {
#if defined(KD_MACOS)
        IoError ioError = IoError::Success;
        if (!IoHelper::readAlias(_absoluteFilePath, _data, _linkTarget, ioError)) {
            LOGW_WARN(_logger, L"Failed to read alias - path=" << Path2WStr(_absoluteFilePath));
            return ExitCode::SystemError;
        }

        if (ioError == IoError::NoSuchFileOrDirectory) {
            LOGW_DEBUG(_logger, L"File doesn't exist - path=" << Path2WStr(_absoluteFilePath));
            return {ExitCode::SystemError, ExitCause::NotFound};
        }

        if (ioError == IoError::AccessDenied) {
            LOGW_DEBUG(_logger, L"File with insufficient access rights - path=" << Path2WStr(_absoluteFilePath));
            return {ExitCode::SystemError, ExitCause::FileAccessError};
        }

        assert(ioError == IoError::Success); // For every other error type, false should have been returned.
#endif
    } else {
        LOG_WARN(_logger, "Link type not managed - type=" << _linkType);
        return ExitCode::SystemError;
    }

    return ExitCode::Ok;
}

} // namespace KDC

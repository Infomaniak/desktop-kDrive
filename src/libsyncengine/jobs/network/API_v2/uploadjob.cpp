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

#include "uploadjob.h"
#include "common/utility.h"
#include "libcommonserver/io/iohelper.h"
#include "libcommonserver/utility/utility.h"
#include "libcommon/utility/jsonparserutility.h"

#include <fstream>

#define TRIALS 5

namespace KDC {

UploadJob::UploadJob(int driveDbId, const SyncPath &filepath, const SyncName &filename, const NodeId &remoteParentDirId,
                     SyncTime modtime) :
    AbstractTokenNetworkJob(ApiType::Drive, 0, 0, driveDbId, 0),
    _filePath(filepath), _filename(filename), _remoteParentDirId(remoteParentDirId), _modtimeIn(modtime) {
    _httpMethod = Poco::Net::HTTPRequest::HTTP_POST;
    _customTimeout = 60;
    _trials = TRIALS;
    setProgress(0);
}

UploadJob::UploadJob(int driveDbId, const SyncPath &filepath, const NodeId &fileId, SyncTime modtime) :
    UploadJob(driveDbId, filepath, SyncName(), "", modtime) {
    _fileId = fileId;
}

UploadJob::~UploadJob() {
    if (_vfsForceStatus) {
        if (!_vfsForceStatus(_filePath, false, 100, true)) {
            LOGW_WARN(_logger, L"Error in vfsForceStatus - path=" << Path2WStr(_filePath).c_str());
        }
    }

    if (_vfsSetPinState) {
        if (!_vfsSetPinState(_filePath, PinState::AlwaysLocal)) {
            LOGW_WARN(_logger, L"Error in vfsSetPinState - path=" << Path2WStr(_filePath).c_str());
        }
    }
}

bool UploadJob::canRun() {
    if (bypassCheck()) {
        return true;
    }

    // Check that the item still exist
    bool exists;
    IoError ioError = IoError::Success;
    if (!IoHelper::checkIfPathExists(_filePath, exists, ioError)) {
        LOGW_WARN(_logger, L"Error in IoHelper::checkIfPathExists: " << Utility::formatIoError(_filePath, ioError));
        _exitCode = ExitCode::SystemError;
        _exitCause = ExitCause::Unknown;
        return false;
    }
    if (ioError == IoError::AccessDenied) {
        LOGW_WARN(_logger, L"Access denied to " << Utility::formatSyncPath(_filePath));
        _exitCode = ExitCode::SystemError;
        _exitCause = ExitCause::FileAccessError;
        return false;
    }

    if (!exists) {
        LOGW_DEBUG(_logger,
                   L"Item does not exist anymore. Aborting current sync and restart " << Utility::formatSyncPath(_filePath));
        _exitCode = ExitCode::NeedRestart;
        _exitCause = ExitCause::UnexpectedFileSystemEvent;
        return false;
    }

    return true;
}

bool UploadJob::handleResponse(std::istream &is) {
    if (!AbstractTokenNetworkJob::handleResponse(is)) {
        return false;
    }

    if (jsonRes()) {
        Poco::JSON::Object::Ptr dataObj = jsonRes()->getObject(dataKey);
        if (dataObj) {
            if (!JsonParserUtility::extractValue(dataObj, idKey, _nodeIdOut)) {
                return false;
            }
            if (!JsonParserUtility::extractValue(dataObj, lastModifiedAtKey, _modtimeOut)) {
                return false;
            }
        }
    }

    return true;
}

std::string UploadJob::getSpecificUrl() {
    std::string str = AbstractTokenNetworkJob::getSpecificUrl();
    str += "/upload";
    return str;
}

void UploadJob::setQueryParameters(Poco::URI &uri, bool &canceled) {
    if (_fileId.empty()) {
        uri.addQueryParameter("file_name", SyncName2Str(_filename));
        uri.addQueryParameter("directory_id", _remoteParentDirId);
    } else {
        uri.addQueryParameter("file_id", _fileId);
    }

    uri.addQueryParameter("total_size", std::to_string(_data.size()));

    uri.addQueryParameter("total_chunk_hash", "xxh3:" + _contentHash);
    uri.addQueryParameter(lastModifiedAtKey, std::to_string(_modtimeIn));

    if (IoHelper::isLink(_linkType)) {
        auto str2HtmlStr = [](const std::string &str) { return str.empty() ? "%02%03" : str; };
        uri.addQueryParameter(symbolicLinkKey, str2HtmlStr(Path2Str(_linkTarget)));
    }

    canceled = false;
}

ExitInfo UploadJob::setData() {
    ItemType itemType;
    if (!IoHelper::getItemType(_filePath, itemType)) {
        LOGW_WARN(_logger, L"Error in IoHelper::getItemType - " << Utility::formatSyncPath(_filePath).c_str());
        return ExitCode::SystemError;
    }

    if (itemType.ioError == IoError::NoSuchFileOrDirectory) {
        LOGW_DEBUG(_logger, L"Item does not exist anymore - " << Utility::formatSyncPath(_filePath).c_str());
        return {ExitCode::SystemError, ExitCause::UnexpectedFileSystemEvent};
    }

    if (itemType.ioError == IoError::AccessDenied) {
        LOGW_DEBUG(_logger, L"Item misses search permission - " << Utility::formatSyncPath(_filePath).c_str());
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

std::string UploadJob::getContentType(bool &canceled) {
    canceled = false;
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
            return AbstractTokenNetworkJob::getContentType(canceled);
    }
}

ExitInfo UploadJob::readFile() {
    // Some applications generate locked temporary files during save operations. To avoid spurious "access denied" errors,
    // we retry for 10 seconds, which is usually sufficient for the application to delete the tmp file. If the file is still
    // locked after 10 seconds, a file access error is displayed to the user. Proper handling is also implemented for
    // "file not found" errors.
    std::ifstream file;
    if (ExitInfo exitInfo = IoHelper::openFile(_filePath, 10, file); !exitInfo) {
        LOGW_WARN(_logger, L"Failed to open file " << Utility::formatSyncPath(_filePath));
        return exitInfo;
    }

    std::ostringstream ostrm;
    ostrm << file.rdbuf();
    if (ostrm.bad()) {
        // Read/writing error or logical error
        LOGW_WARN(_logger, L"Failed to insert file content into string stream - path=" << Path2WStr(_filePath).c_str());
        return {ExitCode::SystemError, ExitCause::FileAccessError};
    }

    ostrm.flush();
    if (ostrm.bad()) {
        // Read/writing error or logical error
        LOGW_WARN(_logger, L"Failed to flush string stream - path=" << Path2WStr(_filePath).c_str());
        return {ExitCode::SystemError, ExitCause::FileAccessError};
    }

    file.close();
    if (file.bad()) {
        // Read/writing error or logical error
        LOGW_WARN(_logger, L"Failed to close file - path=" << Path2WStr(_filePath).c_str());
        return {ExitCode::SystemError, ExitCause::FileAccessError};
    }

    try {
        _data = ostrm.str();
    } catch (const std::bad_alloc &) {
        LOGW_WARN(_logger, L"Memory allocation error when setting data content - path=" << Path2WStr(_filePath).c_str());
        return {ExitCode::SystemError, ExitCause::NotEnoughtMemory};
    }

    return ExitCode::Ok;
}

ExitInfo UploadJob::readLink() {
    if (_linkType == LinkType::Symlink) {
        std::error_code ec;
        _linkTarget = std::filesystem::read_symlink(_filePath, ec);
        if (ec.value() != 0) {
#ifdef _WIN32
            bool exists = (ec.value() != ERROR_FILE_NOT_FOUND && ec.value() != ERROR_PATH_NOT_FOUND &&
                           ec.value() != ERROR_INVALID_DRIVE);
#else
            bool exists = (ec.value() != static_cast<int>(std::errc::no_such_file_or_directory));
#endif
            if (!exists) {
                LOGW_DEBUG(_logger, L"File doesn't exist - path=" << Path2WStr(_filePath).c_str());
                return {ExitCode::SystemError, ExitCause::NotFound};
            }

            LOGW_WARN(_logger, L"Failed to read symlink - path=" << Path2WStr(_filePath).c_str() << L": "
                                                                 << Utility::s2ws(ec.message()).c_str() << L" (" << ec.value()
                                                                 << L")");
            return ExitCode::SystemError;
        }

        _data = Path2Str(_linkTarget);
    } else if (_linkType == LinkType::Hardlink) {
        if (ExitInfo exitInfo = readFile(); !exitInfo) {
            LOGW_WARN(_logger, L"Failed to read file - path=" << Path2WStr(_filePath).c_str());
            return exitInfo;
        }

        _linkTarget = _filePath;
    } else if (_linkType == LinkType::Junction) {
#ifdef _WIN32
        IoError ioError = IoError::Success;
        if (!IoHelper::readJunction(_filePath, _data, _linkTarget, ioError)) {
            LOGW_WARN(_logger, L"Failed to read junction - " << Utility::formatIoError(_filePath, ioError).c_str());
            return ExitCode::SystemError;
        }

        if (ioError == IoError::NoSuchFileOrDirectory) {
            LOGW_DEBUG(_logger, L"File doesn't exist - " << Utility::formatSyncPath(_filePath).c_str());
            return {ExitCode::SystemError, ExitCause::NotFound};
        }

        if (ioError == IoError::AccessDenied) {
            LOGW_DEBUG(_logger, L"File misses search permissions - " << Utility::formatSyncPath(_filePath).c_str());
            _exitCode = ExitCode::SystemError;
            _exitCause = ExitCause::FileAccessError;
            return {ExitCode::SystemError, ExitCause::FileAccessError};
        }
#endif
    } else if (_linkType == LinkType::FinderAlias) {
#ifdef __APPLE__
        IoError ioError = IoError::Success;
        if (!IoHelper::readAlias(_filePath, _data, _linkTarget, ioError)) {
            LOGW_WARN(_logger, L"Failed to read alias - path=" << Path2WStr(_filePath).c_str());
            return ExitCode::SystemError;
        }

        if (ioError == IoError::NoSuchFileOrDirectory) {
            LOGW_DEBUG(_logger, L"File doesn't exist - path=" << Path2WStr(_filePath).c_str());
            return {ExitCode::SystemError, ExitCause::NotFound};
        }

        if (ioError == IoError::AccessDenied) {
            LOGW_DEBUG(_logger, L"File with insufficient access rights - path=" << Path2WStr(_filePath).c_str());
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

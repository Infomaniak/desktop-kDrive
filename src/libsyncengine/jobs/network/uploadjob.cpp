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
#include "utility/jsonparserutility.h"

#include <fstream>

#define TRIALS 5

namespace KDC {

UploadJob::UploadJob(int driveDbId, const SyncPath &filepath, const SyncName &filename, const NodeId &remoteParentDirId,
                     SyncTime modtime)
    : AbstractTokenNetworkJob(ApiDrive, 0, 0, driveDbId, 0),
      _filePath(filepath),
      _filename(filename),
      _remoteParentDirId(remoteParentDirId),
      _modtimeIn(modtime) {
    _httpMethod = Poco::Net::HTTPRequest::HTTP_POST;
    _customTimeout = 60;
    _trials = TRIALS;
    _progress = 0;
}

UploadJob::UploadJob(int driveDbId, const SyncPath &filepath, const NodeId &fileId, SyncTime modtime)
    : UploadJob(driveDbId, filepath, SyncName(), "", modtime) {
    _fileId = fileId;
}

UploadJob::~UploadJob() {
    if (_vfsForceStatus) {
        if (!_vfsForceStatus(_filePath, false, 100, true)) {
            LOGW_WARN(_logger, L"Error in vfsForceStatus - path=" << Path2WStr(_filePath).c_str());
        }
    }

    if (_vfsSetPinState) {
        if (!_vfsSetPinState(_filePath, PinStateAlwaysLocal)) {
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
    IoError ioError = IoErrorSuccess;
    if (!IoHelper::checkIfPathExists(_filePath, exists, ioError)) {
        LOGW_WARN(_logger, L"Error in IoHelper::checkIfPathExists: " << Utility::formatIoError(_filePath, ioError).c_str());
        _exitCode = ExitCodeSystemError;
        _exitCause = ExitCauseFileAccessError;
        return false;
    }

    if (!exists) {
        LOGW_DEBUG(_logger,
                   L"Item does not exist anymore. Aborting current sync and restart - path=" << Path2WStr(_filePath).c_str());
        _exitCode = ExitCodeNeedRestart;
        _exitCause = ExitCauseUnexpectedFileSystemEvent;
        return false;
    }

    return true;
}

void UploadJob::runJob() noexcept {
    if (_vfsForceStatus) {
        if (!_vfsForceStatus(_filePath, true, 0, true)) {
            LOGW_WARN(_logger, L"Error in vfsForceStatus - path=" << Path2WStr(_filePath).c_str());
        }
    }

    AbstractTokenNetworkJob::runJob();
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
        uri.addQueryParameter("conflict", "version");
    } else {
        uri.addQueryParameter("file_id", _fileId);
    }

    uri.addQueryParameter("total_size", std::to_string(_data.size()));

    uri.addQueryParameter("total_chunk_hash", "xxh3:" + _contentHash);
    uri.addQueryParameter(lastModifiedAtKey, std::to_string(_modtimeIn));

    if (IoHelper::isLink(_linkType)) {
        auto str2HtmlStr = [](const std::string str) { return str.empty() ? "%02%03" : str; };
        uri.addQueryParameter(symbolicLinkKey, str2HtmlStr(Path2Str(_linkTarget)));
    }

    canceled = false;
}

void UploadJob::setData(bool &canceled) {
    canceled = true;

    ItemType itemType;
    if (!IoHelper::getItemType(_filePath, itemType)) {
        LOGW_WARN(_logger, L"Error in IoHelper::getItemType - " << Utility::formatSyncPath(_filePath).c_str());
        return;
    }

    if (itemType.ioError == IoErrorNoSuchFileOrDirectory) {
        LOGW_DEBUG(_logger, L"Item does not exist anymore - " << Utility::formatSyncPath(_filePath).c_str());
        return;
    }

    if (itemType.ioError == IoErrorAccessDenied) {
        LOGW_DEBUG(_logger, L"Item misses search permission - " << Utility::formatSyncPath(_filePath).c_str());
        return;
    }

    _linkType = itemType.linkType;
    _targetType = itemType.targetType;

    if (IoHelper::isLink(_linkType)) {
        LOG_DEBUG(_logger, "Read link data - type=" << _linkType);
        if (!readLink()) return;
    } else {
        LOG_DEBUG(_logger, "Read file data");
        if (!readFile()) return;
    }

    _contentHash = Utility::computeXxHash(_data);

    canceled = false;
}

std::string UploadJob::getContentType(bool &canceled) {
    canceled = false;

    if (_linkType == LinkTypeSymlink) {
        return _targetType == NodeTypeFile ? mimeTypeSymlink : mimeTypeSymlinkFolder;
    } else if (_linkType == LinkTypeHardlink) {
        return mimeTypeHardlink;
    } else if (_linkType == LinkTypeFinderAlias) {
        return mimeTypeFinderAlias;
    } else if (_linkType == LinkTypeJunction) {
        return mimeTypeJunction;
    } else {
        return AbstractTokenNetworkJob::getContentType(canceled);
    }
}

bool UploadJob::readFile() {
    std::ifstream file(_filePath, std::ios_base::in | std::ios_base::binary);
    if (!file.is_open()) {
        LOGW_WARN(_logger, L"Failed to open file - path=" << Path2WStr(_filePath).c_str());
        _exitCode = ExitCodeSystemError;
        _exitCause = ExitCauseFileAccessError;
        return false;
    }

    std::ostringstream ostrm;
    ostrm << file.rdbuf();
    if (ostrm.bad()) {
        // Read/writing error or logical error
        LOGW_WARN(_logger, L"Failed to insert file content into string stream - path=" << Path2WStr(_filePath).c_str());
        _exitCode = ExitCodeSystemError;
        _exitCause = ExitCauseFileAccessError;
        return false;
    }

    ostrm.flush();
    if (ostrm.bad()) {
        // Read/writing error or logical error
        LOGW_WARN(_logger, L"Failed to flush string stream - path=" << Path2WStr(_filePath).c_str());
        _exitCode = ExitCodeSystemError;
        _exitCause = ExitCauseFileAccessError;
        return false;
    }

    file.close();
    if (file.bad()) {
        // Read/writing error or logical error
        LOGW_WARN(_logger, L"Failed to close file - path=" << Path2WStr(_filePath).c_str());
        _exitCode = ExitCodeSystemError;
        _exitCause = ExitCauseFileAccessError;
        return false;
    }

    try {
        _data = ostrm.str();
    } catch (const std::bad_alloc &) {
        LOGW_WARN(_logger, L"Memory allocation error when setting data content - path=" << Path2WStr(_filePath).c_str());
        _exitCode = ExitCodeSystemError;
        _exitCause = ExitCauseNotEnoughtMemory;
        return false;
    }

    return true;
}

bool UploadJob::readLink() {
    if (_linkType == LinkTypeSymlink) {
        std::error_code ec;
        _linkTarget = std::filesystem::read_symlink(_filePath, ec);
        if (ec.value() != 0) {
#ifdef _WIN32
            bool exists =
                (ec.value() != ERROR_FILE_NOT_FOUND && ec.value() != ERROR_PATH_NOT_FOUND && ec.value() != ERROR_INVALID_DRIVE);
#else
            bool exists = (ec.value() != static_cast<int>(std::errc::no_such_file_or_directory));
#endif
            if (!exists) {
                LOGW_DEBUG(_logger, L"File doesn't exist - path=" << Path2WStr(_filePath).c_str());
                _exitCode = ExitCodeSystemError;
                _exitCause = ExitCauseFileAccessError;
                return false;
            }

            LOGW_WARN(_logger, L"Failed to read symlink - path=" << Path2WStr(_filePath).c_str() << L": "
                                                                 << Utility::s2ws(ec.message()).c_str() << L" (" << ec.value()
                                                                 << L")");
            _exitCode = ExitCodeSystemError;
            _exitCause = ExitCauseUnknown;
            return false;
        }

        _data = Path2Str(_linkTarget);
    } else if (_linkType == LinkTypeHardlink) {
        if (!readFile()) {
            LOGW_WARN(_logger, L"Failed to read file - path=" << Path2WStr(_filePath).c_str());
            return false;
        }

        _linkTarget = _filePath;
    } else if (_linkType == LinkTypeJunction) {
#ifdef _WIN32
        IoError ioError = IoErrorSuccess;
        if (!IoHelper::readJunction(_filePath, _data, _linkTarget, ioError)) {
            LOGW_WARN(_logger, L"Failed to read junction - " << Utility::formatIoError(_filePath, ioError).c_str());
            _exitCode = ExitCodeSystemError;
            _exitCause = ExitCauseUnknown;

            return false;
        }

        if (ioError == IoErrorNoSuchFileOrDirectory) {
            LOGW_DEBUG(_logger, L"File doesn't exist - " << Utility::formatSyncPath(_filePath).c_str());
            _exitCode = ExitCodeSystemError;
            _exitCause = ExitCauseFileAccessError;
            return false;
        }

        if (ioError == IoErrorAccessDenied) {
            LOGW_DEBUG(_logger, L"File misses search permissions - " << Utility::formatSyncPath(_filePath).c_str());
            _exitCode = ExitCodeSystemError;
            _exitCause = ExitCauseNoSearchPermission;
            return false;
        }
#endif
    } else if (_linkType == LinkTypeFinderAlias) {
#ifdef __APPLE__
        IoError ioError = IoErrorSuccess;
        if (!IoHelper::readAlias(_filePath, _data, _linkTarget, ioError)) {
            LOGW_WARN(_logger, L"Failed to read alias - path=" << Path2WStr(_filePath).c_str());
            _exitCode = ExitCodeSystemError;
            _exitCause = ExitCauseUnknown;

            return false;
        }

        if (ioError == IoErrorNoSuchFileOrDirectory) {
            LOGW_DEBUG(_logger, L"File doesn't exist - path=" << Path2WStr(_filePath).c_str());
            _exitCode = ExitCodeSystemError;
            _exitCause = ExitCauseFileAccessError;

            return false;
        }

        if (ioError == IoErrorAccessDenied) {
            LOGW_DEBUG(_logger, L"File with insufficient access rights - path=" << Path2WStr(_filePath).c_str());
            _exitCode = ExitCodeSystemError;
            _exitCause = ExitCauseNoSearchPermission;

            return false;
        }

        assert(ioError == IoErrorSuccess);  // For every other error type, false should have been returned.
#endif
    } else {
        LOG_WARN(_logger, "Link type not managed - type=" << _linkType);
        return false;
    }

    return true;
}

}  // namespace KDC

// Infomaniak kDrive - Desktop
// Copyright (C) 2023-2025 Infomaniak Network SA
//
// This program is free software: you can redistribute it and/or modify
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

#include "uploadjobreplyhandler.h"

#include "jobs/network/networkjobsparams.h"
#include "utility/jsonparserutility.h"

namespace KDC {

UploadJobReplyHandler::UploadJobReplyHandler(const SyncPath &absoluteFilePath, bool isLink, const SyncTime creationTime,
                                             SyncTime modificationTime) :
    _absoluteFilePath(absoluteFilePath),
    _isLink(isLink),
    _creationTimeIn(creationTime),
    _modificationTimeIn(modificationTime) {}

bool UploadJobReplyHandler::extractData(const Poco::JSON::Object::Ptr jsonRes) {
    if (!jsonRes) return false;

    Poco::JSON::Object::Ptr dataObj = jsonRes->getObject(dataKey);
    if (!dataObj) return false;

    // For UploadSession, the info to extract is encapsulated inside a "file" JSON object.
    if (const Poco::JSON::Object::Ptr fileObj = dataObj->getObject(fileKey); fileObj) {
        dataObj = fileObj;
    }

    if (!JsonParserUtility::extractValue(dataObj, idKey, _nodeIdOut)) return false;
    if (!JsonParserUtility::extractValue(dataObj, createdAtKey, _creationTimeOut)) return false;
    if (!JsonParserUtility::extractValue(dataObj, lastModifiedAtKey, _modificationTimeOut)) return false;
    if (!JsonParserUtility::extractValue(dataObj, sizeKey, _sizeOut)) return false;

    if (_creationTimeIn != _creationTimeOut || _modificationTimeIn != _modificationTimeOut) {
        // The backend refused the creation/modification time(s). To avoid further EDIT operations, we apply the backend's times
        // on local file.
        LOGW_INFO(Log::instance()->getLogger(), L"Applying backend creation/modification times."
                                                        << L" Sent values: " << _creationTimeIn << L"/" << _modificationTimeIn
                                                        << L" Returned values: " << _creationTimeOut << L"/"
                                                        << _modificationTimeOut << L" for "
                                                        << Utility::formatSyncPath(_absoluteFilePath));

        if (const IoError ioError = IoHelper::setFileDates(_absoluteFilePath, _creationTimeOut, _modificationTimeOut, _isLink);
            ioError != IoError::Success) {
            LOGW_WARN(Log::instance()->getLogger(),
                      L"Error in IoHelper::setFileDates: " << Utility::formatIoError(_absoluteFilePath, ioError));
        }
#if defined(KD_LINUX)
        _creationTimeOut = _creationTimeIn; // Unix systems do not support setting creation time, so we keep the original value.
#endif
    }
    return true;
}

} // namespace KDC

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

#include "jobs/network/kDrive_API/filelistjob.h"

#include "jobs/network/kDrive_API/apitranslator.h"
#include "jobs/network/jobexceptions.h"

namespace KDC {

FileListJob::FileListJob(const UserDbId userDbId, const DriveId driveId, RemoteNodeId fileId,
                         const TranslationMode translationMode /* TranslationMode::None */) :
    _userDbId{userDbId},
    _driveId{driveId},
    _fileId(std::move(fileId)),
    _translationMode(translationMode) {
    assert(_userDbId > 0 && "Invalid user DB ID.");
    assert(_driveId > 0 && "Invalid drive ID.");
}

FileListJob::FileListJob(const DriveDbId driveDbId, RemoteNodeId fileId,
                         const TranslationMode translationMode /* TranslationMode::None */) :
    _driveDbId{driveDbId},
    _fileId(std::move(fileId)),
    _translationMode(translationMode) {
    assert(_driveDbId > 0 && "Invalid drive DB ID.");
}

std::string FileListJob::createLogMessage(const std::string &coreMsg) const {
    std::stringstream ss;

    if (_driveDbId) {
        ss << coreMsg << " driveDbId=" << _driveDbId;
    } else {
        ss << coreMsg << " userDbId=" << _userDbId << " driveId=" << _driveId;
    }

    return ss.str();
}

std::string FileListJob::getConstructorFailureLogMessage(const std::exception &e) const {
    return createLogMessage(getConstructorFailureCoreMsg()) + " error=" + e.what();
}

std::string FileListJob::getRunSynchronouslyFailureLogMessage(const ExitInfo &exitInfo) const {
    return createLogMessage(getRunSynchronouslyFailureCoreMsg()) + " exitInfo:" + toString(exitInfo);
}

ExitInfo FileListJob::v2RemoteNodeInfoList(RemoteNodeInfoList &remoteNodeInfoList) const {
    remoteNodeInfoList = _remoteNodeInfoList;
    return ApiTranslator::translateV3ToV2(_userDbId, _driveId, remoteNodeInfoList);
}

} // namespace KDC

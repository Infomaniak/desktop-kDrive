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

#include "jobs/network/kDrive_API/filelistjob.h"
#include "jobs/network/kDrive_API/getfilesindirectoryjob.h"

namespace KDC {

FileListJob::FileListJob(const int userDbId, const DriveId driveId, NodeId fileId, const bool translateV2ToV3) :
    _userDbId{userDbId},
    _driveId{driveId},
    _fileId(std::move(fileId)),
    _translateV2ToV3(translateV2ToV3) {
    assert(_userDbId > 0 && "Invalid user DB ID.");
    assert(_driveId > 0 && "Invalid drive ID.");
}

FileListJob::FileListJob(const int driveDbId, NodeId fileId, const bool translateV2ToV3 /*= false */) :
    _driveDbId{driveDbId},
    _fileId(std::move(fileId)),
    _translateV2ToV3(translateV2ToV3) {
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
    return createLogMessage(getConstructorFailureCoreMsg()) + " exitInfo:" + toString(exitInfo);
}

} // namespace KDC

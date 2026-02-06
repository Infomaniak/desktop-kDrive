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

#include "jobs/network/apitranslator.h"
#include "jobs/network/kDrive_API/getfilelistjob.h"
#include "jobs/network/kDrive_API/getfilesindirectoryjob.h"

namespace KDC {

ApiTranslator::NodeIdCacheMap ApiTranslator::_rootNodeIdCache = {};

ApiTranslator::ApiTranslator() {}

ApiTranslator::~ApiTranslator() {}

ApiTranslator::DriveId ApiTranslator::getDriveId(DriveDbId driveDbId) {
    Drive drive;
    bool found = false;
    if (!ParmsDb::instance()->selectDrive(driveDbId, drive, found)) {
        assert(false);
    }
    if (!found) {
        assert(false);
    }

    return drive.driveId();
}

NodeId ApiTranslator::getUserPrivateRootFolderId(const DriveDbId driveDbId) {
    const auto driveId = getDriveId(driveDbId);

    if (const auto it = _rootNodeIdCache.find(driveId); it != _rootNodeIdCache.cend()) {
        return it->second;
    }

    GetFilesInDirectoryJob fileListJob(driveDbId, NodeId{"1"}, true);
    fileListJob.runSynchronously();

    Poco::JSON::Object::Ptr resObj = fileListJob.jsonRes();

    return {};
}

} // namespace KDC

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
#include "jobs/network/kDrive_API/getallfilesindirectoryjob.h"

namespace KDC {

std::mutex ApiTranslator::_mutex;

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

NodeId ApiTranslator::getRootFolderId(const DriveDbId driveDbId, const NodeInfoList &nodeInfoList) {
    const auto it = std::find_if(nodeInfoList.cbegin(), nodeInfoList.cend(),
                                 [](const NodeInfo &nodeInfo) { return nodeInfo.name() == "Private"; });

    if (it != nodeInfoList.cend()) {
        const std::scoped_lock lock(_mutex);
        _rootNodeIdCache[driveDbId] = it->nodeId().toStdString();

        return _rootNodeIdCache[driveDbId];
    }

    return {};
}

NodeId ApiTranslator::getUserPrivateRootFolderId(const DriveDbId driveDbId) {
    constexpr auto maxNumberOfItems = 1000;
    {
        const std::scoped_lock lock(_mutex);
        if (const auto it = _rootNodeIdCache.find(driveDbId); it != _rootNodeIdCache.cend()) {
            return it->second;
        }
    }

    GetAllFilesInDirectoryJob fileListJob(driveDbId, NodeId{"1"});
    fileListJob.setListingConf({.limit = maxNumberOfItems});
    fileListJob.runSynchronously();

    const auto &nodeInfoList = fileListJob.nodeInfoList();
    if (const auto rootFolderId = getRootFolderId(driveDbId, nodeInfoList); !rootFolderId.empty()) return rootFolderId;

    return {};
}

void ApiTranslator::translateV2ToV3(const DriveDbId driveDbId, NodeId &remoteDirectoryId) {
    if (remoteDirectoryId != "1") return;

    remoteDirectoryId = getUserPrivateRootFolderId(driveDbId);
}

} // namespace KDC

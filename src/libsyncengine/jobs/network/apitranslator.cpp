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

ApiTranslator::RemoteNodeIdCacheMap ApiTranslator::_rootNodeIdCache = {};
ApiTranslator::RemoteNodeIdCacheMap ApiTranslator::_commonDocumentsNodeIdCache = {};
ApiTranslator::RemoteNodeIdCacheMap ApiTranslator::_sharedNodeIdCache = {};

ApiTranslator::ApiTranslator() {}

ApiTranslator::~ApiTranslator() {}

DriveId ApiTranslator::getDriveId(DriveDbId driveDbId) {
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

void ApiTranslator::updateCache(const DriveDbId driveDbId) {
    constexpr auto maxNumberOfItems = 1000;

    GetAllFilesInDirectoryJob fileListJob(driveDbId, NodeId{"1"});
    fileListJob.setListingConf({.dirOnly = true, .limit = maxNumberOfItems});
    fileListJob.runSynchronously(); // Consider logging

    const auto &nodeInfoList = fileListJob.nodeInfoList();

    const std::scoped_lock lock(_mutex);

    auto it = std::find_if(nodeInfoList.cbegin(), nodeInfoList.cend(),
                           [](const NodeInfo &nodeInfo) { return nodeInfo.name() == "Private"; });


    if (it != nodeInfoList.cend()) _rootNodeIdCache[driveDbId] = it->nodeId().toStdString();

    it = std::find_if(nodeInfoList.cbegin(), nodeInfoList.cend(),
                      [](const NodeInfo &nodeInfo) { return nodeInfo.name() == "Shared"; });

    if (it != nodeInfoList.cend()) _sharedNodeIdCache[driveDbId] = it->nodeId().toStdString();


    it = std::find_if(nodeInfoList.cbegin(), nodeInfoList.cend(),
                      [](const NodeInfo &nodeInfo) { return nodeInfo.name() == "Common documents"; });

    if (it != nodeInfoList.cend()) _commonDocumentsNodeIdCache[driveDbId] = it->nodeId().toStdString();
}

RemoteNodeId ApiTranslator::getValue(const DriveDbId driveDbId, const RemoteNodeIdCacheMap &cache) {
    {
        const std::scoped_lock lock(_mutex);
        if (const auto it = cache.find(driveDbId); it != cache.cend()) {
            return it->second;
        }
    }

    return {};
}

RemoteNodeId ApiTranslator::getUserPrivateFolderRemoteId(const DriveDbId driveDbId) {
    if (const auto value = getValue(driveDbId, _rootNodeIdCache); value.empty())
        updateCache(driveDbId);
    else
        return value;

    return getValue(driveDbId, _rootNodeIdCache);
}

RemoteNodeId ApiTranslator::getCommonDocumentsRemoteId(DriveDbId driveDbId) {
    if (const auto value = getValue(driveDbId, _commonDocumentsNodeIdCache); value.empty())
        updateCache(driveDbId);
    else
        return value;

    return getValue(driveDbId, _commonDocumentsNodeIdCache);
}

RemoteNodeId ApiTranslator::getSharedRemoteId(DriveDbId driveDbId) {
    if (const auto value = getValue(driveDbId, _sharedNodeIdCache); value.empty())
        updateCache(driveDbId);
    else
        return value;

    return getValue(driveDbId, _sharedNodeIdCache);
}

void ApiTranslator::translateV2ToV3(const DriveDbId driveDbId, RemoteNodeId &remoteDirectoryId) {
    if (remoteDirectoryId != "1") return;

    remoteDirectoryId = getUserPrivateFolderRemoteId(driveDbId);
}

void ApiTranslator::translateV3ToV2(SyncPath &remotePath) {
    if (remotePath.empty() || *remotePath.begin() != "Private") return;

    remotePath = std::filesystem::relative(remotePath, "Private");
}

void ApiTranslator::translateV3ToV2(const DriveDbId driveDbId, NodeId &remoteNodeId) {
    if (remoteNodeId != getUserPrivateFolderRemoteId(driveDbId)) return;

    remoteNodeId = NodeId{"1"};
}

} // namespace KDC

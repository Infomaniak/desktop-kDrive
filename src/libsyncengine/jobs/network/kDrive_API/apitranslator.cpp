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

#include "jobs/network/kDrive_API/apitranslator.h"
#include "jobs/network/kDrive_API/getallfilesindirectoryjob.h"

namespace KDC {
const RemoteNodeId ApiTranslator::v2RootFolderRemoteId = RemoteNodeId{"1"};
const char *ApiTranslator::v3CommonDocuments = "Common documents";
const char *ApiTranslator::v3UserPrivate = "Private";
const char *ApiTranslator::v3Shared = "Shared";

std::mutex ApiTranslator::_mutex;

ApiTranslator::RemoteNodeIdCacheMap ApiTranslator::_rootNodeIdCache = {};
ApiTranslator::RemoteNodeIdCacheMap ApiTranslator::_commonDocumentsNodeIdCache = {};
ApiTranslator::RemoteNodeIdCacheMap ApiTranslator::_sharedNodeIdCache = {};

DriveId ApiTranslator::getDriveId(const DriveDbId driveDbId) {
    Drive drive;
    bool found = false;
    if (!ParmsDb::instance()->selectDrive(static_cast<int>(driveDbId), drive, found)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectDrive for driveDbId=" << driveDbId);
        assert(false);
    }
    if (!found) {
        LOG_WARN(Log::instance()->getLogger(), "Drive not found for driveDbId=" << driveDbId);
        assert(false);
    }

    return drive.driveId();
}

bool ApiTranslator::getDriveDbIds(DriveDbIdMap &driveDbIdMap) {
    driveDbIdMap.clear();
    DriveList driveList;
    if (!ParmsDb::instance()->selectAllDrives(driveList)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectAllDrives.");
        return false;
    }

    for (const auto &drive: driveList) {
        (void) driveDbIdMap.insert({drive.driveId(), drive.dbId()});
    }

    return true;
}

DriveDbId ApiTranslator::getDriveDbId(const DriveId driveId) {
    DriveDbIdMap driveDbIdMap;
    if (!getDriveDbIds(driveDbIdMap)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ApiTranslator::getDriveDbIds.");
        return -1;
    }

    if (const auto it = driveDbIdMap.find(driveId); it != driveDbIdMap.cend()) {
        return it->second;
    }

    return -1;
}

void ApiTranslator::updateCache(const DriveDbId driveDbId) {
    constexpr auto maxNumberOfItems = 1000;

    GetAllFilesInDirectoryJob fileListJob(driveDbId, NodeId{"1"}, TranslationMode::None);
    fileListJob.setListingConf({.dirOnly = true, .limit = maxNumberOfItems});
    if (const auto exitInfo = fileListJob.runSynchronously(); !exitInfo) {
        LOG_WARN(Log::instance()->getLogger(), "Error in GetAllFilesInDirectoryJob::runSynchronously.");
        return;
    }

    const auto &nodeInfoList = fileListJob.v3RemoteNodeInfoList();

    const std::scoped_lock lock(_mutex);

    auto it = std::find_if(nodeInfoList.cbegin(), nodeInfoList.cend(),
                           [](const NodeInfo &nodeInfo) { return nodeInfo.name() == v3UserPrivate; });


    if (it != nodeInfoList.cend()) _rootNodeIdCache[driveDbId] = it->nodeId().toStdString();

    it = std::find_if(nodeInfoList.cbegin(), nodeInfoList.cend(),
                      [](const NodeInfo &nodeInfo) { return nodeInfo.name() == v3Shared; });

    if (it != nodeInfoList.cend()) _sharedNodeIdCache[driveDbId] = it->nodeId().toStdString();


    it = std::find_if(nodeInfoList.cbegin(), nodeInfoList.cend(),
                      [](const NodeInfo &nodeInfo) { return nodeInfo.name() == v3CommonDocuments; });

    if (it != nodeInfoList.cend()) _commonDocumentsNodeIdCache[driveDbId] = it->nodeId().toStdString();
}

RemoteNodeId ApiTranslator::getValue(const DriveDbId driveDbId, const RemoteNodeIdCacheMap &cache) {
    const std::scoped_lock lock(_mutex);
    if (const auto it = cache.find(driveDbId); it != cache.cend()) return it->second;

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
    if (remoteDirectoryId != v2RootFolderRemoteId) return;

    remoteDirectoryId = getUserPrivateFolderRemoteId(driveDbId);
}

void ApiTranslator::translateV3ToV2(SyncPath &remotePath) {
    if (remotePath.empty() || *remotePath.begin() != v3UserPrivate) return;

    remotePath = std::filesystem::relative(remotePath, v3UserPrivate);
    if (remotePath == ".") remotePath = SyncPath{};
}

void ApiTranslator::translateV3ToV2(const DriveDbId driveDbId, NodeId &remoteNodeId) {
    if (remoteNodeId != getUserPrivateFolderRemoteId(driveDbId)) return;

    remoteNodeId = v2RootFolderRemoteId;
}

void ApiTranslator::translateV3ToV2(const DriveDbId driveDbId, RemoteNodeInfoList &v3RemoteNodeInfoList) {
    const RemoteNodeId privateFolderId = getUserPrivateFolderRemoteId(driveDbId);
    (void) std::erase_if(v3RemoteNodeInfoList, [&privateFolderId](const NodeInfo &nodeInfo) {
        return nodeInfo.nodeId().toStdString() == privateFolderId;
    });

    for (auto &nodeInfo: v3RemoteNodeInfoList) {
        if (nodeInfo.parentNodeId().toStdString() == privateFolderId)
            nodeInfo.setParentNodeId(QString::fromStdString(v2RootFolderRemoteId));
    }
}

} // namespace KDC
